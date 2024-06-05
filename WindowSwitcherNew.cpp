// Window Switcher

#pragma comment(lib, "Dwmapi.lib") // for DwmGetWindowAttribute and so on

#include <yaml-cpp/yaml.h>
#include <windows.h>
#include <iostream>
#include <stdio.h>
#include <map>
#include <vector>
#include <cstdlib>
#include <thread>
#include <string>
#include <conio.h> // ig for _getch
#include <dwmapi.h>
#include <condition_variable>
#include <csignal>

#include "InputRelated.cpp"
#include "Data.cpp"

#include "ConfigOperations.h"
#include "GeneralUtils.h"
#include "CustomHotkeys.h"

std::string currentVersion = "2.3";

// Pre-defining all classes and methods
class WindowGroup;
void deleteWindowGroup(WindowGroup* wg);
//Settings

// Windows, sequences and managers
std::map<HWND, WindowGroup*> handleToGroup;
std::map<WindowGroup*, KeySequence*> groupToSequence;
WindowGroup* lastGroup;
KeySequence* mainSequence;
std::map<std::string, KeySequence*> knownOtherSequences = std::map<std::string, KeySequence*>();
std::vector<std::string> failedHotkeys;
std::atomic<InputsInterruptionManager*> interruptionManager;
std::atomic<Settings*> settings;
RuntimeData runtimeData;

std::map<int, std::vector<HWND>> autoGroups;

// Permanent settings
std::wstring rx_name = L"Roblox";
std::string programPath;

// Main misc settings variables
std::string defaultMacroKey = "e";
int macroDelayInitial;
int macroDelayBeforeSwitching;
int macroDelayBetweenSwitchingAndFocus;
int macroDelayAfterFocus;
int delayWhenDoneAllWindows;
int macroDelayAfterKeyPress;
int macroDelayAfterKeyRelease;
bool specialSingleWindowModeEnabled;
std::string specialSingleWindowModeKeyCode;
std::vector<std::wstring> defaultFastForegroundWindows;
bool usePrimitiveInterruptionAlgorythm = false;
int primitiveWaitInterval = 100;

// Input randomness
int sleepRandomnessPersent = 10;
int sleepRandomnessMaxDiff = 40;

// Runtime/status variables
std::atomic<bool> stopMacroInput = true;
bool initialConfigLoading = true;
bool hideNotMainWindows = false;
int currentHangWindows = 0;
std::wstring customReturnToFgName = L"";
int restoredCustomWindowsAmount = 0;
int globalGroupId = -100;
int noGroupId = -200;

// Permanent config variables
int oldConfigValue_startAnything;
int oldConfigValue_startKeyboardHook;
int oldConfigValue_startMouseHook;

// Therad related
HHOOK keyboardHook;
HHOOK mouseHook;
std::thread* currentMacroLoopThread;
std::thread* macroDelayWatcherThread;
std::thread* keyboardHookThread;
std::thread* mouseHookThread;

// Advanced macro interruptions
std::condition_variable macroWaitCv;
std::mutex* macroWaitMutex = nullptr;
std::unique_lock<std::mutex>* macroWaitLock = nullptr;

// Debug related
bool debugMode = IsDebuggerPresent();;
bool registerKeyboardHookInDebug = false;
bool registerKeyboardMouseInDebug = true;

//std::string mainConfigName = "WsSettings/settings.yml";

bool getDebugMode() {
    return debugMode;
}

std::string getCurrentVersion() {
    return currentVersion;
}

void printTitle() {
    std::cout << "Window Switcher - Version " << getCurrentVersion() << "\n\n";
}

bool checkHungWindow(HWND hwnd) {
    if (IsHungAppWindow(hwnd)) {
        currentHangWindows += 1;
        return true;
    }
    return false;
}

void hungWindowsAnnouncement() {
    if (currentHangWindows != 0) {
        std::string s = "Detected " + std::to_string(currentHangWindows) + " not responding window";
        if (currentHangWindows > 1) s += "s";
        s += " during the last operation and skipped them";
        std::cout << s << '\n';
        currentHangWindows = 0;
    }
}

struct WindowInfo {
    HWND hwnd;
    std::chrono::steady_clock::time_point lastSequenceInputTimestamp;
    
    WindowInfo(HWND hwnd) {
        this->hwnd = hwnd;
    }

    void refreshTimestamp() {
        lastSequenceInputTimestamp = std::chrono::steady_clock::now();
    }
    
    bool operator==(const WindowInfo& other) const {
        return hwnd == other.hwnd && lastSequenceInputTimestamp == other.lastSequenceInputTimestamp;
    }
};

class WindowGroup {
private:
    std::vector<WindowInfo> windows;
    int index = 0;

public:
    std::vector<WindowInfo> getOthers() {
        std::vector<WindowInfo> v;
        removeClosedWindows();
        if (&windows == nullptr || windows.size() == 0) return v;
        for (int i = 0; i < windows.size(); i++) {
            if (i != index) v.push_back(WindowInfo(windows[i]));
        }
        return v;
    }

    HWND getCurrent() {
        removeClosedWindows();
        if (&windows == NULL || windows.size() == 0) return NULL;
        return windows[index].hwnd;
    }

    WindowInfo* getWindowInfoFromHandle(HWND hwnd) {
        for (auto& it : windows) {
            if (it.hwnd == hwnd) return &it;
        }
        return nullptr;
    }

    bool containsWindow(HWND hwnd) {
        removeClosedWindows();
        return (find(windows.begin(), windows.end(), hwnd) != windows.end());
    }

    void addWindow(HWND hwnd) {
        if (containsWindow(hwnd)) return;
        removeClosedWindows();
        windows.push_back(hwnd);
        if (hideNotMainWindows && index != windows.size() - 1) {
            if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_HIDE);
        }
        //testPrintHwnds();
    }

    void removeWindow(HWND hwnd) {
        if (&windows == NULL || windows.size() == 0) return;
        removeClosedWindows();
        //std::cout << "removeWindow " << hwnds.size() << " " << hwnd;
        bool main = false;
        if (hwnd == windows[index].hwnd) main = true;
        handleToGroup.erase(handleToGroup.find(hwnd));
        windows.erase(std::remove(windows.begin(), windows.end(), hwnd), windows.end());
        if (windows.size() == 0) deleteWindowGroup(this);
        if (main) shiftWindows(-1);
        fixIndex();
    }

    void shiftWindows(int times) {
        if (&windows == NULL) return;
        removeClosedWindows();
        if (windows.size() == 0) return;
        HWND oldHwnd = getCurrent();
        if (times >= 0) {
            index = (index + times) % windows.size();
        }
        else {
            index = (windows.size() * (-1 * times) + index + times) % windows.size();
        }
        if (hideNotMainWindows) {
            //std::cout << oldHwnd << " " << getCurrent() << endl;
            HWND newHwnd = getCurrent();
            if (!checkHungWindow(newHwnd)) ShowWindow(newHwnd, SW_SHOW);
            if (!checkHungWindow(oldHwnd)) ShowWindow(oldHwnd, SW_HIDE);
        }
    }

    void fixIndex() {
        if (&windows == NULL) return;
        if (windows.size() == 0) index = 0;
        else index = index % windows.size();
    }

    void removeClosedWindows() {
        if (&windows == NULL || windows.size() == 0) return;
        for (int i = 0; i < windows.size(); i++) {
            if (&windows[i] && !IsWindow(windows[i].hwnd)) {
                handleToGroup.erase(handleToGroup.find(windows[i].hwnd));
                windows.erase(windows.begin() + i);
            }
        }
        fixIndex();
    }

    void hideOrShowOthers() {
        std::vector<WindowInfo> others = getOthers();
        for (auto& it : others) {
            if (!checkHungWindow(it.hwnd)) {
                if (hideNotMainWindows) {
                    ShowWindow(it.hwnd, SW_HIDE);
                }
                else {
                    ShowWindow(it.hwnd, SW_SHOW);
                }
            }
        }
    }

    void testPrintHwnds() {
        for (auto& hwnd : windows) {
            std::cout << "HWND " << &hwnd.hwnd << '\n';
        }
    }

    int size() {
        return windows.size();
    }

    void moveInOrder(HWND* hwnd, int times) {}
};

bool registerSingleHotkey(int id, UINT fsModifiers, UINT vk, std::string hotkeyName, std::string hotkeyDescription, bool hidden) {
    if (RegisterHotKey(NULL, id, fsModifiers, vk)) {
        if (!hidden) std::cout << "Hotkey '" << hotkeyName << "': " << hotkeyDescription << "\n";
        return true;
    }
    else {
        failedHotkeys.push_back(hotkeyName + " (" + hotkeyDescription + ")");
        return false;
    }
}

bool registerSingleHotkey(int id, UINT fsModifiers, UINT vk, std::string hotkeyName, std::string hotkeyDescription) {
    return registerSingleHotkey(id, fsModifiers, vk, hotkeyName, hotkeyDescription, false);
}

void registerHotkeys() {
    failedHotkeys.clear();
    int totalHotkeys = 28;

    std::vector<KeybindInfo>* keybinds = getActiveKeybinds();
    for (KeybindInfo info : *keybinds) {
        RegisterHotKeyFromText(failedHotkeys, info);
    }

    //registerSingleHotkey(1, MOD_ALT | MOD_NOREPEAT, 0xBC, "Alt + ,", "Add window to current group");
    //registerSingleHotkey(2, MOD_ALT | MOD_NOREPEAT, 0xBE, "Alt + .", "Prepare for the next group");
    //registerSingleHotkey(11, MOD_ALT | MOD_NOREPEAT, 0x49, "Alt + I'", "Edit the window's group");
    //registerSingleHotkey(3, MOD_ALT | MOD_NOREPEAT, 0x4B, "Alt + K'", "Shift windows to the left");
    //registerSingleHotkey(4, MOD_ALT | MOD_NOREPEAT, 0x4C, "Alt + L'", "Shift windows to the right");
    //registerSingleHotkey(5, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x4B, "Ctrl + Alt + K'", "Shift ALL windows to the left");
    //registerSingleHotkey(6, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x4C, "Ctrl + Alt + L'", "Shift ALL windows to the right");
    //registerSingleHotkey(26, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x4B, "Ctrl + Shift + K", "Shift ALL OTHER groups to the left");
    //registerSingleHotkey(27, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x4C, "Ctrl + Shift + L", "Shift ALL OTHER groups to the right");
    //registerSingleHotkey(7, MOD_ALT | MOD_NOREPEAT, 0xDD, "Alt + ]", "Remove current window from it's group (Critical Bug)");
    //registerSingleHotkey(8, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0xDD, "Ctrl + Alt + ]", "Delete the entire group current window is in");
    //registerSingleHotkey(15, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0xDB, "Ctrl + Shift + [", "Delete all groups");
    //registerSingleHotkey(9, MOD_ALT | MOD_NOREPEAT, 0x51, "Alt + Q", "Toggle visibility of the opposite windows in this group (NOT SOON WIP)");
    //registerSingleHotkey(13, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x51, "Ctrl + Alt + Q", "Toggle visibility of all not main windows (May not work properly yet with Auto-key macro)");
    //// ctrl shift a do current alt a, but that one should leave window updating in background
    //registerSingleHotkey(10, MOD_ALT | MOD_NOREPEAT, 0x41, "Alt + A", "Toggle visibility of every last window in all the pairs and minimize the linked ones");
    //registerSingleHotkey(18, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x44, "(WIP) Ctrl + Shift + D", "Set the window as main in current group");
    //registerSingleHotkey(19, MOD_ALT | MOD_NOREPEAT, 0x44, "Alt + D", "(WIP) Swap to main window in current group");
    //registerSingleHotkey(23, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x44, "Ctrl + Alt + D", "(WIP) Swap to main window in all groups");
    //registerSingleHotkey(12, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x55, "Ctrl + Alt + U", "Get all RBX and VMWW windows back from background");
    //registerSingleHotkey(24, MOD_CONTROL | MOD_NOREPEAT, 0x55, "Ctrl + U", "Put current foregrounded window to background");
    //registerSingleHotkey(25, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x55, "Ctrl + Shift + U", "Get specific windows from background by their name");
    //registerSingleHotkey(16, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x56, "Ctrl + Shift + V", "(WIP, NOT SOON) Start/Stop adjusting new RBX windows to screen quarters");
    //registerSingleHotkey(14, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x56, "Ctrl + Alt + V", "Connect all RBX windows to 4 quarter groups");
    //registerSingleHotkey(30, MOD_ALT | MOD_NOREPEAT, 0x56, "Alt + V", "Connect absolutely all RBX windows into one single group");
    //registerSingleHotkey(28, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x41, "Ctrl + Shift + A", "Bring all connected windows to foreground");
    //registerSingleHotkey(17, MOD_ALT | MOD_NOREPEAT, 0x47, "Alt + G", "Start/stop the automatical sequence macro for Roblox windows");
    //registerSingleHotkey(29, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x47, "Ctrl + Alt + G", "Set the macro key (or sequence) for this group");
    //registerSingleHotkey(31, MOD_ALT | MOD_NOREPEAT, 0x48, "Alt + H", "Reload all configs");
    //registerSingleHotkey(20, MOD_ALT | MOD_NOREPEAT, 0x50, "Alt + P", "Show the debug list of the linked windows", true);
    //registerSingleHotkey(21, MOD_ALT | MOD_NOREPEAT, 0xDC, "Alt + \\", "Test", true);

    if (failedHotkeys.size() > 0) {
        std::cout << "\nFailed to register " << failedHotkeys.size() << " hotkey";
        if (failedHotkeys.size() > 1) std::cout << "s";
        std::cout << ":\n";

        for (auto& elem : failedHotkeys) {
            std::cout << "[!] Failed to load " << elem << "\n";
        }

        if (failedHotkeys.size() >= totalHotkeys) { // that value can be not updated in time
            std::cout << "WARNING | Most likely you have started multiple instances of this programm, sadly you can use only one at a time\n";
            _getch();
            exit(0);
        }
    }

    std::cout << std::endl;
}

