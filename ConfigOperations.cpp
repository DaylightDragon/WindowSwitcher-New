using namespace std;

#include "WindowSwitcherNew.h"
#include "GeneralUtils.h"
#include <yaml-cpp/yaml.h>
#include <fstream>
#include <iostream>
#include <string>
#include <optional>
#include <cstdlib>
#include <sys/stat.h>
#include <filesystem>
#include <map>
//#include <optio

#define _SILENCE_EXPERIMENTAL_FILESYSTEM_DEPRECATION_WARNING
#include <experimental/filesystem>

namespace fs = std::experimental::filesystem;

#ifdef _WIN32
#include <direct.h>
#else
#include <sys/stat.h>
#endif

bool createDirectory(const std::string& path) {
    int status;
#ifdef _WIN32
    status = _mkdir(path.c_str());
#else
    status = mkdir(path.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
#endif
    return status == 0;
}

bool createDirectories(const std::string& path) {
    if (path.empty()) {
        return true;
    }

    size_t found = path.find_last_of("/\\\\");
    if (found == std::string::npos) {
        return true;
    }

    std::string upperPath = path.substr(0, found);
    if (createDirectories(upperPath)) {
        return createDirectory(path);
    }

    return false;
}

string configLoadingMessages = "";

void addConfigLoadingMessage(string message) {
    if (!configLoadingMessages.empty()) configLoadingMessages += "\n";
    configLoadingMessages += message;
}

void printConfigLoadingMessages() {
    if (!configLoadingMessages.empty()) {
        cout << "Loading config warnings and errors:" << endl << configLoadingMessages << endl << endl;
        configLoadingMessages = "";
    }
}

class OptionalYaml : YAML::Node {
private:
    bool valid = true;
public:
    bool isValid() {
        return valid;
    }
    /*YAML::Node value;
    bool valid = true;

    public:
        bool isValid() {
            return valid;
        }

        template <typename T>
        T as() {
            return value.as<T>();
        }

        YAML::const_iterator begin() {
            return value.begin();
        }

        YAML::const_iterator end() {
            return value.end();
        }

        template <typename Key, typename Value>
        inline void YAML::Node::force_insert(const Key& key, const Value& value) {
            return value.force_insert<Key, Value>(key, value);
        }*/
};

YAML::Node loadYaml(string programPath, const std::string& folderPath, const std::string& fileName, bool& wrongConfig) {
    //struct stat info;
    //if (stat(directory.c_str(), &info) != 0) {
    //    // Create the directory
    //    int status = mkdir(directory.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
    //    if (status != 0) {
    //        // Directory creation failed, handle the error
    //        std::cerr << "Error creating directory: " << directory << std::endl;
    //        return;
    //    }
    //}

    /*fs::path absolutePath = fs::current_path() / relativePath;
    fs::create_directories(absolutePath.parent_path());

    // Create the file
    std::ofstream file(absolutePath);
    if (file) {
        file << "Hello, World!";
        file.close();
        std::cout << "File created successfully!" << std::endl;
    }
    else {
        std::cout << "Failed to create file!" << std::endl;
    }*/

    /*std::string fileName = folderPath.substr(folderPath.find_last_of("/") + 1);
    std::string filePath = folderPath.substr(0, folderPath.find_last_of("/"));
    std::ofstream file(filePath + "/" + fileName);
    if (createDirectories(filePath)) {
        std::ofstream file(filePath + "/" + fileName);
        // File creation success
        file.close();
    }
    else {
        // Error creating folders
        std::cout << "Unable to create folders!" << std::endl;
        return YAML::Node();
    }*/

    //std::ifstream file(fileName);
    //if (!file) {
        //cout << "Can't open the Yaml" << endl;
    //}

    string folderFullPath = getProgramFolderPath(programPath) + "/" + folderPath;
    string fullPath = getProgramFolderPath(programPath) + "/" + folderPath + "/" + fileName;

    if (getProgramFolderPath(programPath) == "C:\Windows\System32") {
        cout << "The program was launched in System32!" << endl; // idk if the path is the working direcotry, but I had such issues in Java
    }

    if (!fs::is_directory(folderFullPath) || !fs::exists(folderFullPath)) { // Check if src folder exists
        fs::create_directory(folderFullPath); // create src folder
    }

    ifstream fileTest(fullPath);
    if (!fileTest) {
        fstream file;
        file.open(folderPath + "/" + fileName, fstream::out);
        file.close();
    }

    std::ifstream file2(fullPath);
    //if (!file2) {
    //    cout << "Can't open the Yaml" << endl;
    //}

    YAML::Node config;
    try {
        config = YAML::Load(file2);
    }
    catch (const std::exception& e) {
        addConfigLoadingMessage("WARNING | Error on reading the config file (not processing!):\nERROR | " + string(e.what()));
        wrongConfig = true;
        return config;
    }

    //cout << "Loaded: '" << config << "'" << endl;
    //cout << "Defined: " << config.IsDefined() << endl;
    //if (!config.IsDefined()) config = YAML::Node();
    //cout << "Null, !Defined: " << config.IsNull() << " " << !config.IsDefined() << endl;
    if (config.IsNull() || !config.IsDefined()) config = YAML::Node();
    return config;
}

void saveYaml(YAML::Node &config, string path) {
    //cout << path << endl;
    YAML::Emitter emitter;
    emitter.SetIndent(4); // Sets the indentation level for nested nodes
    emitter.SetMapFormat(YAML::Block); // Prints maps as a compact, single line 

    std::ofstream file(path);
    emitter << config;
    file << emitter.c_str();
    file.close();
}

YAML::Node getConfigValue(const YAML::Node &config, string path) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size(); i++) {
        node = &(*node)[splittedStrings[i]];
    }

    return *node;
}

