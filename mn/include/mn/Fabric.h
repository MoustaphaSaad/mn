#pragma once

#include "mn/Exports.h"
#include "mn/Base.h"
#include "mn/Task.h"
#include "mn/Ring.h"
#include "mn/Thread.h"
#include "mn/Defer.h"
#include "mn/OS.h"
#include "mn/Stream.h"

#include <atomic>
#include <chrono>
#include <assert.h>

namespace mn
{
	// Worker
	typedef struct IWorker* Worker;

	// worker_new creates a new worker which is a threads with a job queue
	MN_EXPORT Worker
	worker_new(const char* name);

	// worker_free frees the worker and stops its thread
	MN_EXPORT void
	worker_free(Worker self);

	inline static void
	destruct(Worker self)
	{
		worker_free(self);
	}

	// worker_task_do schedules a task into the worker queue
	MN_EXPORT void
	worker_task_do(Worker self, const Task<void()>& task);

	// schedules a task batch in to worker queue
	MN_EXPORT void
	worker_task_batch_do(Worker self, const Task<void()>* ptr, size_t count);

	// worker_do schedules any callable into the worker queue
	template<typename TFunc>
	inline static void
	worker_do(Worker self, TFunc&& f)
	{
		worker_task_do(self, Task<void()>::make(std::forward<TFunc>(f)));
	}

	MN_EXPORT Worker
	worker_local();

	MN_EXPORT void
	worker_block_ahead();

	MN_EXPORT void
	worker_block_clear();

	template<typename TFunc>
	inline static void
	worker_block_on(TFunc&& fn)
	{
		worker_block_ahead();
		while(fn() == false)
			thread_sleep(1);
		worker_block_clear();
	}

	template<typename TFunc>
	inline static void
	worker_block_on_with_timeout(Timeout timeout, TFunc&& fn)
	{
		worker_block_ahead();
		auto start = std::chrono::steady_clock::now();
		while(fn() == false)
		{
			if (timeout == NO_TIMEOUT)
			{
				break;
			}
			else if (timeout != INFINITE_TIMEOUT)
			{
				auto t = std::chrono::steady_clock::now();
				if ((uint64_t)std::chrono::duration_cast<std::chrono::milliseconds>(t - start).count() >= timeout.milliseconds)
					break;
			}
			thread_sleep(1);
		}
		worker_block_clear();
	}


	// fabric
	typedef struct IFabric* Fabric;

	struct Fabric_Settings
	{
		const char* name;
		// default: CPU cores count
		size_t workers_count;
		// default: 1/2 CPU cores count
		size_t put_aside_worker_count;
		// how many milliseconds sysmon will wait for before declaring this worker blocked
		// in case this worker announced that it will block via worker_block_ahead, worker_block_clear
		// default: 10
		uint32_t coop_blocking_threshold_in_ms;
		// how many milliseconds sysmon will wait for before declaring this worker blocked
		// in case this worker didn't announce that it will block via worker_block_ahead, worker_block_clear API
		// default: 1000
		uint32_t external_blocking_threshold_in_ms;
		// threshold of the blocking workers ratio [0, 1] which sysmon
		// will use to start evicting these workers if the blocking_workers_count >= workers_count * blocking_workers_threshold
		// default: 0.5f
		float blocking_workers_threshold;
		Task<void()> after_each_job;
		Task<void()> on_worker_start;
	};

	// fabric_new creates a group of workers
	MN_EXPORT Fabric
	fabric_new(Fabric_Settings settings);

	// fabric_free stops and frees the group of workers
	MN_EXPORT void
	fabric_free(Fabric self);

	inline static void
	destruct(Fabric self)
	{
		fabric_free(self);
	}

	// fabric_task_do adds a task to the fabric
	MN_EXPORT void
	fabric_task_do(Fabric self, const Task<void()>& task);

	// adds a batch of tasks to fabric
	MN_EXPORT void
	fabric_task_batch_do(Fabric self, const Task<void()>* ptr, size_t count);

	template<typename TFunc>
	inline static void
	fabric_do(Fabric self, TFunc&& f)
	{
		fabric_task_do(self, Task<void()>::make(std::forward<TFunc>(f)));
	}

	MN_EXPORT Fabric
	fabric_local();

	struct Compute_Dims
	{
		size_t x, y, z;
	};

	struct Compute_Args
	{
		Compute_Dims workgroup_size;
		Compute_Dims workgroup_num;
		Compute_Dims workgroup_id;
		Compute_Dims local_invocation_id;
		Compute_Dims global_invocation_id;
	};

