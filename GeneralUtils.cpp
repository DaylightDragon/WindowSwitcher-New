using namespace std;

#include <iostream>

//getProgramFolderPath(string(argv[0]));

string getProgramFolderPath(const string& programPath) {
    size_t lastSlashIndex = programPath.find_last_of("/\\\\");
    if (lastSlashIndex != string::npos) {
        return programPath.substr(0, lastSlashIndex);
    }
    return "";
}