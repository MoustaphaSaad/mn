#include "mn/Defer.h"
#include "mn/File.h"
#include "mn/Memory.h"
#include "mn/Scope.h"
#include "mn/Thread.h"
#include "mn/Window.h"

#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include "..\..\..\include\mn\Mutex.h"
#include <Windows.h>
#include <mbstring.h>
#include <tchar.h>

namespace mn
{
	MutexCreationResult
	create_mutex(mn::Str mutex_name, bool isOwner)
	{
		MutexCreationResult res{};

		CreateMutexA(0, isOwner, mutex_name.ptr); // try to create a named mutex

		switch (GetLastError())
		{
		case ERROR_ALREADY_EXISTS:
		{
			res.kind = MutexCreationResult::Result_KIND::ALREADY_EXISTS;
			break;
		}
		case ERROR_ACCESS_DENIED:
		{
			res.kind = MutexCreationResult::Result_KIND::FAILURE;
			break;
		}
		default:
			res.kind = MutexCreationResult::Result_KIND::SUCCESS;
			break;
		}

		return res;
	}

	bool
	wait_mutex(mn::Str mutex_name)
	{
		auto handle = OpenMutexA(MUTEX_ALL_ACCESS, FALSE, mutex_name.ptr);
		if (handle != NULL)
		{
			auto result = WaitForSingleObject(handle, 0);
			if (result == S_OK)
				return true;
		}
		return false;
	}
} // namespace mn