// Non-character ones will work only with interruptions disabled! (add to readme)
std::map<std::string, int> mapOfKeys = {
    {"0",0x0B},
    {"1",0x2},
    {"2",0x3},
    {"3",0x4},
    {"4",0x5},
    {"5",0x6},
    {"6",0x7},
    {"7",0x8},
    {"8",0x9},
    {"9",0xA},
    {"q",0x10},
    {"w",0x11},
    {"e",0x12},
    {"r",0x13},
    {"t",0x14},
    {"y",0x15},
    {"u",0x16},
    {"i",0x17},
    {"o",0x18},
    {"p",0x19},
    {"[",0x1A},
    {"]",0x1B},
    {"enter",0x1C},
    {"ctrl_left",0x1D},
    {"ctrl",0x1D}, // dublicate by default
    {"a",0x1E},
    {"s",0x1F},
    {"d",0x20},
    {"f",0x21},
    {"g",0x22},
    {"h",0x23},
    {"j",0x24},
    {"k",0x25},
    {"l",0x26},
    {";",0x27},
    {"'",0x28},
    {"`",0x29},
    {"shift_left",0x2A},
    {"shift",0x2A}, // just in case
    {"\\",0x2B},
    {"z",0x2C},
    {"x",0x2D},
    {"c",0x2E},
    {"v",0x2F},
    {"b",0x30},
    {"n",0x31},
    {"m",0x32},
    {",",0x33},
    {".",0x34},
    {"/",0x35},
    {"shift_right",0x36},
    {"alt_left",0x38},
    {"alt",0x38}, // dublicate. For now no combinations btw
    {"space",0x39},
};

std::map<std::string, int> getMapOfKeys() {
    return mapOfKeys;
}

std::map<int, std::string> keyboardHookSpecialVirtualKeyCodeToText = {
    {VK_RETURN, "enter"},
    {VK_SPACE, "space"},
    {VK_CONTROL, "ctrl"},
    {VK_LCONTROL, "ctrl"},
    {VK_RCONTROL, "ctrl_right"},
    {VK_SHIFT, "shift"},
    {VK_LSHIFT, "shift"},
    {VK_RSHIFT, "shift_right"},
    {164, "alt"},
    {165, "alt_right"},
    {219, "["},
    {221, "]"},
    {186, ";"},
    {222, "'"},
    {220, "\\"},
    {192, "`"},
    {188, ","},
    {190, "."},
    {191, "/"},
};

void customSleep(int duration) {
    Sleep(randomizeValue(duration, sleepRandomnessPersent, sleepRandomnessMaxDiff));
}

// false in two cases of three!
bool windowIsLinkedManually(HWND hwnd) {
    return handleToGroup.count(hwnd) > 0;
}

// false in two cases of three!
bool windowIsLinkedAutomatically(HWND hwnd) {
    // handleToGroup contains hwnd only if the window is linked and linked manually
    for (auto& autoGroup : autoGroups) {
        for (HWND& autoWindow : autoGroup.second) {
            // ignoring manually linked window
            if (autoWindow == hwnd) {
                return true;
            }
        }
    }
    return false;
}

WindowGroup* getGroup(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        std::map<HWND, WindowGroup*>::iterator it = handleToGroup.find(hwnd);
        WindowGroup* wg = it->second;
        return wg;
    }
    //else return NULL;
}

void ShowOnlyMainInGroup(WindowGroup* wg) {
    std::vector<WindowInfo> others = (*wg).getOthers();
    std::vector<WindowInfo>::iterator iter;
    for (iter = others.begin(); iter != others.end(); ++iter) {
        //std::cout << *iter << " MINIMIZE" << endl;
        if (!hideNotMainWindows) {
            if (!checkHungWindow(iter->hwnd)) ShowWindow(iter->hwnd, SW_MINIMIZE);
        }
    }
    HWND c = (*wg).getCurrent();
    //std::cout << c << " SW_RESTORE" << endl;
    if (IsIconic(c)) {
        if (!checkHungWindow(c)) ShowWindow(c, SW_RESTORE); // what does it return if just z order far away but not minimized, maybe detect large windows above it
    }
}

void ShowOnlyMainInGroup(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        std::map<HWND, WindowGroup*>::iterator it = handleToGroup.find(hwnd);
        ShowOnlyMainInGroup(it->second);
    }
}

void shiftGroup(WindowGroup* group, int shift) {
    (*group).shiftWindows(shift);
    ShowOnlyMainInGroup(group);
}

void shiftGroup(HWND hwnd, int shift) {
    WindowGroup* wg = nullptr;
    wg = getGroup(hwnd); // delete pointers?
    shiftGroup(wg, shift);
}

std::wstring getWindowName(HWND hwnd) {
    int length = GetWindowTextLength(hwnd);
    wchar_t* buffer = new wchar_t[length + 1];
    GetWindowTextW(hwnd, buffer, length + 1);
    std::wstring ws(buffer);
    return ws; // not nullptr later?
}

void shiftAllGroups(int shift) {
    std::vector<WindowGroup*> used;
    std::map<HWND, WindowGroup*>::iterator it;

    for (it = handleToGroup.begin(); it != handleToGroup.end(); it++) {
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            shiftGroup(it->second, shift);
            used.push_back(it->second); // is * refering to it or it->second?
        }
    }
}

void shiftAllOtherGroups(int shift) {
    std::vector<WindowGroup*> used;
    std::map<HWND, WindowGroup*>::iterator it;

    HWND h = GetForegroundWindow();

    for (it = handleToGroup.begin(); it != handleToGroup.end(); it++) {
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            //std::cout << (h == it->first) << endl;
            //std::wcout << getWindowName(it->first) << endl;
            //std::wcout << getWindowName(h) << endl;
            //std::cout << it->second << endl;
            //std::cout << referenceToGroup[h] << endl << endl;
            //std::cout << (referenceToGroup[h] == it->second) << endl;
            if (handleToGroup[h] != it->second) shiftGroup(it->second, shift);
            used.push_back(it->second); // is * refering to it or it->second?
        }
    }

    SetForegroundWindow(h);

    //std::cout << '\n';
}

void deleteWindow(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        WindowGroup* wg = handleToGroup.find(hwnd)->second;
        (*wg).removeWindow(hwnd);
        handleToGroup.erase(handleToGroup.find(hwnd));
    }
}

void deleteWindowGroup(WindowGroup* wg) {
    /*for (auto& p : referenceToGroup)
        std::cout << p.first << " " << p.second << " " << endl;*/
    std::map<HWND, WindowGroup*>::iterator it;
    for (auto it = handleToGroup.begin(); it != handleToGroup.end(); ) {
        if (wg == it->second) {
            it = handleToGroup.erase(it);
        }
        else {
            ++it; // can have a bug
        }
    }
    //delete(wg); // ?
}

void deleteWindowGroup(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        deleteWindowGroup(handleToGroup.find(hwnd)->second);
    }
}

void SwapVisibilityForAll() {
    std::vector<WindowGroup*> used;
    std::map<HWND, WindowGroup*>::iterator it;

    std::vector<HWND> main;
    std::vector<HWND> other;

    for (it = handleToGroup.begin(); it != handleToGroup.end(); it++) {
        //if (IsHungAppWindow(it->first)) std::cout << "Hang up window 8" << endl;
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            WindowGroup* wg = it->second;
            main.push_back((*wg).getCurrent());
            std::vector<WindowInfo> others = (*wg).getOthers();
            std::vector<WindowInfo>::iterator iter;
            for (iter = others.begin(); iter != others.end(); ++iter) {
                other.push_back(iter->hwnd);
            }

            used.push_back(it->second); // is * refering to it or it->second?
        }
    }

    int minizimed = 0;
    for (auto& it : main) {
        if (IsIconic(it)) {
            minizimed++;
        }
    }

    for (auto& it : main) {
        if (!checkHungWindow(it)) {
            if (minizimed == main.size()) { // all hidden, show
                //if (IsHungAppWindow(it)) std::cout << "Hang up window 4" << endl;
                if (IsIconic(it)) {
                    //if (IsHungAppWindow(it)) std::cout << "Hang up window 5" << endl;
                    ShowWindow(it, SW_RESTORE);
                }
            }
            else {
                //if (IsHungAppWindow(it)) std::cout << "Hang up window 6" << endl;
                ShowWindow(it, SW_MINIMIZE);
            }
        }
    }

    for (auto& it : other) {
        //if (IsHungAppWindow(it)) std::cout << "Hang up window 7" << endl;
        if (!checkHungWindow(it)) ShowWindow(it, SW_MINIMIZE); // later add "hide" and "show" mode, after working window enumeration to get them back
    }
}

void checkClosedWindows() {
    bool br = false;
    for (const auto& it : handleToGroup) {
        if (it.first != NULL) {
            if (!IsWindow(it.first)) {
                if (it.second != nullptr) it.second->removeClosedWindows();
                br = true;
                break;
            }
        }
    }
    if (br) checkClosedWindows();
}

// Фурри это классно

