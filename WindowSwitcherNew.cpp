// Window Switcher

#include "WindowSwitcherNew.h"

#pragma comment(lib, "Dwmapi.lib") // for DwmGetWindowAttribute and so on
#pragma comment (lib, "gdiplus.lib")

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
#include <tchar.h>
#include <shared_mutex>
//#include <objidl.h>
#include <gdiplus.h>

#include "ConfigOperations.h"
#include "gui/ConsoleManagement.h"
#include "InputRelated.h"
#include "GeneralUtils.h"
#include "CustomHotkeys.h"
#include "WindowRelated.h"
#include "WindowOperations.h"
#include "Data.h"
#include "DataStash.h"
#include "ConfigMigrations.h"
#include "Overlay.h"
//#include "gui/MainUi.cpp"

std::string currentVersion = "2.3";

// Pre-defining all classes and methods

// Windows, sequences and managers
std::map<HWND, WindowGroup*> handleToGroup;
//std::shared_mutex mapMutex; // MAY CAUSE CRASHES ON START!!!
std::map<WindowGroup*, KeySequence*> groupToSequence;
WindowGroup* lastGroup;
KeySequence* mainSequence;
std::map<std::string, KeySequence*> knownOtherSequences = std::map<std::string, KeySequence*>();
std::vector<std::string> failedHotkeys;
std::atomic<InputsInterruptionManager*> interruptionManager;
std::atomic<WindowSwitcher::Settings*> settings;
WindowSwitcher::RuntimeData runtimeData;

std::map<int, std::vector<HWND>> autoGroups;

// Permanent settings
std::wstring rx_name = L"Roblox";
//std::string programPath;
char programPath[MAX_PATH];

// Main misc settings variables
int macroDelayInitial;
int macroDelayBeforeSwitching;
int macroDelayBetweenSwitchingAndFocus;
int macroDelayAfterFocus;
int delayWhenDoneAllWindows;
int macroDelayAfterKeyPress;
int macroDelayAfterKeyRelease;
bool specialSingleWindowModeEnabled;
std::string specialSingleWindowModeKeyCode;
bool usePrimitiveInterruptionAlgorythm = false;
int primitiveWaitInterval = 100;

// Input randomness
int sleepRandomnessPersent = 10;
int sleepRandomnessMaxDiff = 40;

// Runtime/status variables
std::atomic<bool> stopMacroInput = true;
std::atomic<bool> currentlyWaitingOnInterruption = false;
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
std::atomic<bool> stopAllThreads = false;

// Advanced macro interruptions
std::condition_variable macroWaitCv;
std::mutex* macroWaitMutex = nullptr;
std::unique_lock<std::mutex>* macroWaitLock = nullptr;

// Overlay
std::atomic<float> overlayValue = -1;
std::atomic<int> overlayActiveStateFullAmount;
std::atomic<int> overlayActiveStateCurrentAmount;
std::atomic<bool> globalCooldownWaveRn = false;

// Debug related
bool debugMode = IsDebuggerPresent();
bool registerKeyboardHookInDebug = true;
bool registerKeyboardMouseInDebug = true;

// Single instances of stuff
std::atomic<MacroThreadInstance*> macroThreadInstancePtr = nullptr;
std::atomic<MacroWindowLoopInstance*> macroWindowLoopInstancePtr = nullptr;
std::atomic<MacroThreadFullInfoInstance*> currentMacroThreadFullInfoPrt = nullptr;

//#include <AppCore/App.h>

//ultralight::RefPtr<ultralight::App> app_;

//std::string mainConfigName = "WsSettings/settings.yml";

bool getDebugMode() {
    return debugMode;
}

std::string getCurrentVersion() {
    return currentVersion;
}

std::string getProgramPath() {
    return programPath;
}

float getOverlayValue() {
    return overlayValue.load();
}

int getOverlayActiveStateFullAmount() {
    return overlayActiveStateFullAmount.load();
}

