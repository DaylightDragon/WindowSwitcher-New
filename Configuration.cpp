#include "Configuration.h"

bool initialConfigLoading = true;

// Permanent config variables
int oldConfigValue_startAnything;
int oldConfigValue_startKeyboardHook;
int oldConfigValue_startMouseHook;

void rememberInitialPermanentSettings() {
    oldConfigValue_startAnything = interruptionManager.load()->getShouldStartAnything().load();
    oldConfigValue_startKeyboardHook = interruptionManager.load()->getShouldStartKeyboardHook().load();
    oldConfigValue_startMouseHook = interruptionManager.load()->getShouldStartMouseHook().load();
}

void resetMainSequence(const YAML::Node& config) {
    if (settings.load()->mainSequence != nullptr) {
        delete settings.load()->mainSequence;
        settings.load()->mainSequence = nullptr;
    }
}

void readNewMainSequence(const YAML::Node& config) {
    resetMainSequence(config);
    settings.load()->mainSequence = new KeySequence(getConfigValue(config, "settings/macro/mainKeySequence"));
}

void readDefaultMainSequence(const YAML::Node& config) {
    resetMainSequence(config);
    settings.load()->mainSequence = new KeySequence(getDefaultSequenceList(true));
}

void resetInterruptionManager(const YAML::Node& config) {
    if (interruptionManager != nullptr) {
        //interruptionManager.load().get
        delete interruptionManager;
        interruptionManager = nullptr;
    }
    //std::cout << "resetInterruptionManager\n";
}

void resetSettings(const YAML::Node& config) {
    if (settings != nullptr) {
        delete settings;
        settings = nullptr;
    }
}

void readNewSettings(const YAML::Node& config) {
    resetSettings(config);
    settings = new Settings(config);
}

