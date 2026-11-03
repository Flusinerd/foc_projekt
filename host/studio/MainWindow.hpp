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
    void showTab(int index);

private:
    void openConnectionDialog();
    void onConnectionChanged(bool connected);
    void onExpertToggled(bool on);
    void onExpertMode(bool on);
    void refreshStatusBar();

    DriveController* ctl_;
    QTabWidget* tabs_ = nullptr;
    QWidget* scopePage_ = nullptr;
    QWidget* commissioningPage_ = nullptr;
    QAction* actExpert_ = nullptr;
    QAction* actConnect_ = nullptr;
    QAction* actDisconnect_ = nullptr;
    QPushButton* stop_ = nullptr;
    QLabel* connLabel_ = nullptr;
    QLabel* fwLabel_ = nullptr;
};