int getOverlayActiveStateCurrentAmount() {
    return overlayActiveStateCurrentAmount.load();
}

//std::shared_mutex* getMapMutex() {
//    return &mapMutex;
//}

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
    int failsExitThreshold = 10;

    std::vector<KeybindInfo>* keybinds = getActiveKeybinds();
    for (KeybindInfo info : *keybinds) {
        RegisterHotKeyFromText(failedHotkeys, info);
    }

    if (failedHotkeys.size() > 0) {
        std::cout << "\nFailed to register " << failedHotkeys.size() << " hotkey";
        if (failedHotkeys.size() > 1) std::cout << "s";
        std::cout << ":\n";

        for (auto& elem : failedHotkeys) {
            std::cout << "[!] Failed to load " << elem << "\n";
        }

        if (failedHotkeys.size() >= failsExitThreshold) { // that value can be not updated in time
            std::cout << "WARNING | Most likely you have started multiple instances of this programm, sadly you can use only one at a time\nIf the issue is just overlapping hotkeys, fix that in the keybinds config and restart the app";
            if(isConsoleAllocated()) _getch();
            ExitProcess(0);
        }
    }

    std::cout << std::endl;
}

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
    currentlyWaitingOnInterruption.store(interrupted);

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

std::pair<OverlayState, float> getCurrentCooldownPartitionForWindow(HWND hwnd) {
    WindowInfo* wi = getWindowInfoFromHandle(hwnd);
    if (wi == nullptr) return std::pair<OverlayState, float>(NOT_INITIALIZED, 3);
    KeySequence* keySeq = getKeySequenceFromHandle(hwnd);
    if (keySeq == nullptr) return std::pair<OverlayState, float>(NOT_INITIALIZED, 3);

    auto timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wi->lastSequenceInputTimestamp).count(); // here
    bool cooldown = timePassedSince < keySeq->getCooldownPerWindow();
    if (cooldown) {
        return std::pair<OverlayState, float>(NORMAL, 1 - static_cast<float>(timePassedSince) / static_cast<float>(keySeq->getCooldownPerWindow()));
    }
    else return std::pair<OverlayState, float>(SEQUENCE_ACTIVE, 0);
}

std::pair<OverlayState, float> getTheLeastCooldownPartition() {
    float leastValue = 2;
    OverlayState itsState = NOT_INITIALIZED;
    //std::shared_lock<std::shared_mutex> lock(mapMutex); // MAY CAUSE CRASHES!!!
    for (auto& it : handleToGroup) {
        std::pair<OverlayState, float> newValue = getCurrentCooldownPartitionForWindow(it.first);
        if (newValue.second < leastValue) {
            leastValue = newValue.second;
            itsState = newValue.first;
        }
    }

    return std::pair<OverlayState, float>(itsState, leastValue);
}

bool isGlobalCooldownActive() {
    long long timePassedSince;
    timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - globalLastSequenceInputTimestamp).count();
    //std::cout << "Global passed: " << timePassedSince << "\n";

    bool cooldown = timePassedSince < settings.load()->globalTimerCooldownValue;

    return cooldown;
}

void setNewOverlayValue() {
    if (stopMacroInput.load()) {
        overlayValue.store(0);
        setOverlayState(MACRO_DISABLED);
        redrawOverlay();
        return;
    }

    std::pair<OverlayState, float> leastValue;
    if (settings.load()->useSingleGlobalCooldownTimer && isGlobalCooldownActive()) {
        long long timePassed = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - globalLastSequenceInputTimestamp).count();
        leastValue = std::pair<OverlayState, float>(NORMAL, 1 - static_cast<float>(timePassed) / static_cast<float>(settings.load()->globalTimerCooldownValue));
    }
    else leastValue = getTheLeastCooldownPartition();

    if (leastValue.first == SEQUENCE_ACTIVE) {
        //std::cout << stopMacroInput.load() << " " << currentlyWaitingOnInterruption.load() << "\n";
    }
    if (leastValue.first == SEQUENCE_ACTIVE && (stopMacroInput.load() || currentlyWaitingOnInterruption.load())) {
        leastValue.first = SEQUENCE_READY;
        leastValue.second = interruptionManager.load()->getUntilNextMacroRetryAtomic().load();
    }

    overlayValue.store(leastValue.second);
    setOverlayState(leastValue.first);

    redrawOverlay(); 
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

