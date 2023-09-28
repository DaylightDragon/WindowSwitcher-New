#pragma once

YAML::Node loadYaml(string programPath, const std::string& folderPath, const std::string& fileName);
void saveYaml(YAML::Node config, string path);
void addConfigLoadingMessage(string message);
void printConfigLoadingMessages();
YAML::Node getConfigValue(const YAML::Node config, string path);
void setConfigValue(const YAML::Node config, string path, string value);
void setConfigValue(const YAML::Node config, string path, int value);
void setConfigValue(const YAML::Node config, string path, long value);
void setConfigValue(const YAML::Node config, string path, float value);
void setConfigValue(const YAML::Node config, string path, bool value);
void setConfigValue(const YAML::Node config, string path, vector<string> value);
void setConfigValue(const YAML::Node config, string path, vector<int> value);
void setConfigValue(const YAML::Node config, string path, vector<long> value);
void setConfigValue(const YAML::Node config, string path, vector<float> value);
void setConfigValue(const YAML::Node config, string path, char* value);
void setConfigValue(const YAML::Node config, string path, const char* value);
int getConfigInt(YAML::Node config, string key, int defaultValue);
long getConfigLong(YAML::Node config, string key, long defaultValue);
float getConfigFloat(YAML::Node config, string key, float defaultValue);
bool getConfigBool(YAML::Node config, string key, bool defaultValue);
vector<string> getConfigVectorString(YAML::Node config, string key, vector<string> defaultValue);
string getConfigString(YAML::Node config, string key, string defaultValue);
