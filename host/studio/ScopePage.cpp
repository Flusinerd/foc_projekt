#include "ScopePage.hpp"

#include "DriveController.hpp"
#include "ScopeWidget.hpp"

#include <QCheckBox>
#include <QDateTime>
#include <QDir>
#include <QDoubleSpinBox>
#include <QFileDialog>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QListWidget>
#include <QMessageBox>
#include <QPushButton>
#include <QSpinBox>
#include <QStandardPaths>
#include <QVBoxLayout>

#include <algorithm>

ScopePage::ScopePage(DriveController* ctl, QWidget* parent) : QWidget(parent), ctl_(ctl)
{
    auto* root = new QHBoxLayout(this);
    auto* side = new QVBoxLayout;

    auto* sigLabel = new QLabel(tr("&Signals (max. %1):").arg(FOCP_TELEM_SLOTS));
    signals_ = new QListWidget;
    sigLabel->setBuddy(signals_);
    for (int i = 0; i < FOCP_NUM_SIGNALS; ++i) {
        const auto& s = focp_signal_table[i];
        QString label = QString::fromUtf8(s.name);
        if (s.unit[0]) label += QStringLiteral(" [%1]").arg(QString::fromUtf8(s.unit));
        auto* it = new QListWidgetItem(label);
        it->setToolTip(QString::fromUtf8(s.desc));
        it->setData(Qt::UserRole, i);
        it->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable | Qt::ItemIsSelectable);
        it->setCheckState(Qt::Unchecked);
        signals_->addItem(it);
    }
    side->addWidget(sigLabel);
    side->addWidget(signals_, 1);

    auto* form = new QFormLayout;
    divider_ = new QSpinBox;
    divider_->setRange(1, 10000);
    divider_->setKeyboardTracking(false);
    divider_->setToolTip(tr("Every n-th control cycle is sampled. On CAN the bus load limits the rate: each sample "
                            "needs ceil(n/3) frames; about 3000 frames/s are realistic at 500 kbit/s."));
    form->addRow(tr("&Divider:"), divider_);
    rate_ = new QLabel;
    form->addRow(tr("Sample rate:"), rate_);
    window_ = new QDoubleSpinBox;
    window_->setRange(0.05, 120.0);
    window_->setDecimals(2);
    window_->setSuffix(tr(" s"));
    window_->setValue(5.0);
    form->addRow(tr("&Time window:"), window_);
    side->addLayout(form);

    pause_ = new QCheckBox(tr("&Pause display"));
    side->addWidget(pause_);
    auto* clear = new QPushButton(tr("C&lear"));
    auto* csv = new QPushButton(tr("Export &CSV…"));
    auto* row = new QHBoxLayout;
    row->addWidget(clear);
    row->addWidget(csv);
    side->addLayout(row);
    lost_ = new QLabel;
    lost_->setEnabled(false);
    side->addWidget(lost_);

    auto* sideBox = new QWidget;
    sideBox->setLayout(side);
    sideBox->setMaximumWidth(280);
    root->addWidget(sideBox);
    scope_ = new ScopeWidget(ctl_);
    root->addWidget(scope_, 1);

    connect(signals_, &QListWidget::itemChanged, this, [this] { if (!updating_) applySelection(); });
    connect(divider_, &QSpinBox::valueChanged, this, [this] { if (!updating_) applySelection(); });
    connect(window_, &QDoubleSpinBox::valueChanged, this, [this](double v) { scope_->setWindow(v); });
    connect(pause_, &QCheckBox::toggled, this, [this](bool p) { scope_->setPaused(p); });
    connect(clear, &QPushButton::clicked, this, [this] { ctl_->device().clearScope(); });
    connect(csv, &QPushButton::clicked, this, &ScopePage::exportCsv);
    connect(ctl_, &DriveController::scopeConfigChanged, this, &ScopePage::syncFromController);
    connect(ctl_, &DriveController::updated, this, [this] {
        if (!isVisible()) return;
        updateRateLabel();
        lost_->setText(tr("Lost samples: %1").arg(ctl_->device().lostSamples()));
    });
    scope_->setWindow(window_->value());
    syncFromController();
}

void ScopePage::syncFromController()
{
    updating_ = true;
    const auto& sel = ctl_->scopeSignals();
    const int n = int(sel.size());
    for (int r = 0; r < signals_->count(); ++r) {
        QListWidgetItem* it = signals_->item(r);
        const int sig = it->data(Qt::UserRole).toInt();
        const bool on = std::find(sel.begin(), sel.end(), sig) != sel.end();
        it->setCheckState(on ? Qt::Checked : Qt::Unchecked);
        Qt::ItemFlags fl = Qt::ItemIsSelectable;
        // The speed feeds the overview; further signals only while slots are free.
        if (sig != FOCP_S_SPEED && (on || n < int(FOCP_TELEM_SLOTS))) fl |= Qt::ItemIsEnabled | Qt::ItemIsUserCheckable;
        it->setFlags(fl);
    }
    divider_->setValue(ctl_->scopeDivider());
    updating_ = false;
    updateRateLabel();
}

void ScopePage::applySelection()
{
    std::vector<int> sel;
    for (int r = 0; r < signals_->count(); ++r)
        if (signals_->item(r)->checkState() == Qt::Checked) sel.push_back(signals_->item(r)->data(Qt::UserRole).toInt());
    ctl_->setScopeSignals(sel, divider_->value());
}

void ScopePage::updateRateLabel()
{
    const double base = ctl_->device().telemetryBaseHz();
    rate_->setText(tr("%1 Hz").arg(base / divider_->value(), 0, 'f', 0));
}

void ScopePage::exportCsv()
{
    const QString name = QFileDialog::getSaveFileName(
        this, tr("Export scope data"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/focstudio_scope_%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))),
        tr("CSV files (*.csv)"));
    if (name.isEmpty()) return;
    std::string err;
    // Device::exportCsv opens a narrow path (ANSI code page on Windows).
    if (!ctl_->device().exportCsv(QDir::toNativeSeparators(name).toLocal8Bit().toStdString(), err))
        QMessageBox::warning(this, tr("Export scope data"), QString::fromStdString(err));
}
