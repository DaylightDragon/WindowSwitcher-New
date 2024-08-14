#pragma comment (lib, "gdiplus.lib")

#include "Overlay.h"

#include "GeneralUtils.h"
#include "WindowSwitcherNew.h"

#include <Windows.h>
#include <gdiplus.h>
#include <iostream>
#include <sstream>
#include <iomanip>
#include <locale>
#include <codecvt>

HWND hwnd;
std::atomic<bool> overlayVisible = false;
std::atomic<OverlayState> state = NOT_INITIALIZED;

int barColorDarkening = -150;

void toggleOverlayVisibility() {
    overlayVisible.store(!overlayVisible);
    if (!overlayVisible.load()) ShowWindow(hwnd, SW_HIDE);
    else ShowWindow(hwnd, SW_SHOW);
}

bool getOverlayVisibility() {
    return overlayVisible.load();
}

void setOverlayState(OverlayState newState) {
    state.store(newState);
}

// Функция для вычисления цвета шкалы
Gdiplus::Color GetColorFromValue(float value) {
    // Вычисляем цвет шкалы в зависимости от значения
    // Например, для значений от 0 до 1 используем линейную интерполяцию:
    int red = 255 * (1 - value);
    int green = 255 * value;
    return Gdiplus::Color(red, green, 0); // Зеленый - 1, красный - 0
}

void redrawOverlay() {
    InvalidateRect(hwnd, NULL, TRUE);
}

void drawFont(Gdiplus::Graphics& graphics, std::wstring text, int size, int borderSize, Gdiplus::Color mainColor, Gdiplus::Color outlineColor, Gdiplus::FontFamily fontFamily) {
    int width = settings.load()->overlaySizeX;
    int height = settings.load()->overlaySizeY;
    int overlayBarHeight = settings.load()->overlayBarHeight;
    int overlayTextVerticalOffset = settings.load()->overlayTextVerticalOffset;

    Gdiplus::StringFormat strformat;
    const wchar_t* constText = text.c_str();

    // Создаем объект шрифта
    Gdiplus::Font font(&fontFamily, size, Gdiplus::FontStyleBold, Gdiplus::UnitPixel);

    // Измеряем размер текста
    Gdiplus::RectF textSize;
    graphics.MeasureString(constText, -1, &font, Gdiplus::PointF(0, 0), &textSize);

    Gdiplus::GraphicsPath path;
    path.AddString(constText, wcslen(constText), &fontFamily,
        Gdiplus::FontStyleBold, size, Gdiplus::Point((width - textSize.Width) / 2, height - overlayBarHeight - textSize.Height + overlayTextVerticalOffset), &strformat);
    Gdiplus::Pen pen(outlineColor, borderSize);
    graphics.DrawPath(&pen, &path);
    Gdiplus::SolidBrush brush(mainColor);
    graphics.FillPath(&brush, &path);
}

LRESULT CALLBACK OverlayWindowProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
    switch (msg) {
    case WM_PAINT: {
        PAINTSTRUCT ps;
        HDC hdc = BeginPaint(hwnd, &ps);
        Gdiplus::Graphics graphics(hdc);

        // Rounding the value
        float value = getOverlayValue();
        if (value < 0) break;

        Gdiplus::Color color = GetColorFromValue(value);

        OverlayState st = state.load();

        std::stringstream stream;
        if (st != SEQUENCE_READY) {
            stream << std::fixed << std::setprecision(2) << value;
        }
        else {
            stream << value;
        }
        std::string roundedValue = stream.str();
        std::wstring_convert<std::codecvt_utf8_utf16<wchar_t>> converter;
        std::wstring wideString = converter.from_bytes(roundedValue);

        if (st == NOT_INITIALIZED) {
            wideString = L"NAN";
            color = Gdiplus::Color(255, 255, 255);
        }
        else if (st == MACRO_DISABLED) {
            wideString = L"OFF";
            color = Gdiplus::Color(100, 100, 100);
        }
        else if (st == SEQUENCE_ACTIVE) {
            wideString = L"A " + std::to_wstring(getOverlayActiveStateCurrentAmount()) + L"/" + std::to_wstring(getOverlayActiveStateFullAmount());
        }
        else if (st == SEQUENCE_READY) {
            if (value == 0) wideString = L"READY";
            else wideString = L"R " + wideString;
            color = Gdiplus::Color(153, 102, 255);
        }
        
        // Variables
        int width = settings.load()->overlaySizeX;
        int height = settings.load()->overlaySizeY;
        int overlayBarWidth = settings.load()->overlayBarWidth;
        int overlayBarHeight = settings.load()->overlayBarHeight;
        int overlayBarOutlineSize = settings.load()->overlayBarOutlineSize;

        // Clearing
        graphics.Clear(Gdiplus::Color(0, 0, 0, 0));
        //graphics.FillRectangle(&Gdiplus::SolidBrush(Gdiplus::Color(0, 0, 0, 255)), 0, 0, width, height);

        if (!overlayVisible.load()) break;

        graphics.SetInterpolationMode(Gdiplus::InterpolationModeHighQualityBicubic);

        Gdiplus::Color outlineColor = Gdiplus::Color(60, 60, 60);
        Gdiplus::Color darkenedCurrentColor = changeBrightness(color, barColorDarkening);

        // Drawing the bar
        graphics.FillRectangle(&Gdiplus::SolidBrush(darkenedCurrentColor), (width - overlayBarWidth) / 2 - overlayBarOutlineSize, height - overlayBarHeight - overlayBarOutlineSize * 2,
            overlayBarWidth + overlayBarOutlineSize * 2, overlayBarHeight + overlayBarOutlineSize * 2);

        Gdiplus::SolidBrush brush(color);
        graphics.FillRectangle(&brush, (width - overlayBarWidth) / 2, height - overlayBarHeight - overlayBarOutlineSize,
            overlayBarWidth, overlayBarHeight);
        
        // Drawing the text
        drawFont(graphics, wideString, 18, 4, Gdiplus::Color(255, 255, 255), outlineColor, Gdiplus::FontFamily(L"Arial"));

        EndPaint(hwnd, &ps);
        break;
    }
    case WM_DESTROY:
        PostQuitMessage(0);
        break;
    default:
        return DefWindowProc(hwnd, msg, wParam, lParam);
    }
    return 0;
}

