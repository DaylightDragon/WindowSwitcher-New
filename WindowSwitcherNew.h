#pragma once

#include <iostream>
#include <map>
#include <string>

std::map<std::string, int> getMapOfKeys();
std::atomic<bool>& getStopMacroInput();
std::string getCurrentConfigVersion();
