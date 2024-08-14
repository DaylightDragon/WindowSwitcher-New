#pragma once

#include <string>
#include <vector>
#include <yaml-cpp/yaml.h>
#include "ConfigOperations.h"
#include <iostream>
#include <chrono>
#include <atomic>
#include <condition_variable>

#include "WindowSwitcherNew.h"
#include "DataStash.h"
#include "Data.h"

struct Key {
public:
    std::string keyCode = "e";
    bool enabled = false;
    int beforeKeyPress = 0;
    int holdFor = 1000;
    int afterKeyPress = 10;

    Key();
    Key(std::string keyCode, bool enabled, int beforeKeyPress, int holdFor, int afterKeyPress);
    Key(std::string keyCode);
};

class KeySequence {
private:
    std::vector<Key> keys;
    long cooldownPerWindow = 1000 * 30; // 30 seconds
    int checkEveryOnWait;

public:
    KeySequence();
    KeySequence(std::vector<Key>& keys);
    KeySequence(const YAML::Node& node);
    KeySequence(std::string singleKeySimple);
    ~KeySequence();

    std::vector<Key> getKeys();
    long getCooldownPerWindow();
    void setCooldownPerWindow(long cooldown);
    int getCheckEveryOnWait();
    int countEnabledKeys();
    int countKeysTotal();
};

class ManyInputsConfiguration {
private:
    int manyInputsDetectionAmount = 5;
    int manyInputsDetectionDuration = 5000; // in milliseconds
    int macroPause = 15;

public:
    int getAtAmountOfInputs() const;

    int getinTimeInterval_milliseconds() const;

    int getMacroPause() const;

    ManyInputsConfiguration(const YAML::Node& node);

    ManyInputsConfiguration(int atAmountOfInputs, int manyInputsDetectionDuration, int macroPause);

    bool fitsCurrentCase(std::vector<std::chrono::steady_clock::time_point>* inputsList);

    YAML::Node* toNode();
};

struct SentInput {
    std::string key;
    std::chrono::steady_clock::time_point timestamp;

    SentInput(std::string key, std::chrono::steady_clock::time_point timestamp);
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
    std::atomic<bool> informOnEvents = true;

    // requires restarting the app
    std::atomic<bool> shouldStartAnything = false;
    std::atomic<bool> shouldStartDelayModificationLoop = false;
    std::atomic<bool> shouldStartKeyboardHook = false;
    std::atomic<bool> shouldStartMouseHook = false;

    std::atomic<int> inputsListCapacity = 40;
    std::atomic<int> macroPauseAfterKeyboardInput = 8;
    std::atomic<int> macroPauseAfterMouseInput = 3;
    std::atomic<int> inputSeparationWaitingTimeoutMilliseconds = 1000;

    std::vector<std::chrono::steady_clock::time_point>* getLastTimestamps(InterruptionInputType type);

public:
    std::vector<ManyInputsConfiguration>* getConfigurations(InterruptionInputType type);

    // getters and setters for integers

    std::atomic<int>& getMacroPauseAfterKeyboardInput();
    void setMacroPauseAfterKeyboardInput(int macroPauseAfterKeyboardInput);
    std::atomic<int>& getMacroPauseAfterMouseInput();
    void setMacroPauseAfterMouseInput(int macroPauseAfterMouseInput);
    std::atomic<int>& getInputsListCapacity();
    void setInputsListCapacity(int inputsListCapacity);
    std::atomic<int>& getInputSeparationWaitingTimeout();
    void setInputSeparationWaitingTimeout(int inputSeparationWaitingTimeoutMilliseconds);

    // getters and setters for booleans
    std::atomic<bool>& isModeEnabled();
    void setModeEnabled(bool modeEnabled);
    std::atomic<bool>& getInformOnEvents();
    void setInformOnEvents(bool informOnEvents);
    std::atomic<bool>& getShouldStartAnything();
    void setShouldStartAnything(bool shouldStartAnything);
    std::atomic<bool>& getShouldStartDelayAndTimerThread();
    void setShouldStartDelayAndTimerThread(bool shouldStartDelayModificationLoop);
    std::atomic<bool>& getShouldStartKeyboardHook();
    void setShouldStartKeyboardHook(bool shouldStartKeyboardHook);
    std::atomic<bool>& getShouldStartMouseHook();
    void setShouldStartMouseHook(bool shouldStartMouseHook);

    std::atomic<bool>& InputsInterruptionManager::getShouldStartTimerThread();
    void InputsInterruptionManager::setShouldStartTimerThread(bool shouldStartTimerThread);

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

    std::atomic<int>& getUntilNextMacroRetryAtomic();
    std::unordered_map<int, bool>& getKeyStates();

    // main part

    InputsInterruptionManager();
    ~InputsInterruptionManager();

    void initType(const YAML::Node& node, InterruptionInputType type);
    void setNewDelay(int delay);

    /*void addInput(InterruptionInputType type) {
        std::vector<std::chrono::steady_clock::time_point>* timestamps = getLastTimestamps(type);

        timestamps->push_back(std::chrono::steady_clock::now());
        if (timestamps->size() > 0 && timestamps->size() > this->inputsListCapacity) {
            timestamps->erase(timestamps->begin());
        }
    }*/

    void checkForManyInputsOnNew(InterruptionInputType type);
    void addPendingSentInput(std::string key);
    bool checkIsInputPending(std::string key);
};
