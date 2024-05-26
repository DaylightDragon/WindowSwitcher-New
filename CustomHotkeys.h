#pragma once
#include <string>
#include <Windows.h>

struct KeybindInfo {
    int id;
    string hotkey;
    string internalName;
    string description;
    bool hidden = false;
};

vector<KeybindInfo>* getActiveKeybinds();
WORD ParseHotkeyCode(const std::string& hotKeyText);
bool RegisterHotKeyFromText(std::vector<std::string>& failedHotkeys, KeybindInfo& info);
YAML::Node& loadKeybindingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig);
void helpGenerateKeyMap();
