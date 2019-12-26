#include "mn/Worker.h"
#include "mn/Memory.h"
#include "mn/Ring.h"
#include "mn/Thread.h"
#include "mn/Pool.h"
#include "mn/Buf.h"

#include <atomic>
#include <emmintrin.h>

namespace mn
{
	// Job
	struct IJob
	{
		Worker worker;
		Task<void()> fn;
	};
	typedef struct IJob *Job;

	inline static void
	_job_free(Job self)
	{
		task_free(self->fn);
	}

	inline static void
	destruct(Job self)
	{
		_job_free(self);
	}


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
		Job current;

		Pool job_pool;
		Mutex job_pool_mtx;

		Ring<Job> job_q;
		Mutex job_q_mtx;

		std::atomic<STATE> atomic_state;
		Thread thread;
	};

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

	inline static Job
	_worker_pop(Worker self)
	{
		Job job = nullptr;

		mutex_lock(self->job_q_mtx);
		if (self->job_q.count > 0)
		{
			job = ring_front(self->job_q);
			ring_pop_front(self->job_q);
		}
		mutex_unlock(self->job_q_mtx);

		if (job != nullptr || self->fabric == nullptr)
			return job;

		return job;
	}

	inline static Job
	_worker_pop_steal(Worker self)
	{
		if (auto job = _worker_pop(self))
			return job;

		if (self->fabric == nullptr)
			return nullptr;

		auto worker = fabric_steal_next(self->fabric);
		if (worker == self)
			return nullptr;

		constexpr size_t STEAL_JOB_BATCH_COUNT = 128;
		Job stolen_jobs[STEAL_JOB_BATCH_COUNT];
		auto stolen_count = _worker_steal_jobs(worker, stolen_jobs, STEAL_JOB_BATCH_COUNT);

		if (stolen_count == 0)
			return nullptr;

		mutex_lock(self->job_q_mtx);
		for (size_t i = 1; i < stolen_count; ++i)
			ring_push_back(self->job_q, stolen_jobs[i]);
		mutex_unlock(self->job_q_mtx);

		return stolen_jobs[0];
	}

	inline static void
	_worker_job_run(Job job)
	{
		job->fn();
		task_free(job->fn);

		auto worker = job->worker;
		mutex_lock(worker->job_pool_mtx);
		pool_put(worker->job_pool, job);
		mutex_unlock(worker->job_pool_mtx);
	}

	static void
	_worker_main(void* worker)
	{
		auto self = (Worker)worker;

		while(true)
		{
			auto state = self->atomic_state.load();
			if(state == IWorker::STATE_RUN)
			{
				if (auto job = _worker_pop_steal(self))
					_worker_job_run(job);
				else
					thread_sleep(1);
			}
			else if(state == IWorker::STATE_PAUSE_REQUEST)
			{
				state = IWorker::STATE_PAUSE_ACKNOWLEDGE;
			}
			else if(state == IWorker::STATE_PAUSE_ACKNOWLEDGE)
			{
				thread_sleep(1);
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
	}

	inline static void
	_worker_stop(Worker self)
	{
		self->atomic_state = IWorker::STATE_STOP;
		thread_join(self->thread);
		thread_free(self->thread);
	}

	inline static void
	_worker_free(Worker self)
	{
		destruct(self->job_q);
		mutex_free(self->job_q_mtx);

		pool_free(self->job_pool);
		mutex_free(self->job_pool_mtx);

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
				thread_sleep(1);
			}
		}
	}
	
	inline static void
	_worker_resume(Worker self)
	{
		self->atomic_state = IWorker::STATE_RUN;
	}

	// Fabric
	struct IFabric
	{
		Buf<Worker> workers;
		std::atomic<size_t> atomic_worker_next;
		std::atomic<size_t> atomic_steal_next;
	};

	inline static Worker
	_worker_with_initial_state(const char*, IWorker::STATE init_state)
	{
		auto self = alloc_zerod<IWorker>();

		self->job_pool = pool_new(sizeof(IJob), 1024);
		self->job_pool_mtx = mutex_new("worker job pool mutx");

		self->job_q = ring_new<Job>();
		self->job_q_mtx = mutex_new("worker mutex");

		self->atomic_state = init_state;
		self->thread = thread_new(_worker_main, self, "Worker Thread");

		return self;
	}


	// API
	Worker
	worker_new(const char* name)
	{
		return _worker_with_initial_state(name, IWorker::STATE_RUN);
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
		mutex_lock(self->job_pool_mtx);
		auto job = (Job)pool_get(self->job_pool);
		mutex_unlock(self->job_pool_mtx);

		job->worker = self;
		job->fn = task;

		mutex_lock(self->job_q_mtx);
		ring_push_back(self->job_q, job);
		mutex_unlock(self->job_q_mtx);
	}

	// fabric
	Fabric
	fabric_new(size_t workers_count)
	{
		auto self = alloc_zerod<IFabric>();

		self->workers = buf_with_count<Worker>(workers_count);
		self->atomic_worker_next = 0;
		self->atomic_steal_next = 0;

		for (size_t i = 0; i < workers_count; ++i)
			self->workers[i] = _worker_with_initial_state("fabric worker", IWorker::STATE_PAUSE_ACKNOWLEDGE);

		for (auto worker : self->workers)
			worker->fabric = self;

		for (auto worker : self->workers)
			_worker_resume(worker);

		return self;
	}

	void
	fabric_free(Fabric self)
	{
		for (auto worker : self->workers)
			_worker_stop(worker);

		for (auto worker : self->workers)
			_worker_free(worker);

		buf_free(self->workers);
		free(self);
	}

	Worker
	fabric_worker_next(Fabric self)
	{
		auto ix = self->atomic_worker_next.fetch_add(1);
		return self->workers[ix % self->workers.count];
	}

	Worker
	fabric_steal_next(Fabric self)
	{
		auto ix = self->atomic_steal_next.fetch_add(1);
		return self->workers[ix % self->workers.count];
	}
}