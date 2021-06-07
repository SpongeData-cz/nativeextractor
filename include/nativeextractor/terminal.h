// Copyright (C) 2021 SpongeData s.r.o.
//
// NativeExtractor is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// NativeExtractor is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with NativeExtractor. If not, see <http://www.gnu.org/licenses/>.

#ifndef TERMINAL_H
#define TERMINAL_H

#include <stdio.h>

// Source: http://www.malloc.co/linux/how-to-print-with-color-in-linux-terminal/
// \033[{a1};..;{aN}m
#define TC_RESET      0
#define TC_BRIGHT     1
#define TC_DIM        2
#define TC_UNDERSCORE 4
#define TC_BLINK      5
#define TC_REVERSE    7
#define TC_HIDDEN     8

#define TC_F_BLACK   30
#define TC_F_RED     31
#define TC_F_GREEN   32
#define TC_F_YELLOW  33
#define TC_F_BLUE    34
#define TC_F_MAGENTA 35
#define TC_F_CYAN    36
#define TC_F_WHITE   37

#define TC_B_BLACK   40
#define TC_B_RED     41
#define TC_B_GREEN   42
#define TC_B_YELLOW  43
#define TC_B_BLUE    44
#define TC_B_MAGENTA 45
#define TC_B_CYAN    46
#define TC_B_WHITE   47

#define __TCSTR(a) #a

#define TC1(a) "\033[" __TCSTR(a) "m"

#define TC2(a, b) "\033[" __TCSTR(a) ";" __TCSTR(b) "m"

#define TC3(a, b, c) "\033[" __TCSTR(a) ";" __TCSTR(b) ";" __TCSTR(c) "m"

#define PRINT_SUCCESS(format, ...) \
  printf(TC2(TC_B_GREEN, TC_F_BLACK) "Success: " format TC1(TC_RESET) "\n", ##__VA_ARGS__)

#define PRINT_INFO(format, ...) \
  printf(TC2(TC_B_CYAN, TC_F_BLACK) "Info: " format TC1(TC_RESET) "\n", ##__VA_ARGS__)

#define PRINT_WARNING(format, ...) \
  printf(TC2(TC_B_YELLOW, TC_F_BLACK) "Warning: " format TC1(TC_RESET) "\n", ##__VA_ARGS__)

#define PRINT_ERROR(format, ...) \
  printf(TC2(TC_B_RED, TC_F_BLACK) "Error: " format TC1(TC_RESET) "\n", ##__VA_ARGS__)

#endif //TERMINAL_H
