#include <windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include "WindowSwitcherNew.h"
#include "ConfigOperations.h"
#include "CustomHotkeys.h"
#include "DataStash.h"

std::atomic<std::vector<KeybindInfo>*> activeKeybinds = nullptr;

std::vector<KeybindInfo>* getActiveKeybinds() {
    return activeKeybinds.load();
}

KeybindInfo::KeybindInfo(int id, std::string hotkey, std::string internalName, std::string description) {
    this->id = id;
    this->hotkey = hotkey;
    this->internalName = internalName;
    this->description = description;
}

KeybindInfo& KeybindInfo::setHidden(bool hidden) {
    this->hidden = hidden;
    return *this;
}

YAML::Node* KeybindInfo::toNode() {
    YAML::Node* node = new YAML::Node();
    //setConfigValue(*node, "internalName", this->internalName);
    setConfigValue(*node, "description", this->description);
    setConfigValue(*node, "hotkey", this->hotkey);
    return node;
}

std::string KeybindInfo::toString() {
    std::stringstream ss;
    ss << "{id=" << this->id << ", internalName=" << this->internalName << ", hotkey=" << hotkey << ", description=" << description << "}";
    return ss.str();
}

int ParseHotKeyModifiers(const std::string& hotKeyModifiers) {
    bool hasCtrl = hotKeyModifiers.find("ctrl") != std::string::npos;
    bool hasAlt = hotKeyModifiers.find("alt") != std::string::npos;
    bool hasShift = hotKeyModifiers.find("shift") != std::string::npos;
    bool hasWin = hotKeyModifiers.find("win") != std::string::npos;

    int modifiers = 0;
    if (hasCtrl) modifiers |= MOD_CONTROL;
    if (hasAlt) modifiers |= MOD_ALT;
    if (hasShift) modifiers |= MOD_SHIFT;
    if (hasWin) modifiers |= MOD_WIN;

    // Always include MOD_NOREPEAT
    modifiers |= MOD_NOREPEAT;

    return modifiers;
}

WORD ParseHotkeyCode(const std::string& hotKeyText) {
    // skipping spaces on the right
    int i = hotKeyText.length() - 1;
    while (i >= 0 && hotKeyText[i] == ' ') {
        i--;
    }

    // finding start index
    int end = i;
    while (i >= 0 && hotKeyText[i] != ' ') {
        i--;
    }

    std::string key = hotKeyText.substr(i + 1, end - i);

    //cout << i << ' ' << end << ' ' << key << '\n';
    
    if (keybindingTextToKey.count(key) > 0) {
        return keybindingTextToKey[key];
    }
    else {
        addConfigLoadingMessage("Couldn't process hotkey \"" + hotKeyText + "\"");
        return -1;

        //if (key.length() != 1) {
        //    addConfigLoadingMessage("Couldn't parse hotkey \"" + hotKeyText + "\"");
        //    return -1;
        //}
        //char c = key[key.length() - 1];
        //// for current layout can use GetKeyboardLayout(0);
        //HKL hkl = LoadKeyboardLayout("00000409", KLF_ACTIVATE);
        //if (hkl != NULL) {
        //    //cout << "Using HKL\n";
        //    return VkKeyScanExA(c, hkl) & 0xff;
        //}
        //else {
        //    addConfigLoadingMessage("Couldn't load ENGLISH keyboard layout, ignoring hotkey \"" + hotKeyText + "\"");
        //    return -1;
        //}
    }
}

std::string capitalizeFirstLetter(const std::string& word) {
    std::string capitalizedWord = word;
    capitalizedWord[0] = std::toupper(capitalizedWord[0]);

    return capitalizedWord;
}

std::string makeHkStringPretty(std::string& hotkey) {
    std::istringstream iss(hotkey);
    std::string word;
    std::stringstream ss;

    while (iss >> word) {
        if (keybindingTextToKey.find(word) != keybindingTextToKey.end()) {
            std::string capitalizedWord = capitalizeFirstLetter(word);
            ss << capitalizedWord << " + ";
        }
        else {
            ss << word << " ";
        }
    }
    hotkey = ss.str();

    // Removing extra characters at the end
    int charsToErase = 3;
    if (charsToErase <= hotkey.length()) {
        hotkey.erase(hotkey.length() - charsToErase);
    }

    return hotkey;
}

