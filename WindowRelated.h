#pragma once

#include <Windows.h>
#include <vector>
#include <chrono>

extern std::chrono::steady_clock::time_point globalLastSequenceInputTimestamp;

struct WindowInfo {
	HWND hwnd;
	std::chrono::steady_clock::time_point lastSequenceInputTimestamp;

	WindowInfo(HWND hwnd);
	void refreshTimestamp();
	bool operator==(const WindowInfo& other) const;
};

class WindowGroup {
private:
	std::vector<WindowInfo> windows;
	int index = 0;
public:
	std::vector<WindowInfo> getOthers();
	HWND getCurrent();
	WindowInfo* getWindowInfoFromHandle(HWND hwnd);
	bool containsWindow(HWND hwnd);
	void addWindow(HWND hwnd);
	void removeWindow(HWND hwnd);
	void shiftWindows(int times);
	void fixIndex();
	void removeClosedWindows();
	void hideOrShowOthers();
	void testPrintHwnds();
	int size();
	void moveInOrder(HWND* hwnd, int times);
};