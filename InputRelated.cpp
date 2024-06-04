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
        long cooldownPerWindow = 1000 * 30; // 30 seconds

    public:
        KeySequence() {
            this->keys = std::vector<Key>();
        }

        KeySequence(std::vector<Key> &keys) {
            this->keys = keys;
        }
        
        KeySequence(const YAML::Node &node) {
            this->keys = std::vector<Key>();
            if (checkExists(node, "instructions")) {
                YAML::Node instructions = getConfigValue(node, "instructions");
                if (instructions.IsSequence()) {
                    for (auto& at : instructions) {
                        Key k = Key(
                            getConfigString(at, "keyCode", "e"),
                            getConfigBool(at, "enabled", true, true),
                            getConfigInt(at, "beforeKeyPress", 0, true),
                            getConfigInt(at, "holdFor", 2400, true),
                            getConfigInt(at, "afterKeyPress", 100, true)
                        );
                        if (!getMapOfKeys().count(k.keyCode)) {
                            if (k.keyCode != "EXAMPLE") addConfigLoadingMessage("WARNING | A key with a non-valid keycode \"" + k.keyCode + "\" was not processed.");
                        }
                        else keys.push_back(k);
                    }
                }
            }
            cooldownPerWindow = getConfigLong(node, "cooldownPerWindow_milliseconds", 1000 * 30);
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

        long getCooldownPerWindow() {
            return cooldownPerWindow;
        }

        void setCooldownPerWindow(long cooldown) {
            this->cooldownPerWindow = cooldown;
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

    int getinTimeInterval_milliseconds() const {
        return manyInputsDetectionDuration;
    }

    int getMacroPause() const {
        return macroPause;
    }

    ManyInputsConfiguration(const YAML::Node &node) {
        if (node.IsMap()) {
            this->manyInputsDetectionAmount = getConfigInt(node, "atAmountOfInputs", 5);
            this->manyInputsDetectionDuration = getConfigInt(node, "inTimeInterval_milliseconds", 5000);
            this->macroPause = getConfigInt(node, "pauseMacroFor_seconds", 15);
        }
        else {
            addConfigLoadingMessage("WARNING | A \"many inputs\" configuration instance couldn't be processed, using default values");
        }
    }

    ManyInputsConfiguration(int atAmountOfInputs, int manyInputsDetectionDuration, int macroPause) {
        this->manyInputsDetectionAmount = atAmountOfInputs;
        this->manyInputsDetectionDuration = manyInputsDetectionDuration;
        this->macroPause = macroPause;
    }

    bool fitsCurrentCase(std::vector<std::chrono::steady_clock::time_point>* inputsList) {
        auto begin = inputsList->begin();
        auto end = inputsList->end();

        if (std::distance(begin, end) >= this->manyInputsDetectionAmount) { // Ensure enough elements are present
            auto timeBetweenLastInputs = std::chrono::duration_cast<std::chrono::milliseconds>(*(end - 1) - *(end - this->manyInputsDetectionAmount)).count();

            if (timeBetweenLastInputs <= this->manyInputsDetectionDuration) {
                //cout << "Fits a long case\n";
                return true;
            }

            /*cout << "Not Fits " << this->manyInputsDetectionAmount << ' ' << this->manyInputsDetectionDuration << ' ' << macroPause << '\n' << timeBetweenLastInputs << ' ' << this->manyInputsDetectionDuration << '\n';*/
        }

        //cout << "Not fits\n";

        return false;
    }

    YAML::Node* toNode() {
        YAML::Node* node = new YAML::Node();
        setConfigValue(*node, "atAmountOfInputs", this->manyInputsDetectionAmount);
        setConfigValue(*node, "inTimeInterval_milliseconds", this->manyInputsDetectionDuration);
        setConfigValue(*node, "pauseMacroFor_seconds", this->macroPause);
        return node;
    }
};

struct SentInput {
    std::string key;
    std::chrono::steady_clock::time_point timestamp;

    SentInput(std::string key, std::chrono::steady_clock::time_point timestamp) {
        this->key = key;
        this->timestamp = timestamp;
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
        std::vector<SentInput> pendingSentInputs;
        std::atomic<int> untilNextMacroRetry;
        std::unordered_map<int, bool> keyStates;
        //std::condition_variable cv;
        //std::mutex mtx;
        std::mutex threadSafetyMutex;

        std::atomic<bool> modeEnabled = false;
        std::atomic<bool> informOnEvents = false;

        // requires restarting the app
        std::atomic<bool> shouldStartAnything = false;
        std::atomic<bool> shouldStartDelayModificationLoop = false;
        std::atomic<bool> shouldStartKeyboardHook = false;
        std::atomic<bool> shouldStartMouseHook = false;

        std::atomic<int> inputsListCapacity = 40;
        std::atomic<int> macroPauseAfterKeyboardInput = 8;
        std::atomic<int> macroPauseAfterMouseInput = 3;
        std::atomic<int> inputSeparationWaitingTimeoutMilliseconds = 1000;

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

        // getters and setters for integers

        std::atomic<int>& getMacroPauseAfterKeyboardInput() {
            return macroPauseAfterKeyboardInput;
        }

        void setMacroPauseAfterKeyboardInput(int macroPauseAfterKeyboardInput) {
            this->macroPauseAfterKeyboardInput.store(macroPauseAfterKeyboardInput);
        }

        std::atomic<int>& getMacroPauseAfterMouseInput() {
            return macroPauseAfterMouseInput;
        }

        void setMacroPauseAfterMouseInput(int macroPauseAfterMouseInput) {
            this->macroPauseAfterMouseInput.store(macroPauseAfterMouseInput);
        }

        std::atomic<int>& getInputsListCapacity() {
            return inputsListCapacity;
        }

        void setInputsListCapacity(int inputsListCapacity) {
            this->inputsListCapacity.store(inputsListCapacity);
        }

        std::atomic<int>& getInputSeparationWaitingTimeout() {
            return inputSeparationWaitingTimeoutMilliseconds;
        }

        void setInputSeparationWaitingTimeout(int inputSeparationWaitingTimeoutMilliseconds) {
            this->inputSeparationWaitingTimeoutMilliseconds.store(inputSeparationWaitingTimeoutMilliseconds);
        }

        // getters and setters for booleans

        std::atomic<bool>& isModeEnabled() {
            return modeEnabled;
        }

        void setModeEnabled(bool modeEnabled) {
            this->modeEnabled.store(modeEnabled);
        }

        std::atomic<bool>& getInformOnEvents() {
            return informOnEvents;
        }

        void setInformOnEvents(bool informOnEvents) {
            this->informOnEvents.store(informOnEvents);
        }

        std::atomic<bool>& getShouldStartAnything() {
            return shouldStartAnything;
        }

        void setShouldStartAnything(bool shouldStartAnything) {
            this->shouldStartAnything.store(shouldStartAnything);
        }

        std::atomic<bool>& getShouldStartDelayModificationLoop() {
            return shouldStartDelayModificationLoop;
        }

        void setShouldStartDelayModificationLoop(bool shouldStartDelayModificationLoop) {
            this->shouldStartDelayModificationLoop.store(shouldStartDelayModificationLoop);
        }

        std::atomic<bool>& getShouldStartKeyboardHook() {
            return shouldStartKeyboardHook;
        }

        void setShouldStartKeyboardHook(bool shouldStartKeyboardHook) {
            this->shouldStartKeyboardHook.store(shouldStartKeyboardHook);
        }

        std::atomic<bool>& getShouldStartMouseHook() {
            return shouldStartMouseHook;
        }

        void setShouldStartMouseHook(bool shouldStartMouseHook) {
            this->shouldStartMouseHook.store(shouldStartMouseHook);
        }

        // important getters

        /*std::condition_variable& getConditionVariable() {
            return cv;
        }*/

        //std::unique_lock<std::mutex>& getLock() {
            //return *lock;
        //}

        /*std::mutex& getMacroWaitMutex() {
            return mtx;
        }*/

        std::atomic<int>& getUntilNextMacroRetryAtomic() {
            return untilNextMacroRetry;
        }

        std::unordered_map<int, bool>& getKeyStates() {
            return keyStates;
        }

        // main part

        InputsInterruptionManager() {
            untilNextMacroRetry.store(0);
            //std::unique_lock<std::mutex>* lock;
            //lock = new std::unique_lock<std::mutex>(mtx);
            keyboardConfs = new std::vector<ManyInputsConfiguration>();
            mouseConfs = new std::vector<ManyInputsConfiguration>();
            combinedConfs = new std::vector<ManyInputsConfiguration>();
            lastInputTimestampsKeyboard = new std::vector<std::chrono::steady_clock::time_point>();
            lastInputTimestampsMouse = new std::vector<std::chrono::steady_clock::time_point>();
            lastInputTimestampsCombined = new std::vector<std::chrono::steady_clock::time_point>();
        }

        ~InputsInterruptionManager() {
            //delete lock;
            delete keyboardConfs;
            delete mouseConfs;
            delete combinedConfs;
            delete lastInputTimestampsKeyboard;
            delete lastInputTimestampsMouse;
            delete lastInputTimestampsCombined;
        }

        void initType(const YAML::Node& node, InterruptionInputType type) {
            std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex);

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
            // looks thread safe enough already
            if (untilNextMacroRetry.load() < delay) {
                untilNextMacroRetry.store(delay);
                if (informOnEvents.load()) {
                    if (!getStopMacroInput().load()) {
                        std::cout << "Paused for " << delay << " seconds...\n";
                    }
                }
            }
        }

        /*void addInput(InterruptionInputType type) {
            std::vector<std::chrono::steady_clock::time_point>* timestamps = getLastTimestamps(type);

            timestamps->push_back(std::chrono::steady_clock::now());
            if (timestamps->size() > 0 && timestamps->size() > this->inputsListCapacity) {
                timestamps->erase(timestamps->begin());
            }
        }*/
        
        void checkForManyInputsOnNew(InterruptionInputType type) {
            std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex);

            std::vector<std::chrono::steady_clock::time_point>* lastInputTimestamps = getLastTimestamps(type);
            std::vector<ManyInputsConfiguration>* confs = getConfigurations(type);

            lastInputTimestamps->push_back(std::chrono::steady_clock::now());
            if (lastInputTimestamps->size() > 0 && lastInputTimestamps->size() > inputsListCapacity.load()) {
                lastInputTimestamps->erase(lastInputTimestamps->begin());
            }

            for (auto& item : *confs) {
                if (item.fitsCurrentCase(lastInputTimestamps)) {
                    setNewDelay(item.getMacroPause());
                }
            }

            //cv.notify_all();
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

        void addPendingSentInput(std::string key) {
            SentInput input = SentInput(key, std::chrono::steady_clock::now());
            std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex);
            pendingSentInputs.push_back(input);
        }

        bool checkIsInputPending(std::string key) {
            std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex);

            bool result = false;
            
            auto now = std::chrono::steady_clock::now();
            for (auto it = pendingSentInputs.begin(); it != pendingSentInputs.end();) {
                auto timePassedSince = std::chrono::duration_cast<std::chrono::milliseconds>(now - it->timestamp).count();
                int threshold = inputSeparationWaitingTimeoutMilliseconds.load();

                //cout << it->key << ' ' << timePassedSince << '\n';
                if (threshold >= 0 && timePassedSince >= threshold) {
                    it = pendingSentInputs.erase(it); // returns next
                }
                else {
                    if (it->key == key) {
                        result = true;
                        it = pendingSentInputs.erase(it); // returns next
                    }
                    else {
                        ++it;
                    }
                }
            }

            return result;
        }
};
