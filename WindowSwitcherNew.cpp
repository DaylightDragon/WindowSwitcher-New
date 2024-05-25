#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING

#include <iostream>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include "GeneralUtils.h"
#include "KeySequence.h"

#include <windows.h>
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

#pragma comment(lib, "Dwmapi.lib")

// Pre-defining all classes and methods
class WindowGroup;
void deleteWindowGroup(WindowGroup* wg);

// Windows, sequences and managers
map<HWND, WindowGroup*> referenceToGroup;
map<WindowGroup*, KeySequence*> groupToKey;
WindowGroup* lastGroup;
KeySequence* mainSequence;
map<string, KeySequence*> knownOtherSequences = map<string, KeySequence*>();
std::vector<std::string> failedHotkeys;
InputsInterruptionManager* interruptionManager;

vector<HWND> autoGroup1;
vector<HWND> autoGroup2;
vector<HWND> autoGroup3;
vector<HWND> autoGroup4;
vector<HWND> autoGroupAllWindows;

// Permanent settings
string currentConfigVersion = "2.2";
wstring rx_name = L"Roblox";

// Main misc settings variables
string defaultMacroKey = "e";
int macroDelayInitial;
int macroDelayBeforeSwitching;
int macroDelayBetweenSwitchingAndFocus;
int macroDelayAfterFocus;
int macroDelayAfterKeyPress;
int macroDelayAfterKeyRelease;
bool specialSingleWindowModeEnabled;
string specialSingleWindowModeKeyCode;
vector<string> defaultFastForegroundWindows;

// Input randomness
int sleepRandomnessPersent = 10;
int sleepRandomnessMaxDiff = 40;

// Runtime/status variables
bool stopMacroInput = true;
bool initialConfigLoading = true;
bool hideNotMainWindows = false;
int currentHangWindows = 0;
wstring customReturnToFgName = L"";
int restoredCustomWindowsAmount = 0;

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

bool debugMode = IsDebuggerPresent();;
//string mainConfigName = "WsSettings/settings.yml";

bool checkHungWindow(HWND hwnd) {
    if (IsHungAppWindow(hwnd)) {
        currentHangWindows += 1;
        return true;
    }
    return false;
}

void hungWindowsAnnouncement() {
    if (currentHangWindows != 0) {
        string s = "Detected " + to_string(currentHangWindows) + " not responding window";
        if (currentHangWindows > 1) s += "s";
        s += " during the last operation and skipped them";
        cout << s << endl;
        currentHangWindows = 0;
    }
}

class WindowGroup {
private:
    vector<HWND> hwnds;
    int index = 0;

public:
    vector<HWND> getOthers() {
        vector<HWND> v;
        removeClosedWindows();
        if (&hwnds == NULL || hwnds.size() == 0) return v;
        for (int i = 0; i < hwnds.size(); i++) {
            if (i != index) v.push_back(hwnds[i]);
        }
        return v;
    }

    HWND getCurrent() {
        removeClosedWindows();
        if (&hwnds == NULL || hwnds.size() == 0) return NULL;
        return hwnds[index];
    }

    bool containsWindow(HWND hwnd) {
        removeClosedWindows();
        return (find(hwnds.begin(), hwnds.end(), hwnd) != hwnds.end());
    }

