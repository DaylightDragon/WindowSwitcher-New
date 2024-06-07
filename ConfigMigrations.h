#pragma once

#include "yaml-cpp/yaml.h"

void updateConfig2_0_TO_2_1(YAML::Node& config, bool wrongConfig, bool firstUpdate);
YAML::Node* updateKeySequence2_2_TO_2_3(YAML::Node& sequence);
void updateConfig2_2_TO_2_3(YAML::Node& config, bool wrongConfig, bool firstUpdate);