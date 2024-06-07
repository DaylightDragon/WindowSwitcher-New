#pragma once

#include "yaml-cpp/yaml.h"

#include <Windows.h>
#include <string>
#include <vector>
#include <atomic>

struct KeybindInfo {
    int id;
    std::string hotkey;
    std::string internalName;
    std::string description;
    bool hidden = false;

    KeybindInfo(int id, std::string hotkey, std::string internalName, std::string description);
    KeybindInfo& setHidden(bool hidden);
    YAML::Node* toNode();
    std::string toString();
};

extern std::atomic<std::vector<KeybindInfo>*> activeKeybinds;
std::vector<KeybindInfo>* getActiveKeybinds();
WORD ParseHotkeyCode(const std::string& hotKeyText);
bool RegisterHotKeyFromText(std::vector<std::string>& failedHotkeys, KeybindInfo& info);
YAML::Node& loadKeybindingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig);
void helpGenerateKeyMap();
