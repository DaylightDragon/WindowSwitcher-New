#include "MainUi.h"

using namespace ultralight;


inline WindowSwitcher::HTMLWindow::HTMLWindow(const char* title, const char* url, int x, int y, int width, int height) {
    window_ = Window::Create(App::instance()->main_monitor(), width, height, false,
        kWindowFlags_Titled | kWindowFlags_Resizable | kWindowFlags_Hidden);
    window_->MoveTo(x, y);
    window_->SetTitle(title);
    window_->Show();
    window_->set_listener(this);

    overlay_ = Overlay::Create(window_, window_->width(), window_->height(), 0, 0);
    overlay_->view()->LoadURL(url);
    overlay_->view()->set_view_listener(this);
}


inline RefPtr<View> WindowSwitcher::HTMLWindow::view() { return overlay_->view(); }
inline RefPtr<Window> WindowSwitcher::HTMLWindow::window() { return window_; }
inline RefPtr<Overlay> WindowSwitcher::HTMLWindow::overlay() { return overlay_; }

inline void WindowSwitcher::HTMLWindow::OnClose(ultralight::Window* window) {
    // We quit the application when any of the windows are closed.
    App::instance()->Quit();
}

void WindowSwitcher::HTMLWindow::OnResize(ultralight::Window* window, uint32_t width, uint32_t height) {
    overlay_->Resize(width, height);
}

void WindowSwitcher::HTMLWindow::OnChangeCursor(ultralight::View* caller, ultralight::Cursor cursor) {
    window_->SetCursor(cursor);
}


RefPtr<App> app_;
std::unique_ptr<WindowSwitcher::HTMLWindow> settings_window_;
std::unique_ptr<WindowSwitcher::HTMLWindow> keybindings_window_;

inline void WindowSwitcher::startUiApp() {
    app_ = App::Create();
    app_->Run();
}

inline void WindowSwitcher::showUiWindows() {
    settings_window_.reset(new WindowSwitcher::HTMLWindow("Settings", "file:///settings.html", 50, 50, 600, 700));
    keybindings_window_.reset(new WindowSwitcher::HTMLWindow("Keybindings", "file:///keybindings.html", 700, 50, 600, 700));
}
