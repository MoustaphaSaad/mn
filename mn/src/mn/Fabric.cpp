#include "mn/Fabric.h"
#include "mn/Memory.h"
#include "mn/Pool.h"
#include "mn/Buf.h"
#include "mn/Result.h"

#include <atomic>
#include <emmintrin.h>
#include <chrono>
#include <thread>

namespace mn
{
	constexpr static auto DEFAULT_COOP_BLOCKING_THRESHOLD = 16;
	constexpr static auto DEFAULT_EXTR_BLOCKING_THRESHOLD = 10000;

	inline static void
	_yield()
	{
		thread_sleep(1);
	}

	using Job = Task<void()>;

	// Worker
	struct IWorker
	{
		enum STATE
		{
			STATE_NONE,
			STATE_RUN,
			STATE_STOP,
			STATE_PAUSE_REQUEST,
			STATE_PAUSE_ACKNOWLEDGE
		};

		Fabric fabric;
		std::atomic<uint64_t> atomic_job_start_time_in_ms;
		std::atomic<uint64_t> atomic_block_start_time_in_ms;

		Ring<Job> job_q;
		Mutex job_q_mtx;

		std::atomic<STATE> atomic_state;
		Thread thread;
	};
	thread_local Worker LOCAL_WORKER = nullptr;

	inline static size_t
	_worker_steal_jobs(Worker self, Job* jobs, size_t jobs_count)
	{
		size_t steal_count = 0;

		mutex_lock(self->job_q_mtx);
		steal_count = self->job_q.count / 2;
		steal_count = steal_count > jobs_count ? jobs_count : steal_count;

		for(size_t i = 0; i < steal_count; ++i)
		{
			jobs[i] = ring_back(self->job_q);
			ring_pop_back(self->job_q);
		}
		mutex_unlock(self->job_q_mtx);

		return steal_count;
	}

	enum class Task_Pop_Result
	{
		OK,
		NO_TASK
	};

	inline static Result<Job, Task_Pop_Result>
	_worker_pop(Worker self)
	{
		Job job{};
		auto err = Task_Pop_Result::NO_TASK;

		mutex_lock(self->job_q_mtx);
		if (self->job_q.count > 0)
		{
			job = ring_front(self->job_q);
			ring_pop_front(self->job_q);
			err = Task_Pop_Result::OK;
		}
		mutex_unlock(self->job_q_mtx);

		if (err == Task_Pop_Result::NO_TASK)
			return err;

		return job;
	}

	inline static Result<Job, Task_Pop_Result>
	_worker_pop_steal(Worker self)
	{
		if (auto [job, err] = _worker_pop(self); err == Task_Pop_Result::OK)
			return job;

		if (self->fabric == nullptr)
			return Task_Pop_Result::NO_TASK;

		auto worker = fabric_steal_next(self->fabric);
		if (worker == self)
			return Task_Pop_Result::NO_TASK;

		constexpr size_t STEAL_JOB_BATCH_COUNT = 128;
		Job stolen_jobs[STEAL_JOB_BATCH_COUNT];
		auto stolen_count = _worker_steal_jobs(worker, stolen_jobs, STEAL_JOB_BATCH_COUNT);

		if (stolen_count == 0)
			return Task_Pop_Result::NO_TASK;

		mutex_lock(self->job_q_mtx);
		for (size_t i = 1; i < stolen_count; ++i)
			ring_push_back(self->job_q, stolen_jobs[i]);
		mutex_unlock(self->job_q_mtx);

		return stolen_jobs[0];
	}

	inline static void
	_worker_job_run(Worker self, Job &job)
	{
		self->atomic_job_start_time_in_ms.store(time_in_millis());
		job();
		self->atomic_job_start_time_in_ms.store(0);
		task_free(job);
	}

	static void
	_worker_main(void* worker)
	{
		auto self = (Worker)worker;
		LOCAL_WORKER = self;

		while(true)
		{
			auto state = self->atomic_state.load();

			if(state == IWorker::STATE_RUN)
			{
				if (auto [job, err] = _worker_pop_steal(self); err == Task_Pop_Result::OK)
					_worker_job_run(self, job);
				else
					_yield();
			}
			else if(state == IWorker::STATE_PAUSE_REQUEST)
			{
				self->atomic_state.store(IWorker::STATE_PAUSE_ACKNOWLEDGE);
			}
			else if(state == IWorker::STATE_PAUSE_ACKNOWLEDGE)
			{
				_yield();
			}
			else if(state == IWorker::STATE_STOP)
			{
				break;
			}
			else
			{
				assert(false && "unreachable");
			}
		}

		LOCAL_WORKER = nullptr;
	}

	inline static void
	_worker_stop(Worker self)
	{
		self->atomic_state.store(IWorker::STATE_STOP);
		thread_join(self->thread);
		thread_free(self->thread);
	}

	inline static void
	_worker_free(Worker self)
	{
		destruct(self->job_q);
		mutex_free(self->job_q_mtx);

		free(self);
	}

