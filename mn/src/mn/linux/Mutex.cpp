#include "mn/File.h"
#include "mn/Mutex.h"

#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <sys/stat.h>        /* For mode constants */
#include <semaphore.h>

/* Linux unfortunately does not offer a direct Named Mutex implementation
	similar to windows', so we have to choose One of several options:

	1. Named Semaphores (closest implementation, but kernel persistent).
	2. Regular mutex but created in shared memory area.
	3. Create a regular file and create a lock on it.
	
	I chose the 3rd option since it was the simplest, and did not require 
	black magic.
*/

namespace mn
{
	struct flock 
	{
		short l_type;    /* Type of lock: F_RDLCK,
							F_WRLCK, F_UNLCK */
		short l_whence;  /* How to interpret l_start:
							SEEK_SET, SEEK_CUR, SEEK_END */
		off_t l_start;   /* Starting offset for lock */
		off_t l_len;     /* Number of bytes to lock */
		pid_t l_pid;     /* PID of process blocking our lock
								(F_GETLK only) */
	};

	MutexCreationResult
	create_mutex(mn::Str mutex_name, bool isOwner = true)
	{
		MutexCreationResult result;
		result.linux_handle = -1;

		flock lf{};
		lf.l_type = F_WRLCK;
		lf.l_start = 0;
		lf.l_len = 0;
		// Lock the entire file.
		lf.l_whence = SEEK_SET;
		//lf.l_pid is ignored on setting flags.

		auto fileHandle = mn::file_open(mutex_name, mn::IO_MODE::READ_WRITE, mn::OPEN_MODE::CREATE_ONLY);
		
		if(fileHandle->linux_handle == -1)
		{
			if(errno == EAGAIN)
				result.kind = MutexCreationResult::Result_KIND::ALREADY_EXISTS;
			else
				result.kind = MutexCreationResult::Result_KIND::FAILURE;
			return result;
		}

		auto res = fcntl(fileHandle->linux_handle, F_SETLK, &lf);

		if(res == -1)
		{
			switch (errno)
			{
				case EAGAIN:
				case EACCES:
				{
					result.kind = MutexCreationResult::Result_KIND::ALREADY_EXISTS;
					return result;
				}
				default:
					result.kind = MutexCreationResult::Result_KIND::FAILURE;
					return result;
			}
		}
		else
		{
			result.kind = MutexCreationResult::Result_KIND::SUCCESS;
			result.linux_handle = fileHandle->linux_handle;
		}
		
	}

	bool
	wait_mutex(mn::Str mutex_name)
	{
		auto fileHandle = mn::file_open(mutex_name, mn::IO_MODE::READ_WRITE, mn::OPEN_MODE::CREATE_OVERWRITE);
		if (fileHandle->linux_handle == -1)
			return false;
	}
}