void setConfigValue(const YAML::Node &config, string path, string value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, vector<YAML::Node> value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, YAML::Node value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = YAML::Clone(value);

    /*if (value.IsMap()) (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<map<string, YAML::Node>>();
    else if (value.IsSequence()) (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<vector<YAML::Node>>();
    else {
        try {
            (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<int>();
            return;
        } catch (const YAML::Exception& e) {}
        try {
            (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<long>();
            return;
        } catch (const YAML::Exception& e) {}
        try {
            (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<float>();
            return;
        } catch (const YAML::Exception& e) {}
        try {
            (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<bool>();
            return;
        }
        catch (const YAML::Exception& e) {}
        try {
            (*node)[splittedStrings[splittedStrings.size() - 1]] = value.as<string>();
            return;
        }
        catch (const YAML::Exception& e) {}
        (*node)[splittedStrings[splittedStrings.size() - 1]] = YAML::Clone(value);
    }*/

    //cout << (*node)[splittedStrings[splittedStrings.size() - 1]] << endl;
}

void setValueRecursively(YAML::Node node, YAML::Node value) {

}

void setConfigValue(const YAML::Node &config, string path, int value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, long value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, float value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, bool value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, vector<string> value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, vector<int> value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, vector<long> value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, vector<float> value) {
    std::vector<std::string> splittedStrings;

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;
}

void setConfigValue(const YAML::Node &config, string path, char* value) {
    setConfigValue(config, path, string(value));
}

void setConfigValue(const YAML::Node &config, string path, const char* value) {
    setConfigValue(config, path, string(value));
}

