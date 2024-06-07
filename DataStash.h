#pragma once

extern std::string defaultMacroKey;
extern std::map<std::string, int> mapOfKeys;
extern std::map<int, std::string> keyboardHookSpecialVirtualKeyCodeToText;

YAML::Node getDefaultSequenceList(bool withExample);
YAML::Node getDefaultExtraKeySequences();
YAML::Node* getDefaultInterruptionConfigsList(InterruptionInputType type);