static BOOL CALLBACK enumWindowCallback(HWND hwnd, LPARAM lparam) {
    //int length = GetWindowTextLength(hwnd); // WindowName
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //std::wstring ws(buffer);

    std::wstring windowName = getWindowName(hwnd);
    //std::wstring str(ws.begin(), ws.end()); // wstring to string

    // List visible windows with a non-empty title
    //if (IsWindowVisible(hwnd) && length != 0) {
    //    std::wcout << hwnd << ":  " << ws << endl;
    //}

    //if (ws == rx_name) {
    //    if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_SHOW);
    //}
    //else {
    bool allow = false;
    for (auto& specified : defaultFastForegroundWindows) {
        if (specified.front() == '*' && specified.back() == '*') {
            std::wstring mainPart = specified.substr(1, specified.length() - 2);

            if (specified == windowName || (windowName.find(mainPart) != std::wstring::npos)) {
                allow = true;
                break;
            }
        }
        else if (windowName.find(specified) != std::wstring::npos) {
            allow = true;
            //std::cout << "RBX HERE " << str << endl;
        }
    }

    if (allow && !checkHungWindow(hwnd)) ShowWindow(hwnd, SW_SHOW); //std::cout << GetLastError() << endl;
    //}

    //EnumChildWindows(hwnd, enumWindowCallback, NULL); //TODO ?
    //std::cout << "Good\n";

    return TRUE;
}

void showAllRx() {
    //std::cout << "Enmumerating windows..." << endl;
    hideNotMainWindows = false;
    EnumWindows(enumWindowCallback, NULL);
    //std::cout << "Done\n";
}

void restoreAllConnected() {
    hideNotMainWindows = false;
    for (const auto& it : handleToGroup) {
        if (it.first != NULL && !checkHungWindow(it.first)) {
            ShowWindow(it.first, SW_MINIMIZE); // bad way // though not that much lol
            ShowWindow(it.first, SW_RESTORE);
        }
    }
}

std::atomic<bool>& getStopMacroInput() {
    return stopMacroInput;
}

//std::atomic<bool> interruptedRightNow(false);

//std::atomic<bool>& getInterruptedRightNow() {
//    return interruptedRightNow;
//}

bool waitIfInterrupted() {
    if (interruptionManager == nullptr || !interruptionManager.load()->isModeEnabled().load()) return false;
    int waitFor = interruptionManager.load()->getUntilNextMacroRetryAtomic().load();
    bool interrupted = waitFor > 0;
    if (interrupted) {
        //std::cout << "Waiting...\n";
        //std::unique_lock<std::mutex> macroWaitLock = std::unique_lock<std::mutex>(std::mutex());

        //std::cout << "Actually paused\n" << std::endl;
        //auto lock = std::unique_lock<std::mutex>{ macroWaitMutex, std::defer_lock };
        //macroWaitCv.wait_for(lock, std::chrono::milliseconds(10000), [] { return !interruptedRightNow.load(); }); // (interruptionManager.load()->getUntilNextMacroRetryAtomic().load() <= 0)
        ////std::cout << "Waiting " << interruptionManager.load()->getUntilNextMacroRetryAtomic().load() << '\n';
        ////return (interruptionManager.load()->getUntilNextMacroRetryAtomic().load() <= 0);
        //std::cout << "Actually unpaused\n" << std::endl;

        if (!usePrimitiveInterruptionAlgorythm) {
            if (macroWaitMutex == nullptr) {
                macroWaitMutex = new std::mutex();
            }
            if (macroWaitLock == nullptr) {
                macroWaitLock = new std::unique_lock<std::mutex>{ *macroWaitMutex }; // , std::defer_lock
            }
            //auto lock = std::unique_lock<std::mutex>{ *macroWaitMutex};
            macroWaitCv.wait(*macroWaitLock, [] { return (interruptionManager.load()->getUntilNextMacroRetryAtomic().load() <= 0); });
        }
        else {
            while (interruptionManager.load()->getUntilNextMacroRetryAtomic().load() > 0) {
                Sleep(primitiveWaitInterval);
            }
        }

        //runtimeData.saveCurrentForgroundWindow(); // ?

        //interruptionManager.load()->getConditionVariable().wait(interruptionManager.load()->getLock(), [] { return (interruptionManager.load()->getUntilNextMacroRetryAtomic().load() <= 0); });
        //std::cout << "Done waiting\n";
    }

    return interrupted;
}

void keyPressInput(WORD keyCode)
{
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = keyCode;
    input.ki.dwFlags = KEYEVENTF_SCANCODE;

    SendInput(1, &input, sizeof(INPUT));
}

void keyReleaseInput(WORD keyCode)
{
    INPUT input;
    input.type = INPUT_KEYBOARD;
    input.ki.wScan = keyCode;
    input.ki.dwFlags = KEYEVENTF_SCANCODE | KEYEVENTF_KEYUP;

    SendInput(1, &input, sizeof(INPUT));
}

void keyPressString(std::string key) {
    interruptionManager.load()->addPendingSentInput(key);
    keyPressInput(mapOfKeys[key]);
}

void keyReleaseString(std::string key) {
    keyReleaseInput(mapOfKeys[key]);
}

bool notTargetWindowActive(HWND target) {
    HWND foregr = GetForegroundWindow();
    return foregr != target;
}

bool pressAndUnpressAKey(HWND w, Key k) { // returns true if was paused and needs to repeat the cycle
    if (!k.enabled || k.keyCode == "EXAMPLE") return false;
    if (stopMacroInput.load()) return false;
    if (waitIfInterrupted() || notTargetWindowActive(w)) return true;

    customSleep(k.beforeKeyPress);
    if (stopMacroInput.load()) return false;
    if (waitIfInterrupted() || notTargetWindowActive(w)) return true;
    interruptionManager.load()->addPendingSentInput(k.keyCode); // BEFORE actually pressing! Or the handler will get it before it's added
    keyPressInput(mapOfKeys[k.keyCode]);
    
    customSleep(k.holdFor);
    if (stopMacroInput.load()) return false;
    if (waitIfInterrupted() || notTargetWindowActive(w)) return true;
    keyReleaseInput(mapOfKeys[k.keyCode]);

    customSleep(k.afterKeyPress);
}

bool performASequence(HWND w) {
    bool shouldRestartSequence = false;
    if (groupToSequence.count(handleToGroup[w])) {
        std::vector<Key> keys = groupToSequence[handleToGroup[w]]->getKeys();
        if (keys.size() > 0) {
            for (auto& el : keys) {
                if (stopMacroInput.load()) return false;
                shouldRestartSequence = pressAndUnpressAKey(w, el);
                if (shouldRestartSequence) break;
            }
        }
        else {
            std::cout << "You can't have no actions in a sequence! Waiting a bit instead\n"; // kostil
            customSleep(100);
        }
    }
    else {
        for (auto& el : mainSequence->getKeys()) {
            if (stopMacroInput.load()) return false;
            //std::cout << "Pressing\n";
            shouldRestartSequence = pressAndUnpressAKey(w, el);
            // The macro has been interrupted and paused, and now we need to restart the entire sequence
            if (shouldRestartSequence) break;
        }
    }
    return shouldRestartSequence;
}

WindowInfo* getWindowInfoFromHandle(HWND hwnd) {
    WindowInfo* wi = nullptr;

    WindowGroup* wg = handleToGroup[hwnd];
    if (wg != nullptr) {
        wi = wg->getWindowInfoFromHandle(hwnd);
    }

    return wi;
}

KeySequence* getKeySequenceFromHandle(HWND hwnd) {
    KeySequence* keySeq = nullptr;

    WindowGroup* wg = handleToGroup[hwnd];
    if (wg != nullptr) {
        WindowInfo* wi = wg->getWindowInfoFromHandle(hwnd);
        if (wi != nullptr) {
            keySeq = mainSequence;
            if (groupToSequence.count(wg)) keySeq = groupToSequence[wg];
        }
    }

    return keySeq;
}

bool checkCooldownActiveAndRefresh(HWND hwnd) {
    WindowInfo* wi = getWindowInfoFromHandle(hwnd);
    if (wi == nullptr) return false;
    KeySequence* keySeq = getKeySequenceFromHandle(hwnd);
    if (keySeq == nullptr) return false;

    auto timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wi->lastSequenceInputTimestamp).count();
    bool cooldown = timePassedSince < keySeq->getCooldownPerWindow();
    //std::cout << timePassedSince << " " << keySeq->getCooldownPerWindow() << std::endl;

    //std::cout << hwnd << '\n';
    //if(cooldown) std::cout << timePassedSince << '\n';
    if (!cooldown) wi->refreshTimestamp();
    return cooldown;
}

void focusAndSendSequence(HWND hwnd) { // find this
    //std::cout << "Swapped to " << hwnd << std::endl;

    /*int key = mapOfKeys[defaultMacroKey]; // 0x12
    //if(specialSingleWindowModeKeyCode)
    if (groupToKey.count(referenceToGroup[hwnd])) {
        key = groupToKey[referenceToGroup[hwnd]];
    }*/
    //std::cout << "test\n";
    //std::cout << "key: " << key << endl;

    KeySequence* keySeq = getKeySequenceFromHandle(hwnd);

    bool shouldRestartSequence = true;
    while (shouldRestartSequence) {
        if (stopMacroInput.load()) return;
        waitIfInterrupted();
        if (stopMacroInput.load()) return;
        bool cooldown = checkCooldownActiveAndRefresh(hwnd); // can't call twice, and not two separate funcs to not look for objects again
        //std::cout << cooldown << std::endl;
        if (cooldown) {
            if (keySeq != nullptr) {
                customSleep(keySeq->getCheckEveryOnWait());
            }
            else {
                customSleep(max(10, min(100, macroDelayAfterFocus)));
            }

            // behaviour for staying at that single window till it's out of cooldown
            //continue;
            
            if(!runtimeData.hadCooldownOnPrevWindow.load()) runtimeData.activatePrevActiveWindow();
            else runtimeData.hadCooldownOnPrevWindow.store(cooldown);
            // original intended behaviour
            return;
        }
        runtimeData.hadCooldownOnPrevWindow.store(cooldown);

        if (stopMacroInput.load()) return;
        waitIfInterrupted();
        if (stopMacroInput.load()) return;

        HWND curForegr = GetForegroundWindow();
        if (!windowIsLinkedManually(curForegr) && !windowIsLinkedAutomatically(curForegr)) {
            runtimeData.saveCurrentForgroundWindow();
        }
        SetForegroundWindow(hwnd);
        customSleep(macroDelayBetweenSwitchingAndFocus);
        SetFocus(hwnd);

        customSleep(macroDelayAfterFocus);

        shouldRestartSequence = performASequence(hwnd);
    }
}

void performInputsEverywhere() {
    for (auto& it : handleToGroup) {
        if (stopMacroInput.load()) return;
        focusAndSendSequence(it.first);
        customSleep(macroDelayBeforeSwitching);
    }
}

void startUsualMacroLoop() {
    customSleep(macroDelayInitial);
    while (!stopMacroInput.load()) {
        //for (int i = 0x5; i <= 0x30; i++) { // 5A
            //if (stopInput) return;
            //std::cout << std::hex << key << endl;
            //std::cout << std::dec;
            //key = i;
        performInputsEverywhere();
        customSleep(delayWhenDoneAllWindows);
        // 8000
        /*for (int i = 0; i < 100; i++) {
            //customSleep(160);
            customSleep(50);
            if (stopInput) return;
        }*/
    }
}

void startUsualSequnceMode() {
    currentMacroLoopThread = new std::thread(startUsualMacroLoop);
    interruptionManager.load()->getUntilNextMacroRetryAtomic().store(0);
}

