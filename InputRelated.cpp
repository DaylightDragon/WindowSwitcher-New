#include "WindowSwitcherNew.h"
#include "DataStash.h"
#include "InputRelated.h"
//#include "DataStash.h"

std::chrono::steady_clock::time_point globalLastSequenceInputTimestamp;

Key::Key() {}

Key::Key(std::string keyCode, bool enabled, int beforeKeyPress, int holdFor, int afterKeyPress) {
    this->keyCode = keyCode;
    this->enabled = enabled;
    this->beforeKeyPress = beforeKeyPress;
    this->holdFor = holdFor;
    this->afterKeyPress = afterKeyPress;
}

Key::Key(std::string keyCode) {
    this->keyCode = keyCode;
};

KeySequence::KeySequence() {
    this->keys = std::vector<Key>();
}

KeySequence::KeySequence(std::vector<Key>& keys) {
    this->keys = keys;
}
        
KeySequence::KeySequence(const YAML::Node& node) {
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
                if (!mapOfKeys.count(k.keyCode)) {
                    if (k.keyCode != "EXAMPLE") addConfigLoadingMessage("WARNING | A key with a non-valid keycode \"" + k.keyCode + "\" was not processed.");
                }
                else keys.push_back(k);
            }
        }
    }
    cooldownPerWindow = getConfigLong(node, "cooldownPerWindow/cooldownDuration_milliseconds", 1000 * 15);
    checkEveryOnWait = getConfigInt(node, "cooldownPerWindow/delayBetweenCheckingTwoWindowsOnCooldown_milliseconds", 100);
}

KeySequence::KeySequence(std::string singleKeySimple) {
    this->keys = std::vector<Key>();
    this->keys.push_back(Key(singleKeySimple));
}

KeySequence::~KeySequence() {
    keys.clear();
}

std::vector<Key> KeySequence::getKeys() {
    return keys;
}

long KeySequence::getCooldownPerWindow() {
    return cooldownPerWindow;
}

void KeySequence::setCooldownPerWindow(long cooldown) {
    this->cooldownPerWindow = cooldown;
}

int KeySequence::getCheckEveryOnWait() {
    return checkEveryOnWait;
}

int KeySequence::countEnabledKeys() {
    int c = 0;
    for (auto& el : keys) {
        if (el.enabled && el.keyCode != "EXAMPLE") c++;
    }
    //cout << c << endl;
    return c;
}

int KeySequence::countKeysTotal() {
    return keys.size();
}

int ManyInputsConfiguration::getAtAmountOfInputs() const {
    return manyInputsDetectionAmount;
}

int ManyInputsConfiguration::getinTimeInterval_milliseconds() const {
    return manyInputsDetectionDuration;
}

int ManyInputsConfiguration::getMacroPause() const {
    return macroPause;
}

ManyInputsConfiguration::ManyInputsConfiguration(const YAML::Node &node) {
    if (node.IsMap()) {
        this->manyInputsDetectionAmount = getConfigInt(node, "atAmountOfInputs", 5);
        this->manyInputsDetectionDuration = getConfigInt(node, "inTimeInterval_milliseconds", 5000);
        this->macroPause = getConfigInt(node, "pauseMacroFor_seconds", 15);
    }
    else {
        addConfigLoadingMessage("WARNING | A \"many inputs\" configuration instance couldn't be processed, using default values");
    }
}

ManyInputsConfiguration::ManyInputsConfiguration(int atAmountOfInputs, int manyInputsDetectionDuration, int macroPause) {
    this->manyInputsDetectionAmount = atAmountOfInputs;
    this->manyInputsDetectionDuration = manyInputsDetectionDuration;
    this->macroPause = macroPause;
}

