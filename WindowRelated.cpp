#include <Windows.h>
#include <vector>
#include <chrono>

#include "WindowSwitcherNew.h"
#include "WindowOperations.h"

// ----- WindowInfo -----

WindowInfo::WindowInfo(HWND hwnd) {
    this->hwnd = hwnd;
}

void WindowInfo::refreshTimestamp() {
    lastSequenceInputTimestamp = std::chrono::steady_clock::now();
}

bool WindowInfo::operator==(const WindowInfo& other) const {
    return hwnd == other.hwnd && lastSequenceInputTimestamp == other.lastSequenceInputTimestamp;
}

// ----- WindowGroup -----

std::vector<WindowInfo> WindowGroup::getOthers() {
    std::vector<WindowInfo> v;
    removeClosedWindows();
    if (&windows == nullptr || windows.size() == 0) return v;
    for (int i = 0; i < windows.size(); i++) {
        if (i != index) v.push_back(WindowInfo(windows[i]));
    }
    return v;
}

HWND WindowGroup::getCurrent() {
    removeClosedWindows();
    if (&windows == NULL || windows.size() == 0) return NULL;
    return windows[index].hwnd;
}

WindowInfo* WindowGroup::getWindowInfoFromHandle(HWND hwnd) {
    for (auto& it : windows) {
        if (it.hwnd == hwnd) return &it;
    }
    return nullptr;
}

bool WindowGroup::containsWindow(HWND hwnd) {
    removeClosedWindows();
    return (find(windows.begin(), windows.end(), hwnd) != windows.end());
}

void WindowGroup::addWindow(HWND hwnd) {
    if (containsWindow(hwnd)) return;
    removeClosedWindows();
    windows.push_back(hwnd);
    if (hideNotMainWindows && index != windows.size() - 1) {
        if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_HIDE);
    }
    //testPrintHwnds();
}

void WindowGroup::removeWindow(HWND hwnd) {
    if (&windows == NULL || windows.size() == 0) return;
    removeClosedWindows();
    //std::cout << "removeWindow " << hwnds.size() << " " << hwnd;
    bool main = false;
    if (hwnd == windows[index].hwnd) main = true;
    handleToGroup.erase(handleToGroup.find(hwnd));
    windows.erase(std::remove(windows.begin(), windows.end(), hwnd), windows.end());
    if (windows.size() == 0) deleteWindowGroup(this);
    if (main) shiftWindows(-1);
    fixIndex();
}

void WindowGroup::shiftWindows(int times) {
    if (&windows == NULL) return;
    removeClosedWindows();
    if (windows.size() == 0) return;
    HWND oldHwnd = getCurrent();
    if (times >= 0) {
        index = (index + times) % windows.size();
    }
    else {
        index = (windows.size() * (-1 * times) + index + times) % windows.size();
    }
    if (hideNotMainWindows) {
        //std::cout << oldHwnd << " " << getCurrent() << endl;
        HWND newHwnd = getCurrent();
        if (!checkHungWindow(newHwnd)) ShowWindow(newHwnd, SW_SHOW);
        if (!checkHungWindow(oldHwnd)) ShowWindow(oldHwnd, SW_HIDE);
    }
}

void WindowGroup::fixIndex() {
    if (&windows == NULL) return;
    if (windows.size() == 0) index = 0;
    else index = index % windows.size();
}

void WindowGroup::removeClosedWindows() {
    if (&windows == NULL || windows.size() == 0) return;
    for (int i = 0; i < windows.size(); i++) {
        if (&windows[i] && !IsWindow(windows[i].hwnd)) {
            handleToGroup.erase(handleToGroup.find(windows[i].hwnd));
            windows.erase(windows.begin() + i);
        }
    }
    fixIndex();
}

void WindowGroup::hideOrShowOthers() {
    std::vector<WindowInfo> others = getOthers();
    for (auto& it : others) {
        if (!checkHungWindow(it.hwnd)) {
            if (hideNotMainWindows) {
                ShowWindow(it.hwnd, SW_HIDE);
            }
            else {
                ShowWindow(it.hwnd, SW_SHOW);
            }
        }
    }
}

void WindowGroup::testPrintHwnds() {
    for (auto& hwnd : windows) {
        std::cout << "HWND " << &hwnd.hwnd << '\n';
    }
}

int WindowGroup::size() {
    return windows.size();
}

void WindowGroup::moveInOrder(HWND* hwnd, int times) {}

// ----- Nothing -----
