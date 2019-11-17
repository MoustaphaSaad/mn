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
	MN_EXPORT MutexCreationResult
	create_mutex(mn::Str mutex_name, bool isOwner)
	{
		MutexCreationResult res{};

		auto creationres = CreateMutexA(0, isOwner, mutex_name.ptr); // try to create a named mutex

		printf("Mutex handle:\t%p.\n", creationres);

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
			printf("Mutex Success!\n");
			break;
		}

		return res;
	}
} // namespace mn