	//easy interface
	template<typename TFunc>
	inline static void
	go(Fabric f, TFunc&& fn)
	{
		fabric_task_do(f, Task<void()>::make(std::forward<TFunc>(fn)));
	}

	template<typename TFunc>
	inline static void
	go(Worker worker, TFunc&& fn)
	{
		worker_task_do(worker, Task<void()>::make(std::forward<TFunc>(fn)));
	}

	template<typename TFunc>
	inline static void
	go(TFunc&& fn)
	{
		if (Fabric f = fabric_local())
			fabric_task_do(f, Task<void()>::make(std::forward<TFunc>(fn)));
		else if (Worker w = worker_local())
			worker_task_do(w, Task<void()>::make(std::forward<TFunc>(fn)));
		else
			panic("can't find any local fabric or worker");
	}

	typedef struct IChan_Stream* Chan_Stream;
	struct IChan_Stream final: IStream
	{
		Block data_blob;
		Mutex mtx;
		Cond_Var read_cv;
		Cond_Var write_cv;
		std::atomic<int32_t> atomic_arc;
		std::atomic<bool> atomic_closed;

		MN_EXPORT void
		dispose() override;

		MN_EXPORT size_t
		read(Block data_out) override;

		MN_EXPORT size_t
		write(Block data_in) override;

		int64_t
		size() override
		{
			return 0;
		};

		int64_t
		cursor_operation(STREAM_CURSOR_OP, int64_t) override
		{
			assert(false && "Chan_Stream doesn't support cursor operations");
			return STREAM_CURSOR_ERROR;
		}
	};

	MN_EXPORT Chan_Stream
	chan_stream_new();

	MN_EXPORT void
	chan_stream_free(Chan_Stream self);

	MN_EXPORT Chan_Stream
	chan_stream_ref(Chan_Stream self);

	MN_EXPORT void
	chan_stream_unref(Chan_Stream self);

	MN_EXPORT void
	chan_stream_close(Chan_Stream self);

	MN_EXPORT bool
	chan_stream_closed(Chan_Stream self);

	struct Auto_Chan_Stream
	{
		Chan_Stream handle;

		Auto_Chan_Stream()
			: handle(chan_stream_new())
		{}

		explicit Auto_Chan_Stream(Chan_Stream s)
			: handle(chan_stream_ref(s))
		{}

		Auto_Chan_Stream(const Auto_Chan_Stream& other)
			: handle(chan_stream_ref(other.handle))
		{}

		Auto_Chan_Stream(Auto_Chan_Stream&& other)
			: handle(other.handle)
		{
			other.handle = nullptr;
		}

		Auto_Chan_Stream&
		operator=(const Auto_Chan_Stream& other)
		{
			if (handle) chan_stream_free(handle);
			handle = chan_stream_ref(other.handle);
			return *this;
		}

		Auto_Chan_Stream&
		operator=(Auto_Chan_Stream&& other)
		{
			if (handle) chan_stream_free(handle);
			handle = other.handle;
			other.handle = nullptr;
			return *this;
		}

		~Auto_Chan_Stream()
		{
			if (handle)
				chan_stream_free(handle);
		}

		operator Chan_Stream() const { return handle; }
	};

	template<typename TFunc, typename ... TArgs>
	inline static Auto_Chan_Stream
	lazy_stream(Fabric f, TFunc&& func, mn::Stream stream_in, TArgs&& ... args)
	{
		Auto_Chan_Stream res;
		mn::go(f, [=]{
			func(stream_in, res, args...);
			chan_stream_close(res);
		});
		return res;
	}


	// Channel
	template<typename T>
	struct IChan
	{
		Ring<T> r;
		Mutex mtx;
		Cond_Var read_cv;
		Cond_Var write_cv;
		std::atomic<int32_t> atomic_limit;
		std::atomic<int32_t> atomic_arc;
	};
	template<typename T>
	using Chan = IChan<T>*;

	template<typename T>
	inline static Chan<T>
	chan_new(int32_t limit = 1)
	{
		assert(limit != 0);
		Chan<T> self = alloc<IChan<T>>();

		self->r = ring_new<T>();
		ring_reserve(self->r, limit);
		self->mtx = mn_mutex_new_with_srcloc("Channel Mutex");
		self->read_cv = cond_var_new();
		self->write_cv = cond_var_new();
		self->atomic_limit = limit;
		self->atomic_arc = 1;
		return self;
	}

	template<typename T>
	inline static Chan<T>
	chan_ref(Chan<T> self)
	{
		self->atomic_arc.fetch_add(1);
		return self;
	}

