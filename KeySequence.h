#pragma once
#include <vector>
#include <string>
#include <yaml-cpp/yaml.h>
#include "KeySequence.cpp"

/*struct Key {
public:
    std::string keyCode;
    bool enabled;
    int beforeKeyPress;
    int holdFor;
    int afterKeyPress;
    Key();
    Key(std::string keyCode, bool enabled, int beforeKeyPress, int holdFor, int afterKeyPress);
    Key(std::string keyCode);
};

class KeySequence {
public:
    KeySequence();
    KeySequence(std::vector<Key> &keys);
    KeySequence(const YAML::Node &node);
    KeySequence(std::string singleKeySimple);
    ~KeySequence();
    std::vector<Key> getKeys();
};*/