bool shouldStopMacroThread(MacroThreadFullInfoInstance* ci) {
    //std::cout << stopAllThreads.load() << " " << stopMacroInput.load() << " " << (ci->threadInstance != macroThreadInstancePtr.load()) << "\n";
    if (stopAllThreads.load()) return true;
    if (stopMacroInput.load()) return true;
    if (ci->threadInstance != macroThreadInstancePtr.load()) return true;
    return false;
}

bool shouldStopMacroWindowLoop(MacroThreadFullInfoInstance* ci) {
    if (shouldStopMacroThread(ci)) return true;
    if (ci->loopInstance != macroWindowLoopInstancePtr.load()) return true;
    return false;
}

bool pressAndUnpressAKey(HWND w, Key k, MacroThreadFullInfoInstance* ci) { // returns true if was paused and needs to repeat the cycle
    if (!k.enabled || k.keyCode == "EXAMPLE") return false;
    if (shouldStopMacroWindowLoop(ci)) return false;
    if (waitIfInterrupted() || notTargetWindowActive(w)) return true;

    customSleep(k.beforeKeyPress);
    if (shouldStopMacroWindowLoop(ci)) return false;
    if (waitIfInterrupted() || notTargetWindowActive(w)) return true;
    interruptionManager.load()->addPendingSentInput(k.keyCode); // BEFORE actually pressing! Or the handler will get it before it's added
    keyPressInput(mapOfKeys[k.keyCode]);
    
    customSleep(k.holdFor);
    if (shouldStopMacroWindowLoop(ci)) return false;
    if (waitIfInterrupted() || notTargetWindowActive(w)) return true;
    keyReleaseInput(mapOfKeys[k.keyCode]);

    customSleep(k.afterKeyPress);
}

bool performASequence(HWND w, MacroThreadFullInfoInstance* ci) {
    bool shouldRestartSequence = false;
    if (groupToSequence.count(handleToGroup[w])) {
        std::vector<Key> keys = groupToSequence[handleToGroup[w]]->getKeys();
        if (keys.size() > 0) {
            //setNewOverlayValue();
            for (auto& el : keys) {
                if (shouldStopMacroWindowLoop(ci)) return false;
                shouldRestartSequence = pressAndUnpressAKey(w, el, ci);
                if (shouldRestartSequence) break;
            }
        }
        else {
            std::cout << "You can't have no actions in a sequence! Waiting a bit instead\n"; // kostil
            customSleep(100);
        }
    }
    else {
        //setNewOverlayValue();
        for (auto& el : mainSequence->getKeys()) {
            if (shouldStopMacroWindowLoop(ci)) return false;
            //std::cout << "Pressing\n";
            shouldRestartSequence = pressAndUnpressAKey(w, el, ci);
            // The macro has been interrupted and paused, and now we need to restart the entire sequence
            if (shouldRestartSequence) break;
        }
    }
    return shouldRestartSequence;
}

