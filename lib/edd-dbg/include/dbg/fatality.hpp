// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef FATALITY_HPP_2041_25082012
#define FATALITY_HPP_2041_25082012

namespace dbg 
{
    typedef void (*fatality_handler)(void);

    // Set the function called when a fatal condition is encountered.
    // By default this function is std::abort.
    // The old handler is returned.
    // Typically called at the start of a program.
    fatality_handler set_fatality_handler(fatality_handler handler);

    // Get the current fatality handler.
    fatality_handler get_fatality_handler();

} // dbg

#endif // FATALITY_HPP_2041_25082012
