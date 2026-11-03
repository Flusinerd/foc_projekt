#pragma once
#include <QMainWindow>

class DriveController;
class QAction;
class QLabel;
class QPushButton;
class QTabWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(DriveController* ctl, QWidget* parent = nullptr);

    void startDemo();

private:
    void openConnectionDialog();
    void onConnectionChanged(bool connected);
    void refreshStatusBar();

    DriveController* ctl_;
    QTabWidget* tabs_ = nullptr;
    QAction* actConnect_ = nullptr;
    QAction* actDisconnect_ = nullptr;
    QPushButton* stop_ = nullptr;
    QLabel* connLabel_ = nullptr;
    QLabel* fwLabel_ = nullptr;
};
