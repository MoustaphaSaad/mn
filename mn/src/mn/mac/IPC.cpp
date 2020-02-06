#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"


#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

namespace mn::ipc
{

    Mutex
    mutex_new(const Str& name)
    {
        return (Mutex)mn::file_open(name, mn::IO_MODE::WRITE, mn::OPEN_MODE::CREATE_APPEND);
    }

    void
    mutex_free(Mutex mtx)
    {
        auto self = (File)mtx;
        self->dispose();
    }

    void
    mutex_lock(Mutex mtx)
    {
        worker_block_ahead();

        auto self = (File)mtx;
        file_write_lock(self, 0, 0);

        worker_block_clear();
    }

    LOCK_RESULT
    mutex_try_lock(Mutex mtx)
    {
        auto self = (File)mtx;
        bool res = file_write_try_lock(self, 0, 0);

        if(res)
        {
            return  LOCK_RESULT::OBTAINED;
        }
        else
        {
            return LOCK_RESULT::FAILED;
        }

    }

    void
    mutex_unlock(Mutex mtx)
    {
        auto self = (File)mtx;
        file_write_unlock(self, 0, 0);
    }
}
