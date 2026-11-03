#include "MainWindow.hpp"

#include "ConnectionDialog.hpp"
#include "DashboardPage.hpp"
#include "DriveController.hpp"

#include <QAction>
#include <QApplication>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QStatusBar>
#include <QTabWidget>
#include <QToolBar>

MainWindow::MainWindow(DriveController* ctl, QWidget* parent) : QMainWindow(parent), ctl_(ctl)
{
    setWindowTitle(tr("FOC Studio"));
    resize(1100, 720);

    // --- actions / menu ----------------------------------------------------------------
    actConnect_ = new QAction(tr("&Connect…"), this);
    actConnect_->setShortcut(QKeySequence(tr("Ctrl+K")));
    connect(actConnect_, &QAction::triggered, this, &MainWindow::openConnectionDialog);
    actDisconnect_ = new QAction(tr("&Disconnect"), this);
    connect(actDisconnect_, &QAction::triggered, ctl_, &DriveController::disconnectFrom);
    auto* actDemo = new QAction(tr("Start &demo (virtual drive)"), this);
    connect(actDemo, &QAction::triggered, this, &MainWindow::startDemo);
    auto* actQuit = new QAction(tr("&Quit"), this);
    actQuit->setShortcut(QKeySequence::Quit);
    connect(actQuit, &QAction::triggered, this, &QWidget::close);

    QMenu* file = menuBar()->addMenu(tr("&Drive"));
    file->addAction(actConnect_);
    file->addAction(actDisconnect_);
    file->addSeparator();
    file->addAction(actDemo);
    file->addSeparator();
    file->addAction(actQuit);

    // --- toolbar with the always-visible STOP -------------------------------------------
    auto* tb = addToolBar(tr("Drive"));
    tb->setObjectName(QStringLiteral("driveToolbar"));
    tb->setMovable(false);
    tb->addAction(actConnect_);
    tb->addAction(actDisconnect_);
    auto* spacer = new QWidget;
    spacer->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    tb->addWidget(spacer);
    stop_ = new QPushButton(tr("STOP  (Space / Esc)"));
    stop_->setMinimumSize(220, 44);
    stop_->setStyleSheet(QStringLiteral(
        "QPushButton { background:#c62b2b; color:white; font-weight:bold; font-size:14pt; border-radius:6px; }"
        "QPushButton:pressed { background:#8f1f1f; }"
        "QPushButton:disabled { background:#b9a3a3; }"));
    stop_->setFocusPolicy(Qt::NoFocus);   // Space must never "click" a focused button instead
    stop_->setToolTip(tr("Switches the power stage off immediately."));
    connect(stop_, &QPushButton::clicked, ctl_, &DriveController::stop);
    tb->addWidget(stop_);

    // STOP works from every window of the application, including dialogs.
    for (const QKeySequence& k : {QKeySequence(Qt::Key_Space), QKeySequence(Qt::Key_Escape)}) {
        auto* sc = new QShortcut(k, this);
        sc->setContext(Qt::ApplicationShortcut);
        connect(sc, &QShortcut::activated, ctl_, &DriveController::stop);
    }

    // --- pages ------------------------------------------------------------------------
    tabs_ = new QTabWidget;
    tabs_->addTab(new DashboardPage(ctl_), tr("&Overview"));
    setCentralWidget(tabs_);

    connLabel_ = new QLabel;
    fwLabel_ = new QLabel;
    statusBar()->addWidget(connLabel_, 1);
    statusBar()->addPermanentWidget(fwLabel_);

    connect(ctl_, &DriveController::connectionChanged, this, &MainWindow::onConnectionChanged);
    connect(ctl_, &DriveController::updated, this, &MainWindow::refreshStatusBar);
    onConnectionChanged(ctl_->isConnected());
}

void MainWindow::openConnectionDialog()
{
    ConnectionDialog dlg(this);
    if (dlg.exec() != QDialog::Accepted) return;
    QString err;
    QApplication::setOverrideCursor(Qt::WaitCursor);
    const bool ok = ctl_->connectTo(dlg.config(), dlg.node(), err);
    QApplication::restoreOverrideCursor();
    if (!ok) QMessageBox::warning(this, tr("Connection failed"), err);
}

void MainWindow::startDemo()
{
    QString err;
    if (!ctl_->startDemo(err)) QMessageBox::warning(this, tr("Demo"), err);
}

void MainWindow::onConnectionChanged(bool connected)
{
    actConnect_->setEnabled(!connected);
    actDisconnect_->setEnabled(connected);
    stop_->setEnabled(connected);
    refreshStatusBar();
}

void MainWindow::refreshStatusBar()
{
    if (!ctl_->isConnected()) {
        connLabel_->setText(tr("Not connected"));
        fwLabel_->clear();
        return;
    }
    connLabel_->setText(tr("Connected: %1, node %2, %3 messages")
                            .arg(ctl_->transportName())
                            .arg(ctl_->node())
                            .arg(ctl_->device().rxMessages()));
    const auto& inf = ctl_->info();
    fwLabel_->setText(inf.valid ? tr("Firmware %1.%2, protocol v%3").arg(inf.fwMajor).arg(inf.fwMinor).arg(inf.protocol)
                                : QString());
}