bool RegisterHotKeyFromText(std::vector<std::string>& failedHotkeys, KeybindInfo& info) {
    std::string hotKeyStr = info.hotkey;
    // Lowercase
    std::transform(hotKeyStr.begin(), hotKeyStr.end(), hotKeyStr.begin(), ::tolower);
    // Removing pluses
    hotKeyStr.erase(std::remove(hotKeyStr.begin(), hotKeyStr.end(), '+'), hotKeyStr.end());
    info.hotkey = hotKeyStr;

    // Check disabled
    if (info.hotkey.find("disable") != std::string::npos) {
        return false;
    }

    // Parse the hotkey
    WORD hotKey = ParseHotkeyCode(info.hotkey);
    int modifiers = ParseHotKeyModifiers(info.hotkey);

    // Making it prettier
    info.hotkey = makeHkStringPretty(info.hotkey);

    //std::cout << modifiers << ' ' << hotKey << '\n';
    if (hotKey != -1) if (RegisterHotKey(NULL, info.id, modifiers, hotKey)) {
        //cout << hotKey << '\n';
        if (!info.hidden) std::cout << "Hotkey '" << info.hotkey << "': " << info.description << "\n";
        return true;
    }
    else {
        failedHotkeys.push_back(info.hotkey + " (" + info.description + ")");
        return false;
    }
}

std::vector<KeybindInfo>* getDefaultKeybinds() {
    // Current highest id is 32

    std::vector<KeybindInfo>* result = new std::vector<KeybindInfo>;
    result->push_back(KeybindInfo(1, "Alt + ,", "addToGroup", "Add window to current group"));
    result->push_back(KeybindInfo(2, "Alt + .", "deselectGroup", "Prepare for the next group"));
    result->push_back(KeybindInfo(11, "Alt + I", "selectGroup", "Edit the window's group"));
    result->push_back(KeybindInfo(3, "Alt + K", "shiftThisGroupToLeft", "Shift this group to the left"));
    result->push_back(KeybindInfo(4, "Alt + L", "shiftThisGroupToRight", "Shift this group to the right"));
    result->push_back(KeybindInfo(5, "Ctrl + Alt + K", "shiftAllGroupsToLeft", "Shift ALL groups to the left"));
    result->push_back(KeybindInfo(6, "Ctrl + Alt + L", "shiftAllGroupsToRight", "Shift ALL groups to the right"));
    result->push_back(KeybindInfo(26, "Ctrl + Shift + K", "shiftAllOtherGroupsToLeft", "Shift ALL OTHER groups to the left"));
    result->push_back(KeybindInfo(27, "Ctrl + Shift + L", "shiftAllOtherGroupsToRight", "Shift ALL OTHER groups to the right"));
    result->push_back(KeybindInfo(7, "Alt + ]", "removeFromItsGroup", "Remove current window from it's group (Critical Bug)"));
    result->push_back(KeybindInfo(8, "Ctrl + Alt + ]", "deleteItsGroup", "Delete this window's group"));
    result->push_back(KeybindInfo(15, "Ctrl + Shift + [", "deleteAllGroups", "Delete all groups"));
    result->push_back(KeybindInfo(9, "Alt + Q", "toggleVisibilityOfOppositeInItsGroup", "Toggle visibility of the opposite windows in this group (NOT SOON WIP)"));
    result->push_back(KeybindInfo(13, "Ctrl + Alt + Q", "toggleVisibilityOfEveryNonMainWindow", "Toggle visibility of all not main windows (May not work properly yet with Auto-key macro)"));
    // ctrl shift a do current alt a, but that one should leave window updating in background
    result->push_back(KeybindInfo(10, "Alt + A", "minimizeAllOrShowCurrent", "Minimize all windows or show only the current window in every group"));
    //result->push_back(KeybindInfo(18, "Ctrl + Shift + D", "setAsMainGroupWindow", "(WIP) Set the window as main in current group"));
    //result->push_back(KeybindInfo(19, "Alt + D", "swapToMainGroupWindow", "(WIP) Swap to main window in current group"));
    //result->push_back(KeybindInfo(23, "Ctrl + Alt + D", "swapToMainGroupWindowInAllGroups", "(WIP) Swap to main window in all groups"));
    result->push_back(KeybindInfo(12, "Ctrl + Alt + U", "getAllImportantWindowsFromBackground", "Get all important windows back from background"));
    result->push_back(KeybindInfo(24, "Ctrl + U", "sendCurrentForegroundToBackground", "Send current foreground window to background"));
    result->push_back(KeybindInfo(25, "Ctrl + Shift + U", "bringToForegroundByName", "Get specific windows from background by their name"));
    //result->push_back(KeybindInfo(16, "Ctrl + Shift + V", "startOrStopAdjustingNewGameWindowsToQuarters", "(WIP, NOT SOON) Start/Stop adjusting new game windows to screen quarters"));
    result->push_back(KeybindInfo(14, "Ctrl + Alt + V", "connectAllGameWindowsToQuarters", "Connect all game windows to 4 quarter groups"));
    result->push_back(KeybindInfo(28, "Ctrl + Shift + A", "bringAllConnectedWindowsToForeground", "Bring all connected windows to foreground"));
    result->push_back(KeybindInfo(17, "Alt + G", "toggleSequenceMacro", "Start/stop the automatical sequence macro for game windows"));
    result->push_back(KeybindInfo(29, "Ctrl + Alt + G", "setMacroKeyOrSequenceForGroup", "Set the macro key (or sequence) for this group"));
    result->push_back(KeybindInfo(31, "Alt + H", "reloadAllConfigs", "Reload all configs (NO KEYBINDS RELOADING FOR NOW)"));
    result->push_back(KeybindInfo(32, "Alt + /", "showHideConsole", "Show or hide the console window").setHidden(true));
    result->push_back(KeybindInfo(20, "Disabled Alt + P", "showDebugListOfLinkedWindows", "Show the debug list of the linked windows").setHidden(true));
    result->push_back(KeybindInfo(21, "Disabled Alt + \\", "someTest", "Test").setHidden(true));

    return result;
}

