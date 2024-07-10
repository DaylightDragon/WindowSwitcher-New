#include <algorithm>
#include <iostream>
#include <cstdlib>
#include <ctime>
#include <Windows.h>
#include <gdiplus.h>

//getProgramFolderPath(string(argv[0]));
std::string getProgramFolderPath(const std::string& programPath) {
    size_t lastSlashIndex = programPath.find_last_of("/\\\\");
    if (lastSlashIndex != std::string::npos) {
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

RECT GetTaskbarSize() {
    RECT taskbarRect;
    HWND taskbar = FindWindow("Shell_TrayWnd", NULL);

    GetWindowRect(taskbar, &taskbarRect);

    RECT result = { 0 };

    //std::cout << "Taskbar: " << taskbarRect.left << " X " << taskbarRect.right << ", Y " << taskbarRect.top << " " << taskbarRect.bottom << std::endl;

    APPBARDATA abd = { sizeof(APPBARDATA) };
    SHAppBarMessage(ABM_GETTASKBARPOS, &abd);
    if (abd.uEdge == ABE_BOTTOM) {
        result.bottom = taskbarRect.bottom - taskbarRect.top;
    }
    else if (abd.uEdge == ABE_RIGHT) {
        result.right = taskbarRect.right - taskbarRect.left;
    }
    else if (abd.uEdge == ABE_TOP) {
        result.top = taskbarRect.top - taskbarRect.bottom;
    }
    else if (abd.uEdge == ABE_LEFT) {
        result.left = taskbarRect.left - taskbarRect.right;
    }

    return result;
}

// Структура для передачи данных в функцию EnumMonitorCallback
struct MonitorInfoData {
    int monitorIndex;
    MONITORINFO info;
    int currentMonitorIndex;
};

// Функция для перебора мониторов
BOOL EnumMonitorCallback(HMONITOR hMonitor, HDC hdcMonitor, LPRECT lprcMonitor, LPARAM dwData) {
    // Получаем структуру MONITORINFO для текущего монитора
    MONITORINFO monitorInfo;
    monitorInfo.cbSize = sizeof(MONITORINFO);
    GetMonitorInfo(hMonitor, &monitorInfo);

    // Получаем информацию о нужном мониторе (переданную через dwData)
    auto data = reinterpret_cast<MonitorInfoData*>(dwData);

    // Проверяем, совпадает ли индекс текущего монитора с требуемым
    if (data->currentMonitorIndex == data->monitorIndex) {
        // Копируем информацию о мониторе в структуру info
        memcpy(&data->info, &monitorInfo, sizeof(MONITORINFO));
        // Возвращаем FALSE, чтобы прекратить перебор мониторов
        return FALSE;
    }

    // Увеличиваем счетчик текущего монитора
    ++data->currentMonitorIndex;
    // Продолжаем перебирать мониторы
    return TRUE;
}

MONITORINFO GetMonitorInfoByIndex(int monitorIndex) {
    // Получаем количество подключенных мониторов
    int monitorCount = GetSystemMetrics(SM_CMONITORS);

    // Гарантируем корректность индекса
    if (monitorIndex < 0) monitorIndex = 0;
    if (monitorIndex >= monitorCount) monitorIndex = monitorCount - 1;

    // Создаем структуру для передачи данных
    MonitorInfoData data;
    data.monitorIndex = monitorIndex;
    data.currentMonitorIndex = 0;
    data.info.cbSize = sizeof(MONITORINFO);

    // Перебираем все мониторы
    EnumDisplayMonitors(NULL, NULL, EnumMonitorCallback, (LPARAM)&data);

    //std::cout << "Monitor " << monitorIndex << " of " << monitorCount << ": X " << data.info.rcMonitor.left << " - " << data.info.rcMonitor.right << ", Y " << data.info.rcMonitor.top << " - " << data.info.rcMonitor.bottom << "\n";

    return data.info;
}

// It was not picking up the <algorythm> include

#ifndef max
#define max(a,b)            (((a) > (b)) ? (a) : (b))
#endif

#ifndef min
#define min(a,b)            (((a) < (b)) ? (a) : (b))
#endif

Gdiplus::Color changeBrightness(Gdiplus::Color color, int brightnessCorrection) {
    int red = color.GetR();
    int green = color.GetG();
    int blue = color.GetB();

    red = max(1, min(255, red + brightnessCorrection));
    green = max(1, min(255, green + brightnessCorrection));
    blue = max(1, min(255, blue + brightnessCorrection));

    return Gdiplus::Color(red, green, blue);
}
