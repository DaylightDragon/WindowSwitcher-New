#pragma once
#include <string>
#include <Windows.h>
#include <vector>

struct KeybindInfo {
    int id;
    std::string hotkey;
    std::string internalName;
    std::string description;
    bool modifierOnKeyUp = false;
    bool hidden = false;
    bool disabledByDefault = false;

    KeybindInfo(int id, std::string hotkey, std::string internalName, std::string description);

    KeybindInfo& setModdifierOnKeyUp(bool onKeyUp);

    KeybindInfo& setHidden(bool hidden);
    KeybindInfo& setDisabledByDefault(bool disabled);

    YAML::Node* toNode();

    std::string toString();
};

extern std::atomic<std::vector<KeybindInfo>*> activeKeybinds;
std::vector<KeybindInfo>* getActiveKeybinds();
WORD ParseHotkeyCode(const std::string& hotKeyText);
bool RegisterHotKeyFromText(std::vector<std::string>& failedHotkeys, KeybindInfo& info);
YAML::Node& loadKeybindingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig);
void helpGenerateKeyMap();
