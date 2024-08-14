#pragma once

#include "yaml-cpp/yaml.h"
#include <string>
#include <map>

//#include "Data.h"
//#include "InputRelated.h"

enum ConfigType {
    MAIN_CONFIG,
    KEYBINDS_CONFIG
};

enum InterruptionInputType {
    KEYBOARD,
    MOUSE,
    ANY_INPUT
};

struct MacroThreadInstance {};

struct MacroWindowLoopInstance {};

struct MacroThreadFullInfoInstance {
    MacroThreadInstance* threadInstance = nullptr;
    MacroWindowLoopInstance* loopInstance = nullptr;
    bool shouldApplyInitialDelay = false;
};

extern std::string defaultMacroKey;
extern std::map<std::string, int> mapOfKeys;
extern std::map<int, std::string> keyboardHookSpecialVirtualKeyCodeToText;
extern std::map<std::string, int> keybindingTextToKey;

YAML::Node getDefaultSequenceList(bool withExample);
YAML::Node getDefaultExtraKeySequences();
YAML::Node* getDefaultInterruptionConfigsList(InterruptionInputType type);
std::vector<std::wstring> getDefaultShowBackFromBackgroundList();
std::vector<std::wstring> getDefaultAllowedTobackgroundWindows();

std::string configTypeToString(ConfigType type);
inline std::string inputTypeToString(InterruptionInputType type);