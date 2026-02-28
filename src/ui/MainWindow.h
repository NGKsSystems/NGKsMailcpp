#pragma once

#include <QMainWindow>
#include <QModelIndex>
#include <QPoint>

#include <memory>

namespace ngks::ui {

class MainWindow : public QMainWindow {
    Q_OBJECT

public:
    MainWindow();
    ~MainWindow() override;

protected:
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnRefresh();
    void OnCompose();
    void OnDelete();
    void OnMarkRead();
    void OnSettings();
    void OnSearchTextChanged(const QString& text);
    void OnAccountChanged(const QModelIndex& current, const QModelIndex& previous);
    void OnFolderChanged(const QModelIndex& current, const QModelIndex& previous);
    void OnMessageChanged(const QModelIndex& current, const QModelIndex& previous);
    void OnMessageDoubleClicked(const QModelIndex& index);
    void OnMessageContextMenuRequested(const QPoint& pos);

private:
    void BuildUi();
    void LoadInitialData();
    void RestoreUiState();
    void SaveUiState();

    class Impl;
    std::unique_ptr<Impl> impl_;
};

}
