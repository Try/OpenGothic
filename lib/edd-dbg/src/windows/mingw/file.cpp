// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "file.hpp"

#include <algorithm>
#include <cassert>
#include <cstring>

#include <windows.h>
#undef min
#undef max

namespace dbg 
{
    file::file(const wchar_t *filename) :
        h(CreateFileW(filename, GENERIC_READ, FILE_SHARE_READ, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0)),
        sz(0),
        next(buffer),
        buffer_begin_pos(0)
    {
        if (!h || h == INVALID_HANDLE_VALUE)
            throw file_error("failed to open file");

        LARGE_INTEGER file_size;
        if (!GetFileSizeEx(static_cast<HANDLE>(h), &file_size))
        {
            CloseHandle(static_cast<HANDLE>(h));
            throw file_error("failed to get file size");
        }
        sz = file_size.QuadPart;

        refill(0);
    }

    file::~file()
    {
        CloseHandle(static_cast<HANDLE>(h));
    }


    uint64_t file::size() const
    {
        return sz;
    }

    void file::go(uint64_t pos)
    {
        if (pos > sz)
            throw file_error("bad stream cursor position specified");

        if (position_in_buffer(pos))
        {
            next += (pos - offset());
        }
        else
        {
            LARGE_INTEGER file_pos;
            file_pos.QuadPart = pos;

            LARGE_INTEGER new_cursor;

            if (!SetFilePointerEx(static_cast<HANDLE>(h), file_pos, &new_cursor, FILE_BEGIN))
                throw file_error("failed to move stream cursor");

            refill(pos);
        }
    }

    void file::skip(int64_t delta)
    {
        if (delta == 0)
            return;

        const uint64_t cursor = offset();

        if ((delta < 0 && static_cast<uint64_t>(-delta) > cursor) ||
            (delta > 0 && static_cast<uint64_t>(delta) + cursor > sz))
        {
            throw file_error("bad stream cursor position specified");
        }

        go(cursor + delta);
    }

    uint64_t file::offset() const
    {
        return buffer_begin_pos + (next - buffer);
    }

    uint64_t file::u64()
    {
        return read_integer<uint64_t>();
    }

    uint32_t file::u32()
    {
        return read_integer<uint32_t>();
    }

    uint16_t file::u16()
    {
        return read_integer<uint16_t>();
    }

    uint8_t file::u8()
    {
        return read_integer<uint8_t>();
    }

    void file::read(uint8_t *bytes, std::size_t n)
    {
        if (n + offset() > sz)
            throw file_error("failed to read from file");

        const uint8_t * const buffer_end = buffer + sizeof buffer;
        const uint8_t * const bytes_end = bytes + n;

        while (bytes != bytes_end)
        {
            const std::size_t in_buffer = 
                static_cast<std::size_t>(std::min<uint64_t>(buffer_end - next, sz - offset()));

            const std::size_t to_copy = std::min<std::size_t>(in_buffer, bytes_end - bytes);

            std::memcpy(bytes, next, to_copy);
            bytes += to_copy;
            next += to_copy;

            if (next == buffer_end)
                refill(buffer_begin_pos + sizeof buffer);
        }
    }

    bool file::position_in_buffer(uint64_t pos) const
    {
        const uint64_t buffer_end_pos = std::min(buffer_begin_pos + sizeof buffer, sz);

        return pos >= buffer_begin_pos && pos < buffer_end_pos;
    }

    template<typename T>
    T file::read_integer()
    {
        T ret = 0;
        read(reinterpret_cast<uint8_t *>(&ret), sizeof ret);
        return ret;
    }

    void file::refill(std::size_t new_begin_pos)
    {
        buffer_begin_pos = new_begin_pos;
        next = buffer;

        const std::size_t to_read =
            static_cast<std::size_t>(std::min<uint64_t>(sizeof buffer, sz - buffer_begin_pos));

        if (to_read == 0)
            return;

        assert(to_read <= DWORD(-1)); // due to sizeof buffer

        DWORD num_read = 0;
        if (!ReadFile(static_cast<HANDLE>(h), buffer, static_cast<DWORD>(to_read), &num_read, 0) ||
            num_read != to_read)
        {
            throw file_error("failed to read from file");
        }
    }

} // dbg
