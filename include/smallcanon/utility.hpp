#pragma once

#include <iostream>

#define console_bold "\033[1m"
#define console_green "\033[32m"
#define console_blue "\033[36m"
#define console_orange "\033[33m"
#define console_bright_blue "\033[96m"
#define console_magenta "\033[35m"
#define console_red "\033[33m"
#define console_neutral "\033[0m"

#ifdef PRINT_DEBUG
    #define DEBUG_STREAM std::clog
#else
    #define DEBUG_STREAM if (true) {} else std::clog
#endif