	template<typename T>
	inline static void
	chan_close(Chan<T> self);

	template<typename T>
	inline static Chan<T>
	chan_unref(Chan<T> self)
	{
		if (self->atomic_arc.fetch_sub(1) == 1)
		{
			chan_close(self);

			destruct(self->r);
			mutex_free(self->mtx);
			cond_var_free(self->read_cv);
			cond_var_free(self->write_cv);
			mn::free(self);
			return nullptr;
		}
		return self;
	}

	template<typename T>
	inline static Chan<T>
	chan_new(Chan<T> self)
	{
		chan_ref(self);
		return self;
	}

	template<typename T>
	inline static void
	chan_free(Chan<T> self)
	{
		chan_unref(self);
	}

	template<typename T>
	inline static void
	destruct(Chan<T> self)
	{
		chan_free(self);
	}

	template<typename T>
	inline static bool
	chan_closed(Chan<T> self)
	{
		return self->atomic_limit.load() == 0;
	}

	template<typename T>
	inline static void
	chan_close(Chan<T> self)
	{
		mutex_lock(self->mtx);
		self->atomic_limit.exchange(0);
		mutex_unlock(self->mtx);
		cond_var_notify_all(self->read_cv);
		cond_var_notify_all(self->write_cv);
	}

	template<typename T>
	inline static bool
	chan_can_send(Chan<T> self)
	{
		mutex_lock(self->r_mtx);
			bool res = (self->r.count < size_t(self->atomic_limit.load())) && (chan_closed(self) == false);
		mutex_unlock(self->r_mtx);
		return res;
	}

