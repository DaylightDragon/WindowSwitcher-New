#pragma once

#include "Windows.h"
#include <iostream>
#include <iosfwd>
#include <sstream>

#ifndef WINDOW_SWITCHER_CONSOLE_MANAGEMENT_H
#define WINDOW_SWITCHER_CONSOLE_MANAGEMENT_H

extern bool startWithConsole;
extern HWND console_window;
extern std::streambuf* default_cout_buff;

#endif

void showOrHideConsole();
void showOrHideConsole(bool state);
void initConsole();
bool isConsoleAllocated();