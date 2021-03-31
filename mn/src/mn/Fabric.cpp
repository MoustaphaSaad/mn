#include "mn/Fabric.h"
#include "mn/Memory.h"
#include "mn/Pool.h"
#include "mn/Buf.h"
#include "mn/Log.h"

#include <atomic>
#include <chrono>
#include <thread>

namespace mn
{
	constexpr static auto DEFAULT_COOP_BLOCKING_THRESHOLD = 10;
	constexpr static auto DEFAULT_EXTR_BLOCKING_THRESHOLD = 1000;

	using Job = Task<void()>;

	// Worker
	struct IWorker
	{
		enum STATE
		{
			STATE_RUNNING,
			STATE_PAUSED,
			STATE_STOP_REQUEST,
			STATE_STOP_ACKNOWLEDGED
		};

		Str name;
		Mutex mtx;
		Cond_Var cv;
		Fabric fabric;
		Ring<Job> job_q;
		Thread thread;
		std::atomic<uint64_t> atomic_job_start_time_in_ms;
		std::atomic<uint64_t> atomic_block_start_time_in_ms;
		std::atomic<STATE> atomic_state;
		std::atomic<bool> atomic_disable_block_timing;
	};
	thread_local Worker LOCAL_WORKER = nullptr;

	struct IFabric
	{
		Fabric_Settings settings;
		Str name;
		Str sysmon_name;

		Buf<Worker> workers;
		Buf<Worker> sleepy_side_workers;
		Buf<Worker> ready_side_workers;

		Mutex mtx;
		Cond_Var cv;
		bool is_running;
		std::atomic<size_t> atomic_available_jobs;
		size_t next_worker;
		size_t worker_id_generator;

		Thread sysmon;
	};

	static void
	_worker_main(void* worker)
	{
		auto self = (Worker)worker;
		LOCAL_WORKER = self;

		if (self->fabric)
			if (self->fabric->settings.on_worker_start)
				self->fabric->settings.on_worker_start();

		while(true)
		{
			auto state = self->atomic_state.load();
			if (state == IWorker::STATE_RUNNING)
			{
				Job job{};
				{
					mutex_lock(self->mtx);
					mn_defer(mutex_unlock(self->mtx));

					if (self->job_q.count == 0)
					{
						cond_var_wait(self->cv, self->mtx, [&]{
							return self->job_q.count > 0 ||
								self->atomic_state.load() != IWorker::STATE_RUNNING;
						});
						state = self->atomic_state.load();
					}

					if (state != IWorker::STATE_RUNNING)
						continue;

					job = ring_front(self->job_q);
					ring_pop_front(self->job_q);
				}

				if (job)
				{
					self->atomic_job_start_time_in_ms.store(time_in_millis());
					self->atomic_disable_block_timing = false;
					job();
					self->atomic_disable_block_timing = true;
					self->atomic_job_start_time_in_ms.store(0);
					task_free(job);
					memory::tmp()->clear_all();
					if (self->fabric)
					{
						if (self->fabric->settings.after_each_job)
							self->fabric->settings.after_each_job();
						self->fabric->atomic_available_jobs.fetch_sub(1);
					}
				}
			}
			else if (state == IWorker::STATE_PAUSED)
			{
				mutex_lock(self->mtx);
				mn_defer(mutex_unlock(self->mtx));

				if (self->atomic_state.load() == IWorker::STATE_PAUSED)
				{
					cond_var_wait(self->cv, self->mtx, [&]{
						return self->atomic_state.load() != IWorker::STATE_PAUSED;
					});
				}
			}
			else if (state == IWorker::STATE_STOP_REQUEST)
			{
				break;
			}
			else
			{
				assert(false && "unreachable");
			}
		}

		[[maybe_unused]] auto old_state = self->atomic_state.exchange(IWorker::STATE_STOP_ACKNOWLEDGED);
		assert(old_state == IWorker::STATE_STOP_REQUEST);
		LOCAL_WORKER = nullptr;
	}