// For the overlay data
bool checkCooldownActive(HWND hwnd) {
    WindowInfo* wi = getWindowInfoFromHandle(hwnd);
    if (wi == nullptr) return false;
    KeySequence* keySeq = getKeySequenceFromHandle(hwnd);
    if (keySeq == nullptr) return false;

    bool singleTimer = settings.load()->useSingleGlobalCooldownTimer;

    long long timePassedSince;
    if (singleTimer) timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - globalLastSequenceInputTimestamp).count();
    else timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wi->lastSequenceInputTimestamp).count();

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

    bool singleTimer = settings.load()->useSingleGlobalCooldownTimer;

    if (singleTimer) return false;

    long long timePassedSince = 0;
    //if(singleTimer) timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - globalLastSequenceInputTimestamp).count();
    if(!singleTimer) timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(std::chrono::steady_clock::now() - wi->lastSequenceInputTimestamp).count();

    bool cooldown;
    if (!singleTimer) cooldown = timePassedSince < keySeq->getCooldownPerWindow();
    //std::cout << timePassedSince << " " << keySeq->getCooldownPerWindow() << std::endl;

    //std::cout << hwnd << '\n';
    //if(cooldown) std::cout << timePassedSince << '\n';
    if (!cooldown) {
        if(!singleTimer) wi->refreshTimestamp(); 
        //globalLastSequenceInputTimestamp = std::chrono::steady_clock::now();
        //else wi->refreshTimestamp();
    }
    return cooldown;
}

bool focusAndSendSequence(HWND hwnd, MacroThreadFullInfoInstance* ci) { // find this
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
        if (shouldStopMacroWindowLoop(ci)) return anySequenceInputHappened;
        //setNewOverlayValue();
        waitIfInterrupted();
        if (shouldStopMacroWindowLoop(ci)) return anySequenceInputHappened;
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
            
            //std::cout << "hadCooldownOnPrevWindow: " << runtimeData.hadCooldownOnPrevWindow.load() << '\n';
            if (!runtimeData.hadCooldownOnPrevWindow.load()) {
                //runtimeData.activatePrevActiveWindow();
            }
            runtimeData.hadCooldownOnPrevWindow.store(cooldown);
            // original intended behaviour
            return anySequenceInputHappened;
        }
        runtimeData.hadCooldownOnPrevWindow.store(cooldown);

        if (shouldStopMacroWindowLoop(ci)) return anySequenceInputHappened;
        waitIfInterrupted();
        if (shouldStopMacroWindowLoop(ci)) return anySequenceInputHappened;

        runtimeData.saveCurrentNonLinkedForgroundWindow();
        SetForegroundWindow(hwnd);
        customSleep(macroDelayBetweenSwitchingAndFocus);
        SetFocus(hwnd);

        customSleep(macroDelayAfterFocus);

        //std::cout << "PerformASequence\n";
        shouldRestartSequence = performASequence(hwnd, ci);
        anySequenceInputHappened = true;
    }

    return anySequenceInputHappened;
}

bool performInputsEverywhere(MacroThreadFullInfoInstance* ci) {
    bool anySequenceInputHappened = false;
    overlayActiveStateFullAmount.store(handleToGroup.size());
    overlayActiveStateCurrentAmount.store(1);

    bool fine = false;
    if (!settings.load()->useSingleGlobalCooldownTimer) {
        fine = true;
    }
    if (settings.load()->useSingleGlobalCooldownTimer && !isGlobalCooldownActive()) {
        fine = true;
        globalCooldownWaveRn.store(true);
    }

    //std::cout << "Fine: " << fine << "\n";

    if (fine) {
        MacroWindowLoopInstance* origLoopPrt = new MacroWindowLoopInstance();
        macroWindowLoopInstancePtr.store(origLoopPrt);
        ci->loopInstance = origLoopPrt;

        for (auto& it : handleToGroup) {
            if (shouldStopMacroWindowLoop(ci)) return true;
            if (!IsWindow(it.first)) {
                handleToGroup.erase(it.first); // MAY CAUSE PROBLEMS, idk
                return anySequenceInputHappened;
            }
            if (checkHungWindow(it.first)) {
                return anySequenceInputHappened;
            }
            //std::cout << "Sequence\n";
            if (focusAndSendSequence(it.first, ci)) anySequenceInputHappened = true;
            customSleep(macroDelayBeforeSwitching);

            overlayActiveStateCurrentAmount.store(overlayActiveStateCurrentAmount.load() + 1);
        }
        if (globalCooldownWaveRn.load()) {
            globalLastSequenceInputTimestamp = std::chrono::steady_clock::now();
        }
        globalCooldownWaveRn.store(false);

        delete origLoopPrt;
        macroWindowLoopInstancePtr.store(nullptr);
    }
    return anySequenceInputHappened;
}

