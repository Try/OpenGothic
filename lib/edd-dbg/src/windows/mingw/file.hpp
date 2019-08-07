// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef FILE_HPP_0047_29062012
#define FILE_HPP_0047_29062012

#include <exception>
#include <cstddef>

#include <stdint.h>

namespace dbg 
{
    class file_error : public std::exception
    {
        public:
            explicit file_error(const char *err) : err(err) { }
            ~file_error() throw() { }

            const char *what() const throw() { return err; }

        private:
            const char *err;
    };

    // A simple file class for reading bytes and integers.
    // Supports 64 bit file offsets.
    // It assumes the file content and host system are little endian.
    class file
    {
        public:
            // May throw file_error if the file with the given UTF-16 name
            // cannot be opened or examined.
            explicit file(const wchar_t *filename);
            ~file();

            // Returns the number of bytes in the file's primary data stream.
            uint64_t size() const;

            // Moves the read cursor to the position specified. file_error is
            // thrown on failure, in which case the resulting read cursor
            // position is unspecified.
            void go(uint64_t pos);

            // Moves the read cursor relative to its current position. file_error 
            // is thrown on failure, in which case the resulting read cursor
            // position is unspecified.
            void skip(int64_t delta);

            // Returns the current position of the read cursor.
            uint64_t offset() const;

            // All reading functions update the read cursor on success. On failure,
            // a file_error is thrown and the resulting read cursor position is
            // unspecified.
            uint64_t u64();
            uint32_t u32();
            uint16_t u16();
            uint8_t  u8();

            void read(uint8_t *bytes, std::size_t n);

        private:
            file(const file &);
            file &operator= (const file &);

            bool position_in_buffer(uint64_t pos) const;

            template<typename T>
            T read_integer();

            void refill(std::size_t new_begin_pos);

        private:
            void *h;
            uint64_t sz;

            uint8_t buffer[8 * 1024];
            const uint8_t *next;

            uint64_t buffer_begin_pos;
    };

} // dbg

#endif // FILE_HPP_0047_29062012
