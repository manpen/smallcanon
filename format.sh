#!/bin/bash
find cli benchmarks tests include -type f \( -name '*.hpp' -o -name '*.cpp' -o -name '*.inc' \) -exec clang-format-22 -i {} +
