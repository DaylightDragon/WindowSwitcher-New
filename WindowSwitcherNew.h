#pragma once

#include <iostream>
#include <map>
#include <string>
#include <Windows.h>

std::map<std::string, int> getMapOfKeys();
std::atomic<bool>& getStopMacroInput();
//std::atomic<bool>& getInterruptedRightNow();
std::string getCurrentVersion();
bool windowIsLinkedManually(HWND hwnd);
bool windowIsLinkedAutomatically(HWND hwnd);
