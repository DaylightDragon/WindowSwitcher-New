#pragma once

#include <iostream>
#include <map>
#include <string>
#include <Windows.h>

#include "WindowRelated.h"
#include "Data.h"

extern std::map<std::string, int> mapOfKeys;
extern std::map<HWND, WindowGroup*> handleToGroup;
extern std::atomic<Settings*> settings;;

extern bool hideNotMainWindows;

std::atomic<bool>& getStopMacroInput();
std::string getCurrentVersion();
bool windowIsLinkedManually(HWND hwnd);
bool windowIsLinkedAutomatically(HWND hwnd);

bool checkHungWindow(HWND hwnd);
std::wstring getWindowName(HWND hwnd);