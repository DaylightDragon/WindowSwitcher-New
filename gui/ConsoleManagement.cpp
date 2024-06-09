#include "ConsoleManagement.h"

#include <iostream>
#include <iosfwd>
#include <sstream>
#include "Windows.h"

class CustomBuffer : public std::streambuf {
public:
    int_type overflow(int_type c) override {
        if (c != traits_type::eof()) {
            buffer += c;
        }
        return c;
    }

    std::string getBuffer() const {
        return buffer;
    }

private:
    std::string buffer;
};

bool startWithConsole = true;
HWND console_window = nullptr;
std::streambuf* default_cout_buff = std::cout.rdbuf();
CustomBuffer buffer;

void createConsole() {
    // Redirect to default buffer
    std::cout.rdbuf(default_cout_buff);

    // Create a console window
    AllocConsole();

    // Redirect stdout to console
    FILE* pConsoleStream;
    freopen_s(&pConsoleStream, "CONOUT$", "w", stdout);
}

void activateOwnBuffer() {
    if (!isConsoleAllocated()) {
        std::cout.rdbuf(&buffer);
    }
}

// private implementation
void showOrHideConsole(bool specific, bool state) {
    if ((specific && state) || (console_window == nullptr || !IsWindowVisible(console_window))) {
        if (console_window == nullptr) {
            createConsole();
            std::cout << buffer.getBuffer();
            //<< preConsoleOutput.str();
            //preConsoleOutput.clear();
            console_window = GetConsoleWindow();
        }
        else ShowWindow(console_window, SW_SHOW);
    }
    else {
        ShowWindow(console_window, SW_HIDE);
        activateOwnBuffer();
    }
}

void showOrHideConsole(bool state) {
    showOrHideConsole(true, state);
}

void showOrHideConsole() {
    showOrHideConsole(false, false);
}

void initConsole() {
    activateOwnBuffer();
    if (startWithConsole) showOrHideConsole();
}

bool isConsoleAllocated() {
    return console_window != nullptr;
}