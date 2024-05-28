#pragma once
#include <string>
#include <Windows.h>
#include <vector>

struct KeybindInfo {
    int id;
    std::string hotkey;
    std::string internalName;
    std::string description;
    bool hidden = false;
};

std::vector<KeybindInfo>* getActiveKeybinds();
WORD ParseHotkeyCode(const std::string& hotKeyText);
bool RegisterHotKeyFromText(std::vector<std::string>& failedHotkeys, KeybindInfo& info);
YAML::Node& loadKeybindingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig);
void helpGenerateKeyMap();
