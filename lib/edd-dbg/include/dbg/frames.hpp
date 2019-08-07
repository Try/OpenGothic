// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef FRAMES_HPP_1619_23062012
#define FRAMES_HPP_1619_23062012

#include "dbg/config.hpp"
#include "dbg/static_assert.hpp"

#include <iosfwd>

namespace dbg 
{
    class symdb;

    // A frame_sink is used to collect/print/... frames found by the walk_frames() function.
    class frame_sink
    {
        public:
            virtual ~frame_sink() { }

            // Called for each frame found. false should be returned if/when enough
            // frames have been collected.
            virtual bool on_frame(unsigned level, const void *program_counter) = 0;

        protected:
            frame_sink() { }

        private:
            frame_sink(const frame_sink &);
            frame_sink &operator= (const frame_sink &);
    };

    // Attempts to walk the call stack, calling the sink's on_frame() method as
    // it goes.
    // Frames are delivered beginning at the call site and working outwards, towards
    // the entry point. The value of level passed to on_frame() will have increased
    // by one since the previous call. On the first call, level will have a value of
    // zero.
    // Returns true if the entire stack was enumerated without error or the sink's 
    // on_frame() method returning false.
    bool walk_frames(frame_sink &sink);

    // Fills [pcs, pcs + max_frames) with null-valued pointers. Then the program counters 
    // for the call stack are collected into the array. The first 'ignore' frames of the 
    // call stack are skipped. 
    // A maximum of max_frames elements is stored. The number of frames stored is 
    // returned.
    DBG_NEVER_INLINE unsigned store_frames(const void *pcs[], unsigned max_frames, unsigned ignore = 0);

    // Writes each program counter along with the associated function name (where available),
    // and the module in which the function resides (where available) to the given ostream.
    // Each pc/function/module is written to a separate line, with the given indentation.
    // Iteration over the array stops at index num_pcs, or when a null-valued pointer is
    // found.
    void log_frames(const void * const pcs[], unsigned num_pcs, 
                    const symdb &db,
                    std::ostream &out,
                    const char *indent = "");

    // A simple wrapper for store_frames()/log_frames().
    template<unsigned N>
    class call_stack
    {
        public:
            // Construct a call_stack. collect() must be called to capture the frames.
            call_stack() : n(0) { pcs[0] = 0; }

            // Attempts to capture up to N call frames, ignoring the inner-most 'ignore' frames,
            // starting at the callee's frame.
            DBG_NEVER_INLINE void collect(unsigned ignore) { n = store_frames(pcs, N, ignore + 1); }

            // Returns the number of frames stored in the last call to collect(), or 0 if collect
            // has not yet been called.
            unsigned size() const { return n; }

            // Returns the program_counter for the frame with the given index from the previous
            // collect()ion. A null-valued pointer is returned if i >= size()
            const void *pc(unsigned i) const { return i < n ? pcs[i] : static_cast<const void *>(0); }

            // log the captured frames to the specified ostream, with the names of functions
            // corresponding to captured program counter values where available in the symdb.
            void log(const symdb &db, std::ostream &out, const char *indent = "") const
            {
                log_frames(pcs, n, db, out, indent);
            }

        private:
            DBG_STATIC_ASSERT(N != 0);

            const void *pcs[N];
            unsigned n;
    };



} // dbg

#endif // FRAMES_HPP_1619_23062012
