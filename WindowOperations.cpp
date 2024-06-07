#include "WindowOperations.h"

#include "WindowSwitcherNew.h"

WindowGroup* getGroup(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        std::map<HWND, WindowGroup*>::iterator it = handleToGroup.find(hwnd);
        WindowGroup* wg = it->second;
        return wg;
    }
    //else return NULL;
}

void ShowOnlyMainInGroup(WindowGroup* wg) {
    std::vector<WindowInfo> others = (*wg).getOthers();
    std::vector<WindowInfo>::iterator iter;
    for (iter = others.begin(); iter != others.end(); ++iter) {
        //std::cout << *iter << " MINIMIZE" << endl;
        if (!hideNotMainWindows) {
            if (!checkHungWindow(iter->hwnd)) ShowWindow(iter->hwnd, SW_MINIMIZE);
        }
    }
    HWND c = (*wg).getCurrent();
    //std::cout << c << " SW_RESTORE" << endl;
    if (IsIconic(c)) {
        if (!checkHungWindow(c)) ShowWindow(c, SW_RESTORE); // what does it return if just z order far away but not minimized, maybe detect large windows above it
    }
}

void ShowOnlyMainInGroup(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        std::map<HWND, WindowGroup*>::iterator it = handleToGroup.find(hwnd);
        ShowOnlyMainInGroup(it->second);
    }
}

void shiftGroup(WindowGroup* group, int shift) {
    (*group).shiftWindows(shift);
    ShowOnlyMainInGroup(group);
}

void shiftGroup(HWND hwnd, int shift) {
    WindowGroup* wg = nullptr;
    wg = getGroup(hwnd); // delete pointers?
    shiftGroup(wg, shift);
}

void shiftAllGroups(int shift) {
    std::vector<WindowGroup*> used;
    std::map<HWND, WindowGroup*>::iterator it;

    for (it = handleToGroup.begin(); it != handleToGroup.end(); it++) {
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            shiftGroup(it->second, shift);
            used.push_back(it->second); // is * refering to it or it->second?
        }
    }
}

void shiftAllOtherGroups(int shift) {
    std::vector<WindowGroup*> used;
    std::map<HWND, WindowGroup*>::iterator it;

    HWND h = GetForegroundWindow();

    for (it = handleToGroup.begin(); it != handleToGroup.end(); it++) {
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            //std::cout << (h == it->first) << endl;
            //std::wcout << getWindowName(it->first) << endl;
            //std::wcout << getWindowName(h) << endl;
            //std::cout << it->second << endl;
            //std::cout << referenceToGroup[h] << endl << endl;
            //std::cout << (referenceToGroup[h] == it->second) << endl;
            if (handleToGroup[h] != it->second) shiftGroup(it->second, shift);
            used.push_back(it->second); // is * refering to it or it->second?
        }
    }

    SetForegroundWindow(h);

    //std::cout << '\n';
}

void deleteWindow(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        WindowGroup* wg = handleToGroup.find(hwnd)->second;
        (*wg).removeWindow(hwnd);
        handleToGroup.erase(handleToGroup.find(hwnd));
    }
}

void deleteWindowGroup(WindowGroup* wg) {
    /*for (auto& p : referenceToGroup)
        std::cout << p.first << " " << p.second << " " << endl;*/
    std::map<HWND, WindowGroup*>::iterator it;
    for (auto it = handleToGroup.begin(); it != handleToGroup.end(); ) {
        if (wg == it->second) {
            it = handleToGroup.erase(it);
        }
        else {
            ++it; // can have a bug
        }
    }
    //delete(wg); // ?
}

void deleteWindowGroup(HWND hwnd) {
    if (handleToGroup.count(hwnd)) {
        deleteWindowGroup(handleToGroup.find(hwnd)->second);
    }
}

