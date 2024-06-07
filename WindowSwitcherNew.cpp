// Window Switcher

#pragma comment(lib, "Dwmapi.lib") // for DwmGetWindowAttribute and so on

#include "WindowSwitcherNew.h"

#include "ConfigMigrations.h"
#include "ConfigOperations.h"
#include "Configuration.h"
#include "CustomHotkeys.h"
#include "DataStash.h"
#include "GeneralUtils.h"
#include "InputRelated.h"
#include "WindowOperations.h"

#include "yaml-cpp/yaml.h"

#include <condition_variable>
#include <conio.h> // ig for _getch
#include <cstdlib>
#include <csignal>
#include <dwmapi.h>
#include <map>
#include <stdio.h>
#include <thread>
#include <vector>

std::string currentVersion = "2.3";

// Pre-defining all classes and methods

// Windows, sequences and managers
std::map<HWND, WindowGroup*> handleToGroup;
std::map<WindowGroup*, KeySequence*> groupToSequence;
WindowGroup* lastGroup;
std::vector<std::string> failedHotkeys;
std::atomic<InputsInterruptionManager*> interruptionManager;
std::atomic<Settings*> settings;
RuntimeData runtimeData;

std::map<int, std::vector<HWND>> autoGroups;

// Permanent settings
std::wstring rx_name = L"Roblox";
std::string programPath;

// Runtime/status variables
std::atomic<bool> stopMacroInput = true;
bool hideNotMainWindows = false;
int currentHangWindows = 0;
std::wstring customReturnToFgName = L"";
int restoredCustomWindowsAmount = 0;
int globalGroupId = -100;
int noGroupId = -200;

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

