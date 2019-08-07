// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "dll.hpp"
#include "memcpy_cast.hpp"

#include <algorithm>

#include <windows.h>

namespace dbg 
{
    dll::dll() :
        module(0)
    {
    }

    dll::dll(const char *name) :
        module(LoadLibraryA(name))
    {
    }

    dll::~dll()
    {
        if (module)
            FreeLibrary(static_cast<HMODULE>(module));
    }

    bool dll::loaded() const
    {
        return module != 0;
    }

    dll::opaque_func_ptr dll::find_function(const char *name) const
    {
        if (!module)
            return opaque_func_ptr(0);

        return opaque_func_ptr(
            memcpy_cast<void (*)(void)>(GetProcAddress(static_cast<HMODULE>(module), name))
        );
    }

    void dll::swap(dll &other)
    {
        std::swap(module, other.module);
    }

} // dbg
