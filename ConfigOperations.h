#pragma once
using namespace std;
#include <string>

YAML::Node loadYaml(string programPath, const std::string& folderPath, const std::string& fileName, bool& wrongConfig);
void saveYaml(YAML::Node &config, string path);
void addConfigLoadingMessage(string message);
void printConfigLoadingMessages();
YAML::Node getConfigValue(const YAML::Node &config, string path);
void setConfigValue(const YAML::Node &config, string path, string value);
void setConfigValue(const YAML::Node &config, string path, int value);
void setConfigValue(const YAML::Node &config, string path, long value);
void setConfigValue(const YAML::Node &config, string path, float value);
void setConfigValue(const YAML::Node &config, string path, bool value);
void setConfigValue(const YAML::Node &config, string path, vector<string> value);
void setConfigValue(const YAML::Node &config, string path, vector<int> value);
void setConfigValue(const YAML::Node &config, string path, vector<long> value);
void setConfigValue(const YAML::Node &config, string path, vector<float> value);
void setConfigValue(const YAML::Node &config, string path, char* value);
void setConfigValue(const YAML::Node &config, string path, const char* value);
void setConfigValue(const YAML::Node &config, string path, vector<YAML::Node> value);
void setConfigValue(const YAML::Node &config, string path, YAML::Node value);
int getConfigInt(const YAML::Node &config, string key, int defaultValue);
long getConfigLong(const YAML::Node &config, string key, long defaultValue);
float getConfigFloat(const YAML::Node &config, string key, float defaultValue);
bool getConfigBool(const YAML::Node &config, string key, bool defaultValue);
vector<string> getConfigVectorString(const YAML::Node &config, string key, vector<string> defaultValue);
string getConfigString(const YAML::Node &config, string key, string defaultValue);
void removeConfigValue(YAML::Node &config, string path, bool throwException);
void removeConfigValue(YAML::Node &config, string path);
void copyConfigValue(YAML::Node &config, string key, string newKey, bool throwException);
void copyConfigValue(YAML::Node &config, string key, string newKey);
void moveConfigValue(YAML::Node &config, string key, string newKey, bool throwException);
void moveConfigValue(YAML::Node &config, string key, string newKey);
bool checkExists(const YAML::Node &config, string key);
