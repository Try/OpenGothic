// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef SYMBOLS_HPP_2143_23062012
#define SYMBOLS_HPP_2143_23062012

namespace dbg 
{
    // Processes symbols fed to it by symdb.
    class symsink
    {
        public:
            symsink() { }
            virtual ~symsink() { }

            // Implemented in terms of process_function(). Ensures that the implementation
            // never sees null-valued strings.
            void on_function(const void *program_counter, const char *name, const char *module);

        private:
            // Should be implemented in a derived class in order to process the name and
            // module of a function provided by a symdb. name and module will
            // not be null, though they may not be real symbol names either.
            // The locations pointed to by name and module will be invalid after the
            // call so derived classes should make copies of the strings if required.
            virtual void process_function(const void *program_counter, const char *name, const char *module) = 0;

        private:
            symsink(const symsink &);
            symsink &operator= (const symsink &);
    };

    // Used to lookup the names of functions corresponding to code addresses.
    class symdb
    {
        public:
            symdb();
            ~symdb();

            // Calls sink.on_function() with the name of the function spanning the specified 
            // code address and the module in which the function resides. For each piece of 
            // unavailable information, a null-valued pointer is passed instead.
            bool lookup_function(const void *program_counter, symsink &sink) const;

        private:
            symdb(const symdb &);
            symdb &operator= (const symdb &);

        private:
            struct impl;
            impl *p;
    };

} // dbg

#endif // SYMBOLS_HPP_2143_23062012
