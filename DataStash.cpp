#include "InputRelated.h"

#include "yaml-cpp/yaml.h"
#include "ConfigOperations.h"
#include "DataStash.h"

std::string defaultMacroKey = "e";

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

YAML::Node* getDefaultInterruptionConfigsList(InterruptionInputType type) {
    YAML::Node* list = new YAML::Node(YAML::NodeType::Sequence);

    ManyInputsConfiguration* conf = nullptr;
    if (type == KEYBOARD) conf = new ManyInputsConfiguration(5, 5000, 14);
    else if (type == MOUSE) conf = new ManyInputsConfiguration(5, 5000, 8);
    else if (type == ANY_INPUT) {}
    else {
        addConfigLoadingMessage("unsupported type in getDefaultInterruptionConfigsList");
        return list;
    }

    if (conf != nullptr) {
        list->push_back(*conf->toNode());
        delete conf;
    }
    return list;
}

// made this public because of 2.2 -> 2.3 config migration
// can move such things to a separate file for initializations
std::vector<std::wstring> getDefaultShowBackFromBackgroundList() {
    std::vector<std::wstring> list;
    list.push_back(L"Roblox");
    list.push_back(L"VMware Workstation");
    //list.push_back(L"*WindowSwitcherNew*");
    return list;
};

std::vector<std::wstring> getDefaultAllowedTobackgroundWindows() {
    std::vector<std::wstring> result;
    result.push_back(L"Roblox");
    //result.push_back(L"*WindowSwitcherNew*");
    return result;
}

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

inline std::string inputTypeToString(InterruptionInputType type) {
    switch (type) {
    case KEYBOARD:
        return "Keyboard";
    case MOUSE:
        return "Mouse";
    case ANY_INPUT:
        return "Any input";
    default:
        return "Unknown";
    }
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