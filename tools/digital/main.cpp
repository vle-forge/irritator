// Copyright (c) 2026 INRAE Distributed under the Boost Software License,
// Version 1.0. (See accompanying file LICENSE_1_0.txt or copy at
// http://www.boost.org/LICENSE_1_0.txt)

#include "digital.hpp"

#include <cstdio>

int main(int argc, const char* argv[])
{
    if (argc != 2) {
        std::fputs("usage: irritator-digital directory\n\n"
                   "Writes the components of the 6502 in directory/digital.\n",
                   stderr);
        return 1;
    }

    if (not irt::digital::generate(argv[1])) {
        std::fprintf(stderr,
                     "irritator-digital: fail to write the components in "
                     "%s/digital\n",
                     argv[1]);
        return 1;
    }

    return 0;
}
