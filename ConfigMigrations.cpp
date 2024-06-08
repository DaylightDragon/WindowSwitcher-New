#include "yaml-cpp/yaml.h"
#include "ConfigOperations.h"
#include "DataStash.h"

void updateConfig2_0_TO_2_1(YAML::Node& config, bool wrongConfig, bool firstUpdate) {
    if (firstUpdate) addConfigLoadingMessage("INFO | The config was updated from version 2.0 to 2.1");

    setConfigValue(config, "settings/macro/general/initialDelayBeforeFirstIteration", getConfigInt(config, "settings/macro/delaysInMilliseconds/initialDelayBeforeFirstIteration", 100));
    setConfigValue(config, "settings/macro/general/delayBeforeSwitchingWindow", 0);
    setConfigValue(config, "settings/macro/general/delayAfterSwitchingWindow", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterSettingFocus", 200));
    setConfigValue(config, "settings/macro/general/settingsChangeOnlyWhenReallyNeeded/afterSettingForegroundButBeforeSettingFocus", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterSwitchingToWindow", 10));
    setConfigValue(config, "settings/macro/general/specialSingleWindowMode/enabled", getConfigBool(config, "settings/macro/justHoldWhenSingleWindow", false));
    setConfigValue(config, "settings/macro/general/specialSingleWindowMode/keyCode", getConfigString(config, "settings/macro/defaultKey", defaultMacroKey));

    YAML::Node firstKey = YAML::Node();
    setConfigValue(firstKey, "keyCode", getConfigString(config, "settings/macro/defaultKey", defaultMacroKey));
    setConfigValue(firstKey, "enabled", true);
    setConfigValue(firstKey, "beforeKeyPress", 0);
    setConfigValue(firstKey, "holdFor", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterKeyPress", 2400));
    setConfigValue(firstKey, "afterKeyPress", getConfigInt(config, "settings/macro/delaysInMilliseconds/afterKeyRelease", 10));

    YAML::Node keyExample = YAML::Clone(firstKey);
    setConfigValue(keyExample, "keyCode", "EXAMPLE");
    setConfigValue(keyExample, "enabled", false);

    YAML::Node list = YAML::Node(YAML::NodeType::Sequence);
    list.push_back(firstKey);
    list.push_back(keyExample);

    setConfigValue(config, "settings/macro/mainKeySequence", getDefaultSequenceList(true));
    if (!checkExists(config, "settings/macro/extraKeySequences")) setConfigValue(config, "settings/macro/extraKeySequences", std::vector<YAML::Node>());
    removeConfigValue(config, "settings/macro/delaysInMilliseconds");
    removeConfigValue(config, "settings/macro/justHoldWhenSingleWindow");
    removeConfigValue(config, "settings/macro/defaultKey", true);
}

YAML::Node* updateKeySequence2_2_TO_2_3(YAML::Node& sequence) {
    YAML::Node* node = new YAML::Node();
    //std::cout << "sequence: " << sequence.IsSequence() << std::endl;
    if (sequence.IsSequence()) {
        setConfigValue(*node, "instructions", sequence.as<std::vector<YAML::Node>>());
    }
    return node;
}

void updateConfig2_2_TO_2_3(YAML::Node& config, bool wrongConfig, bool firstUpdate) {
    if (firstUpdate) addConfigLoadingMessage("INFO | The config was updated from version 2.2 to 2.3");

    YAML::Node* newMainSeq = updateKeySequence2_2_TO_2_3(getConfigValue(config, "settings/macro/mainKeySequence"));
    setConfigValue(config, "settings/macro/mainKeySequence", *newMainSeq);
    delete newMainSeq;

    if (checkExists(config, "settings/macro/extraKeySequences")) {
        YAML::Node extraSequences = getConfigValue(config, "settings/macro/extraKeySequences");
        std::map<std::string, YAML::Node> newValues = std::map<std::string, YAML::Node>();
        bool replaceValues = false;
        if (extraSequences.IsSequence() && extraSequences.as<std::vector<YAML::Node>>().size() > 0) {
            int i = 0;
            for (auto& seq : extraSequences) {
                if (seq.IsMap()) {
                    for (auto& keyValue : seq) {
                        std::string name = keyValue.first.as<std::string>();
                        YAML::Node value = keyValue.second;
                        //std::cout << "name " << name << " value " << value << std::endl;
                        YAML::Node* newNode = updateKeySequence2_2_TO_2_3(value);
                        newValues[name] = *newNode;
                        delete newNode;
                    }
                }
                //newValues["unnamed_" + std::to_string(i)] = *updateKeySequence2_2_TO_2_3(seq);
            }
            replaceValues = true;
        }

        if (replaceValues) {
            setConfigValue(config, "settings/macro/extraKeySequences", YAML::Node(newValues));
            //std::cout << "SetNewExtraValues\n";
        }
    }

    setConfigValue(config, "settings/windowOperations/windowVisualState/showingBackFromBackground/alwaysShowSpecificWindows", getConfigVectorWstring(config, "settings/fastReturnToForegroundWindows", getDefaultShowBackFromBackgroundList()));
    removeConfigValue(config, "settings/fastReturnToForegroundWindows");
}