	template<typename T>
	inline static bool
	chan_send_try(Chan<T> self, const T& v)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		if (self->r.count < self->limit)
		{
			ring_push_back(self->r, v);
			return true;
		}
		return false;
	}

	template<typename T>
	inline static void
	chan_send(Chan<T> self, const T& v)
	{
		chan_ref(self);
		mn_defer(chan_unref(self));

		mutex_lock(self->mtx);
		cond_var_wait(self->write_cv, self->mtx, [self] {
			return (self->r.count < size_t(self->atomic_limit.load()) ||
					chan_closed(self));
		});

		if (chan_closed(self))
			panic("cannot send in a closed channel");

		ring_push_back(self->r, v);
		mutex_unlock(self->mtx);

		cond_var_notify(self->read_cv);
	}

	template<typename T>
	inline static bool
	chan_can_recv(Chan<T> self)
	{
		mutex_lock(self->mtx);
			bool res = (self->r.count > 0) && (chan_closed(self) == false);
		mutex_unlock(self->mtx);
		return res;
	}

	template<typename T>
	struct Recv_Result {
		T res;
		bool more;
	};

	template<typename T>
	inline static Recv_Result<T>
	chan_recv_try(Chan<T> self)
	{
		mutex_lock(self->mtx);
		mn_defer(mutex_unlock(self->mtx));

		if (self->r.count > 0)
		{
			T res = ring_front(self->r);
			ring_pop_front(self->r);
			return { res, true };
		}
		return { T{}, false };
	}

	template<typename T>
	inline static Recv_Result<T>
	chan_recv(Chan<T> self)
	{
		chan_ref(self);
		mn_defer(chan_unref(self));

		mutex_lock(self->mtx);
		cond_var_wait(self->read_cv, self->mtx, [self] {
			return self->r.count > 0 || chan_closed(self);
		});

		if(self->r.count > 0)
		{
			T res = ring_front(self->r);
			ring_pop_front(self->r);
			mutex_unlock(self->mtx);

			cond_var_notify(self->write_cv);
			return { res, true };
		}
		else if(chan_closed(self))
		{
			mutex_unlock(self->mtx);
			return { T{}, false };
		}
		else
		{
			panic("unreachable");
		}
	}

	template<typename T>
	struct Chan_Iterator
	{
		Chan<T> chan;
		Recv_Result<T> res;

		Chan_Iterator&
		operator++()
		{
			res = chan_recv(chan);
			return *this;
		}

		Chan_Iterator
		operator++(int)
		{
			auto r = *this;
			res = chan_recv(chan);
			return r;
		}

		bool
		operator==(const Chan_Iterator& other) const
		{
			return chan == other.chan && res.more == other.res.more;
		}

		bool
		operator!=(const Chan_Iterator& other) const
		{
			return !operator==(other);
		}

		T&
		operator*()
		{
			return res.res;
		}

		const T&
		operator*() const
		{
			return res.res;
		}

		T*
		operator->()
		{
			return &res.res;
		}

		const T*
		operator->() const
		{
			return &res.res;
		}
	};

	template<typename T>
	inline static Chan_Iterator<T>
	begin(Chan<T> self)
	{
		return Chan_Iterator<T>{self, chan_recv(self)};
	}

	template<typename T>
	inline static Chan_Iterator<T>
	end(Chan<T> self)
	{
		return Chan_Iterator<T>{self, {}};
	}

	template<typename T>
	struct Auto_Chan
	{
		Chan<T> handle;

		explicit Auto_Chan(int32_t limit = 1)
			: handle(chan_new<T>(limit))
		{}

		Auto_Chan(const Auto_Chan& other)
			: handle(chan_new(other.handle))
		{}

		Auto_Chan(Auto_Chan&& other)
			: handle(other.handle)
		{
			other.handle = nullptr;
		}

		Auto_Chan&
		operator=(const Auto_Chan& other)
		{
			if (handle) chan_free(handle);
			handle = chan_new(other.handle);
			return *this;
		}

		Auto_Chan&
		operator=(Auto_Chan&& other)
		{
			if (handle) chan_free(handle);
			handle = other.handle;
			other.handle = nullptr;
			return *this;
		}

		~Auto_Chan()
		{
			if (handle) chan_free(handle);
		}

		operator Chan<T>() const { return handle; }
		Chan_Iterator<T> begin() { return Chan_Iterator<T>{handle, chan_recv(handle)}; }
		Chan_Iterator<T> end() { return Chan_Iterator<T>{handle, {}}; }
	};

	template<typename T>
	inline static bool
	chan_closed(const Auto_Chan<T> &self)
	{
		return chan_closed(self.handle);
	}

	template<typename T>
	inline static void
	chan_close(Auto_Chan<T> &self)
	{
		chan_close(self.handle);
	}

	template<typename T>
	inline static bool
	chan_can_send(const Auto_Chan<T> &self)
	{
		return chan_can_send(self.handle);
	}

	template<typename T>
	inline static bool
	chan_send_try(Auto_Chan<T> &self, const T& v)
	{
		return chan_send_try(self.handle, v);
	}

	template<typename T>
	inline static void
	chan_send(Auto_Chan<T> &self, const T& v)
	{
		return chan_send(self.handle, v);
	}

	template<typename T>
	inline static bool
	chan_can_recv(const Auto_Chan<T> &self)
	{
		return chan_can_recv(self.handle);
	}

	template<typename T>
	inline static Recv_Result<T>
	chan_recv_try(Auto_Chan<T> &self)
	{
		return chan_recv_try(self.handle);
	}

	template<typename T>
	inline static Recv_Result<T>
	chan_recv(Auto_Chan<T> &self)
	{
		return chan_recv(self.handle);
	}

	template<typename TFunc>
	inline static void
	_single_threaded_compute(Compute_Dims global, Compute_Dims local, TFunc&& fn)
	{
		for (size_t global_z = 0; global_z < global.z; ++global_z)
		{
			for (size_t global_y = 0; global_y < global.y; ++global_y)
			{
				for (size_t global_x = 0; global_x < global.x; ++global_x)
				{
					for (size_t local_z = 0; local_z < local.z; ++local_z)
					{
						for (size_t local_y = 0; local_y < local.y; ++local_y)
						{
							for (size_t local_x = 0; local_x < local.x; ++local_x)
							{
								Compute_Args args;
								args.workgroup_size = local;
								args.workgroup_num = global;
								args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
								args.local_invocation_id = Compute_Dims{ local_x, local_y, local_z };
								// workgroup_id * workgroup_size + local_invocation_id
								args.global_invocation_id = Compute_Dims{
									global_x * local.x + local_x,
									global_y * local.y + local_y,
									global_z * local.z + local_z,
								};
								fn(args);
							}
						}
					}
				}
			}
		}
	}

	MN_EXPORT void
	_multi_threaded_compute(Fabric self, Compute_Dims global, Compute_Dims local, Task<void(Compute_Args)> fn);

	template<typename TFunc>
	inline static void
	compute(Fabric f, Compute_Dims global, Compute_Dims local, TFunc&& fn)
	{
		if (f == nullptr)
			_single_threaded_compute(global, local, std::forward<TFunc>(fn));
		else
			_multi_threaded_compute(f, global, local, mn::Task<void(Compute_Args)>::make(std::forward<TFunc>(fn)));
	}

	template<typename TFunc>
	inline static void
	compute(Compute_Dims global, Compute_Dims local, TFunc&& fn)
	{
		compute(fabric_local(), global, local, std::forward<TFunc>(fn));
	}

	template<typename TFunc>
	inline static void
	_single_threaded_compute_sized(Compute_Dims global, Compute_Dims size, Compute_Dims local, TFunc&& fn)
	{
		for (size_t global_z = 0; global_z < global.z; ++global_z)
		{
			for (size_t global_y = 0; global_y < global.y; ++global_y)
			{
				for (size_t global_x = 0; global_x < global.x; ++global_x)
				{
					for (size_t local_z = 0; local_z < local.z; ++local_z)
					{
						for (size_t local_y = 0; local_y < local.y; ++local_y)
						{
							for (size_t local_x = 0; local_x < local.x; ++local_x)
							{
								Compute_Args args;
								args.workgroup_size = local;
								args.workgroup_num = global;
								args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
								args.local_invocation_id = Compute_Dims{ local_x, local_y, local_z };
								// workgroup_id * workgroup_size + local_invocation_id
								args.global_invocation_id = Compute_Dims{
									global_x * local.x + local_x,
									global_y * local.y + local_y,
									global_z * local.z + local_z,
								};
								if (args.global_invocation_id.x >= size.x || args.global_invocation_id.y >= size.y || args.global_invocation_id.z >= size.z)
									continue;
								fn(args);
							}
						}
					}
				}
			}
		}
	}

	MN_EXPORT void
	_multi_threaded_compute_sized(Fabric self, Compute_Dims global, Compute_Dims size, Compute_Dims local, Task<void(Compute_Args)> fn);

	template<typename TFunc>
	inline static void
	compute_sized(Fabric f, Compute_Dims total_size, Compute_Dims local, TFunc&& fn)
	{
		Compute_Dims global{
			1 + ((total_size.x - 1) / local.x),
			1 + ((total_size.y - 1) / local.y),
			1 + ((total_size.z - 1) / local.z)
		};
		if (f == nullptr)
			_single_threaded_compute_sized(global, total_size, local, std::forward<TFunc>(fn));
		else
			_multi_threaded_compute_sized(f, global, total_size, local, mn::Task<void(Compute_Args)>::make(std::forward<TFunc>(fn)));
	}

	template<typename TFunc>
	inline static void
	compute_sized(Compute_Dims total_size, Compute_Dims local, TFunc&& fn)
	{
		compute_sized(fabric_local(), total_size, local, std::forward<TFunc>(fn));
	}

	template<typename TFunc>
	inline static void
	_single_threaded_compute_tiled(Compute_Dims total_size, Compute_Dims tile_size, TFunc&& fn)
	{
		for (size_t global_z = 0; global_z < total_size.z; ++global_z)
		{
			for (size_t global_y = 0; global_y < total_size.y; ++global_y)
			{
				for (size_t global_x = 0; global_x < total_size.x; ++global_x)
				{
					Compute_Args args{};
					args.workgroup_size = tile_size;
					args.workgroup_num = total_size;
					args.workgroup_id = Compute_Dims{ global_x, global_y, global_z };
					// workgroup_id * workgroup_size + local_invocation_id
					args.global_invocation_id = Compute_Dims{
						global_x * tile_size.x,
						global_y * tile_size.y,
						global_z * tile_size.z
					};
					fn(args);
				}
			}
		}
	}

	MN_EXPORT void
	_multi_threaded_compute_tiled(Fabric self, Compute_Dims total_size, Compute_Dims tile_size, Task<void(Compute_Args)> fn);

	// performs the compute function in tiles so if you have a total size of
	// (100, 100, 100) and tile size of (10, 10, 10) you get (10, 10, 10) = 1000 workgroups
	// but a single local worker for the (10, 10, 10) step/tile
	// so basically your function will be called total_size/step_size number of times
	// and will not be invoked for each local tile individually so you have
	// to process the entire tile in the single call
	template<typename TFunc>
	inline static void
	compute_tiled(Fabric f, Compute_Dims total_size, Compute_Dims tile_size, TFunc&& fn)
	{
		Compute_Dims global{
			1 + ((total_size.x - 1) / tile_size.x),
			1 + ((total_size.y - 1) / tile_size.y),
			1 + ((total_size.z - 1) / tile_size.z)
		};
		if (f == nullptr)
			_single_threaded_compute_tiled(global, tile_size, std::forward<TFunc>(fn));
		else
			_multi_threaded_compute_tiled(f, global, tile_size, mn::Task<void(Compute_Args)>::make(std::forward<TFunc>(fn)));
	}
}