#include "MainWindow.hpp"

#include "AboutDialog.hpp"
#include "CommissioningPage.hpp"
#include "ConnectionDialog.hpp"
#include "ControlPanel.hpp"
#include "DashboardPage.hpp"
#include "DriveController.hpp"
#include "EventLog.hpp"
#include "EventsPage.hpp"
#include "ParametersPage.hpp"
#include "ScopePage.hpp"
#include "ScopeWidget.hpp"

#include <QAction>
#include <QActionGroup>
#include <QApplication>
#include <QSettings>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenuBar>
#include <QMessageBox>
#include <QPushButton>
#include <QShortcut>
#include <QSignalBlocker>
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

    QMenu* view = menuBar()->addMenu(tr("&View"));
    actExpert_ = new QAction(tr("&Expert mode"), this);
    actExpert_->setCheckable(true);
    actExpert_->setChecked(ctl_->expertMode());
    connect(actExpert_, &QAction::toggled, this, &MainWindow::onExpertToggled);
    view->addAction(actExpert_);
    view->addSeparator();
    QMenu* langMenu = view->addMenu(tr("&Language"));
    auto* langGroup = new QActionGroup(this);
    const QString current = QSettings().value(QStringLiteral("ui/language")).toString();
    const std::pair<QString, QString> langs[] = {
        {QString(), tr("&System default")},
        {QStringLiteral("de"), QStringLiteral("&Deutsch")},   // language names stay untranslated
        {QStringLiteral("en"), QStringLiteral("&English")},
    };
    for (const auto& [code, label] : langs) {
        QAction* a = langMenu->addAction(label);
        a->setCheckable(true);
        a->setChecked(code == current);
        langGroup->addAction(a);
        connect(a, &QAction::triggered, this, [this, code = code] {
            QSettings().setValue(QStringLiteral("ui/language"), code);
            QMessageBox::information(this, tr("Language"), tr("The new language is used after restarting FOC Studio."));
        });
    }

    QMenu* help = menuBar()->addMenu(tr("&Help"));
    help->addAction(tr("&About FOC Studio…"), this, [this] { AboutDialog(ctl_, this).exec(); });

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
    stop_->setMinimumHeight(44);
    stop_->setStyleSheet(QStringLiteral(
        "QPushButton { background:#c62b2b; color:white; font-weight:bold; font-size:14pt; border-radius:6px;"
        " padding:4px 22px; }"
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
    auto* overview = new QWidget;
    auto* ov = new QHBoxLayout(overview);
    auto* left = new QVBoxLayout;
    left->addWidget(new DashboardPage(ctl_), 0);
    auto* simpleScope = new ScopeWidget(ctl_);
    simpleScope->setWanted(DriveController::defaultScopeSignals());   // speed and q-current, fixed
    simpleScope->setWindow(10.0);
    left->addWidget(simpleScope, 1);
    ov->addLayout(left, 3);
    ov->addWidget(new ControlPanel(ctl_), 2);
    tabs_->addTab(overview, tr("&Overview"));
    tabs_->addTab(new ParametersPage(ctl_), tr("&Parameters"));
    tabs_->addTab(new EventsPage(new EventLog(ctl_, this)), tr("E&vents"));
    scopePage_ = new ScopePage(ctl_);
    commissioningPage_ = new CommissioningPage(ctl_);
    setCentralWidget(tabs_);

    connLabel_ = new QLabel;
    fwLabel_ = new QLabel;
    statusBar()->addWidget(connLabel_, 1);
    statusBar()->addPermanentWidget(fwLabel_);

    connect(ctl_, &DriveController::connectionChanged, this, &MainWindow::onConnectionChanged);
    connect(ctl_, &DriveController::expertModeChanged, this, &MainWindow::onExpertMode);
    connect(ctl_, &DriveController::updated, this, &MainWindow::refreshStatusBar);
    onConnectionChanged(ctl_->isConnected());
    onExpertMode(ctl_->expertMode());
}

void MainWindow::onExpertToggled(bool on)
{
    if (on && !ctl_->expertMode()) {
        const auto answer = QMessageBox::warning(
            this, tr("Expert mode"),
            tr("Expert mode allows changing controller and motor parameters, running calibrations and recording "
               "raw signals.\n\nWrong values can make the drive unstable or damage motor and power stage. "
               "Continue?"),
            QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel);
        if (answer != QMessageBox::Yes) {
            const QSignalBlocker block(actExpert_);
            actExpert_->setChecked(false);
            return;
        }
    }
    ctl_->setExpertMode(on);
}

void MainWindow::onExpertMode(bool on)
{
    {
        const QSignalBlocker block(actExpert_);
        actExpert_->setChecked(on);
    }
    const int scopeIdx = tabs_->indexOf(scopePage_);
    if (on && scopeIdx < 0) {
        tabs_->addTab(scopePage_, tr("&Scope"));
        tabs_->addTab(commissioningPage_, tr("&Commissioning"));
    } else if (!on && scopeIdx >= 0) {
        tabs_->removeTab(tabs_->indexOf(commissioningPage_));
        tabs_->removeTab(tabs_->indexOf(scopePage_));
    }
    setWindowTitle(on ? tr("FOC Studio – expert mode") : tr("FOC Studio"));
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

void MainWindow::showTab(int index)
{
    if (index >= 0 && index < tabs_->count()) tabs_->setCurrentIndex(index);
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
