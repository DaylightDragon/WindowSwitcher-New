#pragma once

#include "Data.h"
#include "WindowRelated.h"
//#include "InputRelated.h"

#include <atomic>
#include <iostream>
#include <map>
#include <string>
#include <Windows.h>

extern std::string currentVersion;
extern std::string programPath;
extern std::map<std::string, int> mapOfKeys;
extern std::map<HWND, WindowGroup*> handleToGroup;
extern std::atomic<Settings*> settings;
extern std::atomic<InputsInterruptionManager*> interruptionManager;

extern bool hideNotMainWindows;

std::atomic<bool>& getStopMacroInput();
std::string getCurrentVersion();
bool windowIsLinkedManually(HWND hwnd);
bool windowIsLinkedAutomatically(HWND hwnd);

bool checkHungWindow(HWND hwnd);
std::wstring getWindowName(HWND hwnd);
