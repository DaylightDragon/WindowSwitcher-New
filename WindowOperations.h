#pragma once

WindowGroup* getGroup(HWND hwnd);
void ShowOnlyMainInGroup(WindowGroup* wg);
void ShowOnlyMainInGroup(HWND hwnd);
void shiftGroup(WindowGroup* group, int shift);
void shiftGroup(HWND hwnd, int shift);
void shiftAllGroups(int shift);
void shiftAllOtherGroups(int shift);
void deleteWindow(HWND hwnd);
void deleteWindowGroup(WindowGroup* wg);
void deleteWindowGroup(HWND hwnd);
void SwapVisibilityForAll();
static BOOL CALLBACK enumWindowCallback(HWND hwnd, LPARAM lparam);
void showAllRx();
void restoreAllConnected();