void performSingleWindowedHold() {
    HWND w = GetForegroundWindow();
    SetForegroundWindow(w);
    customSleep(macroDelayBetweenSwitchingAndFocus);
    SetFocus(w);
    customSleep(macroDelayAfterFocus);
    std::string key = specialSingleWindowModeKeyCode;
    if (groupToSequence.count(handleToGroup[w])) {
        std::vector<Key> allKeys = groupToSequence[handleToGroup[w]]->getKeys();
        if (allKeys.size() > 0) key = allKeys[0].keyCode;
    }
    keyPressString(key);
    //performASequence(w);
}

void releaseConfiguredKey() {
    HWND w = GetForegroundWindow();
    SetForegroundWindow(w);
    customSleep(macroDelayBetweenSwitchingAndFocus);
    SetFocus(w);
    customSleep(macroDelayAfterFocus);
    std::string key = specialSingleWindowModeKeyCode;
    if (groupToSequence.count(handleToGroup[w])) {
        std::vector<Key> allKeys = groupToSequence[handleToGroup[w]]->getKeys();
        if (allKeys.size() > 0) key = allKeys[0].keyCode;
    }
    keyReleaseString(key);
}

void toggleMacroState() {
    if (handleToGroup.size() == 0) {
        std::cout << "You haven't linked any windows yet!\n";
    }
    else if (stopMacroInput.load()) {
        stopMacroInput.store(false);
        std::cout << "Starting...\n";
        if (specialSingleWindowModeEnabled && handleToGroup.size() == 1) {
            performSingleWindowedHold();
        }
        else {
            startUsualSequnceMode();
        }
    }
    else {
        stopMacroInput.store(true);
        if (handleToGroup.size() == 1) releaseConfiguredKey();
        std::cout << "Stopped\n";
    }
}

void addWindowToGlobalGroup(HWND hwnd) {
    if (checkHungWindow(hwnd)) return;
    bool openCmdLater = false;
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE); // didn't put MINIMIZE before and without if statement bc this entire piece of code is more like a workaround than a feature
        if (!IsIconic(GetConsoleWindow())) {
            openCmdLater = true;
        }
    }
    autoGroups[globalGroupId].push_back(hwnd);
}

struct MonitorData {
    HMONITOR hMonitor;
    RECT rect;
    int monitorNum;
};

BOOL CALLBACK MonitorEnumProc(HMONITOR hMonitor, HDC, LPRECT, LPARAM lParam) {
    MONITORINFO info = { sizeof(MONITORINFO) };
    GetMonitorInfo(hMonitor, &info);

    std::vector<MonitorData>* monitors = reinterpret_cast<std::vector<MonitorData>*>(lParam);
    monitors->push_back({ hMonitor, info.rcMonitor, static_cast<int>(monitors->size()) });

    return TRUE;
}

int GetMonitorNumber(HMONITOR hMonitor, const std::vector<MonitorData>& monitors) {
    auto it = std::find_if(monitors.begin(), monitors.end(), [hMonitor](const MonitorData& data) {
        return hMonitor == data.hMonitor;
        });

    return it != monitors.end() ? it->monitorNum : -1;
}

POINT getMaxGridDimensionsForScreen(MonitorData& monitorData) {
    RECT monitorRect = monitorData.rect;

    POINT p;
    p.x = (monitorRect.right - monitorRect.left) / settings.load()->minimalGameWindowSizeX;
    p.y = (monitorRect.bottom - monitorRect.top) / settings.load()->minimalGameWindowSizeY;

    //std::cout << "Unclamped x y: " << p.x << ' ' << p.y << '\n';
    //std::cout << "Min max x: " << settings.load()->minCellsGridSizeX << ' ' << settings.load()->maxCellsGridSizeX << '\n';
    //std::cout << "Min max y: " << settings.load()->minCellsGridSizeY << ' ' << settings.load()->maxCellsGridSizeY << '\n';

    p.x = min(settings.load()->maxCellsGridSizeX, max(settings.load()->minCellsGridSizeX, p.x));
    p.y = min(settings.load()->maxCellsGridSizeY, max(settings.load()->minCellsGridSizeY, p.y));
    
    //std::cout << "Clamped x y: " << p.x << ' ' << p.y << '\n';

    //std::cout << "X: " << (monitorRect.right - monitorRect.left) << " / " << settings.load()->minimalGameWindowSizeX << " = " << p.x << '\n';
    //std::cout << "Y: " << (monitorRect.bottom - monitorRect.top) << " / " << settings.load()->minimalGameWindowSizeY << " = " << p.y << '\n';
    
    return p;
}

int GetWindowCell(HWND hwnd) {
    RECT windowRect;
    GetWindowRect(hwnd, &windowRect);

    // Get information for all monitors
    std::vector<MonitorData> monitors;
    EnumDisplayMonitors(NULL, NULL, MonitorEnumProc, reinterpret_cast<LPARAM>(&monitors));

    int cell = 0;
    int i = -1;

    //std::cout << monitors[1].rect.left << " " << monitors[1].rect.right << '\n' << monitors[1].rect.top << " " << monitors[1].rect.bottom << '\n';
    //monitors[1].rect.right = 1920 + 2560;
    //monitors[1].rect.bottom = 1440;

    //std::cout << monitors.size() << " monitors" << std::endl;
    for (MonitorData& monitorData : monitors) {
        i++;

        POINT gridDimensions = getMaxGridDimensionsForScreen(monitorData);
        //std::cout << "Grid: " << gridDimensions.x << " " << gridDimensions.y << std::endl;

        RECT monitorRect = monitorData.rect;

        int cellWidth = (monitorRect.right - monitorRect.left) / gridDimensions.x;
        int cellHeight = (monitorRect.bottom - monitorRect.top) / gridDimensions.y;

        POINT windowCenter;
        windowCenter.x = (windowRect.left + windowRect.right) / 2; //  - monitorRect.left
        windowCenter.y = (windowRect.top + windowRect.bottom) / 2; //  - monitorRect.top

        //std::cout << "monitorRect: X " << monitorRect.left << " to " << monitorRect.right << ", Y " << monitorRect.top << " to " << monitorRect.bottom << std::endl;
        //std::cout << "windowCenter: " << windowCenter.x << " " << windowCenter.y << std::endl;

        bool windowInThisMonitor = PtInRect(&monitorRect, windowCenter);
        //bool windowInThisMonitor = monitorRect.
        bool checkingLastMonitor = i + 1 >= monitors.size();

        if (windowInThisMonitor || checkingLastMonitor) {
            if (!windowInThisMonitor && checkingLastMonitor) {
                // fully to the right, weird case, can also not do the thing below
                cell += 200;
            }
            int cellRow = (windowCenter.y - monitorRect.top) / cellHeight;
            int cellCol = (windowCenter.x - monitorRect.left) / cellWidth;
            //std::cout << "Extra: " << -monitorRect.left << " " << -monitorRect.left << std::endl;
            //std::cout << "Row: " << windowCenter.y << " / " << cellHeight << " = " << (windowCenter.y) / cellHeight << std::endl;
            //std::cout << "Col: " << windowCenter.x << " / " << cellWidth << " = " << (windowCenter.x) / cellWidth << std::endl;
            cell += cellRow * gridDimensions.x + cellCol;
            //std::cout << "Row Col " << cellRow << " " << cellCol << std::endl;
            break;
        }
        else {
            cell += gridDimensions.x * gridDimensions.y;
        }
    }

    return cell;
}

void distributeRxHwndsToGroups(HWND hwnd) {
    if (checkHungWindow(hwnd)) return;
    if (settings.load()->ignoreManuallyLinkedWindows && windowIsLinkedManually(hwnd)) return;

    RECT r = { NULL };
    bool openCmdLater = false;
    if (IsIconic(hwnd)) {
        //if (IsHungAppWindow(hwnd)) std::cout << "Hang up window 3'" << endl;
        ShowWindow(hwnd, SW_RESTORE); // didn't put MINIMIZE before and without if statement bc this entire piece of code is more like a workaround than a feature
        //if (IsHungAppWindow(hwnd)) std::cout << "Hang up window 4'" << endl;
        if (!IsIconic(GetConsoleWindow())) {
            //if (IsHungAppWindow(hwnd)) std::cout << "Hang up window 5'" << endl;
            openCmdLater = true;
        }
    }
    customSleep(1);
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(RECT));
    // GetWindowRect(hwnd, &r)
    //std::cout << r.left << " " << r.top << " " << r.right << " " << r.bottom << " " << endl;

    RECT desktop;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &desktop, 0);
    int desktopX = desktop.right;
    int desktopY = desktop.bottom;

    //if (IsHungAppWindow(hwnd)) std::cout << "Hang up window 3" << endl;
    //if (r.left == -7 && r.top == 0 && r.right == 967 && r.bottom == 527) autoGroup1.push_back(hwnd); // Explorer window
    //std::cout << r.right - r.left << "   " << r.bottom - r.top << endl;
    
    //std::cout << "Getting cell...\n";
    int cell = GetWindowCell(hwnd);
    //std::cout << cell << '\n';
    autoGroups[cell].push_back(hwnd);
    //std::cout << "Added\n";

    //if (r.right - r.left == desktopX / 2 && (r.bottom - r.top == 638 || r.bottom - r.top == 631)) { // 631 is considered as height when at the top, check on different Windows versions and
    //    if (r.left == 0 && r.top == 0) autoGroup1.push_back(hwnd); // actual sizes         // find out which one doesn't work and change to + or - border size //TODO
    //    else if (r.left == desktopX / 2 && r.top == 0) autoGroup2.push_back(hwnd);
    //    else if (r.left == 0 && r.top == desktopY - 638) autoGroup3.push_back(hwnd);
    //    else if (r.left == desktopX / 2 && r.top == desktopY - 638) autoGroup4.push_back(hwnd);
    //}

    if (openCmdLater) {
        /*std::cout << "restoring\n";
        customSleep(1);
        if(IsIconic(GetConsoleWindow())) ShowWindow(GetConsoleWindow(), SW_RESTORE);*/
    }
}

static BOOL CALLBACK rxConnectivityCallback(HWND hwnd, LPARAM lparam) {
    //int length = GetWindowTextLength(hwnd); // WindowName
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //std::wstring ws(buffer);
    //std::string str(ws.begin(), ws.end());

    //if(IsWindowVisible(hwnd))
    // 
    //std::wcout << "\"" << ws << "\"" << endl;
    //std::cout << str << endl;

    std::wstring ws = getWindowName(hwnd);

    if (ws == rx_name) { // ws == L"HangApp"
        distributeRxHwndsToGroups(hwnd);
        //std::cout << "Found!\n";
    }

    //EnumChildWindows(hwnd, enumWindowCallback, NULL); //TODO ?
    return TRUE;
}

static BOOL CALLBACK rxConnectivityCompletelyAllCallback(HWND hwnd, LPARAM lparam) {
    //int length = GetWindowTextLength(hwnd); // Window Name
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //std::wstring ws(buffer);
    std::wstring ws = getWindowName(hwnd);

    //std::string str(ws.begin(), ws.end());

    if (ws == rx_name) {
        addWindowToGlobalGroup(hwnd);
    }

    return TRUE;
}

int createSingleConnectedGroup(std::vector<HWND>* windows) {
    if (windows->size() == 0) return 0;
    WindowGroup* wg;
    wg = new WindowGroup();
    int amount = 0;
    for (auto& w : *windows) {
        if (handleToGroup.count(w)) {
            std::map<HWND, WindowGroup*>::iterator it = handleToGroup.find(w);
            handleToGroup.find(w)->second->removeWindow(w);
            //referenceToGroup.erase(referenceToGroup.find(w));
        }
        wg->addWindow(w);
        handleToGroup[w] = wg;
        amount++;
    }
    return amount;
}

