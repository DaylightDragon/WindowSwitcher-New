#pragma once

#include <Windows.h>

HWND CreateOverlayWindow(HINSTANCE hInstance, const TCHAR* windowName, int opacity);
void redrawOverlay();
void repositionTheOverlay();
void toggleOverlayVisibility();
bool getOverlayVisibility();

enum OverlayState {
    NOT_INITIALIZED,
    NORMAL,
    SEQUENCE_READY,
    SEQUENCE_ACTIVE,
    MACRO_DISABLED
};

void setOverlayState(OverlayState newState);