void startUsualMacroLoop() {
    bool prev = false;

    customSleep(macroDelayInitial);

    MacroThreadInstance* origThreadPtr = new MacroThreadInstance();
    macroThreadInstancePtr.store(origThreadPtr);
    MacroThreadFullInfoInstance* ci = new MacroThreadFullInfoInstance{macroThreadInstancePtr.load(), nullptr};
    currentMacroThreadFullInfoPrt.store(ci);

    while (!shouldStopMacroThread(ci)) {
        if (ci->shouldApplyInitialDelay) {
            customSleep(macroDelayInitial);
            ci->shouldApplyInitialDelay = false;
        }
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
        bool anySequenceInputHappened = performInputsEverywhere(ci);
        //std::cout << "old: " << prev << ", new: " << anySequenceInputHappened << '\n';
        if (anySequenceInputHappened && !prev) {
            runtimeData.activatePrevActiveWindow();
            //std::cout << "Changed, activating\n";
        }
        prev = anySequenceInputHappened;
        customSleep(delayWhenDoneAllWindows);
        // 8000
        /*for (int i = 0; i < 100; i++) {
            //customSleep(160);
            customSleep(50);
            if (stopInput) return;
        }*/
    }

    delete origThreadPtr;
    delete ci;
    currentMacroThreadFullInfoPrt.store(nullptr);

    macroThreadInstancePtr.store(nullptr);
}

void resetTimersToOld() {
    for (auto& pair : handleToGroup) {
        WindowInfo* wi = getWindowInfoFromHandle(pair.first);
        if (wi != nullptr) {
            wi->lastSequenceInputTimestamp = std::chrono::steady_clock::time_point();
        }
    }
    globalLastSequenceInputTimestamp = std::chrono::steady_clock::time_point();
}

void resetTimersToCurrent() {
    for (auto& pair : handleToGroup) {
        WindowInfo* wi = getWindowInfoFromHandle(pair.first);
        if (wi != nullptr) {
            wi->lastSequenceInputTimestamp = std::chrono::steady_clock::now();
        }
    }
    globalLastSequenceInputTimestamp = std::chrono::steady_clock::now();
}

