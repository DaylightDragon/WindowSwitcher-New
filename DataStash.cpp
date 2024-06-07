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
    list.push_back(L"*WindowSwitcherNew*");
    return list;
};

std::vector<std::wstring> getDefaultAllowedTobackgroundWindows() {
    std::vector<std::wstring> result;
    result.push_back(L"Roblox");
    result.push_back(L"*WindowSwitcherNew*");
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