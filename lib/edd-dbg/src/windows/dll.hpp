// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef DLL_HPP_2329_27082012
#define DLL_HPP_2329_27082012

namespace dbg 
{
    // Small wrapper around a dynamically loaded DLL.
    class dll
    {
        public:
            struct opaque_func_ptr
            {
                explicit opaque_func_ptr(void (*func)(void)) : func(func) { }

                template<typename FuncPtr>
                operator FuncPtr () const { return reinterpret_cast<FuncPtr>(func); }

                void (*func)(void);
            };

            // Creates an dll object where loaded() is false;
            dll();

            // Attemps to load the DLL with the given name. loaded() returns
            // true if the DLL was loaded succesfully.
            dll(const char *name);

            // Unloads (or at least decreases the reference count) of the associated
            // module.
            ~dll();

            // Returns true if this dll refers to a loaded module.
            bool loaded() const;

            // Lookup a function with the given name in the dll.
            // Returns a 'null' opaque_func_ptr is the dll isn't loaded, or if a
            // function with the specified name could not be found.
            opaque_func_ptr find_function(const char *name) const;

            // swaps the guts of this dll with those of other.
            void swap(dll &other);

        private:
            dll(const dll &);
            dll &operator=(const dll &);

        private:
            void * module;
    };

} // dbg

#endif // DLL_HPP_2329_27082012