	inline static void
	_worker_pause_wait(Worker self)
	{
		constexpr int SPIN_LIMIT = 128;
		int spin_count = 0;

		self->atomic_state = IWorker::STATE_PAUSE_REQUEST;
		while(self->atomic_state.load() == IWorker::STATE_PAUSE_REQUEST)
		{
			if(spin_count < SPIN_LIMIT)
			{
				++spin_count;
				_mm_pause();
			}
			else
			{
				_yield();
			}
		}
	}
	
	inline static void
	_worker_resume(Worker self)
	{
		self->atomic_state = IWorker::STATE_RUN;
	}

	inline static Worker
	_worker_with_initial_state(
		const char*,
		IWorker::STATE init_state,
		Fabric fabric,
		Ring<Job> stolen_jobs = ring_new<Job>()
	)
	{
		auto self = alloc_zerod<IWorker>();

		self->fabric = fabric;
		self->atomic_job_start_time_in_ms.store(0);
		self->atomic_block_start_time_in_ms.store(0);

		self->job_q = stolen_jobs;
		self->job_q_mtx = mutex_new("worker mutex");

		self->atomic_state = init_state;
		self->thread = thread_new(_worker_main, self, "Worker Thread");

		return self;
	}

	// Fabric
	struct IFabric
	{
		uint32_t coop_blocking_threshold;
		size_t put_aside_worker_count;

		Buf<Worker> workers;
		Mutex_RW workers_mtx;

		std::atomic<size_t> atomic_worker_next;
		std::atomic<size_t> atomic_steal_next;

		Buf<Worker> sleepy_side_workers;
		Buf<Worker> ready_side_workers;

		std::atomic<bool> atomic_sysmon_close;
		Thread sysmon;
	};

	struct Blocking_Worker
	{
		Worker worker;
		size_t index;
	};

	static void
	_sysmon_main(void* fabric)
	{
		auto self = (Fabric)fabric;

		auto blocking_workers = buf_with_capacity<Blocking_Worker>(self->workers.count);
		mn_defer(buf_free(blocking_workers));

		while(self->atomic_sysmon_close.load() == false)
		{
			// first check if any sleepy worker is ready and move it
			buf_remove_if(self->sleepy_side_workers, [self](Worker worker) {
				if (worker->atomic_state.load() == IWorker::STATE_PAUSE_ACKNOWLEDGE)
				{
					if (self->ready_side_workers.count < self->put_aside_worker_count)
					{
						buf_push(self->ready_side_workers, worker);
					}
					else
					{
						_worker_stop(worker);
						_worker_free(worker);
					}
					return true;
				}
				return false;
			});

			// then check for blocking workers
			mutex_read_lock(self->workers_mtx);
			for(size_t i = 0; i < self->workers.count; ++i)
			{
				auto block_start_time = self->workers[i]->atomic_block_start_time_in_ms.load();
				if(block_start_time != 0)
				{
					auto block_time = time_in_millis() - block_start_time;
					if(block_time > self->coop_blocking_threshold)
					{
						buf_push(blocking_workers, Blocking_Worker{ self->workers[i], i });
						continue;
					}
				}

				auto job_start_time = self->workers[i]->atomic_job_start_time_in_ms.load();
				if (job_start_time != 0)
				{
					auto job_run_time = time_in_millis() - job_start_time;
					if(job_run_time > DEFAULT_EXTR_BLOCKING_THRESHOLD)
					{
						buf_push(blocking_workers, Blocking_Worker{ self->workers[i], i });
						continue;
					}
				}
			}
			mutex_read_unlock(self->workers_mtx);

			// request all the blocking workers to pause
			for(auto blocking_worker: blocking_workers)
				blocking_worker.worker->atomic_state.store(IWorker::STATE_PAUSE_REQUEST);

			// replace the blocking workers with a new set of workers
			if(blocking_workers.count > 0)
			{
				mutex_write_lock(self->workers_mtx);
				for (auto blocking_worker : blocking_workers)
				{
					mutex_lock(blocking_worker.worker->job_q_mtx);

					// find a suitable worker
					if (self->ready_side_workers.count > 0)
					{
						auto new_worker = buf_top(self->ready_side_workers);
						buf_pop(self->ready_side_workers);

						self->workers[blocking_worker.index] = new_worker;
						std::swap(blocking_worker.worker->job_q, new_worker->job_q);

						_worker_resume(new_worker);
					}
					else
					{
						auto new_worker = _worker_with_initial_state(
							"fabric worker",
							IWorker::STATE_PAUSE_ACKNOWLEDGE,
							self,
							blocking_worker.worker->job_q
						);

						self->workers[blocking_worker.index] = new_worker;
						blocking_worker.worker->job_q = ring_new<Task<void()>>();

						_worker_resume(new_worker);
					}

					mutex_unlock(blocking_worker.worker->job_q_mtx);
				}
				mutex_write_unlock(self->workers_mtx);
			}

			// now that we have replaced all the blocking workers with a newly created workers
			// we need to store the blocking workers away to be reused later in the replacement above
			for (auto blocking_worker : blocking_workers)
				buf_push(self->sleepy_side_workers, blocking_worker.worker);

			// clear the blocking workers list
			buf_clear(blocking_workers);

			// get some rest sysmon, you deserve it
			thread_sleep(1);
		}
	}


