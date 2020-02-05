#include "mn/IPC.h"
#include "mn/Str.h"
#include "mn/Memory.h"
#include "mn/Fabric.h"
#include "mn/File.h"

#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <sys/mman.h>
#include <unistd.h>
#include <assert.h>

namespace mn::ipc
{

    struct IIPC_Mutex
    {
        mn::File mtx;
    };

    Mutex
    mutex_new(const Str& name)
    {

        auto mtx = mn::file_open(name, mn::IO_MODE::WRITE, mn::OPEN_MODE::CREATE_APPEND);
        if(mtx == nullptr)
            return  nullptr;

        auto self = alloc<IIPC_Mutex>();
        self->mtx = mtx;
        return self;
    }

    void
    mutex_free(Mutex self)
    {
       self->mtx->dispose();

       mn::free(self);
    }

    void
    mutex_lock(Mutex self)
    {
        worker_block_ahead();

        file_write_lock(self->mtx, 0, 0);

        worker_block_clear();
    }

    LOCK_RESULT
    mutex_try_lock(Mutex self)
    {
        bool res = file_write_try_lock(self->mtx, 0, 0);

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
    mutex_unlock(Mutex self)
    {
        file_write_unlock(self->mtx, 0, 0);
    }
}
