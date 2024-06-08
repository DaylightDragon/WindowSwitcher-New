#pragma once

#include "WindowSwitcherNew.h"
#include "ConfigMigrations.h"
#include "CustomHotkeys.h"
#include "GeneralUtils.h"

void rememberInitialPermanentSettings();
void resetMainSequence(const YAML::Node& config);
void readNewMainSequence(const YAML::Node& config);
void readDefaultMainSequence(const YAML::Node& config);
void resetInterruptionManager(const YAML::Node& config);
void resetSettings(const YAML::Node& config);
void readNewSettings(const YAML::Node& config);
void readNewInterruptionManager(const YAML::Node& config);
void readDefaultInterruptionManager(const YAML::Node& config);
YAML::Node& loadSettingsConfig(YAML::Node& config, bool wasEmpty, bool wrongConfig);
bool loadConfig(ConfigType type);
void reloadConfigs();