	inline static void
	_worker_stop(Worker self)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		self->atomic_state = IWorker::STATE_STOP_REQUEST;
		cond_var_notify(self->cv);
	}

	inline static void
	_worker_pause(Worker self)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		assert(self->atomic_state == IWorker::STATE_RUNNING);

		self->atomic_state = IWorker::STATE_PAUSED;
		cond_var_notify(self->cv);
	}

	inline static void
	_worker_resume(Worker self)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		assert(self->atomic_state == IWorker::STATE_PAUSED);

		self->atomic_state = IWorker::STATE_RUNNING;
		cond_var_notify(self->cv);
	}

	inline static Worker
	_worker_new(mn::Str name, Fabric fabric, Ring<Job> stolen_jobs = ring_new<Job>())
	{
		auto self = alloc_zerod<IWorker>();
		self->name = name;
		self->mtx = mn_mutex_new_with_srcloc(self->name.ptr);
		self->cv = cond_var_new();
		self->fabric = fabric;
		self->job_q = stolen_jobs;
		self->atomic_state = IWorker::STATE_RUNNING;
		self->atomic_disable_block_timing = true;
		self->thread = thread_new(_worker_main, self, self->name.ptr);
		return self;
	}

	inline static void
	_worker_free(Worker self)
	{
		[[maybe_unused]] auto state = self->atomic_state.load();
		assert(state == IWorker::STATE_STOP_REQUEST ||
			   state == IWorker::STATE_STOP_ACKNOWLEDGED);

		thread_join(self->thread);
		thread_free(self->thread);

		str_free(self->name);
		mutex_free(self->mtx);
		cond_var_free(self->cv);
		destruct(self->job_q);

		free(self);
	}

	// Fabric
	struct Blocking_Worker
	{
		Worker worker;
		size_t index;
	};

	static void
	_sysmon_main(void* fabric)
	{
		_disable_profiling_for_this_thread();

		auto self = (Fabric)fabric;

		auto blocking_workers = buf_with_capacity<Blocking_Worker>(self->workers.count);
		mn_defer(buf_free(blocking_workers));

		auto dead_workers = buf_with_capacity<Worker>(self->workers.count);
		mn_defer(destruct(dead_workers));

		auto tmp_jobs = buf_new<Job>();
		mn_defer(destruct(tmp_jobs));

		auto timeslice = self->settings.coop_blocking_threshold_in_ms;
		if (timeslice > self->settings.external_blocking_threshold_in_ms)
			timeslice = self->settings.external_blocking_threshold_in_ms;

		timeslice /= 2;
		if (timeslice == 0)
			timeslice = 1;

		while(true)
		{
			// dispose of dead workers before holding the mutex
			buf_remove_if(dead_workers, [](Worker worker){
				auto state = worker->atomic_state.load();

				if (state == IWorker::STATE_STOP_REQUEST)
					return false;

				assert(state == IWorker::STATE_STOP_ACKNOWLEDGED);
				_worker_free(worker);
				return true;
			});

			bool slept_on_cond_var = false;

			{
				mutex_lock(self->mtx);
				mn_defer(mutex_unlock(self->mtx));

				if (self->atomic_available_jobs.load() == 0 &&
					self->sleepy_side_workers.count == 0)
				{
					slept_on_cond_var = true;
					cond_var_wait(self->cv, self->mtx, [&]{
						return self->atomic_available_jobs.load() > 0 ||
							self->is_running == false ||
							self->sleepy_side_workers.count > 0;
					});
				}

				if (self->is_running == false)
					return;
			}

			// SYSMON rest station, sysmon needs to sleep for some time, he does a lot of work, he deserves it
			if (slept_on_cond_var == false)
				thread_sleep(timeslice);

			// get the max/min jobs
			size_t busiest_worker = 0;
			size_t max_jobs = 0;
			size_t idle_worker = SIZE_MAX;
			size_t min_jobs = SIZE_MAX;
			for (size_t i = 0; i < self->workers.count; ++i)
			{
				auto worker = self->workers[i];

				mutex_lock(worker->mtx);
				mn_defer(mutex_unlock(worker->mtx));

				if (worker->job_q.count > max_jobs)
				{
					busiest_worker = i;
					max_jobs = worker->job_q.count;
				}

				if (worker->atomic_job_start_time_in_ms.load() == 0 && worker->job_q.count < min_jobs)
				{
					idle_worker = i;
					min_jobs = worker->job_q.count;
				}
			}

			// steal from the max jobs and give to min jobs, if we have a worker that's idle
			if (idle_worker < self->workers.count && max_jobs > min_jobs && min_jobs == 0)
			{
				{
					auto max_worker = self->workers[busiest_worker];

					mutex_lock(max_worker->mtx);
					mn_defer(mutex_unlock(max_worker->mtx));

					max_jobs = max_worker->job_q.count;
					size_t job_steal_count = max_jobs;
					if (job_steal_count > 1)
						job_steal_count /= 2;

					for (size_t i = 0; i < job_steal_count; ++i)
					{
						auto job = ring_back(max_worker->job_q);
						ring_pop_back(max_worker->job_q);

						buf_push(tmp_jobs, job);
					}
				}

				{
					auto min_worker = self->workers[idle_worker];

					mutex_lock(min_worker->mtx);
					mn_defer(mutex_unlock(min_worker->mtx));

					for (auto job: tmp_jobs)
						ring_push_back(min_worker->job_q, job);
					buf_clear(tmp_jobs);

					cond_var_notify(min_worker->cv);
				}
			}

			// check if any sleepy worker is ready and move it either to the ready workers list
			// or free it because we don't really need it
			buf_remove_if(self->sleepy_side_workers, [self, &dead_workers](Worker worker) {
				if (worker->atomic_job_start_time_in_ms.load() == 0)
				{
					if (self->ready_side_workers.count < self->settings.put_aside_worker_count)
					{
						buf_push(self->ready_side_workers, worker);
					}
					else
					{
						_worker_stop(worker);
						buf_push(dead_workers, worker);
					}
					return true;
				}
				return false;
			});

			// detect blocking workers
			for (size_t i = 0; i < self->workers.count; ++i)
			{
				auto block_start_time = self->workers[i]->atomic_block_start_time_in_ms.load();
				if(block_start_time != 0)
				{
					auto block_time = time_in_millis() - block_start_time;
					if(block_time > self->settings.coop_blocking_threshold_in_ms)
					{
						buf_push(blocking_workers, Blocking_Worker{ self->workers[i], i });
						continue;
					}
				}

				auto job_start_time = self->workers[i]->atomic_job_start_time_in_ms.load();
				if (job_start_time != 0)
				{
					auto job_run_time = time_in_millis() - job_start_time;
					if(job_run_time > self->settings.external_blocking_threshold_in_ms)
					{
						buf_push(blocking_workers, Blocking_Worker{ self->workers[i], i });
						continue;
					}
				}
			}

			// if we have some free workers then it's okay, ignore it this is normal
			// we only care about total system blocking
			if (blocking_workers.count < self->workers.count * self->settings.blocking_workers_threshold)
				buf_clear(blocking_workers);

			// pause all the blocking workers
			for (auto blocking_worker: blocking_workers)
				_worker_pause(blocking_worker.worker);

			// move the blocking workers out
			for (auto blocking_worker: blocking_workers)
			{
				Ring<Job> job_q{};
				{
					mutex_lock(blocking_worker.worker->mtx);
					mn_defer(mutex_unlock(blocking_worker.worker->mtx));

					job_q = blocking_worker.worker->job_q;
					blocking_worker.worker->job_q = ring_new<Job>();
				}

				{
					mutex_lock(self->mtx);
					mn_defer(mutex_unlock(self->mtx));

					// find a suitable worker
					if (self->ready_side_workers.count > 0)
					{
						auto new_worker = buf_top(self->ready_side_workers);
						buf_pop(self->ready_side_workers);

						self->workers[blocking_worker.index] = new_worker;
						ring_free(new_worker->job_q);
						new_worker->job_q = job_q;

						_worker_resume(new_worker);
					}
					else
					{
						auto new_worker = _worker_new(
							strf("{} worker #{}", self->name, self->worker_id_generator++),
							self,
							job_q
						);

						self->workers[blocking_worker.index] = new_worker;
					}
				}
			}

			// now that we have replaced all the blocking workers with a newly created workers
			// we need to store the blocking workers away to be reused later in the replacement above
			for (auto blocking_worker: blocking_workers)
				buf_push(self->sleepy_side_workers, blocking_worker.worker);

			// clear the blocking workers list
			buf_clear(blocking_workers);
		}
	}


	// API
	Worker
	worker_new(const char* name)
	{
		return _worker_new(mn::str_from_c(name), nullptr);
	}

	void
	worker_free(Worker self)
	{
		_worker_stop(self);
		_worker_free(self);
	}

	void
	worker_task_do(Worker self, const Task<void()>& task)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		ring_push_back(self->job_q, task);
		cond_var_notify(self->cv);
	}

	Worker
	worker_local()
	{
		return LOCAL_WORKER;
	}

	void
	worker_block_ahead()
	{
		if (LOCAL_WORKER == nullptr)
			return;

		if (LOCAL_WORKER->atomic_disable_block_timing.load() == true)
			return;

		LOCAL_WORKER->atomic_block_start_time_in_ms.store(time_in_millis());
	}

	void
	worker_block_clear()
	{
		if (LOCAL_WORKER == nullptr)
			return;

		if (LOCAL_WORKER->atomic_disable_block_timing.load() == true)
			return;

		LOCAL_WORKER->atomic_block_start_time_in_ms.store(0);
	}


	// fabric
	Fabric
	fabric_new(Fabric_Settings settings)
	{
		if (settings.name == nullptr)
			settings.name = "fabric";

		if (settings.workers_count == 0)
			settings.workers_count = std::thread::hardware_concurrency();
		if (settings.coop_blocking_threshold_in_ms == 0)
			settings.coop_blocking_threshold_in_ms = DEFAULT_COOP_BLOCKING_THRESHOLD;
		if (settings.external_blocking_threshold_in_ms == 0)
			settings.external_blocking_threshold_in_ms = DEFAULT_EXTR_BLOCKING_THRESHOLD;
		if (settings.put_aside_worker_count == 0)
			settings.put_aside_worker_count = settings.workers_count / 2;
		if (settings.blocking_workers_threshold == 0.0f)
			settings.blocking_workers_threshold = 0.5f;


		auto self = alloc_zerod<IFabric>();
		self->settings = settings;
		self->name = strf("{}", settings.name);
		self->sysmon_name = strf("{} sysmon thread", settings.name);
		self->workers = buf_with_count<Worker>(self->settings.workers_count);
		self->sleepy_side_workers = buf_new<Worker>();
		self->ready_side_workers = buf_new<Worker>();
		self->mtx = mn_mutex_new_with_srcloc(self->name.ptr);
		self->cv = cond_var_new();
		self->is_running = true;
		self->atomic_available_jobs = 0;
		self->next_worker = 0;
		self->worker_id_generator = 0;

		for (size_t i = 0; i < self->workers.count; ++i)
		{
			self->workers[i] = _worker_new(
				strf("{} worker #{}", self->name, self->worker_id_generator++),
				self
			);
		}

		self->sysmon = thread_new(_sysmon_main, self, self->sysmon_name.ptr);

		return self;
	}

	void
	fabric_free(Fabric self)
	{
		{
			mutex_lock(self->mtx);
			mn_defer(mutex_unlock(self->mtx));

			self->is_running = false;
			cond_var_notify(self->cv);
		}

		thread_join(self->sysmon);
		thread_free(self->sysmon);

		for (auto worker : self->workers)
			_worker_stop(worker);

		for (auto worker : self->sleepy_side_workers)
			_worker_stop(worker);

		for (auto worker : self->ready_side_workers)
			_worker_stop(worker);

		for (auto worker : self->workers)
			_worker_free(worker);
		buf_free(self->workers);

		for (auto worker : self->sleepy_side_workers)
			_worker_free(worker);
		buf_free(self->sleepy_side_workers);

		for (auto worker : self->ready_side_workers)
			_worker_free(worker);
		buf_free(self->ready_side_workers);

		cond_var_free(self->cv);
		mutex_free(self->mtx);
		str_free(self->name);
		str_free(self->sysmon_name);
		task_free(self->settings.after_each_job);
		task_free(self->settings.on_worker_start);
		free(self);
	}

	void
	fabric_task_do(Fabric self, const Task<void()>& task)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		auto next_worker = self->next_worker++;
		next_worker %= self->workers.count;

		auto worker = self->workers[next_worker];
		worker_task_do(worker, task);

		self->atomic_available_jobs.fetch_add(1);
		cond_var_notify(self->cv);
	}

	Fabric
	fabric_local()
	{
		Fabric res = nullptr;
		if (auto w = worker_local())
			res = w->fabric;
		return res;
	}

	void
	_multi_threaded_compute(Fabric self, Compute_Dims global, Compute_Dims local, Task<void(Compute_Args)> fn)
	{
		Auto_Waitgroup wg;
		for (size_t z = 0; z < global.z; ++z)
		{
			for (size_t y = 0; y < global.y; ++y)
			{
				for (size_t x = 0; x < global.x; ++x)
				{
					wg.add(1);
					fabric_do(self, [global_x = x, global_y = y, global_z = z, global, local, &wg, &fn] {
						for (size_t z = 0; z < local.z; ++z)
						{
							for (size_t y = 0; y < local.y; ++y)
							{
								for (size_t x = 0; x < local.x; ++x)
								{
									Compute_Args args;
									args.workgroup_size = local;
									args.workgroup_num = global;
									args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
									args.local_invocation_id = Compute_Dims{ x, y, z };
									// workgroup_id * workgroup_size + local_invocation_id
									args.global_invocation_id = Compute_Dims{
										global_x * local.x + x,
										global_y * local.y + y,
										global_z * local.z + z,
									};
									fn(args);
								}
							}
						}
						wg.done();
					});
				}
			}
		}
		wg.wait();
		task_free(fn);
	}

	void
	_multi_threaded_compute_sized(Fabric self, Compute_Dims global, Compute_Dims size, Compute_Dims local, Task<void(Compute_Args)> fn)
	{
		Auto_Waitgroup wg;
		for (size_t z = 0; z < global.z; ++z)
		{
			for (size_t y = 0; y < global.y; ++y)
			{
				for (size_t x = 0; x < global.x; ++x)
				{
					wg.add(1);
					fabric_do(self, [global_x = x, global_y = y, global_z = z, global, size, local, &wg, &fn] {
						for (size_t z = 0; z < local.z; ++z)
						{
							for (size_t y = 0; y < local.y; ++y)
							{
								for (size_t x = 0; x < local.x; ++x)
								{
									Compute_Args args;
									args.workgroup_size = local;
									args.workgroup_num = global;
									args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
									args.local_invocation_id = Compute_Dims{ x, y, z };
									// workgroup_id * workgroup_size + local_invocation_id
									args.global_invocation_id = Compute_Dims{
										global_x * local.x + x,
										global_y * local.y + y,
										global_z * local.z + z,
									};
									if (args.global_invocation_id.x >= size.x || args.global_invocation_id.y >= size.y || args.global_invocation_id.z >= size.z)
										continue;
									fn(args);
								}
							}
						}
						wg.done();
					});
				}
			}
		}
		wg.wait();
		task_free(fn);
	}

	// channel stream
	void
	IChan_Stream::dispose()
	{
		if (this->atomic_arc.fetch_sub(1) == 1)
		{
			chan_stream_close(this);

			mutex_free(this->mtx);
			cond_var_free(this->read_cv);
			cond_var_free(this->write_cv);
			free(this);
		}
	}

	size_t
	IChan_Stream::read(Block data_out)
	{
		chan_stream_ref(this);
		mn_defer(chan_stream_unref(this));

		mutex_lock(this->mtx);
		if (this->data_blob.size == 0)
		{
			if (chan_stream_closed(this))
			{
				mutex_unlock(this->mtx);
				return 0;
			}

			cond_var_wait(this->read_cv, this->mtx, [this]{
				return this->data_blob.size > 0 || chan_stream_closed(this);
			});
		}

		auto read_size = data_out.size;
		if (this->data_blob.size < read_size)
			read_size = this->data_blob.size;

		::memcpy(data_out.ptr, this->data_blob.ptr, read_size);
		auto ptr = (char*)this->data_blob.ptr;
		ptr += read_size;
		this->data_blob.ptr = ptr;
		this->data_blob.size -= read_size;
		bool ready_to_write = this->data_blob.size == 0;

		mutex_unlock(this->mtx);

		if (ready_to_write)
			cond_var_notify(this->write_cv);

		return read_size;
	}

	size_t
	IChan_Stream::write(Block data_in)
	{
		chan_stream_ref(this);
		mn_defer(chan_stream_unref(this));

		// wait until there's available space
		mutex_lock(this->mtx);
		if (this->data_blob.size > 0)
		{
			cond_var_wait(this->write_cv, this->mtx, [this] {
				return this->data_blob.size == 0 || chan_stream_closed(this);
			});
		}

		if (chan_stream_closed(this))
			panic("cannot write in a closed Chan_Stream");

		// get the data
		this->data_blob = data_in;

		// notify the read
		cond_var_notify(this->read_cv);
		cond_var_wait(this->write_cv, this->mtx, [this] {
			return this->data_blob.size == 0 || chan_stream_closed(this);
		});
		auto res = data_in.size - this->data_blob.size;
		mutex_unlock(this->mtx);

		return res;
	}

	Chan_Stream
	chan_stream_new()
	{
		auto self = alloc_construct<IChan_Stream>();
		self->mtx = mn_mutex_new_with_srcloc("chan stream mutex");
		self->read_cv = cond_var_new();
		self->write_cv = cond_var_new();
		self->atomic_arc = 1;
		return self;
	}

	void
	chan_stream_free(Chan_Stream self)
	{
		if (self == nullptr)
			return;
		chan_stream_unref(self);
	}

	Chan_Stream
	chan_stream_ref(Chan_Stream self)
	{
		if (self == nullptr)
			return nullptr;

		self->atomic_arc.fetch_add(1);
		return self;
	}

	void
	chan_stream_unref(Chan_Stream self)
	{
		if (self == nullptr)
			return;

		self->dispose();
	}

	void
	chan_stream_close(Chan_Stream self)
	{
		if (self == nullptr)
			return;

		mutex_lock(self->mtx);
		self->atomic_closed.store(true);
		mutex_unlock(self->mtx);
		cond_var_notify_all(self->read_cv);
		cond_var_notify_all(self->write_cv);
	}

	bool
	chan_stream_closed(Chan_Stream self)
	{
		if (self == nullptr)
			return true;

		return self->atomic_closed.load();
	}
}
