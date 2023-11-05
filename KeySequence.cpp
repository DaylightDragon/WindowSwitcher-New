#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include "WindowSwitcherNew.h"
#include <iostream>

struct Key {
    public:
        std::string keyCode = "e";
        bool enabled = false;
        int beforeKeyPress = 0;
        int holdFor = 1000;
        int afterKeyPress = 10;

        Key() {}

        Key(std::string keyCode, bool enabled, int beforeKeyPress, int holdFor, int afterKeyPress) {
            this->keyCode = keyCode;
            this->enabled = enabled;
            this->beforeKeyPress = beforeKeyPress;
            this->holdFor = holdFor;
            this->afterKeyPress = afterKeyPress;
        }

        Key(std::string keyCode) {
            this->keyCode = keyCode;
        }
};

class KeySequence {
    private:
        std::vector<Key> keys;

    public:
        KeySequence() {
            this->keys = std::vector<Key>();
        }

        KeySequence(std::vector<Key> &keys) {
            this->keys = keys;
        }
        
        KeySequence(const YAML::Node node) {
            this->keys = std::vector<Key>();
            if (node.IsSequence()) {
                //vector<YAML::Node> nodes = node.as<vector<YAML::Node>>();
                //for (YAML::const_iterator at = node.begin(); at != node.end(); at++) {
                //std::cout << node << std::endl << endl;
                for (auto & at : node) {
                    //std::cout << at << std::endl << endl;
                    //std::cout << at->first.as<YAML::Node>() << std::endl;
                    Key k = Key(
                        getConfigString(at, "keyCode", "e"),
                        getConfigBool(at, "enabled", true),
                        getConfigInt(at, "beforeKeyPress", 0),
                        getConfigInt(at, "holdFor", 2400),
                        getConfigInt(at, "afterKeyPress", 10)
                    );
                    if (!getMapOfKeys().count(k.keyCode)) {
                        if(k.keyCode != "EXAMPLE") addConfigLoadingMessage("WARNING | A key with a non-valid keycode \"" + k.keyCode + "\" was not processed.");
                    }
                    else keys.push_back(k);
                }
            }
        }

        KeySequence(std::string singleKeySimple) {
            this->keys = std::vector<Key>();
            this->keys.push_back(Key(singleKeySimple));
        }

        ~KeySequence() {
            keys.clear();
        }

        std::vector<Key> getKeys() {
            return keys;
        }

        int countEnabledKeys() {
            int c = 0;
            for (auto& el : keys) {
                if (el.enabled && el.keyCode != "EXAMPLE") c++;
            }
            //cout << c << endl;
            return c;
        }
};