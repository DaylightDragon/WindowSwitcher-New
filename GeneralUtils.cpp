using namespace std;

#include <iostream>
#include <cstdlib>
#include <ctime>

//getProgramFolderPath(string(argv[0]));
std::string getProgramFolderPath(const std::string& programPath) {
    size_t lastSlashIndex = programPath.find_last_of("/\\\\");
    if (lastSlashIndex != string::npos) {
        return programPath.substr(0, lastSlashIndex);
    }
    return "";
}

void initializeRandom() {
    //srand(time(NULL));
    srand(static_cast<unsigned>(time(0)));
}

/*int randomizeByPercentage(int value, int percentage) {
    double lowerBound = value * (1 - percentage / 100.0);
    double upperBound = value * (1 + percentage / 100.0);
    int randomizedValue = static_cast<int>(lowerBound + rand() % (static_cast<int>(upperBound - lowerBound) + 1));
    return randomizedValue;
}*/

int randomizeValue(int value, int percentage, int maxDifference) {
    int maxDiff = (value * percentage) / 100;
    if (maxDiff > maxDifference) maxDiff = maxDifference;
    int randomDiff = rand() % (2 * maxDiff + 1) - maxDiff;
    int randomizedValue = value + randomDiff;
    return randomizedValue;
}