    void addWindow(HWND hwnd) {
        if (containsWindow(hwnd)) return;
        removeClosedWindows();
        hwnds.push_back(hwnd);
        if (hideNotMainWindows && index != hwnds.size() - 1) {
            if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_HIDE);
        }
        //testPrintHwnds();
    }

    void removeWindow(HWND hwnd) {
        if (&hwnds == NULL || hwnds.size() == 0) return;
        removeClosedWindows();
        //cout << "removeWindow " << hwnds.size() << " " << hwnd;
        bool main = false;
        if (hwnd == hwnds[index]) main = true;
        referenceToGroup.erase(referenceToGroup.find(hwnd));
        hwnds.erase(std::remove(hwnds.begin(), hwnds.end(), hwnd), hwnds.end());
        if (hwnds.size() == 0) deleteWindowGroup(this);
        if (main) shiftWindows(-1);
        fixIndex();
    }

    void shiftWindows(int times) {
        if (&hwnds == NULL) return;
        removeClosedWindows();
        if (hwnds.size() == 0) return;
        HWND oldHwnd = getCurrent();
        if (times >= 0) {
            index = (index + times) % hwnds.size();
        }
        else {
            index = (hwnds.size() * (-1 * times) + index + times) % hwnds.size();
        }
        if (hideNotMainWindows) {
            //cout << oldHwnd << " " << getCurrent() << endl;
            HWND newHwnd = getCurrent();
            if (!checkHungWindow(newHwnd)) ShowWindow(newHwnd, SW_SHOW);
            if (!checkHungWindow(oldHwnd)) ShowWindow(oldHwnd, SW_HIDE);
        }
    }

    void fixIndex() {
        if (&hwnds == NULL) return;
        if (hwnds.size() == 0) index = 0;
        else index = index % hwnds.size();
    }

    void removeClosedWindows() {
        if (&hwnds == NULL || hwnds.size() == 0) return;
        for (int i = 0; i < hwnds.size(); i++) {
            if (&hwnds[i] && !IsWindow(hwnds[i])) {
                referenceToGroup.erase(referenceToGroup.find(hwnds[i]));
                hwnds.erase(hwnds.begin() + i);
            }
        }
        fixIndex();
    }

    void hideOrShowOthers() {
        vector<HWND> others = getOthers();
        for (auto& it : others) {
            if (!checkHungWindow(it)) {
                if (hideNotMainWindows) {
                    ShowWindow(it, SW_HIDE);
                }
                else {
                    ShowWindow(it, SW_SHOW);
                }
            }
        }
    }

    void testPrintHwnds() {
        for (auto& hwnd : hwnds) {
            cout << "HWND " << &hwnd << endl;
        }
    }

    int size() {
        return hwnds.size();
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

    registerSingleHotkey(1, MOD_ALT | MOD_NOREPEAT, 0xBC, "Alt + ,", "Add window to current group");
    registerSingleHotkey(2, MOD_ALT | MOD_NOREPEAT, 0xBE, "Alt + .", "Prepare for the next group");
    registerSingleHotkey(11, MOD_ALT | MOD_NOREPEAT, 0x49, "Alt + I'", "Edit the window's group");
    registerSingleHotkey(3, MOD_ALT | MOD_NOREPEAT, 0x4B, "Alt + K'", "Shift windows to the left");
    registerSingleHotkey(4, MOD_ALT | MOD_NOREPEAT, 0x4C, "Alt + L'", "Shift windows to the right");
    registerSingleHotkey(5, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x4B, "Ctrl + Alt + K'", "Shift ALL windows to the left");
    registerSingleHotkey(6, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x4C, "Ctrl + Alt + L'", "Shift ALL windows to the right");
    registerSingleHotkey(26, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x4B, "Ctrl + Shift + K", "Shift ALL OTHER groups to the left");
    registerSingleHotkey(27, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x4C, "Ctrl + Shift + L", "Shift ALL OTHER groups to the right");
    registerSingleHotkey(7, MOD_ALT | MOD_NOREPEAT, 0xDD, "Alt + ]", "Remove current window from it's group (Critical Bug)");
    registerSingleHotkey(8, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0xDD, "Ctrl + Alt + ]", "Delete the entire group current window is in");
    registerSingleHotkey(15, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0xDB, "Ctrl + Shift + [", "Delete all groups");
    registerSingleHotkey(9, MOD_ALT | MOD_NOREPEAT, 0x51, "Alt + Q", "Toggle visibility of the opposite windows in this group (NOT SOON WIP)");
    registerSingleHotkey(13, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x51, "Ctrl + Alt + Q", "Toggle visibility of all not main windows (May not work properly yet with Auto-key macro)");
    // ctrl shift a do current alt a, but that one should leave window updating in background
    registerSingleHotkey(10, MOD_ALT | MOD_NOREPEAT, 0x41, "Alt + A", "Toggle visibility of every last window in all the pairs and minimize the linked ones");
    registerSingleHotkey(18, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x44, "(WIP) Ctrl + Shift + D", "Set the window as main in current group");
    registerSingleHotkey(19, MOD_ALT | MOD_NOREPEAT, 0x44, "Alt + D", "(WIP) Swap to main window in current group");
    registerSingleHotkey(23, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x44, "Ctrl + Alt + D", "(WIP) Swap to main window in all groups");
    registerSingleHotkey(12, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x55, "Ctrl + Alt + U", "Get all RBX and VMWW windows back from background");
    registerSingleHotkey(24, MOD_CONTROL | MOD_NOREPEAT, 0x55, "Ctrl + U", "Put current foregrounded window to background");
    registerSingleHotkey(25, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x55, "Ctrl + Shift + U", "Get specific windows from background by their name");
    registerSingleHotkey(16, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x56, "Ctrl + Shift + V", "(WIP, NOT SOON) Start/Stop adjusting new RBX windows to screen quarters");
    registerSingleHotkey(14, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x56, "Ctrl + Alt + V", "Connect all RBX windows to 4 quarter groups");
    registerSingleHotkey(30, MOD_ALT | MOD_NOREPEAT, 0x56, "Alt + V", "Connect absolutely all RBX windows into one single group");
    registerSingleHotkey(28, MOD_CONTROL | MOD_SHIFT | MOD_NOREPEAT, 0x41, "Ctrl + Shift + A", "Bring all connected windows to foreground");
    registerSingleHotkey(17, MOD_ALT | MOD_NOREPEAT, 0x47, "Alt + G", "Start/stop the automatical sequence macro for Roblox windows");
    registerSingleHotkey(29, MOD_CONTROL | MOD_ALT | MOD_NOREPEAT, 0x47, "Ctrl + Alt + G", "Set the macro key (or sequence) for this group");
    registerSingleHotkey(31, MOD_ALT | MOD_NOREPEAT, 0x48, "Alt + H", "Reload all configs");
    registerSingleHotkey(20, MOD_ALT | MOD_NOREPEAT, 0x50, "Alt + P", "Show the debug list of the linked windows", true);
    registerSingleHotkey(21, MOD_ALT | MOD_NOREPEAT, 0xDC, "Alt + \\", "Test", true);

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

    cout << endl;
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

WindowGroup* getGroup(HWND hwnd) {
    if (referenceToGroup.count(hwnd)) {
        map<HWND, WindowGroup*>::iterator it = referenceToGroup.find(hwnd);
        WindowGroup* wg = it->second;
        return wg;
    }
    //else return NULL;
}

void ShowOnlyMainInGroup(WindowGroup* wg) {
    vector<HWND> others = (*wg).getOthers();
    vector<HWND>::iterator iter;
    for (iter = others.begin(); iter != others.end(); ++iter) {
        //cout << *iter << " MINIMIZE" << endl;
        if (!hideNotMainWindows) {
            if (!checkHungWindow(*iter)) ShowWindow(*iter, SW_MINIMIZE);
        }
    }
    HWND c = (*wg).getCurrent();
    //cout << c << " SW_RESTORE" << endl;
    if (IsIconic(c)) {
        if (!checkHungWindow(c)) ShowWindow(c, SW_RESTORE); // what does it return if just z order far away but not minimized, maybe detect large windows above it
    }
}

void ShowOnlyMainInGroup(HWND hwnd) {
    if (referenceToGroup.count(hwnd)) {
        map<HWND, WindowGroup*>::iterator it = referenceToGroup.find(hwnd);
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

wstring getWindowName(HWND hwnd) {
    int length = GetWindowTextLength(hwnd);
    wchar_t* buffer = new wchar_t[length + 1];
    GetWindowTextW(hwnd, buffer, length + 1);
    wstring ws(buffer);
    return ws; // not nullptr later?
}

void shiftAllGroups(int shift) {
    vector<WindowGroup*> used;
    map<HWND, WindowGroup*>::iterator it;

    for (it = referenceToGroup.begin(); it != referenceToGroup.end(); it++) {
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            shiftGroup(it->second, shift);
            used.push_back(it->second); // is * refering to it or it->second?
        }
    }
}

void shiftAllOtherGroups(int shift) {
    vector<WindowGroup*> used;
    map<HWND, WindowGroup*>::iterator it;

    HWND h = GetForegroundWindow();

    for (it = referenceToGroup.begin(); it != referenceToGroup.end(); it++) {
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            //cout << (h == it->first) << endl;
            //wcout << getWindowName(it->first) << endl;
            //wcout << getWindowName(h) << endl;
            //cout << it->second << endl;
            //cout << referenceToGroup[h] << endl << endl;
            //cout << (referenceToGroup[h] == it->second) << endl;
            if (referenceToGroup[h] != it->second) shiftGroup(it->second, shift);
            used.push_back(it->second); // is * refering to it or it->second?
        }
    }

    SetForegroundWindow(h);

    cout << endl;
}

void deleteWindow(HWND hwnd) {
    if (referenceToGroup.count(hwnd)) {
        WindowGroup* wg = referenceToGroup.find(hwnd)->second;
        (*wg).removeWindow(hwnd);
        referenceToGroup.erase(referenceToGroup.find(hwnd));
    }
}

void deleteWindowGroup(WindowGroup* wg) {
    /*for (auto& p : referenceToGroup)
        cout << p.first << " " << p.second << " " << endl;*/
    map<HWND, WindowGroup*>::iterator it;
    for (auto it = referenceToGroup.begin(); it != referenceToGroup.end(); ) {
        if (wg == it->second) {
            it = referenceToGroup.erase(it);
        }
        else {
            ++it; // can have a bug
        }
    }
    //delete(wg); // ?
}

void deleteWindowGroup(HWND hwnd) {
    if (referenceToGroup.count(hwnd)) {
        deleteWindowGroup(referenceToGroup.find(hwnd)->second);
    }
}

void SwapVisibilityForAll() {
    vector<WindowGroup*> used;
    map<HWND, WindowGroup*>::iterator it;

    vector<HWND> main;
    vector<HWND> other;

    for (it = referenceToGroup.begin(); it != referenceToGroup.end(); it++) {
        //if (IsHungAppWindow(it->first)) cout << "Hang up window 8" << endl;
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            WindowGroup* wg = it->second;
            main.push_back((*wg).getCurrent());
            vector<HWND> others = (*wg).getOthers();
            vector<HWND>::iterator iter;
            for (iter = others.begin(); iter != others.end(); ++iter) {
                other.push_back(*iter);
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
                //if (IsHungAppWindow(it)) cout << "Hang up window 4" << endl;
                if (IsIconic(it)) {
                    //if (IsHungAppWindow(it)) cout << "Hang up window 5" << endl;
                    ShowWindow(it, SW_RESTORE);
                }
            }
            else {
                //if (IsHungAppWindow(it)) cout << "Hang up window 6" << endl;
                ShowWindow(it, SW_MINIMIZE);
            }
        }
    }

    for (auto& it : other) {
        //if (IsHungAppWindow(it)) cout << "Hang up window 7" << endl;
        if (!checkHungWindow(it)) ShowWindow(it, SW_MINIMIZE); // later add "hide" and "show" mode, after working window enumeration to get them back
    }
}

