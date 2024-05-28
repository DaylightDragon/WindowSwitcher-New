#pragma once

#include <string>
#include <yaml-cpp/yaml.h>

YAML::Node loadYaml(std::string programPath, const std::string& folderPath, const std::string& fileName, bool& wrongConfig);
void saveYaml(YAML::Node &config, std::string path);
void addConfigLoadingMessage(std::string message);
void printConfigLoadingMessages();
YAML::Node getConfigValue(const YAML::Node &config, std::string path);
void setConfigValue(const YAML::Node &config, std::string path, std::string value);
void setConfigValue(const YAML::Node &config, std::string path, int value);
void setConfigValue(const YAML::Node &config, std::string path, long value);
void setConfigValue(const YAML::Node &config, std::string path, float value);
void setConfigValue(const YAML::Node &config, std::string path, bool value);
void setConfigValue(const YAML::Node &config, std::string path, std::vector<std::string> value);
void setConfigValue(const YAML::Node &config, std::string path, std::vector<int> value);
void setConfigValue(const YAML::Node &config, std::string path, std::vector<long> value);
void setConfigValue(const YAML::Node &config, std::string path, std::vector<float> value);
void setConfigValue(const YAML::Node &config, std::string path, char* value);
void setConfigValue(const YAML::Node &config, std::string path, const char* value);
void setConfigValue(const YAML::Node &config, std::string path, std::vector<YAML::Node> value);
void setConfigValue(const YAML::Node &config, std::string path, YAML::Node value);
int getConfigInt(const YAML::Node &config, std::string key, int defaultValue);
long getConfigLong(const YAML::Node &config, std::string key, long defaultValue);
float getConfigFloat(const YAML::Node &config, std::string key, float defaultValue);
bool getConfigBool(const YAML::Node &config, std::string key, bool defaultValue);
std::vector<std::string> getConfigVectorString(const YAML::Node &config, std::string key, std::vector<std::string> defaultValue);
std::string getConfigString(const YAML::Node &config, std::string key, std::string defaultValue);
void removeConfigValue(YAML::Node &config, std::string path, bool throwException);
void removeConfigValue(YAML::Node &config, std::string path);
void copyConfigValue(YAML::Node &config, std::string key, std::string newKey, bool throwException);
void copyConfigValue(YAML::Node &config, std::string key, std::string newKey);
void moveConfigValue(YAML::Node &config, std::string key, std::string newKey, bool throwException);
void moveConfigValue(YAML::Node &config, std::string key, std::string newKey);
bool checkExists(const YAML::Node &config, std::string key);
