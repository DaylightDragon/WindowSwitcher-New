#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include "WindowSwitcherNew.h"
#include <iostream>
#include <chrono>
#include <condition_variable>

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
        
        KeySequence(const YAML::Node &node) {
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

enum InterruptionInputType {
    KEYBOARD,
    MOUSE,
    ANY_INPUT
};

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

class ManyInputsConfiguration {
private:
    int manyInputsDetectionAmount = 5;
    int manyInputsDetectionDuration = 5000; // in milliseconds
    int macroPause = 15;

public:
    int getAtAmountOfInputs() const {
        return manyInputsDetectionAmount;
    }

    int getInIntervalOfMilliseconds() const {
        return manyInputsDetectionDuration;
    }

    int getMacroPause() const {
        return macroPause;
    }

    ManyInputsConfiguration(const YAML::Node &node) {
        if (node.IsMap()) {
            this->manyInputsDetectionAmount = getConfigInt(node, "atAmountOfInputs", 5);
            this->manyInputsDetectionDuration = getConfigInt(node, "inIntervalOfMilliseconds", 5000);
            this->macroPause = getConfigInt(node, "pauseMacroForSeconds", 15);
        }
        else {
            addConfigLoadingMessage("WARNING | A \"many inputs\" configuration instance couldn't be processed, using default values");
        }
    }

    ManyInputsConfiguration(int atAmountOfInputs, int inIntervalOfMilliseconds, int pauseMacroForSeconds) {
        this->manyInputsDetectionAmount = atAmountOfInputs;
        this->manyInputsDetectionDuration = inIntervalOfMilliseconds;
        this->macroPause = pauseMacroForSeconds;
    }

    bool fitsCurrentCase(std::vector<std::chrono::steady_clock::time_point>* inputsList, std::condition_variable& cv) {
        auto begin = inputsList->begin();
        auto end = inputsList->end();

        if (std::distance(begin, end) >= this->manyInputsDetectionAmount) { // Ensure enough elements are present
            auto timeBetweenLastInputs = std::chrono::duration_cast<std::chrono::milliseconds>(*(end - 1) - *(end - this->manyInputsDetectionAmount)).count();

            if (timeBetweenLastInputs <= this->manyInputsDetectionDuration) {
                cout << "Fits a long case\n";
                return true;
            }

            /*cout << "Not Fits " << this->manyInputsDetectionAmount << ' ' << this->manyInputsDetectionDuration << ' ' << macroPause << '\n' << timeBetweenLastInputs << ' ' << this->manyInputsDetectionDuration << '\n';*/
        }

        return false;
    }

    YAML::Node* toNode() {
        YAML::Node* node = new YAML::Node();
        setConfigValue(*node, "atAmountOfInputs", this->manyInputsDetectionAmount);
        setConfigValue(*node, "inIntervalOfMilliseconds", this->manyInputsDetectionDuration);
        setConfigValue(*node, "pauseMacroForSeconds", this->macroPause);
        return node;
    }
};

class InputsInterruptionManager {
    private:
        // WHEN ADDING NEW TYPES, PLEASE INIT THEM IN THE CONSTRUCTOR, SO IT DOESN't CRASH (THEY ARE POINTERS)
        std::vector<ManyInputsConfiguration>* keyboardConfs;
        std::vector<ManyInputsConfiguration>* mouseConfs;
        std::vector<ManyInputsConfiguration>* combinedConfs;
        std::vector<std::chrono::steady_clock::time_point>* lastInputTimestampsKeyboard;
        std::vector<std::chrono::steady_clock::time_point>* lastInputTimestampsMouse;
        std::vector<std::chrono::steady_clock::time_point>* lastInputTimestampsCombined;
        std::atomic<int> untilNextMacroRetry;
        std::unordered_map<int, bool> keyStates;
        std::condition_variable cv;
        std::mutex mtx;
        std::unique_lock<std::mutex>* lock;

        int inputsListCapacity = 40;
        int macroPauseAfterKeyboardInput = 8;
        int macroPauseAfterMouseInput = 3;
        // no pause after the combined one

        std::vector<std::chrono::steady_clock::time_point>* getLastTimestamps(InterruptionInputType type) {
            if (type == KEYBOARD) return lastInputTimestampsKeyboard;
            else if (type == MOUSE) return lastInputTimestampsMouse;
            else if (type == ANY_INPUT) return lastInputTimestampsCombined;
            else return nullptr;
        }

    public:
        std::vector<ManyInputsConfiguration>* getConfigurations(InterruptionInputType type) {
            if (type == KEYBOARD) return this->keyboardConfs;
            else if (type == MOUSE) return this->mouseConfs;
            else if (type == ANY_INPUT) return this->combinedConfs;
            else return nullptr;
        }

        int getMacroPauseAfterKeyboardInput() {
            return macroPauseAfterKeyboardInput;
        }

        int getMacroPauseAfterMouseInput() {
            return macroPauseAfterMouseInput;
        }

        int getInputsListCapacity() {
            return inputsListCapacity;
        }

        void setMacroPauseAfterKeyboardInput(int macroPauseAfterKeyboardInput) {
            this->macroPauseAfterKeyboardInput = macroPauseAfterKeyboardInput;
        }

        void setMacroPauseAfterMouseInput(int macroPauseAfterMouseInput) {
            this->macroPauseAfterMouseInput = macroPauseAfterMouseInput;
        }

        void setInputsListCapacity(int inputsListCapacity) {
            this->inputsListCapacity = inputsListCapacity;
        }

        std::condition_variable& getConditionVariable() {
            return cv;
        }

        std::unique_lock<std::mutex>& getLock() {
            return *lock;
        }

        std::atomic<int>& getUntilNextMacroRetryAtomic() {
            return untilNextMacroRetry;
        }

        std::unordered_map<int, bool>& getKeyStates() {
            return keyStates;
        }

        InputsInterruptionManager() {
            untilNextMacroRetry.store(0);
            lock = new std::unique_lock<std::mutex>(mtx);
            keyboardConfs = new std::vector<ManyInputsConfiguration>();
            mouseConfs = new std::vector<ManyInputsConfiguration>();
            combinedConfs = new std::vector<ManyInputsConfiguration>();
            lastInputTimestampsKeyboard = new std::vector<std::chrono::steady_clock::time_point>();
            lastInputTimestampsMouse = new std::vector<std::chrono::steady_clock::time_point>();
            lastInputTimestampsCombined = new std::vector<std::chrono::steady_clock::time_point>();
        }

        ~InputsInterruptionManager() {
            delete lock;
            delete keyboardConfs;
            delete mouseConfs;
            delete combinedConfs;
            delete lastInputTimestampsKeyboard;
            delete lastInputTimestampsMouse;
            delete lastInputTimestampsCombined;
        }

        void initType(const YAML::Node& node, InterruptionInputType type) {
            //cout << "Init " << inputTypeToString(type) << endl;

            if (node.IsSequence()) {
                std::vector<ManyInputsConfiguration>* confs = getConfigurations(type);

                //std::cout << &confs << " " << &(this->keyboardConfs) << std::endl;

                for (auto& at : node) {
                    ManyInputsConfiguration* signleConf = new ManyInputsConfiguration(at);
                    //cout << "Adding\n";
                    confs->push_back(*signleConf);
                }
            }
            else {
                //addConfigLoadingMessage("WARNING | InputsInterruptionManager for " + inputTypeToString(type) + " couldn't be processed, using default values");
            }
        }

        void setNewDelay(int delay) {
            if (untilNextMacroRetry.load() < delay) untilNextMacroRetry.store(delay);
        }

        /*void addInput(InterruptionInputType type) {
            std::vector<std::chrono::steady_clock::time_point>* timestamps = getLastTimestamps(type);

            timestamps->push_back(std::chrono::steady_clock::now());
            if (timestamps->size() > 0 && timestamps->size() > this->inputsListCapacity) {
                timestamps->erase(timestamps->begin());
            }
        }*/
        
        void checkForManyInputsOnNew(InterruptionInputType type) {
            std::vector<std::chrono::steady_clock::time_point>* lastInputTimestamps = getLastTimestamps(type);
            std::vector<ManyInputsConfiguration>* confs = getConfigurations(type);

            lastInputTimestamps->push_back(std::chrono::steady_clock::now());
            if (lastInputTimestamps->size() > 0 && lastInputTimestamps->size() > inputsListCapacity) {
                lastInputTimestamps->erase(lastInputTimestamps->begin());
            }

            for (auto& item : *confs) {
                if (item.fitsCurrentCase(lastInputTimestamps, cv)) {
                    setNewDelay(item.getMacroPause());
                }
            }

            cv.notify_all();
        }

        static YAML::Node* getDefaultInterruptionConfigsList(InterruptionInputType type) {
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
};