int createConnectedCellGroups() {
    int amount = 0;
    for (auto it = autoGroups.begin(); it != autoGroups.end(); ++it) {
        if (it->first != globalGroupId) {
            amount += createSingleConnectedGroup(&(it->second));
        }
    }
    return amount;
}

int createConnectedGroupsForCompletelyAll() {
    int amount = 0;
    amount += createSingleConnectedGroup(&(autoGroups[globalGroupId]));
    return amount;
}

void connectAllQuarters() {
    std::cout << "Connecting RBX windows...\n";
    
    // clearing all except the global one
    for (auto it = autoGroups.begin(); it != autoGroups.end();) {
        if (it->first != globalGroupId) {
            it->second.clear();
            it = autoGroups.erase(it);
        }
        else {
            ++it;
        }
    }

    currentHangWindows = 0;
    EnumWindows(rxConnectivityCallback, NULL);
    if (currentHangWindows > 0) std::cout << "Found " << currentHangWindows << " window(s) that isn't (aren't) responding, skipping them\n";
    int amount = createConnectedCellGroups();
    if (amount > 0) {
        std::cout << "Successfully connected " << amount << " window";
        if (amount > 1) std::cout << "s";
        std::cout << '\n';
    }
    else {
        std::cout << "Didn't find any matching windows!\n";
    }
}

void connectAllRbxsNoMatterWhat() {
    std::cout << "Connecting Absolutely all RBX windows...\n";
    autoGroups[globalGroupId].clear();
    currentHangWindows = 0;
    EnumWindows(rxConnectivityCompletelyAllCallback, NULL);
    if (currentHangWindows > 0) std::cout << "Found " << currentHangWindows << " window(s) that isn't (aren't) responding, skipping them\n";
    int amount = createConnectedGroupsForCompletelyAll();
    if (amount > 0) {
        std::cout << "Successfully connected " << amount << " RBX window";
        if (amount > 1) std::cout << "s";
        std::cout << std::endl;
    }
    else {
        std::cout << "Didn't find any RBX windows!" << std::endl;
    }
}

void performShowOrHideAllNotMain() {
    hideNotMainWindows = !hideNotMainWindows;
    for (const auto& it : handleToGroup) {
        if (it.second != NULL) {
            it.second->hideOrShowOthers();
        }
    }
}

void windowPosTest(HWND hwnd, int type) {
    RECT p;
    GetWindowRect(hwnd, &p);
    //RECT p = WINDOWPLACEMENT;
    std::cout << p.left << " " << p.top << '\n';
    std::cout << p.right << " " << p.bottom << '\n';
    // -7, 0, 967, 638     // 953, 0, 1927, 638     // -7, 402, 967, 1047     // 953, 402, 1927, 1047
    //if (type == 1) { SetWindowPos(hwnd, NULL, -7, 0, 974, 638, NULL); }
    //else if (type == 2) { SetWindowPos(hwnd, NULL, 953, 0, 974, 638, NULL); } // 1390
    //else if (type == 3) { SetWindowPos(hwnd, NULL, -7, 402, 974, 645, NULL); }
    //else if (type == 4) { SetWindowPos(hwnd, NULL, 953, 402, 974, 645, NULL); }

    if (type == 1) { SetWindowPos(hwnd, NULL, -7, 0, 974, 638, NULL); }
    else if (type == 2) { SetWindowPos(hwnd, NULL, 953, 0, 974, 638, NULL); }
    else if (type == 3) { SetWindowPos(hwnd, NULL, -7, 306, 974, 645, NULL); }
    else if (type == 4) { SetWindowPos(hwnd, NULL, 953, 306, 974, 645, NULL); } // 952 304 1925 949

    RECT p2;
    GetWindowRect(hwnd, &p2);
    //RECT p = WINDOWPLACEMENT;
    std::cout << p2.left << " " << p2.top << '\n';
    std::cout << p2.right << " " << p2.bottom << '\n';

    std::cout << '\n';
    //SetWindowPlacement(hwnd,
}

void hideForgr() {
    HWND h = GetForegroundWindow();
    //std::wstring name = getWindowName(h);
    //std::wcout << h << endl;
    //std::cout << (h == FindWindow(L"Shell_TrayWnd", NULL)) << endl;
    //std::cout << (h == FindWindow(L"ToolbarWindow32", L"Running Applications")) << endl;
    //std::cout << (h == FindWindow(L"SysTabControl32", NULL)) << endl;
    //std::wcout << getWindowName(GetParent(h)) << endl;
    //std::cout << (curHwnd == GetDesktopWindow());

    // taskbar, other desktop components get back on their own
    if (h != FindWindow("Shell_TrayWnd", NULL) && !checkHungWindow(h)) {
        bool allow = false;
        if (settings.load()->allowAnyToBackgroundWindows) {
            allow = true;
        }
        else {
            std::wstring ws = getWindowName(h);

            for (const std::wstring& specified : settings.load()->allowToBackgroundWindows) {
                if (specified.front() == '*' && specified.back() == '*') {
                    std::wstring mainPart = specified.substr(1, specified.length() - 2);

                    //std::wcout << ws << " and " << mainPart << "\n";
                    if (specified == ws || (ws.find(mainPart) != std::wstring::npos)) {
                        allow = true;
                        break;
                    }
                }
                else {
                    for (const std::wstring& specified : settings.load()->allowToBackgroundWindows) {
                        if (specified == ws) {
                            allow = true;
                            break;
                        }
                    }
                }
            }
        }

        if (allow) ShowWindow(h, SW_HIDE); 
    }
    //get back from std::vector somehow, maybe input
}

static BOOL CALLBACK fgCustonWindowCallback(HWND hwnd, LPARAM lparam) {
    if (customReturnToFgName == L"") return TRUE;

    //int length = GetWindowTextLength(hwnd); // WindowName
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //std::wstring ws(buffer);

    std::wstring ws = getWindowName(hwnd);

    if (ws == customReturnToFgName || (ws.find(customReturnToFgName) != std::wstring::npos)) {
        if (!checkHungWindow(hwnd)) {
            if (!ShowWindow(hwnd, SW_SHOW)) std::cout << GetLastError() << '\n';
            if (!IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE); // std::cout << GetLastError() << endl;
            }
            restoredCustomWindowsAmount++;
        }
        //return FALSE; // IDK HOW TO MAKE IT EASY TO SPECIFY if one window or all
    }

    //EnumChildWindows(hwnd, fgCustonWindowCallback, NULL);

    return TRUE;
}

void getFromBackgroundSpecific() {
    HWND itself = GetConsoleWindow();
    if (!checkHungWindow(itself)) { // just in case lol
        ShowWindow(itself, SW_SHOW);
        //if (IsIconic(itself)) {
        //    ShowWindow(itself, SW_MINIMIZE);
        //}
        ShowWindow(itself, SW_RESTORE);
    }
    SetForegroundWindow(itself);

    SetForegroundWindow(GetConsoleWindow());
    restoredCustomWindowsAmount = 0;
    std::wstring targetName = L"";
    std::wstring checkEmpty = L"";
    std::wcout << "Enter the text (Eng only):\n";
    std::getline(std::wcin, targetName);
    //std::wcout << "Entered: '" << targetName << "'" << endl;

    checkEmpty = targetName;

    checkEmpty.erase(std::remove(checkEmpty.begin(), checkEmpty.end(), ' '), checkEmpty.end());

    //std::wcout << "\"" << checkEmpty << "\"\n";
    if (checkEmpty != L"") {
        customReturnToFgName = targetName;
    }
    //else if (customFgName != L"") std::cout << "Using the previous window name\n";
    //else {
    //    std::cout << "The input data is incorrect" << endl;
    //}
    EnumWindows(fgCustonWindowCallback, NULL);
    if (restoredCustomWindowsAmount == 1) std::cout << "Success\n";
    else std::cout << "Couldn't find such window!\n";
}

void setGroupKey(HWND h) {
    if (handleToGroup.count(h)) {
        SetForegroundWindow(GetConsoleWindow());
        std::cout << "Enter the key (Eng only, lowercase, no combinations) or the extra sequence name with \"!\" in the beggining:\n";
        std::string keyName = "";
        std::getline(std::cin, keyName);
        if (keyName.rfind("!", 0) == 0) { //  && groupToKey.count(referenceToGroup[h]) // why was this here?
            groupToSequence[handleToGroup[h]] = knownOtherSequences[keyName.substr(1)];
            std::cout << "Have set the sequence \"" << keyName.substr(1) << "\" for that window's group" << std::endl;
        }
        else if (mapOfKeys.count(keyName)) {
            groupToSequence[handleToGroup[h]] = &KeySequence(keyName); // then use as mapOfKeys[keyName] // important comment
            std::cout << "Have set the key for that group" << std::endl;
            SetForegroundWindow(h);
        }
        else {
            std::cout << "Such key (or sequence name) doesn't exist or isn't supported yet!" << std::endl;
        }
    }
    else {
        std::cout << "No window group is focused!" << std::endl;
    }
}

YAML::Node getDefaultSequenceList(bool withExample) {
    YAML::Node list = YAML::Node(YAML::NodeType::Sequence);

    YAML::Node firstKey = YAML::Node();
    setConfigValue(firstKey, "keyCode", defaultMacroKey);
    setConfigValue(firstKey, "enabled", true);
    setConfigValue(firstKey, "beforeKeyPress", 0);
    setConfigValue(firstKey, "holdFor", 2400);
    setConfigValue(firstKey, "afterKeyPress", 10);

    list.push_back(firstKey);

    if (withExample) {
        //YAML::Node keyExample = YAML::Clone(firstKey); // cool thing
        YAML::Node keyExample = YAML::Node();
        setConfigValue(keyExample, "keyCode", "EXAMPLE");
        setConfigValue(keyExample, "enabled", false);

        list.push_back(keyExample);
    }

    YAML::Node node = YAML::Node();
    node["instructions"] = list;
    return node;
}

YAML::Node getDefaultExtraKeySequences() {
    YAML::Node node = YAML::Node(std::map<std::string, YAML::Node>());
    
    YAML::Node example = YAML::Node();
    example["instructions"] = std::vector<YAML::Node>();
    
    node["example"] = example;
    return node;
}

void addNewKeysToSequence(YAML::Node& config, std::string key, YAML::Node extraKeys) {
    YAML::Node resultInstructions = YAML::Node(YAML::NodeType::Sequence);
    YAML::Node oldInstructions = getConfigValue(getConfigValue(config, key), "instructions");
    YAML::Node newInstructions = getConfigValue(extraKeys, "instructions");
    //std::cout << "Old instr " << oldInstructions << oldInstructions.IsDefined() << '\n';
    //std::cout << "New instr " << newInstructions << '\n';
    if (oldInstructions.IsSequence()) {
        for (YAML::Node instr : oldInstructions) {
            resultInstructions.push_back(instr);
        }
    }
    if (newInstructions.IsSequence()) {
        for (YAML::Node instr : newInstructions) {
            resultInstructions.push_back(instr);
        }
    }
    setConfigValue(config, key + "/instructions", resultInstructions);
}

void resetMainSequence(const YAML::Node &config) {
    if (mainSequence != nullptr) {
        delete mainSequence;
        mainSequence = nullptr;
    }
}

void readNewMainSequence(const YAML::Node& config) {
    resetMainSequence(config);
    mainSequence = new KeySequence(getConfigValue(config, "settings/macro/mainKeySequence"));
}

void readDefaultMainSequence(const YAML::Node& config) {
    resetMainSequence(config);
    mainSequence = new KeySequence(getDefaultSequenceList(true));
}