int getConfigInt(const YAML::Node &config, string key, int defaultValue) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | WARNING | Unable to read config value \"" + key + "\", using default value \"" + to_string(defaultValue) + "\" instead.");
        return defaultValue;
    }
    if (!data.IsDefined()) {
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    if (data.Type() != YAML::NodeType::Scalar) {
        addConfigLoadingMessage("WARNING | Detected non-scalar value at INT type key \"" + key + "\"! Replacing it with the default value \"" + to_string(defaultValue) + "\".");
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    char* pEnd = NULL;
    int v = strtol(data.as<string>().c_str(), &pEnd, 10);
    if (*pEnd != '\0') {
        addConfigLoadingMessage("WARNING | Wrong format for INT type at key \"" + key + "\"! Using default value \"" + to_string(defaultValue) + "\" instead.");
        return defaultValue;
    }
    else {
        return v;
    }
}

long getConfigLong(const YAML::Node &config, string key, long defaultValue) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | Unable to read config value \"" + key + "\", using default value \"" + to_string(defaultValue) + "\" instead.");
        return defaultValue;
    }
    if (!data.IsDefined()) {
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    if (data.Type() != YAML::NodeType::Scalar) {
        addConfigLoadingMessage("WARNING | Detected non-scalar value at LONG type key \"" + key + "\"! Replacing it with the default value \"" + to_string(defaultValue) + "\".");
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    char* pEnd = NULL;
    long v = strtol(data.as<string>().c_str(), &pEnd, 10);
    if (*pEnd != '\0') {
        addConfigLoadingMessage("WARNING | Wrong format for LONG type at key \"" + key + "\"! Using default value \"" + to_string(defaultValue) + "\" instead.");
        return defaultValue;
    }
    else {
        return v;
    }
}

float getConfigFloat(const YAML::Node &config, string key, float defaultValue) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | Unable to read config value \"" + key + "\", using default value \"" + to_string(defaultValue) + "\" instead.");
        return defaultValue;
    }
    if (!data.IsDefined()) {
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    if (data.Type() != YAML::NodeType::Scalar) {
        addConfigLoadingMessage("WARNING | Detected non-scalar value at FLOAT type key \"" + key + "\"! Replacing it with the default value \"" + to_string(defaultValue) + "\".");
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    char* pEnd = NULL;
    float v = strtof(data.as<string>().c_str(), &pEnd);
    if (*pEnd != '\0') {
        addConfigLoadingMessage("WARNING | Wrong format for FLOAT type at key \"" + key + "\"! Using default value \"" + to_string(defaultValue) + "\" instead.");
        return defaultValue;
    }
    else {
        return v;
    }
}

bool stringToBool(const std::string& str) {
    // Convert the string to lowercase for case-insensitive comparison
    std::string lowercaseStr;
    for (const char& c : str) {
        lowercaseStr += std::tolower(c);
    }

    // Check if the string matches true or false
        if (lowercaseStr == "true" || lowercaseStr == "yes" || lowercaseStr == "1") {
            return true;
        }
        else if (lowercaseStr == "false" || lowercaseStr == "no" || lowercaseStr == "0") {
            return false;
        }

    // Throw an exception or return a default value if the string is not a valid bool
    throw std::invalid_argument("Invalid bool value: " + str);
}

string boolToStrDeco(bool b) {
    if (b) return "true";
    return "false";
}

bool getConfigBool(const YAML::Node &config, string key, bool defaultValue) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | Unable to read config value \"" + key + "\", using default value \"" + boolToStrDeco(defaultValue) + "\" instead.");
        return defaultValue;
    }
    if (!data.IsDefined()) {
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    if (data.Type() != YAML::NodeType::Scalar) {
        addConfigLoadingMessage("WARNING | Detected non-scalar value at BOOLEAN type key \"" + key + "\"! Replacing it with the default value \"" + boolToStrDeco(defaultValue) + "\".");
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    //char* pEnd = NULL;
    //bool v = static_cast<bool>(strtol(data.as<string>().c_str(), &pEnd, 10));
    bool v;
    try {
        v = stringToBool(data.as<string>().c_str());
    }
    catch (const std::invalid_argument& e) {
    //if (*pEnd != '\0') {
        addConfigLoadingMessage("WARNING | Wrong format for BOOLEAN type at key \"" + key + "\"! Using default value \"" + boolToStrDeco(defaultValue) + "\" instead.");
        return defaultValue;
    }
    return v;
}

vector<string> getConfigVectorString(const YAML::Node &config, string key, vector<string> defaultValue) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | Unable to read config value \"" + key + "\", using default value instead.");
        return defaultValue;
    }
    if (!data.IsDefined()) {
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    if (data.Type() != YAML::NodeType::Sequence) {
        addConfigLoadingMessage("WARNING | Detected non-sequence value at SEQUENCE type key \"" + key + "\"! Replacing it with the default value.");
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }

    vector<string> v;

    for (YAML::const_iterator it = data.begin(); it != data.end(); ++it) {
        string value = it->as<string>();
        v.push_back(value);
    }
    return v;
}