	// API
	Worker
	worker_new(const char* name)
	{
		return _worker_with_initial_state(name, IWorker::STATE_RUN, nullptr);
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
		mutex_lock(self->job_q_mtx);
		ring_push_back(self->job_q, task);
		mutex_unlock(self->job_q_mtx);
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
		LOCAL_WORKER->atomic_block_start_time_in_ms.store(time_in_millis());
	}

	void
	worker_block_clear()
	{
		if (LOCAL_WORKER == nullptr)
			return;
		LOCAL_WORKER->atomic_block_start_time_in_ms.store(0);
	}


	// fabric
	Fabric
	fabric_new(size_t workers_count, uint32_t blocking_threshold_in_ms, size_t put_aside_worker_count)
	{
		auto self = alloc_zerod<IFabric>();

		if (workers_count == 0)
			workers_count = std::thread::hardware_concurrency();

		self->coop_blocking_threshold = blocking_threshold_in_ms;

		self->put_aside_worker_count = put_aside_worker_count;
		if(self->put_aside_worker_count == 0)
			self->put_aside_worker_count = workers_count / 2;

		self->workers = buf_with_count<Worker>(workers_count);
		self->workers_mtx = mutex_rw_new("fabric workers mutex");

		self->atomic_worker_next = 0;
		self->atomic_steal_next = 0;

		for (size_t i = 0; i < workers_count; ++i)
			self->workers[i] = _worker_with_initial_state("fabric worker", IWorker::STATE_PAUSE_ACKNOWLEDGE, self);

		for (size_t i = 0; i < workers_count; ++i)
			_worker_resume(self->workers[i]);

		self->sleepy_side_workers = buf_new<Worker>();
		self->ready_side_workers = buf_new<Worker>();

		self->atomic_sysmon_close.store(false);

		self->sysmon = thread_new(_sysmon_main, self, "fabric sysmon thread");

		if(self->coop_blocking_threshold == 0)
			self->coop_blocking_threshold = DEFAULT_COOP_BLOCKING_THRESHOLD;

		return self;
	}

	void
	fabric_free(Fabric self)
	{
		self->atomic_sysmon_close.store(true);
		thread_join(self->sysmon);
		thread_free(self->sysmon);

		for (auto worker : self->workers)
			worker->atomic_state.store(IWorker::STATE_STOP);

		for (auto worker : self->sleepy_side_workers)
			worker->atomic_state.store(IWorker::STATE_STOP);

		for (auto worker : self->ready_side_workers)
			worker->atomic_state.store(IWorker::STATE_STOP);

		for (auto worker : self->workers)
		{
			thread_join(worker->thread);
			thread_free(worker->thread);
		}

		for (auto worker : self->sleepy_side_workers)
		{
			thread_join(worker->thread);
			thread_free(worker->thread);
		}

		for (auto worker : self->ready_side_workers)
		{
			thread_join(worker->thread);
			thread_free(worker->thread);
		}

		for (auto worker : self->workers)
			_worker_free(worker);
		mutex_rw_free(self->workers_mtx);
		buf_free(self->workers);


		for (auto worker : self->sleepy_side_workers)
			_worker_free(worker);
		buf_free(self->sleepy_side_workers);


		for (auto worker : self->ready_side_workers)
			_worker_free(worker);
		buf_free(self->ready_side_workers);

		free(self);
	}

	Worker
	fabric_worker_next(Fabric self)
	{
		auto ix = self->atomic_worker_next.fetch_add(1);

		mutex_read_lock(self->workers_mtx);
		auto worker = self->workers[ix % self->workers.count];
		mutex_read_unlock(self->workers_mtx);

		return worker;
	}

	Worker
	fabric_steal_next(Fabric self)
	{
		auto ix = self->atomic_steal_next.fetch_add(1);

		mutex_read_lock(self->workers_mtx);
		auto worker = self->workers[ix % self->workers.count];
		mutex_read_unlock(self->workers_mtx);

		return worker;
	}

	Fabric
	fabric_local()
	{
		Fabric res = nullptr;
		if (auto w = worker_local())
			res = w->fabric;
		return res;
	}

	// Waitgroup
	void
	waitgroup_wait(Waitgroup& self)
	{
		constexpr int SPIN_LIMIT = 128;
		int spin_count = 0;

		while(self.load() > 0)
		{
			if(spin_count < SPIN_LIMIT)
			{
				++spin_count;
				_mm_pause();
			}
			else
			{
				_yield();
			}
		}
	}
}