void SwapVisibilityForAll() {
    std::vector<WindowGroup*> used;
    std::map<HWND, WindowGroup*>::iterator it;

    std::vector<HWND> main;
    std::vector<HWND> other;

    for (it = handleToGroup.begin(); it != handleToGroup.end(); it++) {
        //if (IsHungAppWindow(it->first)) std::cout << "Hang up window 8" << endl;
        if (used.empty() || !(find(used.begin(), used.end(), it->second) != used.end())) {
            WindowGroup* wg = it->second;
            main.push_back((*wg).getCurrent());
            std::vector<WindowInfo> others = (*wg).getOthers();
            std::vector<WindowInfo>::iterator iter;
            for (iter = others.begin(); iter != others.end(); ++iter) {
                other.push_back(iter->hwnd);
            }

            used.push_back(it->second); // is * refering to it or it->second?
        }
    }

    int minizimed = 0;
    for (auto& it : main) {
        if (IsIconic(it)) {
            minizimed++;
        }
    }

    for (auto& it : main) {
        if (!checkHungWindow(it)) {
            if (minizimed == main.size()) { // all hidden, show
                //if (IsHungAppWindow(it)) std::cout << "Hang up window 4" << endl;
                if (IsIconic(it)) {
                    //if (IsHungAppWindow(it)) std::cout << "Hang up window 5" << endl;
                    ShowWindow(it, SW_RESTORE);
                }
            }
            else {
                //if (IsHungAppWindow(it)) std::cout << "Hang up window 6" << endl;
                ShowWindow(it, SW_MINIMIZE);
            }
        }
    }

    for (auto& it : other) {
        //if (IsHungAppWindow(it)) std::cout << "Hang up window 7" << endl;
        if (!checkHungWindow(it)) ShowWindow(it, SW_MINIMIZE); // later add "hide" and "show" mode, after working window enumeration to get them back
    }
}

static BOOL CALLBACK enumWindowCallback(HWND hwnd, LPARAM lparam) {
    //int length = GetWindowTextLength(hwnd); // WindowName
    //wchar_t* buffer = new wchar_t[length + 1];
    //GetWindowTextW(hwnd, buffer, length + 1);
    //std::wstring ws(buffer);

    std::wstring windowName = getWindowName(hwnd);
    //std::wstring str(ws.begin(), ws.end()); // wstring to string

    // List visible windows with a non-empty title
    //if (IsWindowVisible(hwnd) && length != 0) {
    //    std::wcout << hwnd << ":  " << ws << endl;
    //}

    //if (ws == rx_name) {
    //    if (!checkHungWindow(hwnd)) ShowWindow(hwnd, SW_SHOW);
    //}
    //else {
    bool allow = false;
    for (auto& specified : settings.load()->showingBackFromBackgroundWindows) {
        if (specified.front() == '*' && specified.back() == '*') {
            std::wstring mainPart = specified.substr(1, specified.length() - 2);

            if (specified == windowName || (windowName.find(mainPart) != std::wstring::npos)) {
                allow = true;
                break;
            }
        }
        else if (windowName.find(specified) != std::wstring::npos) {
            allow = true;
            //std::cout << "RBX HERE " << str << endl;
        }
    }

    if (allow && !checkHungWindow(hwnd)) ShowWindow(hwnd, SW_SHOW); //std::cout << GetLastError() << endl;
    //}

    //EnumChildWindows(hwnd, enumWindowCallback, NULL); //TODO ?
    //std::cout << "Good\n";

    return TRUE;
}

void showAllRx() {
    //std::cout << "Enmumerating windows..." << endl;
    hideNotMainWindows = false;
    EnumWindows(enumWindowCallback, NULL);
    //std::cout << "Done\n";
}

void restoreAllConnected() {
    hideNotMainWindows = false;
    for (const auto& it : handleToGroup) {
        if (it.first != NULL && !checkHungWindow(it.first)) {
            ShowWindow(it.first, SW_MINIMIZE); // bad way // though not that much lol
            ShowWindow(it.first, SW_RESTORE);
        }
    }
}