string getConfigString(const YAML::Node &config, string key, string defaultValue) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        addConfigLoadingMessage("WARNING | Unable to read config value \"" + key + "\", using default value \"" + defaultValue + "\" instead.");
        return defaultValue;
    }

    if (!data.IsDefined()) {
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    if (data.Type() != YAML::NodeType::Scalar) {
        addConfigLoadingMessage("WARNING | Detected non-scalar value at STRING type key \"" + key + "\"! Replacing it with the default value \"" + defaultValue + "\".");
        setConfigValue(config, key, defaultValue);
        return defaultValue;
    }
    char* pEnd = NULL;
    string v = data.as<string>();
    return v;
}

void removeConfigValue(YAML::Node &config, string path, bool throwException) {
    //cout << "REMOVE START" << endl;
    std::vector<std::string> splittedStrings;
    std::vector<YAML::Node> nodes;
    nodes.push_back(config);

    std::stringstream ss(path);
    std::string token;

    while (std::getline(ss, token, '/')) {
        splittedStrings.push_back(token);
    }

    if (splittedStrings.size() == 1) {
        config.remove(splittedStrings[0]);
        return;
    }
    else if (splittedStrings.size() == 0) {
        if (throwException) {
            string text = "No such value! (" + path + ")";
            throw(exception(text.c_str()));
        } else return;
    }

    for (int max = splittedStrings.size() - 1; max > 0; max--) {
        string key = "";
        for (int i = 0; i < max; i++) {
            key += splittedStrings[i];
            if (i + 1 < max) key += "/";
        }

        //cout << key << endl;

        YAML::Node data;
        try {
            data = getConfigValue(config, key);
        }
        catch (const YAML::Exception& e) {
            if (throwException) throw e;
            else return;
        }

        //cout << "'" << data << "'" << endl << "Looking for " << splittedStrings[max] << endl;
        //cout << "First choice: " << (max == splittedStrings.size() - 1) << endl;

        if (max == splittedStrings.size() - 1) data.remove(splittedStrings[splittedStrings.size() - 1]);
        else {
            //cout << (data[splittedStrings[max]].IsNull()) << endl;
            //cout << "|||||" << data[splittedStrings[max]] << "|||||" << " IsMap: " << data[splittedStrings[max]].IsMap() << endl;
            //if (data[splittedStrings[max]].IsMap()) cout << data[splittedStrings[max]].as<std::map<string, YAML::Node>>().empty() << endl;
            //cout << "Second choice: " << (data[splittedStrings[max]].IsMap() && data[splittedStrings[max]].as<std::map<string, YAML::Node>>().empty()) << endl;
            if (data[splittedStrings[max]].IsMap() && data[splittedStrings[max]].as<std::map<string, YAML::Node>>().empty()) {
                data.remove(splittedStrings[max]);
            }
            else {
                //cout << endl;
                return;
            }
            //cout << splittedStrings[max] << endl;
        }

        //cout << endl;
    }

    /*YAML::Node* node = new YAML::Node();
    *node = config;

    for (int i = 0; i < splittedStrings.size() - 1; i++) {
        node = &(*node)[splittedStrings[i]];
    }

    (*node)[splittedStrings[splittedStrings.size() - 1]] = value;*/
}

void removeConfigValue(YAML::Node &config, string path) {
    removeConfigValue(config, path, false);
}

void copyConfigValue(YAML::Node &config, string key, string newKey, bool throwException) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        if (throwException) throw e;
        else return;
    }
    setConfigValue(config, newKey, data);
}

void copyConfigValue(YAML::Node &config, string key, string newKey) {
    copyConfigValue(config, key, newKey, false);
}

void moveConfigValue(YAML::Node &config, string key, string newKey, bool throwException) {
    try {
        copyConfigValue(config, key, newKey, true);
    } catch (const YAML::Exception& e) {
        if (throwException) throw e;
        else return;
    }
    removeConfigValue(config, key);
}

void moveConfigValue(YAML::Node &config, string key, string newKey) {
    moveConfigValue(config, key, newKey, false);
}

bool checkExists(const YAML::Node& config, string key) {
    YAML::Node data;
    try {
        data = getConfigValue(config, key);
    }
    catch (const YAML::Exception& e) {
        return false;
    }
    return data.IsDefined();
}