void customSleep(int duration) {
    Sleep(randomizeValue(duration, settings.load()->sleepRandomnessPersent, settings.load()->sleepRandomnessMaxDiff));
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

std::wstring getWindowName(HWND hwnd) {
    int length = GetWindowTextLength(hwnd);
    wchar_t* buffer = new wchar_t[length + 1];
    GetWindowTextW(hwnd, buffer, length + 1);
    std::wstring ws(buffer);
    return ws; // not nullptr later?
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

        if (!settings.load()->usePrimitiveInterruptionAlgorythm) {
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
                Sleep(settings.load()->primitiveWaitInterval);
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
        for (Key& el : settings.load()->mainSequence->getKeys()) {
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
            keySeq = settings.load()->mainSequence;
            if (groupToSequence.count(wg)) keySeq = groupToSequence[wg];
        }
    }

    return keySeq;
}

bool checkCooldownActive(HWND hwnd) {
    WindowInfo* wi = getWindowInfoFromHandle(hwnd);
    if (wi == nullptr) return false;
    KeySequence* keySeq = getKeySequenceFromHandle(hwnd);
    if (keySeq == nullptr) return false;

    auto timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wi->lastSequenceInputTimestamp).count();
    bool cooldown = timePassedSince < keySeq->getCooldownPerWindow();
    //std::cout << timePassedSince << " " << keySeq->getCooldownPerWindow() << std::endl;

    //std::cout << hwnd << '\n';
    //if(cooldown) std::cout << timePassedSince << '\n';
    return cooldown;
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

bool focusAndSendSequence(HWND hwnd) { // find this
    //std::cout << "Swapped to " << hwnd << std::endl;

    /*int key = mapOfKeys[defaultMacroKey]; // 0x12
    //if(specialSingleWindowModeKeyCode)
    if (groupToKey.count(referenceToGroup[hwnd])) {
        key = groupToKey[referenceToGroup[hwnd]];
    }*/
    //std::cout << "test\n";
    //std::cout << "key: " << key << endl;

    bool anySequenceInputHappened = false;
    KeySequence* keySeq = getKeySequenceFromHandle(hwnd);

    bool shouldRestartSequence = true;
    while (shouldRestartSequence) {
        if (stopMacroInput.load()) return anySequenceInputHappened;
        waitIfInterrupted();
        if (stopMacroInput.load()) return anySequenceInputHappened;
        bool cooldown = checkCooldownActiveAndRefresh(hwnd); // can't call twice, and not two separate funcs to not look for objects again
        //std::cout << cooldown << std::endl;
        if (cooldown) {
            if (keySeq != nullptr) {
                customSleep(keySeq->getCheckEveryOnWait());
            }
            else {
                customSleep(max(10, min(100, settings.load()->macroDelayAfterFocus)));
            }

            // behaviour for staying at that single window till it's out of cooldown
            //continue;
            
            //std::cout << "hadCooldownOnPrevWindow: " << runtimeData.hadCooldownOnPrevWindow.load() << '\n';
            if (!runtimeData.hadCooldownOnPrevWindow.load()) {
                //runtimeData.activatePrevActiveWindow();
            }
            runtimeData.hadCooldownOnPrevWindow.store(cooldown);
            // original intended behaviour
            return anySequenceInputHappened;
        }
        runtimeData.hadCooldownOnPrevWindow.store(cooldown);

        if (stopMacroInput.load()) return anySequenceInputHappened;
        waitIfInterrupted();
        if (stopMacroInput.load()) return anySequenceInputHappened;

        runtimeData.saveCurrentNonLinkedForgroundWindow();
        SetForegroundWindow(hwnd);
        customSleep(settings.load()->macroDelayBetweenSwitchingAndFocus);
        SetFocus(hwnd);

        customSleep(settings.load()->macroDelayAfterFocus);

        shouldRestartSequence = performASequence(hwnd);
        anySequenceInputHappened = true;
    }

    return anySequenceInputHappened;
}

bool performInputsEverywhere() {
    bool anySequenceInputHappened = false;
    for (auto& it : handleToGroup) {
        if (stopMacroInput.load()) return true;
        if (focusAndSendSequence(it.first)) anySequenceInputHappened = true;
        customSleep(settings.load()->macroDelayBeforeSwitching);
    }
    return anySequenceInputHappened;
}

void startUsualMacroLoop() {
    bool prev = false;

    customSleep(settings.load()->macroDelayInitial);
    while (!stopMacroInput.load()) {
        //for (int i = 0x5; i <= 0x30; i++) { // 5A
            //if (stopInput) return;
            //std::cout << std::hex << key << endl;
            //std::cout << std::dec;
            //key = i;
        /*bool anyOnCooldown = false;
        for (auto& it : handleToGroup) {
            if (checkCooldownActive(it.first)) {
                anyOnCooldown = true;
            }
        }*/
        //if (!stopMacroInput.load()) return;
        bool anySequenceInputHappened = performInputsEverywhere();
        //std::cout << "old: " << prev << ", new: " << anySequenceInputHappened << '\n';
        if (anySequenceInputHappened && !prev) {
            runtimeData.activatePrevActiveWindow();
            //std::cout << "Changed, activating\n";
        }
        prev = anySequenceInputHappened;
        customSleep(settings.load()->delayWhenDoneAllWindows);
        // 8000
        /*for (int i = 0; i < 100; i++) {
            //customSleep(160);
            customSleep(50);
            if (stopInput) return;
        }*/
    }
}

void startUsualSequnceMode() {
    interruptionManager.load()->getUntilNextMacroRetryAtomic().store(0);
    for (auto& pair : handleToGroup) {
        WindowInfo* wi = getWindowInfoFromHandle(pair.first);
        if(wi != nullptr) {
            wi->lastSequenceInputTimestamp = std::chrono::steady_clock::time_point();
        }
    }

    currentMacroLoopThread = new std::thread(startUsualMacroLoop);
}

void performSingleWindowedHold() {
    HWND w = GetForegroundWindow();
    SetForegroundWindow(w);
    customSleep(settings.load()->macroDelayBetweenSwitchingAndFocus);
    SetFocus(w);
    customSleep(settings.load()->macroDelayAfterFocus);
    std::string key = settings.load()->specialSingleWindowModeKeyCode;
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
    customSleep(settings.load()->macroDelayBetweenSwitchingAndFocus);
    SetFocus(w);
    customSleep(settings.load()->macroDelayAfterFocus);
    std::string key = settings.load()->specialSingleWindowModeKeyCode;
    if (groupToSequence.count(handleToGroup[w])) {
        std::vector<Key> allKeys = groupToSequence[handleToGroup[w]]->getKeys();
        if (allKeys.size() > 0) key = allKeys[0].keyCode;
    }
    keyReleaseString(key);
}

void toggleKeySequenceMacroState() {
    if (handleToGroup.size() == 0) {
        std::cout << "You haven't linked any windows yet!\n";
    }
    else if (stopMacroInput.load()) {
        stopMacroInput.store(false);
        std::cout << "Starting...\n";
        if (settings.load()->specialSingleWindowModeEnabled && handleToGroup.size() == 1) {
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

static BOOL CALLBACK fgCustomWindowCallback(HWND hwnd, LPARAM lparam) {
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
    EnumWindows(fgCustomWindowCallback, NULL);
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
            groupToSequence[handleToGroup[h]] = settings.load()->knownOtherSequences[keyName.substr(1)];
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
    std::cout << hwnd << std::endl;
    return;

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
                toggleKeySequenceMacroState();
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