#pragma once

#include "mn/Exports.h"
#include "mn/Task.h"

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

	// worker_do schedules any callable into the worker queue
	template<typename TFunc>
	inline static void
	worker_do(Worker self, TFunc&& f)
	{
		worker_task_do(self, Task<void()>::make(std::forward<TFunc>(f)));
	}


	// fabric
	typedef struct IFabric* Fabric;

	// fabric_new creates a group of workers
	MN_EXPORT Fabric
	fabric_new(size_t worker_count = 0, uint32_t blocking_threshold_in_ms = 0, size_t put_aside_worker_count = 0);

	// fabric_free stops and frees the group of workers
	MN_EXPORT void
	fabric_free(Fabric self);

	inline static void
	destruct(Fabric self)
	{
		fabric_free(self);
	}

	// fabric_worker_next gets the next worker to push the task into, it uses round robin technique
	MN_EXPORT Worker
	fabric_worker_next(Fabric self);

	MN_EXPORT Worker
	fabric_steal_next(Fabric self);

	template<typename TFunc>
	inline static void
	fabric_do(Fabric self, TFunc&& f)
	{
		worker_task_do(fabric_worker_next(self), Task<void()>::make(std::forward<TFunc&&>(f)));
	}
}