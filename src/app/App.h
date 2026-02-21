#pragma once

#include <memory>

// Forward declare the REAL MainWindow type (namespaced)
namespace ngks::ui {
class MainWindow;
}

namespace ngks::app {

struct MainWindowDeleter {
    void operator()(ngks::ui::MainWindow* p) noexcept;
};

class App {
public:
    App() = default;
    ~App() = default;

    int Run(int argc, char* argv[]);

private:
    std::unique_ptr<ngks::ui::MainWindow, MainWindowDeleter> mainWindow_;
};

} // namespace ngks::app