//std::vector<YAML::Node>* getDefaultKeybindsList() {
//    std::vector<KeybindInfo>* raw = getDefaultKeybinds();
//    std::vector<YAML::Node>* result = new std::vector<YAML::Node>();
//
//    for (auto& keybind : raw) {
//        result->push_back(*keybind.toNode());
//    }
//
//    delete raw;
//    return result;
//}

void resetActiveKeybinds() {
    std::vector<KeybindInfo>* oldPtr = activeKeybinds.load();
    if (oldPtr != nullptr) {
        delete oldPtr;
    }
    //activeKeybinds.store(nullptr);
    activeKeybinds.store(new std::vector<KeybindInfo>);
}

void readNewKeybinds(YAML::Node& config) {
    resetActiveKeybinds();
    std::vector<KeybindInfo>* raw = getDefaultKeybinds();
    std::vector<KeybindInfo>* result; // I don't want to make it std::vector<KeybindInfo*>* and clear all of that every time
    for (KeybindInfo info : *raw) {
        YAML::Node* node = info.toNode();
        YAML::Node userNode = getConfigValue(config, "keybindings/specific/" + info.internalName);
        if (userNode.IsDefined()) {
            std::string userHotkey = getConfigString(userNode, "hotkey", info.hotkey);
            setConfigValue(*node, "hotkey", userHotkey);
            info.hotkey = userHotkey;
            //cout << info.hotkey << "\n";
        }

        setConfigValue(config, "keybindings/specific/" + info.internalName, *node);
        activeKeybinds.load()->push_back(info);
        delete node;
    }

    //activeKeybinds.store(raw);
    delete raw;
}

void readDefaultKeybindsNoModification(YAML::Node& config) {
    activeKeybinds.store(getDefaultKeybinds());
}

YAML::Node& loadKeybindingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig) {
    if (!wasEmpty) {
        std::string oldConfigVersion;
        YAML::Node versionData;
        try {
            versionData = getConfigValue(config, "internal/configVersion");
            oldConfigVersion = versionData.as<std::string>();
        }
        catch (const YAML::Exception& e) {
            oldConfigVersion = "2.2";
        }
    }
        
    // if no yaml structure errors
    if (!wrongConfig) {
        // resetting main sequence and reading what config has
        readNewKeybinds(config);
    }
    else {
        readDefaultKeybindsNoModification(config);
    }

    /*for (auto& it : *activeKeybinds) {
        cout << it.toString() << '\n';
    }*/

    setConfigValue(config, "internal/configVersion", getCurrentVersion());
    return config;
}

void helpGenerateKeyMap() {
    std::string allKeyboardCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890`~!@#$%^&*()-_=+[{]}\\|;:'\",<.>/?";

    for (char c : allKeyboardCharacters) {
        std::string hotKeyText(1, c);
        WORD result = ParseHotkeyCode(hotKeyText);
        std::cout << "{\"" << hotKeyText << "\", " << result << "},\n";
    }
}