void resetInterruptionManager(const YAML::Node& config) {
    if (interruptionManager != nullptr) {
        //interruptionManager.load().get
        delete interruptionManager;
        interruptionManager = nullptr;
    }
    //std::cout << "resetInterruptionManager\n";
}

void resetSettings(const YAML::Node& config) {
    if (settings != nullptr) {
        delete settings;
        settings = nullptr;
    }
}

void readNewSettings(const YAML::Node& config) {
    resetSettings(config);
    settings = new Settings(config);
}

void rememberInitialPermanentSettings() {
    oldConfigValue_startAnything = interruptionManager.load()->getShouldStartAnything().load();
    oldConfigValue_startKeyboardHook = interruptionManager.load()->getShouldStartKeyboardHook().load();
    oldConfigValue_startMouseHook = interruptionManager.load()->getShouldStartMouseHook().load();
}

void readSimpleInteruptionManagerSettings(const YAML::Node& config) {
    interruptionManager.load()->setMacroPauseAfterKeyboardInput(getConfigInt(config, "settings/macro/interruptions/keyboard/startWithDelay_seconds", 8));
    interruptionManager.load()->setMacroPauseAfterMouseInput(getConfigInt(config, "settings/macro/interruptions/mouse/startWithDelay_seconds", 3));
    interruptionManager.load()->setInputsListCapacity(getConfigInt(config, "settings/macro/interruptions/rememberMaximumInputsEach", 40));
    interruptionManager.load()->setInputSeparationWaitingTimeout(getConfigInt(config, "settings/macro/interruptions/macroInputArrivalTimeoutToSeparateFromUserInputs", 1000));
    interruptionManager.load()->setInformOnEvents(getConfigBool(config, "settings/macro/interruptions/informOnChangingDelay", true));
    interruptionManager.load()->setModeEnabled(getConfigBool(config, "settings/macro/interruptions/interruptionsWorkingRightNow", false));
    
    // hooks
    interruptionManager.load()->setShouldStartAnything(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/enableTheseOptions", true));
    interruptionManager.load()->setShouldStartKeyboardHook(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/keyboardListener", true));
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartKeyboardHook(false);
    interruptionManager.load()->setShouldStartMouseHook(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/mouseListener", true));
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartMouseHook(false);
    interruptionManager.load()->setShouldStartDelayModificationLoop(interruptionManager.load()->getShouldStartKeyboardHook().load() || interruptionManager.load()->getShouldStartMouseHook().load());
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartDelayModificationLoop(false);

    if (!initialConfigLoading && (
        oldConfigValue_startKeyboardHook != interruptionManager.load()->getShouldStartKeyboardHook().load() ||
        oldConfigValue_startMouseHook != interruptionManager.load()->getShouldStartMouseHook().load())) {
        addConfigLoadingMessage("WARNING | Some config values can't be applied in runtime, you need to restart the application for that");
    }
}

void readNewInterruptionManager(const YAML::Node& config) {
    resetInterruptionManager(config);
    interruptionManager = new InputsInterruptionManager();
    //std::cout << "readNewInterruptionManager\n";

    interruptionManager.load()->initType(getConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases"), KEYBOARD);
    interruptionManager.load()->initType(getConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases"), MOUSE);
    interruptionManager.load()->initType(getConfigValue(config, "settings/macro/interruptions/anyInput/manyInputsCases"), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);

    //std::cout << interruptionManager << '\n';
    //if (macroDelayWatcherThread != nullptr) {
        //macroDelayWatcherThread->
    //}
    //std::thread(testWait).detach();
}

void readDefaultInterruptionManager(const YAML::Node& config) {
    resetInterruptionManager(config);
    interruptionManager = new InputsInterruptionManager();
    //std::cout << "readDefaultInterruptionManager\n";

    interruptionManager.load()->initType(*InputsInterruptionManager::getDefaultInterruptionConfigsList(KEYBOARD), KEYBOARD);
    interruptionManager.load()->initType(*InputsInterruptionManager::getDefaultInterruptionConfigsList(MOUSE), MOUSE);
    interruptionManager.load()->initType(*InputsInterruptionManager::getDefaultInterruptionConfigsList(ANY_INPUT), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);
}

void updateConfig2_0_TO_2_1(YAML::Node &config, bool wrongConfig, bool firstUpdate) {
    if (firstUpdate) addConfigLoadingMessage("INFO | The config was updated from version 2.0 to 2.1");

    setConfigValue(config, "settings/macro/general/initialDelayBeforeFirstIteration", getConfigInt(config, "settings/macro/delaysInMilliseconds/initialDelayBeforeFirstIteration", 100));
    setConfigValue(config, "settings/macro/general/delayBeforeSwitchingWindow", 0);
    setConfigValue(config, "settings/macro/general/delayAfterSwitchingWindow", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterSettingFocus", 200));
    setConfigValue(config, "settings/macro/general/settingsChangeOnlyWhenReallyNeeded/afterSettingForegroundButBeforeSettingFocus", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterSwitchingToWindow", 10));
    setConfigValue(config, "settings/macro/general/specialSingleWindowMode/enabled", getConfigBool(config, "settings/macro/justHoldWhenSingleWindow", false));
    setConfigValue(config, "settings/macro/general/specialSingleWindowMode/keyCode", getConfigString(config, "settings/macro/defaultKey", defaultMacroKey));

    YAML::Node firstKey = YAML::Node();
    setConfigValue(firstKey, "keyCode", getConfigString(config, "settings/macro/defaultKey", defaultMacroKey));
    setConfigValue(firstKey, "enabled", true);
    setConfigValue(firstKey, "beforeKeyPress", 0);
    setConfigValue(firstKey, "holdFor", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterKeyPress", 2400));
    setConfigValue(firstKey, "afterKeyPress", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterKeyRelease", 10));

    YAML::Node keyExample = YAML::Clone(firstKey);
    setConfigValue(keyExample, "keyCode", "EXAMPLE");
    setConfigValue(keyExample, "enabled", false);

    YAML::Node list = YAML::Node(YAML::NodeType::Sequence);
    list.push_back(firstKey);
    list.push_back(keyExample);

    setConfigValue(config, "settings/macro/mainKeySequence", getDefaultSequenceList(true));
    if(!checkExists(config, "settings/macro/extraKeySequences")) setConfigValue(config, "settings/macro/extraKeySequences", std::vector<YAML::Node>());
    removeConfigValue(config, "settings/macro/delaysInMilliseconds");
    removeConfigValue(config, "settings/macro/justHoldWhenSingleWindow");
    removeConfigValue(config, "settings/macro/defaultKey", true);
}

YAML::Node* updateKeySequence2_2_TO_2_3(YAML::Node& sequence) {
    YAML::Node* node = new YAML::Node();
    std::cout << "sequence: " << sequence.IsSequence() << std::endl;
    if (sequence.IsSequence()) {
        setConfigValue(*node, "instructions", sequence.as<std::vector<YAML::Node>>());
    }
    return node;
}

void updateConfig2_2_TO_2_3(YAML::Node& config, bool wrongConfig, bool firstUpdate) {
    if (firstUpdate) addConfigLoadingMessage("INFO | The config was updated from version 2.2 to 2.3");

    YAML::Node* newMainSeq = updateKeySequence2_2_TO_2_3(getConfigValue(config, "settings/macro/mainKeySequence"));
    setConfigValue(config, "settings/macro/mainKeySequence", *newMainSeq);
    delete newMainSeq;

    if (checkExists(config, "settings/macro/extraKeySequences")) {
        YAML::Node extraSequences = getConfigValue(config, "settings/macro/extraKeySequences");
        std::map<std::string, YAML::Node> newValues = std::map<std::string, YAML::Node>();
        bool replaceValues = false;
        if (extraSequences.IsSequence() && extraSequences.as<std::vector<YAML::Node>>().size() > 0) {
            int i = 0;
            for (auto& seq : extraSequences) {
                if (seq.IsMap()) {
                    for (auto& keyValue : seq) {
                        std::string name = keyValue.first.as<std::string>();
                        YAML::Node value = keyValue.second;
                        std::cout << "name " << name << " value " << value << std::endl;
                        YAML::Node* newNode = updateKeySequence2_2_TO_2_3(value);
                        newValues[name] = *newNode;
                        delete newNode;
                    }
                }
                //newValues["unnamed_" + std::to_string(i)] = *updateKeySequence2_2_TO_2_3(seq);
            }
            replaceValues = true;
        }

        if (replaceValues) {
            setConfigValue(config, "settings/macro/extraKeySequences", YAML::Node(newValues));
            //std::cout << "SetNewExtraValues\n";
        }
    }

    setConfigValue(config, "settings/windowOperations/windowVisualState/showingBackFromBackground/alwaysShowSpecificWindows", getConfigVectorWstring(config, "settings/fastReturnToForegroundWindows", getDefaultShowBackFromBackgroundList()));
    removeConfigValue(config, "settings/fastReturnToForegroundWindows");
}

enum ConfigType {
    MAIN_CONFIG,
    KEYBINDS_CONFIG
};

std::string configTypeToString(ConfigType type) {
    switch (type)
    {
    case MAIN_CONFIG:
        return "Settings";
    case KEYBINDS_CONFIG:
        return "Keybindings";
    default:
        return "Unknown config type";
    }
}

YAML::Node& loadSettingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig) {
    if (!wasEmpty) {
        std::string oldConfigVersion;
        YAML::Node versionData;
        try {
            versionData = getConfigValue(config, "internal/configVersion");
            oldConfigVersion = versionData.as<std::string>();
        }
        catch (const YAML::Exception& e) {
            oldConfigVersion = "2.0";
        }

        bool firstUpdate = true;
        if (oldConfigVersion == "2.0") {
            updateConfig2_0_TO_2_1(config, wrongConfig, firstUpdate);
            firstUpdate = false;
            oldConfigVersion = "2.1";
        }
        if (oldConfigVersion == "2.1") {
            // no refactoring
            oldConfigVersion = "2.2";
        }
        if (oldConfigVersion == "2.2") {
            updateConfig2_2_TO_2_3(config, wrongConfig, firstUpdate);
            firstUpdate = false;
            oldConfigVersion = "2.3";
        }
        //if (oldConfigVersion != getCurrentVersion) {
        //}

    }

    macroDelayInitial = getConfigInt(config, "settings/macro/general/initialDelayBeforeFirstIteration", 100);
    macroDelayBeforeSwitching = getConfigInt(config, "settings/macro/general/delayBeforeSwitchingWindow", 25);
    macroDelayAfterFocus = getConfigInt(config, "settings/macro/general/delayAfterSwitchingWindow", 200);
    delayWhenDoneAllWindows = getConfigInt(config, "settings/macro/general/delayWhenDoneAllWindows", 100);
    macroDelayBetweenSwitchingAndFocus = getConfigInt(config, "settings/macro/general/settingsChangeOnlyWhenReallyNeeded/afterSettingForegroundButBeforeSettingFocus", 10);
    specialSingleWindowModeEnabled = getConfigBool(config, "settings/macro/general/specialSingleWindowMode/enabled", false);
    specialSingleWindowModeKeyCode = getConfigString(config, "settings/macro/general/specialSingleWindowMode/keyCode", defaultMacroKey);

    sleepRandomnessPersent = getConfigInt(config, "settings/macro/general/randomness/delays/delayOffsetPersentage", 10);
    sleepRandomnessMaxDiff = getConfigInt(config, "settings/macro/general/randomness/delays/delayOffsetLimit", 40);

    // if no yaml structure errors
    if (!wrongConfig) {
        // resetting main sequence and reading what config has
        readNewMainSequence(config);
        // if there is nothing
        if (mainSequence->countEnabledKeys() == 0) {
            // set default values
            if(mainSequence->countKeysTotal() == 0) addNewKeysToSequence(config, "settings/macro/mainKeySequence", getDefaultSequenceList(true));
            else addNewKeysToSequence(config, "settings/macro/mainKeySequence", getDefaultSequenceList(false));
            // reset the sequence and read the yaml object again
            readNewMainSequence(config);
        }
    }
    else {
        readDefaultMainSequence(config);
    }

    // extra sequences
    if (!checkExists(config, "settings/macro/extraKeySequences")) setConfigValue(config, "settings/macro/extraKeySequences", getDefaultExtraKeySequences());
    YAML::Node extraSequences = getConfigValue(config, "settings/macro/extraKeySequences");
    if (extraSequences.IsMap()) {
        //for (auto& otherSeq : knownOtherSequences) {
        //    delete &otherSeq;
        //}
        knownOtherSequences.clear();
        for (YAML::const_iterator at = extraSequences.begin(); at != extraSequences.end(); at++) {
            knownOtherSequences[at->first.as<std::string>()] = new KeySequence(at->second);
        }
    }

    // if no yaml structure errors
    if (!wrongConfig) {
        // resetting input interruption manager and reading what config has
        readNewInterruptionManager(config);
        bool nothingKeyboard = interruptionManager.load()->getConfigurations(KEYBOARD)->size() == 0;
        bool nothingMouse = interruptionManager.load()->getConfigurations(MOUSE)->size() == 0;
        // if there is nothing
        if (nothingKeyboard || nothingMouse) {
            //std::cout << "Nothing\n";
            // set default values
            YAML::Node* defaultList = interruptionManager.load()->getDefaultInterruptionConfigsList(KEYBOARD);
            if (nothingKeyboard) setConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases", *defaultList);
            delete defaultList;

            defaultList = interruptionManager.load()->getDefaultInterruptionConfigsList(MOUSE);
            if (nothingMouse) setConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases", *defaultList);
            delete defaultList;

            defaultList = interruptionManager.load()->getDefaultInterruptionConfigsList(ANY_INPUT);
            if (nothingMouse) setConfigValue(config, "settings/macro/interruptions/anyInput/manyInputsCases", *defaultList);
            delete defaultList;

            // reset the manager and read the yaml object again
            readNewInterruptionManager(config);
        }
    }
    else {
        readDefaultMainSequence(config);
        readDefaultInterruptionManager(config);
    }

    usePrimitiveInterruptionAlgorythm = getConfigBool(config, "settings/macro/interruptions/advanced/primitiveInterruptionsAlgorythm/enabled", true);
    primitiveWaitInterval = getConfigInt(config, "settings/macro/interruptions/advanced/primitiveInterruptionsAlgorythm/checkEvery_milliseconds", 100);

    readNewSettings(config);

    // saving
    setConfigValue(config, "internal/configVersion", currentVersion);
    return config;
}

bool loadConfig(ConfigType type) {
    try {
        std::string folderPath = "WindowSwitcherSettings";
        std::string fileName;
        switch (type)
        {
        case MAIN_CONFIG:
            fileName = "settings.yml";
            break;
        case KEYBINDS_CONFIG:
            fileName = "keybindings.yml";
            break;
        default:
            std::cout << "Unsupported config type\n";
            return false;
        }
        YAML::Node config;
        bool wrongConfig = false;
        bool wasEmpty = false;

        try {
            config = loadYaml(programPath, folderPath, fileName, wrongConfig);
            wasEmpty = config.IsNull();
        }
        /*catch (YAML::ParserException e) {
            addConfigLoadingMessage("THE CONFIG HAS INVALID STRUCTURE AND CANNOT BE LOADED!\nUsing ALL default values");
            config = YAML::Node();
            wrongConfig = true;
        }
        catch (YAML::BadSubscript e) {
            addConfigLoadingMessage("THE CONFIG HAS INVALID STRUCTURE AND CANNOT BE LOADED!\nUsing ALL default values");
            config = YAML::Node();
            wrongConfig = true;
        }*/
        catch (const YAML::Exception& e) {
            addConfigLoadingMessage("WARNING | UNKNOWN ERROR WHILE LOADING " + configTypeToString(type) + " CONFIG!\nERROR TEXT | " + std::string(e.what()));
            config = YAML::Node();
            wrongConfig = true;
        }

        if (wrongConfig) {
            addConfigLoadingMessage("WARNING | For safery reasons, only default values will be used and NO CHANGES WILL BE APPLIED to the actual config (" + configTypeToString(type) + ")");
        }
        
        YAML::Node newConfig;

        switch (type)
        {
        case MAIN_CONFIG:
            newConfig = loadSettingsConfig(config, wasEmpty, wrongConfig);
            break;
        case KEYBINDS_CONFIG:
            newConfig = loadKeybindingsConfig(config, wasEmpty, wrongConfig);
            break;
        default:
            std::cout << "Unsupported config type\n";
            return false;
        }

        if (!wrongConfig) saveYaml(newConfig, getProgramFolderPath(programPath) + "/" + folderPath + "/" + fileName);
        //if (!wrongConfig) saveYaml(newConfig, getProgramFolderPath(programPath) + "/" + folderPath + "/" + "test_" + fileName); // test

        initialConfigLoading = false;

        return true;
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::Exception error happened while loading the config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const std::exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected std::exception error happened while loading the config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const YAML::BadConversion& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::BadConversion error happened while loading the config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
}

void reloadConfigs() {
    loadConfig(MAIN_CONFIG);
    loadConfig(KEYBINDS_CONFIG);
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    //std::cout << interruptionManager.load()->isModeEnabled().load();
    if (interruptionManager != nullptr && interruptionManager.load()->isModeEnabled().load()) { // interruptionManager.load()->getModeEnabled().load()
        if (nCode >= 0) {
            bool ignore = false;

            //std::cout << interruptionManager.load()->getConfigurations(KEYBOARD)->size() << '\n';
            //std::cout << interruptionManager.load()->getConfigurations(MOUSE)->size() << '\n';

            int key = reinterpret_cast<LPKBDLLHOOKSTRUCT>(lParam)->vkCode;
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN || wParam == WM_CHAR) {

                std::string formattedKey;
                KBDLLHOOKSTRUCT* pkbhs = (KBDLLHOOKSTRUCT*)lParam;
                
                bool isCharInput = (pkbhs->vkCode >= 'A' && pkbhs->vkCode <= 'Z') ||
                    (pkbhs->vkCode >= '0' && pkbhs->vkCode <= '9');

                if (isCharInput) {
                    // Character or number
                    char lowercaseChar = tolower(static_cast<char>(pkbhs->vkCode));
                    formattedKey = std::string(1, lowercaseChar);
                }
                else {
                    // Special character
                    int intForm = (int)pkbhs->vkCode;
                    formattedKey = std::to_string(intForm);

                    //std::cout << "{\"" << intForm << "\", " << ParseHotkeyCode(formattedKey) << "},\n";

                    if (keyboardHookSpecialVirtualKeyCodeToText.count(intForm) > 0) {
                        formattedKey = keyboardHookSpecialVirtualKeyCodeToText[intForm];
                    }
                }

                if (interruptionManager.load()->checkIsInputPending(formattedKey)) ignore = true;
                
                if (!ignore) {
                    interruptionManager.load()->setNewDelay(interruptionManager.load()->getMacroPauseAfterKeyboardInput().load());
                    interruptionManager.load()->checkForManyInputsOnNew(KEYBOARD);
                    interruptionManager.load()->checkForManyInputsOnNew(ANY_INPUT);
                }

                //interruptionManager.load()->getKeyStates()[key] = true;
                //setNewDelay(macroPauseAfterKeyboardKeyHeldDown);
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                //interruptionManager.load()->getKeyStates()[key] = false;
                /*interruptionManager.load()->setNewDelay(interruptionManager.load()->getMacroPauseAfterKeyboardInput().load());*/
                //if (untilNextMacroRetry.load() <= macroPauseAfterKeyboardKeyHeldDown) untilNextMacroRetry.store(macroPauseAfterKeyboardInput);
            }

            // Check if any key is being held down
            /*bool anyKeyHeldDown = false;
            for (const auto& pair : keyStates) {
                if (pair.second) {
                    anyKeyHeldDown = true;
                    break;
                }
            }
            if (anyKeyHeldDown) {
                // Handle case when any key is held down
            }*/
        }
    }
    // Call the next hook in the hook chain
    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

LRESULT CALLBACK MouseProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (interruptionManager != nullptr && interruptionManager.load()->isModeEnabled().load()) {
        if (nCode >= 0) {
            switch (wParam) {
            case WM_LBUTTONDOWN:
                // Left mouse button down event
                //if (untilNextMacroRetry.load() <= macroPauseAfterKeyboardKeyHeldDown) untilNextMacroRetry.store(macroPauseAfterMouseInput);
                interruptionManager.load()->setNewDelay(interruptionManager.load()->getMacroPauseAfterMouseInput().load());
                interruptionManager.load()->checkForManyInputsOnNew(MOUSE);
                interruptionManager.load()->checkForManyInputsOnNew(ANY_INPUT);
                break;
            case WM_LBUTTONUP:
                // Left mouse button up event
                interruptionManager.load()->setNewDelay(interruptionManager.load()->getMacroPauseAfterMouseInput().load());
                break;
            case WM_RBUTTONDOWN:
                // Right mouse button down event
                interruptionManager.load()->setNewDelay(interruptionManager.load()->getMacroPauseAfterMouseInput().load());
                interruptionManager.load()->checkForManyInputsOnNew(MOUSE);
                interruptionManager.load()->checkForManyInputsOnNew(ANY_INPUT);
                break;
            case WM_RBUTTONUP:
                // Right mouse button up event
                interruptionManager.load()->setNewDelay(interruptionManager.load()->getMacroPauseAfterMouseInput().load());
                break;
                // Add cases for other mouse events as needed
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void keyboardHookFunc() {
    if (registerKeyboardHookInDebug || !debugMode) {
        keyboardHook = SetWindowsHookEx(WH_KEYBOARD_LL, KeyboardProc, NULL, 0);
        if (keyboardHook == NULL) std::cout << "Failed to register the keyboard hook for macro pauses" << std::endl;

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            //TranslateMessage(&msg);
            //DispatchMessage(&msg);
        }

        if (keyboardHook) UnhookWindowsHookEx(keyboardHook);
    }
    else {
        std::cout << "Not registering the keyboard hook due to debug mode\n";
    }
}

void MouseHookFunc() {
    if (registerKeyboardMouseInDebug || !debugMode) {
        mouseHook = SetWindowsHookEx(WH_MOUSE_LL, MouseProc, NULL, 0);
        if (mouseHook == NULL) std::cout << "Failed to register the mouse hook for macro pauses" << std::endl;

        MSG msg;
        while (GetMessage(&msg, NULL, 0, 0)) {
            //TranslateMessage(&msg);
            //DispatchMessage(&msg);
        }

        if (mouseHook) UnhookWindowsHookEx(mouseHook);
    }
    else {
        std::cout << "Not registering the mouse hook due to debug mode\n";
    }
}

void notifyTheMacro() {
    //std::cout << "Value on notifyTheMacro(): " << interruptionManager.load()->getUntilNextMacroRetryAtomic().load() << std::endl;
    //interruptedRightNow.store(false);
    macroWaitCv.notify_all();
    //std::cout << "Notified" << std::endl;
}

void storeCurDelay(int delay) {
    if (delay < 0) delay = 0;
    interruptionManager.load()->getUntilNextMacroRetryAtomic().store(delay);
}

void macroDelayModificationLoop() {
    while (true) {
        // important!
        //std::cout << "";
        if (interruptionManager != nullptr && interruptionManager.load()->isModeEnabled().load()) {
            int curValue = interruptionManager.load()->getUntilNextMacroRetryAtomic().load();
            //std::cout << curValue << endl;

            curValue -= 1;
            if (curValue == 0) {
                storeCurDelay(curValue);
                notifyTheMacro();
                //interruptedRightNow.store(false);
                if(interruptionManager.load()->getInformOnEvents() && !getStopMacroInput().load()) std::cout << "Unpaused\n";
                
            }
            else {
                storeCurDelay(curValue);
            }
        }
        Sleep(1000);
    }
    //std::cout << "Waining...\n";
    //cv.wait(lock, [] { return (untilNextMacroRetry.load() <= 0); });
    //std::cout << "Done waiting\n";
}

void testHotkey() {
    /*HWND hwnd = GetForegroundWindow();

    RECT windowSize = { NULL };
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &windowSize, sizeof(RECT));
    std::cout << windowSize.right - windowSize.left << " " << windowSize.bottom - windowSize.top << std::endl;*/

    // The number of cells in each row/column of the grid
    HWND hwnd = GetForegroundWindow();
    int cell = GetWindowCell(hwnd);

    std::cout << cell << std::endl;
    return;

    RECT windowSize = { NULL };
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &windowSize, sizeof(RECT));
    // GetWindowRect(hwnd, &r)
    //std::cout << r.left << " " << r.top << " " << r.right << " " << r.bottom << " " << endl;

    RECT desktop;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &desktop, 0);
    int desktopX = desktop.right;
    int desktopY = desktop.bottom;

    //if (IsHungAppWindow(hwnd)) std::cout << "Hang up window 3" << endl;
    //if (r.left == -7 && r.top == 0 && r.right == 967 && r.bottom == 527) autoGroup1.push_back(hwnd); // Explorer window
    //std::cout << r.right - r.left << "   " << r.bottom - r.top << endl;
    //if (r.right - r.left == desktopX / 2 && (r.bottom - r.top == 638 || r.bottom - r.top == 631)) { // 631 is considered as height when at the top, check on different Windows versions and
    //    if (r.left == 0 && r.top == 0) autoGroup1.push_back(hwnd); // actual sizes         // find out which one doesn't work and change to + or - border size //TODO
    //    else if (r.left == desktopX / 2 && r.top == 0) autoGroup2.push_back(hwnd);
    //    else if (r.left == 0 && r.top == desktopY - 638) autoGroup3.push_back(hwnd);
    //    else if (r.left == desktopX / 2 && r.top == desktopY - 638) autoGroup4.push_back(hwnd);
    //}

    std::cout << "X " << windowSize.left << " " << windowSize.right << " , Y " << windowSize.top << " " << windowSize.bottom << std::endl;
    std::cout << "Screen " << desktopX << " " << desktopY << std::endl;
}

void terminationOnFailure() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        ShowWindow(hwnd, SW_HIDE);
        ShowWindow(hwnd, SW_RESTORE);
    }

    std::cout << "[!!!] The application encountered an error that it doesn't want to ingnore, so it has exited." << std::endl;
    _getch();
    ExitProcess(1);
}

void signalHandler(int signum) {
    if (signum == SIGFPE) {
        std::cerr << "Caught signal: SIGFPE (Arithmetic exception)" << std::endl;
        std::cerr << "Arithmetic exception occurred." << std::endl;
    }
    else if (signum == SIGSEGV) {
        std::cerr << "Caught signal: SIGSEGV (Segmentation fault)" << std::endl;
        std::cerr << "Segmentation fault / access violation occurred." << std::endl;
    }
    else {
        std::cerr << "Caught signal: " << signum << std::endl;
    }
    terminationOnFailure();
}

LONG WINAPI UnhandledExceptionHandler(EXCEPTION_POINTERS* exceptionInfo) {
    // Get the exception record from ExceptionInfo
    auto pExceptionRecord = exceptionInfo->ExceptionRecord;

    // Print out the exception type (exception code)
    std::cerr << "Unhandled exception caught. Exception type: 0x" << std::hex << pExceptionRecord->ExceptionCode << std::endl;

    // Print out the exception message (exception information)
    char exceptionMessage[1024];
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_IGNORE_INSERTS, NULL, pExceptionRecord->ExceptionCode,
        MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), exceptionMessage, 1024, NULL);

    std::cerr << "Exception message: " << exceptionMessage << std::endl;

    terminationOnFailure();
    return EXCEPTION_EXECUTE_HANDLER;
}

void initSomeValues() {
    
}

int actualMain(int argc, char* argv[]) {
    // Setting error handlers for non-debug configuration
    if (true || !debugMode) {
        signal(SIGSEGV, signalHandler);
        signal(SIGFPE, signalHandler);
        SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
    }

    initSomeValues();

    //throw exception("aaa");
    /*int a = 0;
    int b = 0;
    std::cout << a / b;*/

    printTitle();

    setlocale(0, "");
    programPath = argv[0];
    initializeRandom();
    reloadConfigs();
    std::cout << "Config Done" << std::endl;
    rememberInitialPermanentSettings();
    registerHotkeys();
    printConfigLoadingMessages();

    //helpGenerateKeyMap();

    // Listening for inputs for macro interuptions
    if (interruptionManager.load()->getShouldStartDelayModificationLoop().load()) {
        macroDelayWatcherThread = new std::thread(macroDelayModificationLoop);
        macroDelayWatcherThread->detach();
    }
    if (interruptionManager.load()->getShouldStartKeyboardHook().load()) {
        keyboardHookThread = new std::thread(keyboardHookFunc);
        keyboardHookThread->detach();
        //std::cout << "Priority " << GetThreadPriority(keyboardHook) << endl;
        //SetThreadPriority(keyboardHook, THREAD_PRIORITY_HIGHEST);
    }
    if (interruptionManager.load()->getShouldStartMouseHook().load()) {
        mouseHookThread = new std::thread(MouseHookFunc);
        mouseHookThread->detach();
    }

    if (debugMode) std::cout << "[! DEBUG MODE !]\n\n";

    //UINT VKCode = LOBYTE(VkKeyScan('e'));
    //UINT ScanCode = MapVirtualKey(VKCode, 0);
    //std::cout << ScanCode << endl; // VkKeyScan('e') // wscanf_s(L"a")
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_HOTKEY) {
            checkClosedWindows();
            HWND curHwnd = GetForegroundWindow();
            //std::cout << "curHwnd = " << curHwnd << endl;
            //std::cout << msg.wParam << endl;
            if (msg.wParam == 1) {
                if (curHwnd != GetConsoleWindow()) {
                    if (lastGroup == NULL) {
                        lastGroup = new WindowGroup();
                    }
                    (*lastGroup).addWindow(curHwnd);
                    handleToGroup[curHwnd] = lastGroup;
                }
            }
            else if (msg.wParam == 2) {
                lastGroup = NULL;
            }
            else if (msg.wParam == 10) {
                SwapVisibilityForAll();
            }
            else if (msg.wParam == 12) {
                showAllRx();
            }
            else if (msg.wParam == 14) {
                connectAllQuarters();
            }
            else if (msg.wParam == 15) {
                handleToGroup.clear();
            }
            else if (msg.wParam == 21) { // test

                //keyPress(0x12);
                //customSleep(10000);
                //keyRelease(0x12);
                // 
                //windowPosTest(curHwnd, 3);
                //distributeRxHwndsToGroups(curHwnd); // also uncomment inside if needed to use
                //windowPosTest(curHwnd, 1);
            }
            else if (msg.wParam == 17) {
                toggleMacroState();
            }
            else if (msg.wParam == 24) {
                hideForgr();
            }
            else if (msg.wParam == 25) {
                getFromBackgroundSpecific();
            }
            else if (msg.wParam == 28) {
                restoreAllConnected();
            }
            else if (msg.wParam == 29) {
                setGroupKey(curHwnd);
            }
            else if (msg.wParam == 30) {
                connectAllRbxsNoMatterWhat();
            }
            else if (msg.wParam == 31) {
                reloadConfigs();
                std::cout << "\nHave reloaded all configs" << std::endl;
                printConfigLoadingMessages();
            }
            // EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE
            if (handleToGroup.count(curHwnd)) {
                if (msg.wParam == 3) {
                    shiftGroup(curHwnd, -1);
                }
                else if (msg.wParam == 4) {
                    shiftGroup(curHwnd, 1);
                }
                else if (msg.wParam == 5) {
                    shiftAllGroups(-1);
                }
                else if (msg.wParam == 6) {
                    shiftAllGroups(1);
                }
                else if (msg.wParam == 7) {
                    deleteWindow(curHwnd);
                }
                else if (msg.wParam == 8) {
                    deleteWindowGroup(curHwnd);
                }
                else if (msg.wParam == 9) {

                }
                else if (msg.wParam == 11) {
                    lastGroup = getGroup(curHwnd);
                }
                else if (msg.wParam == 13) {
                    performShowOrHideAllNotMain();
                }
                else if (msg.wParam == 26) {
                    shiftAllOtherGroups(-1);
                }
                else if (msg.wParam == 27) {
                    shiftAllOtherGroups(1);
                }
            }
            if (msg.wParam == 20) { // debug
                //std::cout << ((&referenceToGroup==NULL) || referenceToGroup.size()) << endl;
                if (handleToGroup.size() > 0) {
                    //std::cout << "The group and size (make sure not console): ";
                    //if (referenceToGroup.count(curHwnd)) std::cout << (referenceToGroup.find(curHwnd)->second) << " " << (*(referenceToGroup.find(curHwnd)->second)).size() << endl;

                    std::cout << "Current HWND->WindowGroup* std::map:\n";
                    for (const auto& it : handleToGroup) {
                        std::cout << it.first << " " << it.second;
                        if (it.first == curHwnd) std::cout << " (Current)";
                        std::cout << '\n';
                    }
                }
            }
            if (msg.wParam == 21) { // real debug
                testHotkey();
                //std::cout << "Waiting2...\n";
                //std::unique_lock<std::mutex> macroWaitLock = std::unique_lock<std::mutex>(std::mutex());
                //macroWaitCv.wait(*macroWaitLock, [] { return (interruptionManager.load()->getUntilNextMacroRetryAtomic().load() <= 0); });
                //std::cout << "Done2\n";
            }
            hungWindowsAnnouncement();
        }
    }

    return 0;
}

int main(int argc, char* argv[]) {
    if (debugMode) {
        actualMain(argc, argv);
    }
    else {
        try {
            actualMain(argc, argv);
        }
        catch (const YAML::Exception& e) {
            std::cerr << "ERROR | A YAML::Exception caught: " << typeid(e).name() << std::endl;
            std::cerr << "ERROR | It's message: " << e.what() << std::endl;
            terminationOnFailure();
        }
        catch (const std::exception& e) {
            std::cerr << "ERROR | An std::exception caught: " << typeid(e).name() << std::endl;
            std::cerr << "ERROR | It's message: " << e.what() << std::endl;
            terminationOnFailure();
        }
        catch (const std::runtime_error& e) {
            std::cerr << "ERROR | An std::runtime_error caught: " << typeid(e).name() << std::endl;
            std::cerr << "ERROR | It's message: " << e.what() << std::endl;
            terminationOnFailure();
        }
        catch (...) {
            std::cerr << "ERROR | Unknown exception caught, can't provide any more information" << std::endl;
            terminationOnFailure();
        }
    }
}