void readSimpleInteruptionManagerSettings(const YAML::Node& config) {
    interruptionManager.load()->setMacroPauseAfterKeyboardInput(getConfigInt(config, "settings/macro/interruptions/keyboard/startWithDelay_seconds", 8));
    interruptionManager.load()->setMacroPauseAfterMouseInput(getConfigInt(config, "settings/macro/interruptions/mouse/startWithDelay_seconds", 4));
    interruptionManager.load()->setInputsListCapacity(getConfigInt(config, "settings/macro/interruptions/rememberMaximumInputsEach", 40));
    interruptionManager.load()->setInputSeparationWaitingTimeout(getConfigInt(config, "settings/macro/interruptions/macroInputArrivalTimeoutToSeparateFromUserInputs", 1000));
    interruptionManager.load()->setInformOnEvents(getConfigBool(config, "settings/macro/interruptions/informOnChangingDelay", true));
    interruptionManager.load()->setModeEnabled(getConfigBool(config, "settings/macro/interruptions/interruptionsWorkingRightNow", false));

    // hooks
    interruptionManager.load()->setShouldStartAnything(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/enableTheseOptions", true));
    interruptionManager.load()->setShouldStartKeyboardHook(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/keyboardListener", true));
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartKeyboardHook(false);
    interruptionManager.load()->setShouldStartMouseHook(getConfigBool(config, "settings/requiresApplicationRestart/macroInterruptions/mouseListener", true));
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartMouseHook(false);
    interruptionManager.load()->setShouldStartDelayModificationLoop(interruptionManager.load()->getShouldStartKeyboardHook().load() || interruptionManager.load()->getShouldStartMouseHook().load());
    if (!interruptionManager.load()->getShouldStartAnything().load()) interruptionManager.load()->setShouldStartDelayModificationLoop(false);

    if (!initialConfigLoading && (
        oldConfigValue_startKeyboardHook != interruptionManager.load()->getShouldStartKeyboardHook().load() ||
        oldConfigValue_startMouseHook != interruptionManager.load()->getShouldStartMouseHook().load())) {
        addConfigLoadingMessage("WARNING | Some config values can't be applied in runtime, you need to restart the application for that");
    }
}

void readNewInterruptionManager(const YAML::Node& config) {
    resetInterruptionManager(config);
    interruptionManager = new InputsInterruptionManager();
    //std::cout << "readNewInterruptionManager\n";

    interruptionManager.load()->initType(getConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases"), KEYBOARD);
    interruptionManager.load()->initType(getConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases"), MOUSE);
    interruptionManager.load()->initType(getConfigValue(config, "settings/macro/interruptions/anyInput/manyInputsCases"), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);

    //std::cout << interruptionManager << '\n';
    //if (macroDelayWatcherThread != nullptr) {
        //macroDelayWatcherThread->
    //}
    //std::thread(testWait).detach();
}

void readDefaultInterruptionManager(const YAML::Node& config) {
    resetInterruptionManager(config);
    interruptionManager = new InputsInterruptionManager();
    //std::cout << "readDefaultInterruptionManager\n";

    interruptionManager.load()->initType(*getDefaultInterruptionConfigsList(KEYBOARD), KEYBOARD);
    interruptionManager.load()->initType(*getDefaultInterruptionConfigsList(MOUSE), MOUSE);
    interruptionManager.load()->initType(*getDefaultInterruptionConfigsList(ANY_INPUT), ANY_INPUT);

    readSimpleInteruptionManagerSettings(config);
}

void addNewKeysToSequence(YAML::Node& config, std::string key, YAML::Node extraKeys) {
    YAML::Node resultInstructions = YAML::Node(YAML::NodeType::Sequence);
    YAML::Node oldInstructions = getConfigValue(getConfigValue(config, key), "instructions");
    YAML::Node newInstructions = getConfigValue(extraKeys, "instructions");
    //std::cout << "Old instr " << oldInstructions << oldInstructions.IsDefined() << '\n';
    //std::cout << "New instr " << newInstructions << '\n';
    if (oldInstructions.IsSequence()) {
        for (YAML::Node instr : oldInstructions) {
            resultInstructions.push_back(instr);
        }
    }
    if (newInstructions.IsSequence()) {
        for (YAML::Node instr : newInstructions) {
            resultInstructions.push_back(instr);
        }
    }
    setConfigValue(config, key + "/instructions", resultInstructions);
}

YAML::Node& loadSettingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig) {
    if (!wasEmpty) {
        std::string oldConfigVersion;
        YAML::Node versionData;
        try {
            versionData = getConfigValue(config, "internal/configVersion");
            oldConfigVersion = versionData.as<std::string>();
        }
        catch (const YAML::Exception& e) {
            oldConfigVersion = "2.0";
        }

        bool firstUpdate = true;
        if (oldConfigVersion == "2.0") {
            updateConfig2_0_TO_2_1(config, wrongConfig, firstUpdate);
            firstUpdate = false;
            oldConfigVersion = "2.1";
        }
        if (oldConfigVersion == "2.1") {
            // no refactoring
            oldConfigVersion = "2.2";
        }
        if (oldConfigVersion == "2.2") {
            updateConfig2_2_TO_2_3(config, wrongConfig, firstUpdate);
            firstUpdate = false;
            oldConfigVersion = "2.3";
        }
        //if (oldConfigVersion != getCurrentVersion) {
        //}

    }

    settings.load()->macroDelayInitial = getConfigInt(config, "settings/macro/general/initialDelayBeforeFirstIteration", 100);
    settings.load()->macroDelayBeforeSwitching = getConfigInt(config, "settings/macro/general/delayBeforeSwitchingWindow", 25);
    settings.load()->macroDelayAfterFocus = getConfigInt(config, "settings/macro/general/delayAfterSwitchingWindow", 200);
    settings.load()->delayWhenDoneAllWindows = getConfigInt(config, "settings/macro/general/delayWhenDoneAllWindows", 100);
    settings.load()->macroDelayBetweenSwitchingAndFocus = getConfigInt(config, "settings/macro/general/settingsChangeOnlyWhenReallyNeeded/afterSettingForegroundButBeforeSettingFocus", 10);
    settings.load()->specialSingleWindowModeEnabled = getConfigBool(config, "settings/macro/general/specialSingleWindowMode/enabled", false);
    settings.load()->specialSingleWindowModeKeyCode = getConfigString(config, "settings/macro/general/specialSingleWindowMode/keyCode", defaultMacroKey);

    settings.load()->sleepRandomnessPersent = getConfigInt(config, "settings/macro/general/randomness/delays/delayOffsetPersentage", 10);
    settings.load()->sleepRandomnessMaxDiff = getConfigInt(config, "settings/macro/general/randomness/delays/delayOffsetLimit", 40);

    // if no yaml structure errors
    if (!wrongConfig) {
        // resetting main sequence and reading what config has
        readNewMainSequence(config);
        // if there is nothing
        if (settings.load()->mainSequence->countEnabledKeys() == 0) {
            // set default values
            if (settings.load()->mainSequence->countKeysTotal() == 0) addNewKeysToSequence(config, "settings/macro/mainKeySequence", getDefaultSequenceList(true));
            else addNewKeysToSequence(config, "settings/macro/mainKeySequence", getDefaultSequenceList(false));
            // reset the sequence and read the yaml object again
            readNewMainSequence(config);
        }
    }
    else {
        readDefaultMainSequence(config);
    }

    // extra sequences
    if (!checkExists(config, "settings/macro/extraKeySequences")) setConfigValue(config, "settings/macro/extraKeySequences", getDefaultExtraKeySequences());
    YAML::Node extraSequences = getConfigValue(config, "settings/macro/extraKeySequences");
    if (extraSequences.IsMap()) {
        //for (auto& otherSeq : settings.load()->knownOtherSequences) {
        //    delete &otherSeq;
        //}
        settings.load()->knownOtherSequences.clear();
        for (YAML::const_iterator at = extraSequences.begin(); at != extraSequences.end(); at++) {
            settings.load()->knownOtherSequences[at->first.as<std::string>()] = new KeySequence(at->second);
        }
    }

    // if no yaml structure errors
    if (!wrongConfig) {
        // resetting input interruption manager and reading what config has
        readNewInterruptionManager(config);
        bool nothingKeyboard = interruptionManager.load()->getConfigurations(KEYBOARD)->size() == 0;
        bool nothingMouse = interruptionManager.load()->getConfigurations(MOUSE)->size() == 0;
        // if there is nothing
        if (nothingKeyboard || nothingMouse) {
            //std::cout << "Nothing\n";
            // set default values
            YAML::Node* defaultList = getDefaultInterruptionConfigsList(KEYBOARD);
            if (nothingKeyboard) setConfigValue(config, "settings/macro/interruptions/keyboard/manyInputsCases", *defaultList);
            delete defaultList;

            defaultList = getDefaultInterruptionConfigsList(MOUSE);
            if (nothingMouse) setConfigValue(config, "settings/macro/interruptions/mouse/manyInputsCases", *defaultList);
            delete defaultList;

            defaultList = getDefaultInterruptionConfigsList(ANY_INPUT);
            if (nothingMouse) setConfigValue(config, "settings/macro/interruptions/anyInput/manyInputsCases", *defaultList);
            delete defaultList;

            // reset the manager and read the yaml object again
            readNewInterruptionManager(config);
        }
    }
    else {
        readDefaultMainSequence(config);
        readDefaultInterruptionManager(config);
    }

    settings.load()->usePrimitiveInterruptionAlgorythm = getConfigBool(config, "settings/macro/interruptions/advanced/primitiveInterruptionsAlgorythm/enabled", true);
    settings.load()->primitiveWaitInterval = getConfigInt(config, "settings/macro/interruptions/advanced/primitiveInterruptionsAlgorythm/checkEvery_milliseconds", 100);

    readNewSettings(config);

    // saving
    setConfigValue(config, "internal/configVersion", getCurrentVersion());
    return config;
}

bool loadConfig(ConfigType type) {
    try {
        std::string folderPath = "WindowSwitcherSettings";
        std::string fileName;
        switch (type)
        {
        case MAIN_CONFIG:
            fileName = "settings.yml";
            break;
        case KEYBINDS_CONFIG:
            fileName = "keybindings.yml";
            break;
        default:
            std::cout << "Unsupported config type\n";
            return false;
        }
        YAML::Node config;
        bool wrongConfig = false;
        bool wasEmpty = false;

        try {
            config = loadYaml(getProgramPath(), folderPath, fileName, wrongConfig);
            wasEmpty = config.IsNull();
        }
        /*catch (YAML::ParserException e) {
            addConfigLoadingMessage("THE CONFIG HAS INVALID STRUCTURE AND CANNOT BE LOADED!\nUsing ALL default values");
            config = YAML::Node();
            wrongConfig = true;
        }
        catch (YAML::BadSubscript e) {
            addConfigLoadingMessage("THE CONFIG HAS INVALID STRUCTURE AND CANNOT BE LOADED!\nUsing ALL default values");
            config = YAML::Node();
            wrongConfig = true;
        }*/
        catch (const YAML::Exception& e) {
            addConfigLoadingMessage("WARNING | UNKNOWN ERROR WHILE LOADING " + configTypeToString(type) + " CONFIG!\nERROR TEXT | " + std::string(e.what()));
            config = YAML::Node();
            wrongConfig = true;
        }

        if (wrongConfig) {
            addConfigLoadingMessage("WARNING | For safery reasons, only default values will be used and NO CHANGES WILL BE APPLIED to the actual config (" + configTypeToString(type) + ")");
        }

        YAML::Node newConfig;

        switch (type)
        {
        case MAIN_CONFIG:
            newConfig = loadSettingsConfig(config, wasEmpty, wrongConfig);
            break;
        case KEYBINDS_CONFIG:
            newConfig = loadKeybindingsConfig(config, wasEmpty, wrongConfig);
            break;
        default:
            std::cout << "Unsupported config type\n";
            return false;
        }

        if (!wrongConfig) saveYaml(newConfig, getProgramFolderPath(getProgramPath()) + "/" + folderPath + "/" + fileName);
        //if (!wrongConfig) saveYaml(newConfig, getProgramFolderPath(programPath) + "/" + folderPath + "/" + "test_" + fileName); // test

        initialConfigLoading = false;

        return true;
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::Exception error happened while loading the config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const std::exception& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected std::exception error happened while loading the config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
    catch (const YAML::BadConversion& e) {
        addConfigLoadingMessage("WARNING | A completely unexpected YAML::BadConversion error happened while loading the config:\nERROR | " + std::string(e.what()) + "\nWARNING | You can report this bug to the developer.\n");
    }
}

void reloadConfigs() {
    loadConfig(MAIN_CONFIG);
    loadConfig(KEYBINDS_CONFIG);
}