void checkClosedWindows() {
    bool br = false;
    for (const auto& it : referenceToGroup) {
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
    //wstring ws(buffer);

    wstring ws = getWindowName(hwnd);
    string str(ws.begin(), ws.end());

    // List visible windows with a non-empty title
    //if (IsWindowVisible(hwnd) && length != 0) {
    //    wcout << hwnd << ":  " << ws << endl;
    //}

    //if (ws == rx_name) {
    //    if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_SHOW);
    //}
    //else {
        for (auto& it : defaultFastForegroundWindows) {
            if (str.find(it) != wstring::npos) {
                if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_SHOW); //cout << GetLastError() << endl;
                //cout << "RBX HERE " << str << endl;
            }
        }
    //}

    //EnumChildWindows(hwnd, enumWindowCallback, NULL); //TODO ?
    //cout << "Good\n";

    return TRUE;
}

void showAllRx() {
    //cout << "Enmumerating windows..." << endl;
    hideNotMainWindows = false;
    EnumWindows(enumWindowCallback, NULL);
    //cout << "Done\n";
}

void restoreAllConnected() {
    hideNotMainWindows = false;
    for (const auto& it : referenceToGroup) {
        if (it.first != NULL && !checkHungWindow(it.first)) {
            ShowWindow(it.first, SW_MINIMIZE); // bad way // though not that much lol
            ShowWindow(it.first, SW_RESTORE);
        }
    }
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

void keyPressString(string key) {
    interruptionManager->addPendingSentInput(key);
    keyPressInput(mapOfKeys[key]);
}

void keyReleaseString(string key) {
    keyReleaseInput(mapOfKeys[key]);
}

void pressAndUnpressAKey(HWND w, Key k) {
    if (!k.enabled || k.keyCode == "EXAMPLE") return;
    if (stopMacroInput) return;

    customSleep(k.beforeKeyPress);
    if (stopMacroInput) return;

    interruptionManager->addPendingSentInput(k.keyCode); // BEFORE actually pressing! Or the handler will get it before it's added
    keyPressInput(mapOfKeys[k.keyCode]);
    
    customSleep(k.holdFor);
    keyReleaseInput(mapOfKeys[k.keyCode]);
    if (stopMacroInput) return;

    customSleep(k.afterKeyPress);
}

void performASequence(HWND w) {
    if (groupToKey.count(referenceToGroup[w])) {
        vector<Key> keys = groupToKey[referenceToGroup[w]]->getKeys();
        if (keys.size() > 0) {
            for (auto& el : keys) {
                if (stopMacroInput) return;
                pressAndUnpressAKey(w, el);
            }
        }
        else {
            cout << "You can't have no actions in a sequence! Waiting a bit instead" << endl; // kostil
            customSleep(100);
        }
    }
    else {
        for (auto& el : mainSequence->getKeys()) {
            if (stopMacroInput) return;
            pressAndUnpressAKey(w, el);
        }
    }
}

void focusAndSendSequence(HWND hwnd) { // find this
    /*int key = mapOfKeys[defaultMacroKey]; // 0x12
    //if(specialSingleWindowModeKeyCode)
    if (groupToKey.count(referenceToGroup[hwnd])) {
        key = groupToKey[referenceToGroup[hwnd]];
    }*/
    //cout << "test\n";
    //cout << "key: " << key << endl;

    SetForegroundWindow(hwnd);
    customSleep(macroDelayBetweenSwitchingAndFocus);
    SetFocus(hwnd);

    customSleep(macroDelayAfterFocus);

    performASequence(hwnd);
}

void performInputsEverywhere()
{
    for (auto& it : referenceToGroup) {
        if (stopMacroInput) return;
        //cout << it.first << endl;
        focusAndSendSequence(it.first);
        customSleep(macroDelayBeforeSwitching);
    }
}

void startUsualMacroLoop() {
    customSleep(macroDelayInitial);
    while (!stopMacroInput) {
        //for (int i = 0x5; i <= 0x30; i++) { // 5A
            //if (stopInput) return;
            //cout << std::hex << key << endl;
            //cout << std::dec;
            //key = i;
        performInputsEverywhere();
        // 8000
        /*for (int i = 0; i < 100; i++) {
            //customSleep(160);
            customSleep(50);
            if (stopInput) return;
        }*/
    }
}

void startUsualSequnceMode() {
    currentMacroLoopThread = new thread(startUsualMacroLoop);
}

void performSingleWindowedHold() {
    HWND w = GetForegroundWindow();
    SetForegroundWindow(w);
    customSleep(macroDelayBetweenSwitchingAndFocus);
    SetFocus(w);
    customSleep(macroDelayAfterFocus);
    string key = specialSingleWindowModeKeyCode;
    if (groupToKey.count(referenceToGroup[w])) {
        vector<Key> allKeys = groupToKey[referenceToGroup[w]]->getKeys();
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
    string key = specialSingleWindowModeKeyCode;
    if (groupToKey.count(referenceToGroup[w])) {
        vector<Key> allKeys = groupToKey[referenceToGroup[w]]->getKeys();
        if (allKeys.size() > 0) key = allKeys[0].keyCode;
    }
    keyReleaseString(key);
}

void toggleMacroState() {
    if (referenceToGroup.size() == 0) {
        cout << "You haven't linked any windows yet!" << endl;
    }
    else if (stopMacroInput) {
        stopMacroInput = false;
        cout << "Starting...\n";
        if (specialSingleWindowModeEnabled && referenceToGroup.size() == 1) {
            performSingleWindowedHold();
        }
        else {
            startUsualSequnceMode();
        }
    }
    else {
        stopMacroInput = true;
        if (referenceToGroup.size() == 1) releaseConfiguredKey();
        cout << "Stopped\n";
    }
}

void addWindowNoMatterWhat(HWND hwnd) {
    if (checkHungWindow(hwnd)) return;
    bool openCmdLater = false;
    if (IsIconic(hwnd)) {
        ShowWindow(hwnd, SW_RESTORE); // didn't put MINIMIZE before and without if statement bc this entire piece of code is more like a workaround than a feature
        if (!IsIconic(GetConsoleWindow())) {
            openCmdLater = true;
        }
    }
    autoGroupAllWindows.push_back(hwnd);
}

void distributeRxHwndsToGroups(HWND hwnd) {
    if (checkHungWindow(hwnd)) return;
    RECT r = { NULL };
    bool openCmdLater = false;
    if (IsIconic(hwnd)) {
        //if (IsHungAppWindow(hwnd)) cout << "Hang up window 3'" << endl;
        ShowWindow(hwnd, SW_RESTORE); // didn't put MINIMIZE before and without if statement bc this entire piece of code is more like a workaround than a feature
        //if (IsHungAppWindow(hwnd)) cout << "Hang up window 4'" << endl;
        if (!IsIconic(GetConsoleWindow())) {
            //if (IsHungAppWindow(hwnd)) cout << "Hang up window 5'" << endl;
            openCmdLater = true;
        }
    }
    customSleep(1);
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &r, sizeof(RECT));
    // GetWindowRect(hwnd, &r)
    //cout << r.left << " " << r.top << " " << r.right << " " << r.bottom << " " << endl;

    RECT desktop;
    SystemParametersInfo(SPI_GETWORKAREA, 0, &desktop, 0);
    int desktopX = desktop.right;
    int desktopY = desktop.bottom;

    //if (IsHungAppWindow(hwnd)) cout << "Hang up window 3" << endl;
    //if (r.left == -7 && r.top == 0 && r.right == 967 && r.bottom == 527) autoGroup1.push_back(hwnd); // Explorer window
    //cout << r.right - r.left << "   " << r.bottom - r.top << endl;
    if (r.right - r.left == desktopX / 2 && (r.bottom - r.top == 638 || r.bottom - r.top == 631)) { // 631 is considered as height when at the top, check on different Windows versions and
        if (r.left == 0 && r.top == 0) autoGroup1.push_back(hwnd); // actual sizes         // find out which one doesn't work and change to + or - border size //TODO
        else if (r.left == desktopX / 2 && r.top == 0) autoGroup2.push_back(hwnd);
        else if (r.left == 0 && r.top == desktopY - 638) autoGroup3.push_back(hwnd);
        else if (r.left == desktopX / 2 && r.top == desktopY - 638) autoGroup4.push_back(hwnd);
    }

    if (openCmdLater) {
        /*cout << "restoring\n";
        customSleep(1);
        if(IsIconic(GetConsoleWindow())) ShowWindow(GetConsoleWindow(), SW_RESTORE);*/
    }
}

static BOOL CALLBACK rxConnectivityCallback(HWND hwnd, LPARAM lparam) {
    //int length = GetWindowTextLength(hwnd); // WindowName
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //wstring ws(buffer);
    //string str(ws.begin(), ws.end());

    //if(IsWindowVisible(hwnd))
    // 
    //wcout << "\"" << ws << "\"" << endl;
    //cout << str << endl;

    wstring ws = getWindowName(hwnd);

    if (ws == rx_name) { // ws == L"HangApp"
        distributeRxHwndsToGroups(hwnd);
        //cout << "Found!\n";
    }

    //EnumChildWindows(hwnd, enumWindowCallback, NULL); //TODO ?
    return TRUE;
}

static BOOL CALLBACK rxConnectivityCompletelyAllCallback(HWND hwnd, LPARAM lparam) {
    //int length = GetWindowTextLength(hwnd); // Window Name
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //wstring ws(buffer);
    wstring ws = getWindowName(hwnd);

    //string str(ws.begin(), ws.end());

    if (ws == rx_name) {
        addWindowNoMatterWhat(hwnd);
    }

    return TRUE;
}

int createSingleConnectedGroup(vector<HWND>* windows) {
    if (windows->size() == 0) return 0;
    WindowGroup* wg;
    wg = new WindowGroup();
    int amount = 0;
    for (auto& w : *windows) {
        if (referenceToGroup.count(w)) {
            map<HWND, WindowGroup*>::iterator it = referenceToGroup.find(w);
            referenceToGroup.find(w)->second->removeWindow(w);
            //referenceToGroup.erase(referenceToGroup.find(w));
        }
        wg->addWindow(w);
        referenceToGroup[w] = wg;
        amount++;
    }
    return amount;
}

int createConnectedGroups() {
    int amount = 0;
    amount += createSingleConnectedGroup(&autoGroup4);
    amount += createSingleConnectedGroup(&autoGroup3);
    amount += createSingleConnectedGroup(&autoGroup2);
    amount += createSingleConnectedGroup(&autoGroup1);
    return amount;
}

int createConnectedGroupsForCompletelyAll() {
    int amount = 0;
    amount += createSingleConnectedGroup(&autoGroupAllWindows);
    return amount;
}

void connectAllQuarters() {
    cout << "Connecting RBX windows..." << endl;
    autoGroup1.clear(); autoGroup2.clear(); autoGroup3.clear(); autoGroup4.clear();
    currentHangWindows = 0;
    EnumWindows(rxConnectivityCallback, NULL);
    if (currentHangWindows > 0) cout << "Found " << currentHangWindows << " window(s) that isn't (aren't) responding, skipping them" << endl;
    int amount = createConnectedGroups();
    if (amount > 0) {
        cout << "Successfully connected " << amount << " window";
        if (amount > 1) cout << "s";
        cout << endl;
    }
    else {
        cout << "Didn't find any matching windows!" << endl;
    }
}

void connectAllRbxsNoMatterWhat() {
    cout << "Connecting Absolutely all RBX windows..." << endl;
    autoGroupAllWindows.clear();
    currentHangWindows = 0;
    EnumWindows(rxConnectivityCompletelyAllCallback, NULL);
    if (currentHangWindows > 0) cout << "Found " << currentHangWindows << " window(s) that isn't (aren't) responding, skipping them" << endl;
    int amount = createConnectedGroupsForCompletelyAll();
    if (amount > 0) {
        cout << "Successfully connected " << amount << " RBX window";
        if (amount > 1) cout << "s";
        cout << endl;
    }
    else {
        cout << "Didn't find any RBX windows!" << endl;
    }
}

void performShowOrHideAllNotMain() {
    hideNotMainWindows = !hideNotMainWindows;
    for (const auto& it : referenceToGroup) {
        if (it.second != NULL) {
            it.second->hideOrShowOthers();
        }
    }
}

void windowPosTest(HWND hwnd, int type) {
    RECT p;
    GetWindowRect(hwnd, &p);
    //RECT p = WINDOWPLACEMENT;
    cout << p.left << " " << p.top << endl;
    cout << p.right << " " << p.bottom << endl;
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
    cout << p2.left << " " << p2.top << endl;
    cout << p2.right << " " << p2.bottom << endl;

    cout << endl;
    //SetWindowPlacement(hwnd,
}

void hideForgr() {
    HWND h = GetForegroundWindow();
    //wstring name = getWindowName(h);
    //wcout << h << endl;
    //cout << (h == FindWindow(L"Shell_TrayWnd", NULL)) << endl;
    //cout << (h == FindWindow(L"ToolbarWindow32", L"Running Applications")) << endl;
    //cout << (h == FindWindow(L"SysTabControl32", NULL)) << endl;
    //wcout << getWindowName(GetParent(h)) << endl;
    //cout << (curHwnd == GetDesktopWindow());
    if (h != FindWindow("Shell_TrayWnd", NULL) && !checkHungWindow(h)) ShowWindow(h, SW_HIDE); // taskbar, other desktop components get back on their own
    //get back from vector somehow, maybe input
}

static BOOL CALLBACK fgCustonWindowCallback(HWND hwnd, LPARAM lparam) {
    if (customReturnToFgName == L"") return TRUE;

    //int length = GetWindowTextLength(hwnd); // WindowName
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //wstring ws(buffer);

    wstring ws = getWindowName(hwnd);

    if (ws == customReturnToFgName || (ws.find(customReturnToFgName) != wstring::npos)) {
        if (!checkHungWindow(hwnd)) {
            if (!ShowWindow(hwnd, SW_SHOW)) cout << GetLastError() << endl;
            if (!IsWindowVisible(hwnd)) {
                ShowWindow(hwnd, SW_RESTORE); // cout << GetLastError() << endl;
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
    wstring targetName = L"";
    wstring checkEmpty = L"";
    wcout << "Enter the text (Eng only):\n";
    getline(wcin, targetName);
    //wcout << "Entered: '" << targetName << "'" << endl;

    checkEmpty = targetName;

    checkEmpty.erase(std::remove(checkEmpty.begin(), checkEmpty.end(), ' '), checkEmpty.end());

    //wcout << "\"" << checkEmpty << "\"\n";
    if (checkEmpty != L"") {
        customReturnToFgName = targetName;
    }
    //else if (customFgName != L"") cout << "Using the previous window name\n";
    //else {
    //    cout << "The input data is incorrect" << endl;
    //}
    EnumWindows(fgCustonWindowCallback, NULL);
    if (restoredCustomWindowsAmount == 1) cout << "Success\n";
    else cout << "Couldn't find such window!\n";
}

void setGroupKey(HWND h) {
    if (referenceToGroup.count(h)) {
        SetForegroundWindow(GetConsoleWindow());
        cout << "Enter the key (Eng only, lowercase, no combinations) or the extra sequence name with \"!\" in the beggining:\n";
        string keyName = "";
        getline(cin, keyName);
        if (keyName.rfind("!", 0) == 0 && groupToKey.count(referenceToGroup[h])) {
            groupToKey[referenceToGroup[h]] = knownOtherSequences[keyName.substr(1)];
        }
        else if (mapOfKeys.count(keyName)) {
            groupToKey[referenceToGroup[h]] = &KeySequence(keyName); // then use as mapOfKeys[keyName] // important comment
            cout << "Set the key for that group" << endl;
            SetForegroundWindow(h);
        }
        else {
            cout << "Such key (or sequence name) doesn't esist or isn't supported yet!" << endl;
        }
    }
    else {
        cout << "No window group is focused!" << endl;
    }
}

YAML::Node getDefaultSequenceList() {
    YAML::Node firstKey = YAML::Node();
    setConfigValue(firstKey, "keyCode", defaultMacroKey);
    setConfigValue(firstKey, "enabled", true);
    setConfigValue(firstKey, "beforeKeyPress", 0);
    setConfigValue(firstKey, "holdFor", 2400);
    setConfigValue(firstKey, "afterKeyPress", 10);

    YAML::Node keyExample = YAML::Clone(firstKey);
    setConfigValue(keyExample, "keyCode", "EXAMPLE");
    setConfigValue(keyExample, "enabled", false);

    YAML::Node list = YAML::Node(YAML::NodeType::Sequence);
    list.push_back(firstKey);
    list.push_back(keyExample);

    return list;
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
    mainSequence = new KeySequence(getDefaultSequenceList());
}

void resetInterruptionManager(const YAML::Node& config) {
    if (interruptionManager != nullptr) {
        delete interruptionManager;
        interruptionManager = nullptr;
    }
    //cout << "resetInterruptionManager\n";
}

void rememberInitialPermanentSettings() {
    oldConfigValue_startAnything = interruptionManager->getShouldStartAnything().load();
    oldConfigValue_startKeyboardHook = interruptionManager->getShouldStartKeyboardHook().load();
    oldConfigValue_startMouseHook = interruptionManager->getShouldStartMouseHook().load();
}

void readSimpleInteruptionManagerSettings(const YAML::Node& config) {
    interruptionManager->setMacroPauseAfterKeyboardInput(getConfigInt(config, "settings/macro/interruptions/keyboard/startWithDelay_seconds", 8));
    interruptionManager->setMacroPauseAfterMouseInput(getConfigInt(config, "settings/macro/interruptions/mouse/startWithDelay_seconds", 3));
    interruptionManager->setInputsListCapacity(getConfigInt(config, "settings/macro/interruptions/rememberMaximumInputsEach", 40));
    interruptionManager->setInputSeparationWaitingTimeout(getConfigInt(config, "settings/macro/interruptions/macroInputArrivalTimeoutToSeparateFromUserInputs", 1000));
    interruptionManager->setModeEnabled(getConfigBool(config, "settings/macro/interruptions/interruptionsWorkingRightNow", false));
    
    interruptionManager->setShouldStartAnything(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/enableTheseOptions", true));
    interruptionManager->setShouldStartKeyboardHook(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/keyboardListener", true));
    if (!interruptionManager->getShouldStartAnything().load()) interruptionManager->setShouldStartKeyboardHook(false);
    interruptionManager->setShouldStartMouseHook(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/mouseListener", true));
    if (!interruptionManager->getShouldStartAnything().load()) interruptionManager->setShouldStartMouseHook(false);
    interruptionManager->setShouldStartDelayModificationLoop(interruptionManager->getShouldStartKeyboardHook().load() || interruptionManager->getShouldStartMouseHook().load());
    if (!interruptionManager->getShouldStartAnything().load()) interruptionManager->setShouldStartDelayModificationLoop(false);

    if (!initialConfigLoading && (
        oldConfigValue_startKeyboardHook != interruptionManager->getShouldStartKeyboardHook().load() ||
        oldConfigValue_startMouseHook != interruptionManager->getShouldStartMouseHook().load())) {
        addConfigLoadingMessage("WARNING | Some config values can't be applied in runtime, you need to restart the application for that");
    }
}

void readNewInterruptionManager(const YAML::Node& config) {
    resetInterruptionManager(config);
    interruptionManager = new InputsInterruptionManager();
    //cout << "readNewInterruptionManager\n";

    interruptionManager->initType(getConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases"), KEYBOARD);
    interruptionManager->initType(getConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases"), MOUSE);
    interruptionManager->initType(getConfigValue(config, "settings/macro/interruptions/anyInput/manyInputsCases"), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);

    //cout << interruptionManager << '\n';
    //if (macroDelayWatcherThread != nullptr) {
        //macroDelayWatcherThread->
    //}
    //std::thread(testWait).detach();
}

void readDefaultInterruptionManager(const YAML::Node& config) {
    resetInterruptionManager(config);
    interruptionManager = new InputsInterruptionManager();
    //cout << "readDefaultInterruptionManager\n";

    interruptionManager->initType(*InputsInterruptionManager::getDefaultInterruptionConfigsList(KEYBOARD), KEYBOARD);
    interruptionManager->initType(*InputsInterruptionManager::getDefaultInterruptionConfigsList(MOUSE), MOUSE);
    interruptionManager->initType(*InputsInterruptionManager::getDefaultInterruptionConfigsList(ANY_INPUT), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);
}

void updateConfig2_0_TO_2_1(YAML::Node &config, bool wrongConfig) {
    addConfigLoadingMessage("INFO | The config was updated from version 2.0 to 2.1");

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

    setConfigValue(config, "settings/macro/mainKeySequence", getDefaultSequenceList());
    if(!checkExists(config, "settings/macro/extraKeySequences")) setConfigValue(config, "settings/macro/extraKeySequences", vector<YAML::Node>());
    removeConfigValue(config, "settings/macro/delaysInMilliseconds");
    removeConfigValue(config, "settings/macro/justHoldWhenSingleWindow");
    removeConfigValue(config, "settings/macro/defaultKey", true);
}

bool loadConfig(string programPath) {
    try {
        string folderPath = "WindowSwitcherSettings";
        string fileName = "settings.yml";
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
            addConfigLoadingMessage("WARNING | UNKNOWN ERROR WHILE LOADING THE CONFIG!\nERROR TEXT | " + string(e.what()));
            config = YAML::Node();
            wrongConfig = true;
        }

        if (wrongConfig) {
            addConfigLoadingMessage("WARNING | For safery reasons, only default values will be used and NO CHANGES WILL BE APPLIED to the actual config");
        }

        if (!wasEmpty) {
            string oldConfigVersion;
            YAML::Node versionData;
            try {
                versionData = getConfigValue(config, "internal/configVersion");
                oldConfigVersion = versionData.as<string>();
            }
            catch (const YAML::Exception& e) {
                oldConfigVersion = "2.0";
            }
            if (oldConfigVersion == "2.0") {
                updateConfig2_0_TO_2_1(config, wrongConfig);
                oldConfigVersion = "2.1";
            }
            if (oldConfigVersion == "2.1") {
                // no refactoring
                oldConfigVersion = "2.2";
            }
        }

        macroDelayInitial = getConfigInt(config, "settings/macro/general/initialDelayBeforeFirstIteration", 100);
        macroDelayBeforeSwitching = getConfigInt(config, "settings/macro/general/delayBeforeSwitchingWindow", 0);
        macroDelayAfterFocus = getConfigInt(config, "settings/macro/general/delayAfterSwitchingWindow", 200);
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
                setConfigValue(config, "settings/macro/mainKeySequence", getDefaultSequenceList());
                // reset the sequence and read the yaml object again
                readNewMainSequence(config);
            }
        }
        else {
            readDefaultMainSequence(config);
        }

        // extra sequences
        if (!checkExists(config, "settings/macro/extraKeySequences")) setConfigValue(config, "settings/macro/extraKeySequences", vector<YAML::Node>());
        YAML::Node extraSequences = getConfigValue(config, "settings/macro/extraKeySequences");
        if (extraSequences.IsMap()) {
            //for (auto& otherSeq : knownOtherSequences) {
            //    delete &otherSeq;
            //}
            knownOtherSequences.clear();
            for (YAML::const_iterator at = extraSequences.begin(); at != extraSequences.end(); at++) {
                knownOtherSequences[at->first.as<string>()] = new KeySequence(at->second);
            }
        }

        // if no yaml structure errors
        if (!wrongConfig) {
            // resetting input interruption manager and reading what config has
            readNewInterruptionManager(config);
            bool nothingKeyboard = interruptionManager->getConfigurations(KEYBOARD)->size() == 0;
            bool nothingMouse = interruptionManager->getConfigurations(MOUSE)->size() == 0;
            // if there is nothing
            if (nothingKeyboard || nothingMouse) {
                //cout << "Nothing\n";
                // set default values
                YAML::Node* defaultList = interruptionManager->getDefaultInterruptionConfigsList(KEYBOARD);
                if(nothingKeyboard) setConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases", *defaultList);
                delete defaultList;

                defaultList = interruptionManager->getDefaultInterruptionConfigsList(MOUSE);
                if (nothingMouse) setConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases", *defaultList);
                delete defaultList;

                defaultList = interruptionManager->getDefaultInterruptionConfigsList(ANY_INPUT);
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
        
        vector<string> localDefaultFastForegroundWindows;
        localDefaultFastForegroundWindows.push_back("Roblox");
        localDefaultFastForegroundWindows.push_back("VMware Workstation");
        defaultFastForegroundWindows = getConfigVectorString(config, "settings/fastReturnToForegroundWindows", localDefaultFastForegroundWindows);

        // saving
        setConfigValue(config, "internal/configVersion", currentConfigVersion);
        if (!wrongConfig) saveYaml(config, getProgramFolderPath(programPath) + "/" + folderPath + "/" + fileName);
        //if (!wrongConfig) saveYaml(config, getProgramFolderPath(programPath) + "/" + folderPath + "/" + "test_" + fileName); // test
        
        initialConfigLoading = false;
        return true;
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::Exception error happened while loading the config:\nERROR | " + string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const std::exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected std::exception error happened while loading the config:\nERROR | " + string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const YAML::BadConversion& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::BadConversion error happened while loading the config:\nERROR | " + string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
}

LRESULT CALLBACK KeyboardProc(int nCode, WPARAM wParam, LPARAM lParam) {
    if (interruptionManager != nullptr && interruptionManager->getModeEnabled().load()) { // interruptionManager->getModeEnabled().load()
        if (nCode >= 0) {
            bool ignore = false;

            //cout << interruptionManager->getConfigurations(KEYBOARD)->size() << '\n';
            //cout << interruptionManager->getConfigurations(MOUSE)->size() << '\n';

            int key = reinterpret_cast<LPKBDLLHOOKSTRUCT>(lParam)->vkCode;
            if (wParam == WM_KEYDOWN || wParam == WM_SYSKEYDOWN || wParam == WM_CHAR) {

                string formattedKey;
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
                    if (keyboardHookSpecialVirtualKeyCodeToText.count(intForm) > 0) {
                        formattedKey = keyboardHookSpecialVirtualKeyCodeToText[intForm];
                    }
                }

                if (interruptionManager->checkIsInputPending(formattedKey)) ignore = true;
                
                if (!ignore) {
                    interruptionManager->setNewDelay(interruptionManager->getMacroPauseAfterKeyboardInput().load());
                    interruptionManager->checkForManyInputsOnNew(KEYBOARD);
                    interruptionManager->checkForManyInputsOnNew(ANY_INPUT);
                }

                //interruptionManager->getKeyStates()[key] = true;
                //setNewDelay(macroPauseAfterKeyboardKeyHeldDown);
            }
            else if (wParam == WM_KEYUP || wParam == WM_SYSKEYUP) {
                //interruptionManager->getKeyStates()[key] = false;
                /*interruptionManager->setNewDelay(interruptionManager->getMacroPauseAfterKeyboardInput().load());*/
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
    if (interruptionManager != nullptr && interruptionManager->getModeEnabled().load()) {
        if (nCode >= 0) {
            switch (wParam) {
            case WM_LBUTTONDOWN:
                // Left mouse button down event
                //if (untilNextMacroRetry.load() <= macroPauseAfterKeyboardKeyHeldDown) untilNextMacroRetry.store(macroPauseAfterMouseInput);
                interruptionManager->setNewDelay(interruptionManager->getMacroPauseAfterMouseInput().load());
                interruptionManager->checkForManyInputsOnNew(MOUSE);
                interruptionManager->checkForManyInputsOnNew(ANY_INPUT);
                break;
            case WM_LBUTTONUP:
                // Left mouse button up event
                interruptionManager->setNewDelay(interruptionManager->getMacroPauseAfterMouseInput().load());
                break;
            case WM_RBUTTONDOWN:
                // Right mouse button down event
                interruptionManager->setNewDelay(interruptionManager->getMacroPauseAfterMouseInput().load());
                interruptionManager->checkForManyInputsOnNew(MOUSE);
                interruptionManager->checkForManyInputsOnNew(ANY_INPUT);
                break;
            case WM_RBUTTONUP:
                // Right mouse button up event
                interruptionManager->setNewDelay(interruptionManager->getMacroPauseAfterMouseInput().load());
                break;
                // Add cases for other mouse events as needed
            }
        }
    }

    return CallNextHookEx(NULL, nCode, wParam, lParam);
}

void keyboardHookFunc() {
    if (!debugMode) {
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
    if (!debugMode) {
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

void macroDelayModificationLoop() {
    while (true) {
        // important!
        //std::cout << "";
        if (interruptionManager != nullptr && interruptionManager->getModeEnabled().load()) {
            int curValue = interruptionManager->getUntilNextMacroRetryAtomic().load();
            cout << curValue << endl;

            curValue -= 1;
            if (curValue < 0) curValue = 0;
            interruptionManager->getUntilNextMacroRetryAtomic().store(curValue);
        }
        Sleep(1000);
    }
    //std::cout << "Waining...\n";
    //cv.wait(lock, [] { return (untilNextMacroRetry.load() <= 0); });
    //std::cout << "Done waiting\n";
}

void terminationOnFailure() {
    HWND hwnd = GetConsoleWindow();
    if (hwnd) {
        if (IsIconic(hwnd)) {
            ShowWindow(hwnd, SW_RESTORE);
        }
    }

    cout << "[!!!] The application encountered an error that it doesn't want to ingnore, so it has exited." << endl;
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

int actualMain(int argc, char* argv[]) {
    // Setting error handlers for non-debug configuration
    if (true || !debugMode) {
        signal(SIGSEGV, signalHandler);
        signal(SIGFPE, signalHandler);
        SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
    }

    //throw exception("aaa");

    /*int a = 0;
    int b = 0;
    cout << a / b;*/

    setlocale(0, "");

    initializeRandom();
    loadConfig(argv[0]);
    rememberInitialPermanentSettings();
    registerHotkeys();
    printConfigLoadingMessages();

    // Listening for inputs for macro interuptions
    if (interruptionManager->getShouldStartDelayModificationLoop().load()) {
        macroDelayWatcherThread = new std::thread(macroDelayModificationLoop);
        macroDelayWatcherThread->detach();
    }
    if (interruptionManager->getShouldStartKeyboardHook().load()) {
        keyboardHookThread = new std::thread(keyboardHookFunc);
        keyboardHookThread->detach();
        //cout << "Priority " << GetThreadPriority(keyboardHook) << endl;
        //SetThreadPriority(keyboardHook, THREAD_PRIORITY_HIGHEST);
    }
    if (interruptionManager->getShouldStartMouseHook().load()) {
        mouseHookThread = new std::thread(MouseHookFunc);
        mouseHookThread->detach();
    }

    if (debugMode) cout << "[! DEBUG MODE !]\n\n";

    //UINT VKCode = LOBYTE(VkKeyScan('e'));
    //UINT ScanCode = MapVirtualKey(VKCode, 0);
    //cout << ScanCode << endl; // VkKeyScan('e') // wscanf_s(L"a")
    MSG msg;
    while (GetMessage(&msg, NULL, 0, 0) != 0) {
        TranslateMessage(&msg);
        DispatchMessage(&msg);

        if (msg.message == WM_HOTKEY) {
            checkClosedWindows();
            HWND curHwnd = GetForegroundWindow();
            //cout << "curHwnd = " << curHwnd << endl;
            //cout << msg.wParam << endl;
            if (msg.wParam == 1) {
                if (curHwnd != GetConsoleWindow()) {
                    if (lastGroup == NULL) {
                        lastGroup = new WindowGroup();
                    }
                    (*lastGroup).addWindow(curHwnd);
                    referenceToGroup[curHwnd] = lastGroup;
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
                referenceToGroup.clear();
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
                loadConfig(argv[0]);
                printConfigLoadingMessages();
            }
            // EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE EDGE
            if (referenceToGroup.count(curHwnd)) {
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
                //cout << ((&referenceToGroup==NULL) || referenceToGroup.size()) << endl;
                if ((&referenceToGroup != NULL) && referenceToGroup.size() > 0) {
                    //cout << "The group and size (make sure not console): ";
                    //if (referenceToGroup.count(curHwnd)) cout << (referenceToGroup.find(curHwnd)->second) << " " << (*(referenceToGroup.find(curHwnd)->second)).size() << endl;

                    cout << "Current HWND->WindowGroup* map:" << endl;
                    for (const auto& it : referenceToGroup) {
                        cout << it.first << " " << it.second;
                        if (it.first == curHwnd) cout << " (Current)";
                        cout << endl;
                    }
                }
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