bool ManyInputsConfiguration::fitsCurrentCase(std::vector<std::chrono::steady_clock::time_point>* inputsList) {
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

YAML::Node* ManyInputsConfiguration::toNode() {
    YAML::Node* node = new YAML::Node();
    setConfigValue(*node, "atAmountOfInputs", this->manyInputsDetectionAmount);
    setConfigValue(*node, "inTimeInterval_milliseconds", this->manyInputsDetectionDuration);
    setConfigValue(*node, "pauseMacroFor_seconds", this->macroPause);
    return node;
}

SentInput::SentInput(std::string key, std::chrono::steady_clock::time_point timestamp) {
    this->key = key;
    this->timestamp = timestamp;
}

std::vector<std::chrono::steady_clock::time_point>* InputsInterruptionManager::getLastTimestamps(InterruptionInputType type) {
    if (type == KEYBOARD) return this->lastInputTimestampsKeyboard;
    else if (type == MOUSE) return this->lastInputTimestampsMouse;
    else if (type == ANY_INPUT) return this->lastInputTimestampsCombined;
    else return nullptr;
}

std::vector<ManyInputsConfiguration>* InputsInterruptionManager::getConfigurations(InterruptionInputType type) {
    if (type == KEYBOARD) return keyboardConfs;
    else if (type == MOUSE) return mouseConfs;
    else if (type == ANY_INPUT) return combinedConfs;
    else return nullptr;
}

// getters and setters for integers

std::atomic<int>& InputsInterruptionManager::getMacroPauseAfterKeyboardInput() {
    return macroPauseAfterKeyboardInput;
}

void InputsInterruptionManager::setMacroPauseAfterKeyboardInput(int macroPauseAfterKeyboardInput) {
    this->macroPauseAfterKeyboardInput.store(macroPauseAfterKeyboardInput);
}

std::atomic<int>& InputsInterruptionManager::getMacroPauseAfterMouseInput() {
    return macroPauseAfterMouseInput;
}

void InputsInterruptionManager::setMacroPauseAfterMouseInput(int macroPauseAfterMouseInput) {
    this->macroPauseAfterMouseInput.store(macroPauseAfterMouseInput);
}

std::atomic<int>& InputsInterruptionManager::getInputsListCapacity() {
    return inputsListCapacity;
}

void InputsInterruptionManager::setInputsListCapacity(int inputsListCapacity) {
    this->inputsListCapacity.store(inputsListCapacity);
}

std::atomic<int>& InputsInterruptionManager::getInputSeparationWaitingTimeout() {
    return inputSeparationWaitingTimeoutMilliseconds;
}

void InputsInterruptionManager::setInputSeparationWaitingTimeout(int inputSeparationWaitingTimeoutMilliseconds) {
    this->inputSeparationWaitingTimeoutMilliseconds.store(inputSeparationWaitingTimeoutMilliseconds);
}

// getters and setters for booleans

std::atomic<bool>& InputsInterruptionManager::isModeEnabled() {
    return modeEnabled;
}

void InputsInterruptionManager::setModeEnabled(bool modeEnabled) {
    this->modeEnabled.store(modeEnabled);
}

std::atomic<bool>& InputsInterruptionManager::getInformOnEvents() {
    return informOnEvents;
}

void InputsInterruptionManager::setInformOnEvents(bool informOnEvents) {
    this->informOnEvents.store(informOnEvents);
}

std::atomic<bool>& InputsInterruptionManager::getShouldStartAnything() {
    return shouldStartAnything;
}

void InputsInterruptionManager::setShouldStartAnything(bool shouldStartAnything) {
    this->shouldStartAnything.store(shouldStartAnything);
}

std::atomic<bool>& InputsInterruptionManager::getShouldStartDelayAndTimerThread() {
    return shouldStartDelayModificationLoop;
}

void InputsInterruptionManager::setShouldStartDelayAndTimerThread(bool shouldStartDelayModificationLoop) {
    this->shouldStartDelayModificationLoop.store(shouldStartDelayModificationLoop);
}

std::atomic<bool>& InputsInterruptionManager::getShouldStartKeyboardHook() {
    return shouldStartKeyboardHook;
}

void InputsInterruptionManager::setShouldStartKeyboardHook(bool shouldStartKeyboardHook) {
    this->shouldStartKeyboardHook.store(shouldStartKeyboardHook);
}

std::atomic<bool>& InputsInterruptionManager::getShouldStartTimerThread() {
    return shouldStartKeyboardHook;
}

void InputsInterruptionManager::setShouldStartTimerThread(bool shouldStartTimerThread) {
    this->shouldStartKeyboardHook.store(shouldStartTimerThread);
}

std::atomic<bool>& InputsInterruptionManager::getShouldStartMouseHook() {
    return shouldStartMouseHook;
}

void InputsInterruptionManager::setShouldStartMouseHook(bool shouldStartMouseHook) {
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

std::atomic<int>& InputsInterruptionManager::getUntilNextMacroRetryAtomic() {
    return untilNextMacroRetry;
}

std::unordered_map<int, bool>& InputsInterruptionManager::getKeyStates() {
    return keyStates;
}

// main part

InputsInterruptionManager::InputsInterruptionManager() {
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

InputsInterruptionManager::~InputsInterruptionManager() {
    //delete lock;
    delete keyboardConfs;
    delete mouseConfs;
    delete combinedConfs;
    delete lastInputTimestampsKeyboard;
    delete lastInputTimestampsMouse;
    delete lastInputTimestampsCombined;
}

void InputsInterruptionManager::initType(const YAML::Node& node, InterruptionInputType type) {
    std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex, std::adopt_lock);

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

void InputsInterruptionManager::setNewDelay(int delay) {
    // looks thread safe enough already
    if (untilNextMacroRetry.load() < delay) {
        untilNextMacroRetry.store(delay);
        if (informOnEvents.load()) {
            if (!getStopMacroInput().load()) {
                std::cout << "Paused for " << delay << " seconds...\n";
                //getInterruptedRightNow().store(true);
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
        
void InputsInterruptionManager::checkForManyInputsOnNew(InterruptionInputType type) {
    std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex, std::adopt_lock);

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

void InputsInterruptionManager::addPendingSentInput(std::string key) {
    SentInput input = SentInput(key, std::chrono::steady_clock::now());
    std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex, std::adopt_lock);
    pendingSentInputs.push_back(input);
}

bool InputsInterruptionManager::checkIsInputPending(std::string key) {
    std::lock_guard<std::mutex> threadSafetyLock(threadSafetyMutex, std::adopt_lock);

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
