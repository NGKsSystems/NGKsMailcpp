#pragma once

#include <QMainWindow>

#include <memory>

namespace ngks::ui::compose {

class IComposeDispatcher;

class ComposeWindow final : public QMainWindow {
    Q_OBJECT

public:
    explicit ComposeWindow(std::shared_ptr<IComposeDispatcher> dispatcher, QWidget* parent = nullptr);
    ~ComposeWindow() override;

private slots:
    void OnToggleBcc();
    void OnAddAttachment();
    void OnRemoveAttachment();
    void OnSend();
    void OnCancel();
    void OnSaveDraft();

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};

} // namespace ngks::ui::compose