void startUsualSequnceMode() {
    interruptionManager.load()->getUntilNextMacroRetryAtomic().store(0);
    resetTimersToOld();

    currentMacroLoopThread = new std::thread(startUsualMacroLoop);
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

void toggleKeySequenceMacroState() {
    if (handleToGroup.size() == 0 && stopMacroInput.load()) {
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
    std::wstring ws = getWindowName(hwnd);

    if (ws == rx_name) { // ws == L"HangApp"
        distributeRxHwndsToGroups(hwnd);
    }

    //EnumChildWindows(hwnd, enumWindowCallback, NULL); //TODO ?
    return TRUE;
}

static BOOL CALLBACK rxConnectivityCompletelyAllCallback(HWND hwnd, LPARAM lparam) {
    std::wstring ws = getWindowName(hwnd);

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
    std::cout << "Connecting the windows...\n";
    
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
    std::cout << "Connecting absolutely all specified windows...\n";
    autoGroups[globalGroupId].clear();
    currentHangWindows = 0;
    EnumWindows(rxConnectivityCompletelyAllCallback, NULL);
    if (currentHangWindows > 0) std::cout << "Found " << currentHangWindows << " window(s) that isn't (aren't) responding, skipping them\n";
    int amount = createConnectedGroupsForCompletelyAll();
    if (amount > 0) {
        std::cout << "Successfully connected " << amount << " window";
        if (amount > 1) std::cout << "s";
        std::cout << std::endl;
    }
    else {
        std::cout << "Didn't find any specified windows!" << std::endl;
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
    if (h != FindWindow("Shell_TrayWnd", NULL) && !checkHungWindow(h) && h != GetConsoleWindow()) {
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
    settings = new WindowSwitcher::Settings(config);
}

void rememberInitialPermanentSettings() {
    oldConfigValue_startAnything = interruptionManager.load()->getShouldStartAnything().load();
    oldConfigValue_startKeyboardHook = interruptionManager.load()->getShouldStartKeyboardHook().load();
    oldConfigValue_startMouseHook = interruptionManager.load()->getShouldStartMouseHook().load();
}

void readSimpleInteruptionManagerSettings(const YAML::Node& config) {
    interruptionManager.load()->setMacroPauseAfterKeyboardInput(getConfigInt(config, "settings/macro/interruptions/keyboard/startWithDelay_seconds", 8));
    interruptionManager.load()->setMacroPauseAfterMouseInput(getConfigInt(config, "settings/macro/interruptions/mouse/startWithDelay_seconds", 4));
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
    interruptionManager.load()->setShouldStartTimerThread(getConfigBool(config, "settings/requiresApplicationRestart/threads/timersThread", true));
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartTimerThread(false);

    interruptionManager.load()->setShouldStartDelayAndTimerThread(interruptionManager.load()->getShouldStartKeyboardHook().load() || interruptionManager.load()->getShouldStartMouseHook().load());
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartDelayAndTimerThread(false);

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

    interruptionManager.load()->initType(*getDefaultInterruptionConfigsList(KEYBOARD), KEYBOARD);
    interruptionManager.load()->initType(*getDefaultInterruptionConfigsList(MOUSE), MOUSE);
    interruptionManager.load()->initType(*getDefaultInterruptionConfigsList(ANY_INPUT), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);
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
            YAML::Node* defaultList = getDefaultInterruptionConfigsList(KEYBOARD);
            if (nothingKeyboard) setConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases", *defaultList);
            delete defaultList;

            defaultList = getDefaultInterruptionConfigsList(MOUSE);
            if (nothingMouse) setConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases", *defaultList);
            delete defaultList;

            defaultList = getDefaultInterruptionConfigsList(ANY_INPUT);
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
            config = loadYaml(programPath, folderPath, fileName, wrongConfig, configTypeToString(type));
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
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::Exception error happened while loading " + configTypeToString(type) + " config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const std::exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected std::exception error happened while loading " + configTypeToString(type) + " config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const YAML::BadConversion& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::BadConversion error happened while loading " + configTypeToString(type) + " config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
}

void reloadConfigs() {
    loadConfig(MAIN_CONFIG);
    loadConfig(KEYBINDS_CONFIG);
    repositionTheOverlay();
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

void macroDelayModificationAndTimers() {
    while (true) {
        // important! // ??
        if (interruptionManager != nullptr) {
            if (interruptionManager.load()->isModeEnabled().load()) {
                int curValue = interruptionManager.load()->getUntilNextMacroRetryAtomic().load();
                //std::cout << curValue << endl;

                curValue -= 1;
                if (curValue == 0) {
                    storeCurDelay(curValue);
                    notifyTheMacro();
                    //interruptedRightNow.store(false);
                    if (interruptionManager.load()->getInformOnEvents() && !getStopMacroInput().load()) std::cout << "Unpaused\n";
                }
                else {
                    storeCurDelay(curValue);
                }

            }
            else if (getOverlayVisibility()) {
                setNewOverlayValue();
            }
        }

        Sleep(1000);
    }
    //std::cout << "Waining...\n";
    //cv.wait(lock, [] { return (untilNextMacroRetry.load() <= 0); });
    //std::cout << "Done waiting\n";
}

void stopCurrentWindowLoop() {
    macroWindowLoopInstancePtr.store(nullptr);
    interruptionManager.load()->getUntilNextMacroRetryAtomic().store(0);
    resetTimersToCurrent();
}

void skipToNextWindowLoop() {
    macroWindowLoopInstancePtr.store(nullptr);
    interruptionManager.load()->getUntilNextMacroRetryAtomic().store(0);
    resetTimersToOld();
    if (currentMacroThreadFullInfoPrt.load() != nullptr) currentMacroThreadFullInfoPrt.load()->shouldApplyInitialDelay = true;
}

void testHotkey() {
    /*HWND hwnd = GetForegroundWindow();

    RECT windowSize = { NULL };
    DwmGetWindowAttribute(hwnd, DWMWA_EXTENDED_FRAME_BOUNDS, &windowSize, sizeof(RECT));
    std::cout << windowSize.right - windowSize.left << " " << windowSize.bottom - windowSize.top << std::endl;*/

    // The number of cells in each row/column of the grid
    HWND hwnd = GetForegroundWindow();
    std::cout << hwnd << " (the foreground window) is a hang up window: " << checkHungWindow(hwnd) << std::endl;
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
    if (isConsoleAllocated()) _getch();
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

Gdiplus::GdiplusStartupInput gdiplusStartupInput;
ULONG_PTR gdiplusToken;

//#include <optional> // TESTING THAT C++ 17 WORKS YAY
//std::optional<int> testVar;

int actualMain(HINSTANCE hInstance) {
    // Setting error handlers for non-debug configuration
    if (true || !debugMode) {
        signal(SIGSEGV, signalHandler);
        signal(SIGFPE, signalHandler);
        SetUnhandledExceptionFilter(&UnhandledExceptionHandler);
    }
    initConsole();

    /*HWND consoleWindow = GetConsoleWindow();
    if (consoleWindow == NULL)
    {
        AllocConsole();
    }*/

    //std::cout << testVar.has_value() << std::endl;
    //std::cout << __cplusplus << std::endl;

    GetModuleFileName(NULL, programPath, MAX_PATH);
    //std::cout << "Path to the executable file: " << std::endl << programPath << std::endl;

    initSomeValues();

    //throw exception("aaa");
    /*int a = 0;
    int b = 0;
    std::cout << a / b;*/

    printTitle();

    //app_ = ultralight::App::Create();

    setlocale(0, "");
    initializeRandom();
    reloadConfigs();
    rememberInitialPermanentSettings();
    registerHotkeys();
    printConfigLoadingMessages();
    //WindowSwitcher::startUiApp();

    // Инициализируем GDI+
    GdiplusStartup(&gdiplusToken, &gdiplusStartupInput, NULL);
    HWND timeOverlayWindow = CreateOverlayWindow(hInstance, _T("WSBarOverlayWindow"), 200);
    //DrawScale(timeOverlayWindow, 1);

    //helpGenerateKeyMap();

    // Listening for inputs for macro interuptions
    if (interruptionManager.load()->getShouldStartDelayAndTimerThread().load()) {
        macroDelayWatcherThread = new std::thread(macroDelayModificationAndTimers);
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
                //testHotkey();
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
            else if (msg.wParam == 32) {
                //std::cout << "Testing..." << std::endl;
                showOrHideConsole();
            }
            else if (msg.wParam == 33) {
                toggleOverlayVisibility();
            }
            else if (msg.wParam == 34) {
                //WindowSwitcher::showUiWindows();
            }
            else if (msg.wParam == 35) {
                skipToNextWindowLoop();
            }
            else if (msg.wParam == 36) {
                stopCurrentWindowLoop();
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

                    std::cout << "\n";
                    std::cout << "                Addresses:\n";
                    std::cout << "      Window        |      Window Group\n";
                    std::cout << "-----------------------------------------\n";
                    for (const auto& it : handleToGroup) {
                        std::cout << it.first << "    |    " << it.second;
                        if (it.first == curHwnd) std::cout << " (Focused window)";
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

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow)
{
    if (debugMode) {
        actualMain(hInstance);
    }
    else {
        try {
            actualMain(hInstance);
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

int main(int argc, char* argv[]) {
    
}