// Copyright Edd Dawson 2012
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "pe.hpp"
#include "file.hpp"

#include <cassert>
#include <cstring>
#include <vector>

namespace dbg 
{
    namespace
    {
        void read_string_table(file &pe, uint64_t tabpos, std::vector<char> &strtab)
        {
            const uint64_t old_pos = pe.offset();

            pe.go(tabpos);

            const uint32_t tab_size = pe.u32(); // includes this size field
            if (tab_size < 4)
                throw file_error("invalid size in COFF string table");
            
            std::vector<char> temp(tab_size + 1, '\0'); // extra '\0' on the end just in case
            pe.read(reinterpret_cast<uint8_t *>(&temp[4]), tab_size - 4);

            strtab.swap(temp);
            pe.go(old_pos);
        }

        void read_section_offsets(file &pe, uint64_t tabpos, uint16_t number_of_sections, std::vector<uint32_t> &section_offsets)
        {
            section_offsets.reserve(number_of_sections);

            const uint64_t old_pos = pe.offset();

            pe.go(tabpos);

            for (uint16_t s = 0; s != number_of_sections; ++s)
            {
                pe.skip(8 + 4); // name + virtual size
                section_offsets.push_back(pe.u32());
                pe.skip(24); // the rest
            }

            pe.go(old_pos);
        }

        // Returns the number of aux. symbols
        uint8_t read_symbol(file &pe, pe_sink &sink, const std::vector<uint32_t> &section_offsets, const std::vector<char> &strtab)
        {
            const uint64_t short_name = pe.u64();
            const uint32_t value = pe.u32();
            const uint16_t section = pe.u16();
            const uint16_t type = pe.u16();
            pe.skip(1); // storage class
            const uint8_t aux_symbols = pe.u8();

            if (type == 0x20) // symbol is a function
            {
                char namebuff[9];
                const char *name = 0;

                if ((short_name & 0xFFFFffff) == 0)
                {
                    const uint32_t strtab_offset = (short_name >> 32) & 0xFFFFffff;
                    name = &strtab.at(strtab_offset);
                }
                else
                {
                    std::memcpy(namebuff, &short_name, 8);
                    namebuff[8] = '\0';
                    name = namebuff;
                }

                assert(name);

                if (section != 0 && section <= section_offsets.size())
                    sink.on_symbol(name, value + section_offsets[section-1]);
            }

            return aux_symbols;
        }

        void read_symbol_table(file &pe,
                               pe_sink &sink,
                               uint64_t tabpos,
                               uint32_t number_of_symbols,
                               const std::vector<uint32_t> &section_offsets,
                               const std::vector<char> &strtab)
        {
            const uint64_t old_pos = pe.offset();

            pe.go(tabpos);

            for (uint32_t s = 0; s != number_of_symbols; ++s)
            {
                const uint8_t aux_symbols = read_symbol(pe, sink, section_offsets, strtab);

                if (aux_symbols)
                {
                    s += aux_symbols;
                    if (s >= number_of_symbols)
                        throw file_error("Invalid aux. symbol count in COFF symbol");

                    pe.skip(18 * aux_symbols);
                }
            }

            pe.go(old_pos);
        }

    } // anonymous

    void scan_mingw_pe_file(file &pe, pe_sink &sink)
    {
        pe.go(0x3C);
        pe.go(pe.u32());
    
        // Check PE signature.
        if (pe.u8() != 'P' || pe.u8() != 'E' || pe.u16() != 0) 
            throw file_error("file is not a Portable Executable");


        // Read COFF header

        pe.skip(2); // machine, number of sections, timestamp
        const uint16_t number_of_sections = pe.u16();
        pe.skip(4); // timestamp

        const uint32_t abs_pos_symtab = pe.u32();
        const uint32_t number_of_symbols = pe.u32();

        const uint16_t size_of_optional_header = pe.u16();
        pe.skip(2); // characteristics

        if (abs_pos_symtab == 0) 
            throw file_error("no COFF symbols");


        // Read 'optional' header

        const uint64_t abs_pos_optional_header = pe.offset();

        const uint16_t magic = pe.u16();
        if (magic != 0x10b && magic != 0x20b) 
            throw file_error("magic number in optional header not recognized");

        const bool pe_plus = (magic == 0x20b);

        if (pe_plus) pe.skip(106);
        else pe.skip(90);

        const uint32_t number_of_rva_and_sizes = pe.u32();

        // Data directories
        pe.skip(number_of_rva_and_sizes * 8);

        if (pe.offset() - abs_pos_optional_header != size_of_optional_header)
            throw file_error("size of optional header did not match expectation");

        std::vector<uint32_t> section_offsets;
        read_section_offsets(pe, pe.offset(), number_of_sections, section_offsets);

        std::vector<char> strtab;
        read_string_table(pe, abs_pos_symtab + 18 * number_of_symbols, strtab);

        read_symbol_table(pe, sink, abs_pos_symtab, number_of_symbols, section_offsets, strtab);
    }

} // dbg
