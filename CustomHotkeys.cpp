#include <windows.h>
#include <vector>
#include <string>
#include <iostream>
#include <algorithm>
#include "WindowSwitcherNew.h"
#include "ConfigOperations.h"

struct KeybindInfo;
std::atomic<vector<KeybindInfo>*> activeKeybinds = nullptr;

vector<KeybindInfo>* getActiveKeybinds() {
    return activeKeybinds.load();
}

struct KeybindInfo {
    int id;
    string hotkey;
    string internalName;
    string description;
    bool hidden = false;

    KeybindInfo(int id, string hotkey, string internalName, string description) {
        this->id = id;
        this->hotkey = hotkey;
        this->internalName = internalName;
        this->description = description;
    }

    KeybindInfo& setHidden(bool hidden) {
        this->hidden = hidden;
        return *this;
    }

    YAML::Node* toNode() {
        YAML::Node* node = new YAML::Node();
        //setConfigValue(*node, "internalName", this->internalName);
        setConfigValue(*node, "description", this->description);
        setConfigValue(*node, "hotkey", this->hotkey);
        return node;
    }

    string toString() {
        std::stringstream ss;
        ss << "{id=" << this->id << ", internalName=" << this->internalName << ", hotkey=" << hotkey << ", description=" << description << "}";
        return ss.str();
    }
};

std::map<std::string, int> keybindingTextToKey = {
    {"a", 65},
    {"b", 66},
    {"c", 67},
    {"d", 68},
    {"e", 69},
    {"f", 70},
    {"g", 71},
    {"h", 72},
    {"i", 73},
    {"j", 74},
    {"k", 75},
    {"l", 76},
    {"m", 77},
    {"n", 78},
    {"o", 79},
    {"p", 80},
    {"q", 81},
    {"r", 82},
    {"s", 83},
    {"t", 84},
    {"u", 85},
    {"v", 86},
    {"w", 87},
    {"x", 88},
    {"y", 89},
    {"z", 90},
    {"A", 65},
    {"B", 66},
    {"C", 67},
    {"D", 68},
    {"E", 69},
    {"F", 70},
    {"G", 71},
    {"H", 72},
    {"I", 73},
    {"J", 74},
    {"K", 75},
    {"L", 76},
    {"M", 77},
    {"N", 78},
    {"O", 79},
    {"P", 80},
    {"Q", 81},
    {"R", 82},
    {"S", 83},
    {"T", 84},
    {"U", 85},
    {"V", 86},
    {"W", 87},
    {"X", 88},
    {"Y", 89},
    {"Z", 90},
    {"1", 49},
    {"2", 50},
    {"3", 51},
    {"4", 52},
    {"5", 53},
    {"6", 54},
    {"7", 55},
    {"8", 56},
    {"9", 57},
    {"0", 48},
    {"`", 192},
    {"~", 192},
    {"!", 49},
    {"@", 50},
    {"#", 51},
    {"$", 52},
    {"%", 53},
    {"^", 54},
    {"&", 55},
    {"*", 56},
    {"(", 57},
    {")", 48},
    {"-", 189},
    {"_", 189},
    {"=", 187},
    {"+", 187},
    {"[", 219},
    {"{", 219},
    {"]", 221},
    {"}", 221},
    {"\\", 220},
    {"|", 220},
    {";", 186},
    {":", 186},
    {"'", 222},
    {"\"", 222},
    {",", 188},
    {"<", 188},
    {".", 190},
    {">", 190},
    {"/", 191},
    {"?", 191},
    { "shift", VK_SHIFT },
    { "ctrl", VK_CONTROL },
    { "alt", VK_MENU },
    { "tab", VK_TAB },
    { "numpad0", VK_NUMPAD0 },
    { "numpad1", VK_NUMPAD1 },
    { "numpad2", VK_NUMPAD2 },
    { "numpad3", VK_NUMPAD3 },
    { "numpad4", VK_NUMPAD4 },
    { "numpad5", VK_NUMPAD5 },
    { "numpad6", VK_NUMPAD6 },
    { "numpad7", VK_NUMPAD7 },
    { "numpad8", VK_NUMPAD8 },
    { "numpad9", VK_NUMPAD9 },
    { "f1", VK_F1 },
    { "f2", VK_F2 },
    { "f3", VK_F3 },
    { "f4", VK_F4 },
    { "f5", VK_F5 },
    { "f6", VK_F6 },
    { "f7", VK_F7 },
    { "f8", VK_F8 },
    { "f9", VK_F9 },
    { "f10", VK_F10 },
    { "f11", VK_F11 },
    { "f12", VK_F12 },
    { "f13", VK_F13 },
    { "f14", VK_F14 },
    { "f15", VK_F15 },
    { "f16", VK_F16 },
    { "f17", VK_F17 },
    { "f18", VK_F18 },
    { "f19", VK_F19 },
    { "f20", VK_F20 },
    { "f21", VK_F21 },
    { "f22", VK_F22 },
    { "f23", VK_F23 },
    { "f24", VK_F24 },
};

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

    string key = hotKeyText.substr(i + 1, end - i);

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

string makeHkStringPretty(string& hotkey) {
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
    string hotKeyStr = info.hotkey;
    std::transform(hotKeyStr.begin(), hotKeyStr.end(), hotKeyStr.begin(), ::tolower);
    hotKeyStr.erase(std::remove(hotKeyStr.begin(), hotKeyStr.end(), '+'), hotKeyStr.end());
    info.hotkey = hotKeyStr;

    WORD hotKey = ParseHotkeyCode(hotKeyStr);
    int modifiers = ParseHotKeyModifiers(hotKeyStr);

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

vector<KeybindInfo>* getDefaultKeybinds() {
    vector<KeybindInfo>* result = new vector<KeybindInfo>;
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
    result->push_back(KeybindInfo(20, "Alt + P", "showDebugListOfLinkedWindows", "Show the debug list of the linked windows").setHidden(true));
    result->push_back(KeybindInfo(21, "Alt + \\", "someTest", "Test").setHidden(true));

    return result;
}

//vector<YAML::Node>* getDefaultKeybindsList() {
//    vector<KeybindInfo>* raw = getDefaultKeybinds();
//    vector<YAML::Node>* result = new vector<YAML::Node>();
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
    vector<KeybindInfo>* raw = getDefaultKeybinds();
    vector<KeybindInfo>* result; // I don't want to make it vector<KeybindInfo*>* and clear all of that every time
    for (KeybindInfo info : *raw) {
        YAML::Node* node = info.toNode();
        YAML::Node userNode = getConfigValue(config, "keybindings/specific/" + info.internalName);
        if (userNode.IsDefined()) {
            string userHotkey = getConfigString(userNode, "hotkey", info.hotkey);
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
        string oldConfigVersion;
        YAML::Node versionData;
        try {
            versionData = getConfigValue(config, "internal/configVersion");
            oldConfigVersion = versionData.as<string>();
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

    setConfigValue(config, "internal/configVersion", getCurrentConfigVersion());
    return config;
}

void helpGenerateKeyMap() {
    std::string allKeyboardCharacters = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ1234567890`~!@#$%^&*()-_=+[{]}\\|;:'\",<.>/?";

    for (char c : allKeyboardCharacters) {
        std::string hotKeyText(1, c);
        WORD result = ParseHotkeyCode(hotKeyText);
        cout << "{\"" << hotKeyText << "\", " << result << "},\n";
    }
}