POINT getWindowCoords() {
    int width = settings.load()->overlaySizeX;
    int height = settings.load()->overlaySizeY;

    // Вычисляем координаты и размер окна
    MONITORINFO mi = GetMonitorInfoByIndex(settings.load()->overlayMonitorNumber - 1);
    RECT taskbarPadding = GetTaskbarSize();

    int paddingX = settings.load()->overlayPositionPaddingX;
    int paddingY = settings.load()->overlayPositionPaddingY;

    int x = -taskbarPadding.right + taskbarPadding.left;
    int y = -taskbarPadding.bottom + taskbarPadding.top;

    int leftOrRight = settings.load()->overlayPositionLeftRightOrCenter;
    if (leftOrRight == 1) {
        x += mi.rcMonitor.left + paddingX;
    }
    else if (leftOrRight == 2) {
        x += mi.rcMonitor.right - paddingX - width;
    }
    else if (leftOrRight == 3) {
        x += mi.rcMonitor.left + (mi.rcMonitor.right - mi.rcMonitor.left) / 2 + paddingX - width / 2;
    }

    int topOrBottom = settings.load()->overlayPositionTopBottomOrCenter;
    if (topOrBottom == 1) {
        y += mi.rcMonitor.top + paddingY;
    }
    else if (topOrBottom == 2) {
        y += mi.rcMonitor.bottom - paddingY - height;
    }
    else if (topOrBottom == 3) {
        y += mi.rcMonitor.top + (mi.rcMonitor.bottom - mi.rcMonitor.top) / 2 + paddingY - height / 2;
    }

    //std::cout << "A " << x << " " << y << "\n";

    return POINT{ x, y };
}

void repositionTheOverlay() {
    POINT coords = getWindowCoords();
    int width = settings.load()->overlaySizeX;
    int height = settings.load()->overlaySizeY;

    SetWindowPos(hwnd, NULL, coords.x, coords.y, width, height, NULL); // HWND_TOPMOST
}

HWND CreateOverlayWindow(HINSTANCE hInstance, const TCHAR* windowName, int opacity) {
    // Создаем класс окна
    WNDCLASS wc = { 0 };
    wc.lpfnWndProc = OverlayWindowProc;
    wc.hInstance = hInstance;
    wc.lpszClassName = windowName;

    RegisterClass(&wc);

    // Получаем информацию о главном мониторе
    //MONITORINFO mi = { sizeof(MONITORINFO) };
    //GetMonitorInfo(MonitorFromWindow(NULL, MONITOR_DEFAULTTOPRIMARY), &mi);

    POINT coords = getWindowCoords();
    int width = settings.load()->overlaySizeX;
    int height = settings.load()->overlaySizeY;

    // Создаем окно оверлея
    hwnd = CreateWindowEx(
        WS_EX_LAYERED | WS_EX_TRANSPARENT | WS_EX_TOPMOST, // Дополнительные стили
        windowName,
        NULL,
        WS_POPUP,
        coords.x, coords.y, width, height,
        NULL,
        NULL,
        hInstance,
        NULL
    );

    // Устанавливаем прозрачность окна
    SetLayeredWindowAttributes(hwnd, 0, opacity, LWA_COLORKEY);

    // Делаем окно невидимым в ALT-TAB
    SetWindowLong(hwnd, GWL_EXSTYLE, GetWindowLong(hwnd, GWL_EXSTYLE) | WS_EX_TOOLWINDOW);

    ShowWindow(hwnd, SW_SHOW);

    // Тест
    //HDC hdc = GetDC(hwnd);
    //Gdiplus::Graphics graphics(hdc);

    //// Используем цвет RGB(255, 0, 0) - красный 
    //Gdiplus::Color color = Gdiplus::Color(255, 0, 0);
    //Gdiplus::SolidBrush brush(color);
    //graphics.FillRectangle(&brush, 0, 0, width, height);

    //ReleaseDC(hwnd, hdc);

    return hwnd;
}