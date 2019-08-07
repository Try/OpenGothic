// Copyright Edd Dawson 2013
// Distributed under the Boost Software License, Version 1.0.
// (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#ifndef PE_SYMTAB_HPP_1210_17022013
#define PE_SYMTAB_HPP_1210_17022013

#include <vector>

namespace dbg 
{
    class file;

    struct pe_symbol
    {
        pe_symbol() : address(0), name_offset(0) { }
        std::size_t address;
        std::size_t name_offset;
    };

    inline bool operator< (const pe_symbol &lhs, const pe_symbol &rhs)
    {
        return lhs.address < rhs.address;
    }

    class pe_symtab
    {
        public:
            pe_symtab();
            pe_symtab(const void *module, file &pe);

            const char *function_spanning(const void *address) const;

            void swap(pe_symtab &other);

        private:
            pe_symtab(const pe_symtab &);
            pe_symtab &operator= (const pe_symtab &);

        private:
            std::vector<char> strtab;
            std::vector<pe_symbol> symbols;
    };

} // dbg

#endif // PE_SYMTAB_HPP_1210_17022013
