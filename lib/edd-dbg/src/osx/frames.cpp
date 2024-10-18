// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dbg/frames.hpp"
#include "memcpy_cast.hpp"

#include <pthread.h>
#include <stdint.h>

#if !defined(__i386__) && !defined(__amd64__) && !defined(__arm64__)
#   error "unsupported architecture :("
#endif

namespace dbg 
{
    namespace
    {
        struct frame_ptr_check
        {
            frame_ptr_check() :
                top(pthread_get_stackaddr_np(pthread_self())),
                bottom(static_cast<const char *>(top) - pthread_get_stacksize_np(pthread_self()))
            {
            }

            bool valid(const void *p) const
            {
#if defined(__i386__)
                const uintptr_t alignment = 8;
#else
                const uintptr_t alignment = 0;
#endif
                const uintptr_t pi = memcpy_cast<uintptr_t>(p);

                return
                    (pi & 0x0F) == alignment &&
                    p >= bottom &&
                    p <= top;
            }

            const void * const top;
            const void * const bottom;
        };

    } // anonymous

    bool walk_frames(frame_sink &sink)
    {
        unsigned level = 0;

        frame_ptr_check fpcheck;

        void **frame = static_cast<void **>(__builtin_frame_address(1)); // skip this frame

        if (!fpcheck.valid(frame))
            return true;

        while (true)
        {
            // Caller pushes arguments followed by return address.
            // On top of that, the callee function's prologue saves (pushes) the value of EBP/RBP
            // so that it can be restored when the function returns.
            //
            //          (higher addresses)
            //        | ...                |
            //    |   +--------------------+
            //  g |   | Function arguments |   <-- stored by caller
            //  r |   +--------------------+
            //  o |   | Return address     |   <-- stored by caller
            //  w |   +--------------------+
            //  t |   | Saved EBP/RBP      |   <-- stored by callee's prologue
            //  h |   +--------------------+
            //    v   | Function locals    |
            //        | ...                |
            //          (lower adresses)
            //
            // 'frame' points to the saved EBP/RBP, so frame[1] is the entry below that, i.e.
            // the return address.

            // FIXME: would like a better way to check if frame[1] points to executable code.
            const uintptr_t pc = memcpy_cast<uintptr_t>(frame[1]);

            if (pc < 0x10)
                break;

            if (!sink.on_frame(level++, frame[1]))
                return false;

            void **next_frame = static_cast<void **>(frame[0]);

            // Note: If the -fomit-frame-pointer optimization is enabled the compiler can
            // elect not to save EBP/RBP in order to save a few instructions. In this case,
            // the walk will produce junk.
            //
            // These validity checks ensure that our walk always heads down the stack in
            // the direction of earlier frames and that what we believe to be saved EBP/RBP
            // values do at least point in to the stack.

            if (!fpcheck.valid(next_frame) || next_frame <= frame)
                break;

            frame = next_frame;
        }

        return true;
    }

} // dbg
