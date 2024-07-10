#pragma once

#include <AppCore/App.h>
#include <AppCore/Window.h>
#include <AppCore/Overlay.h>
//#include <AppCore/JSHelpers.h>
//#include <Ultralight/RefPtr.h>
//#include <Ultralight/String.h>
//#include <Ultralight/Listener.h>
//#include <Ultralight/View.h>
//#include <Ultralight/platform/Config.h>
#include <memory>

using namespace ultralight;

namespace WindowSwitcher {
    class HTMLWindow : public ultralight::WindowListener,
        public ultralight::ViewListener {
        RefPtr<Window> window_;
        RefPtr<Overlay> overlay_;
    public:
        HTMLWindow::HTMLWindow(const char* title, const char* url, int x, int y, int width, int height);

        RefPtr<View> view();
        RefPtr<Window> window();
        RefPtr<Overlay> overlay();

        void OnClose(ultralight::Window* window) override;
        void OnResize(ultralight::Window* window, uint32_t width, uint32_t height) override;
        void OnChangeCursor(ultralight::View* caller, ultralight::Cursor cursor) override;
    };

    extern RefPtr<App> app_;
    extern std::unique_ptr<HTMLWindow> settings_window_;
    extern std::unique_ptr<HTMLWindow> keybindings_window_;
    
    void startUiApp();
    void showUiWindows();
}
