#include "ConsoleManagement.h"

#include <iostream>
#include <iosfwd>
#include <sstream>
#include "Windows.h"

bool startWithConsole = true;
HWND g_hwndConsole = nullptr;
std::ostringstream preConsoleOutput;
std::streambuf* default_cout_buff = std::cout.rdbuf();

void createConsole() {
    // Redirect to default buffer
    std::cout.rdbuf(default_cout_buff);

    // Create a console window
    AllocConsole();

    // Redirect stdout to console
    FILE* pConsoleStream;
    freopen_s(&pConsoleStream, "CONOUT$", "w", stdout);
}

// private implementation
void showOrHideConsole(bool specific, bool state) {
    if ((specific && state) || (g_hwndConsole == nullptr || !IsWindowVisible(g_hwndConsole))) {
        if (g_hwndConsole == nullptr) {
            createConsole();
            std::cout << preConsoleOutput.str();
            preConsoleOutput.clear();
            g_hwndConsole = GetConsoleWindow();
        }
        else ShowWindow(g_hwndConsole, SW_SHOW);
    }
    else ShowWindow(g_hwndConsole, SW_HIDE);
}

void showOrHideConsole(bool state) {
    showOrHideConsole(true, state);
}

void showOrHideConsole() {
    showOrHideConsole(false, false);
}

void initConsole() {
    if (startWithConsole) showOrHideConsole();
}