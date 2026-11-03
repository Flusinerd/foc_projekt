#include "ControlPanel.hpp"

#include "DriveController.hpp"
#include "FaultText.hpp"

#include <QAbstractItemView>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QPushButton>
#include <QSlider>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
constexpr int kSliderSteps = 1000;
const char* const kUnits[] = {"", "A", "rpm", "°", "rpm"};
}

ControlPanel::ControlPanel(DriveController* ctl, QWidget* parent) : QGroupBox(tr("Operation"), parent), ctl_(ctl)
{
    auto* root = new QVBoxLayout(this);

    enable_ = new QPushButton(tr("&Enable"));
    enable_->setMinimumHeight(40);
    enable_->setStyleSheet(QStringLiteral(
        "QPushButton { background:#2e9e4f; color:white; font-weight:bold; border-radius:5px; }"
        "QPushButton:disabled { background:#a9c8b2; }"));
    enable_->setToolTip(tr("Switches the power stage on. Only possible in state Ready."));
    ack_ = new QPushButton(tr("&Acknowledge fault"));
    auto* buttons = new QHBoxLayout;
    buttons->addWidget(enable_, 2);
    buttons->addWidget(ack_, 1);
    root->addLayout(buttons);

    auto* form = new QFormLayout;
    mode_ = new QComboBox;
    for (int m = FOCP_MODE_OFF; m <= FOCP_MODE_OPENLOOP; ++m) mode_->addItem(DriveTexts::mode(uint16_t(m)), m);
    mode_->setToolTip(tr("Can only be changed while the drive is not running."));
    form->addRow(tr("Control &mode:"), mode_);

    ref_ = new QDoubleSpinBox;
    ref_->setDecimals(1);
    ref_->setKeyboardTracking(false);   // value counts after Enter / focus change
    refLabel_ = new QLabel(tr("&Setpoint:"));
    refLabel_->setBuddy(ref_);
    zero_ = new QPushButton(tr("0"));
    zero_->setToolTip(tr("Set the setpoint to zero and send it."));
    send_ = new QPushButton(tr("Se&nd"));
    auto* refRow = new QHBoxLayout;
    refRow->addWidget(ref_, 1);
    refRow->addWidget(zero_);
    refRow->addWidget(send_);
    form->addRow(refLabel_, refRow);
    slider_ = new QSlider(Qt::Horizontal);
    slider_->setRange(0, kSliderSteps);
    slider_->setAccessibleName(tr("Setpoint slider"));
    form->addRow(QString(), slider_);
    root->addLayout(form);

    hint_ = new QLabel;
    hint_->setWordWrap(true);
    hint_->setEnabled(false);
    root->addWidget(hint_);
    root->addStretch(1);

    connect(enable_, &QPushButton::clicked, this, &ControlPanel::onEnable);
    connect(ack_, &QPushButton::clicked, this, [this] { ctl_->device().ackFault(); });
    connect(mode_, &QComboBox::activated, this, &ControlPanel::onModeChosen);
    connect(ref_, &QDoubleSpinBox::valueChanged, this, [this](double v) {
        if (updating_) return;
        setSetpoint(v, false);
        sendSetpoint();
    });
    connect(slider_, &QSlider::valueChanged, this, [this](int s) {
        if (updating_) return;
        setSetpoint(lo_ + (hi_ - lo_) * s / kSliderSteps, true);
    });
    connect(slider_, &QSlider::sliderReleased, this, &ControlPanel::sendSetpoint);
    connect(zero_, &QPushButton::clicked, this, [this] { setSetpoint(0.0, false); sendSetpoint(); });
    connect(send_, &QPushButton::clicked, this, &ControlPanel::sendSetpoint);
    connect(ctl_, &DriveController::updated, this, &ControlPanel::refresh);
    refresh();
}

void ControlPanel::refresh()
{
    const bool conn = ctl_->isConnected();
    const auto& st = ctl_->status();
    const auto& p = ctl_->params();
    const bool valid = conn && st.valid;
    const bool running = valid && (st.state == FOCP_ST_RUN || st.state == FOCP_ST_CALIB);
    const bool modeKnown = conn && p[FOCP_P_CTRL_MODE].valid;
    const int mode = modeKnown ? std::clamp(int(std::lround(p[FOCP_P_CTRL_MODE].value)), 0, 4) : 0;

    enable_->setEnabled(valid && st.state == FOCP_ST_READY && mode != FOCP_MODE_OFF);
    ack_->setEnabled(valid && st.state == FOCP_ST_FAULT);
    mode_->setEnabled(modeKnown && !running);
    if (!mode_->view()->isVisible()) mode_->setCurrentIndex(mode);

    const bool hasRef = modeKnown && mode != FOCP_MODE_OFF;
    for (QWidget* w : {static_cast<QWidget*>(ref_), static_cast<QWidget*>(slider_), static_cast<QWidget*>(zero_),
                       static_cast<QWidget*>(send_)})
        w->setEnabled(hasRef);
    if (mode != shownMode_ || modeKnown) applyLimits(mode);
    // Show setpoints sent elsewhere (e.g. demo), unless the user is editing right now.
    if (ctl_->referenceSerial() != shownRefSerial_ && !ref_->hasFocus() && !slider_->isSliderDown()) {
        shownRefSerial_ = ctl_->referenceSerial();
        setSetpoint(ctl_->lastReference(), false);
    }

    if (!conn) hint_->setText(tr("Not connected."));
    else if (!modeKnown) hint_->setText(tr("Reading parameters…"));
    else if (mode == FOCP_MODE_OFF) hint_->setText(tr("Choose a control mode to enable the drive."));
    else if (st.state == FOCP_ST_FAULT) hint_->setText(tr("Remove the cause shown under Active faults, then acknowledge."));
    else hint_->setText(running ? tr("Setpoint changes are sent with Enter, Send or when releasing the slider.")
                                : QString());
}

void ControlPanel::applyLimits(int mode)
{
    const auto& p = ctl_->params();
    double lo = 0, hi = 0;
    switch (mode) {
    case FOCP_MODE_TORQUE:   hi = p[FOCP_P_SPD_IQ_MAX].value; lo = -hi; break;
    case FOCP_MODE_SPEED:
    case FOCP_MODE_OPENLOOP: hi = p[FOCP_P_MOTOR_N_MAX].value; lo = -hi; break;
    case FOCP_MODE_POSITION: hi = 1080.0; lo = -hi; break;   // ±3 turns, telemetry range ±3276°
    default: break;
    }
    if (mode == shownMode_ && lo == lo_ && hi == hi_) return;
    shownMode_ = mode;
    lo_ = lo;
    hi_ = hi;
    updating_ = true;
    ref_->setRange(lo, hi);
    ref_->setSuffix(QStringLiteral(" ") + QString::fromUtf8(kUnits[std::clamp(mode, 0, 4)]));
    ref_->setSingleStep(mode == FOCP_MODE_TORQUE ? 0.1 : 10.0);
    refPerMode_[size_t(std::clamp(mode, 0, 4))] = std::clamp(refPerMode_[size_t(std::clamp(mode, 0, 4))], lo, hi);
    updating_ = false;
    setSetpoint(refPerMode_[size_t(std::clamp(mode, 0, 4))], false);
}

void ControlPanel::setSetpoint(double v, bool fromSlider)
{
    v = std::clamp(v, lo_, hi_);
    refPerMode_[size_t(std::clamp(shownMode_, 0, 4))] = v;
    updating_ = true;
    ref_->setValue(v);
    if (!fromSlider) slider_->setValue(hi_ > lo_ ? int(std::lround((v - lo_) / (hi_ - lo_) * kSliderSteps)) : 0);
    updating_ = false;
}

void ControlPanel::sendSetpoint()
{
    if (!ctl_->isConnected() || shownMode_ <= FOCP_MODE_OFF) return;
    ctl_->setReference(float(refPerMode_[size_t(shownMode_)]));
    shownRefSerial_ = ctl_->referenceSerial();
}

void ControlPanel::onModeChosen(int index)
{
    const int mode = mode_->itemData(index).toInt();
    ctl_->device().writeParam(FOCP_P_CTRL_MODE, float(mode));
}

void ControlPanel::onEnable()
{
    if (!confirmSafety()) return;
    sendSetpoint();
    ctl_->device().enable();
}

bool ControlPanel::confirmSafety()
{
    // The virtual drive needs no confirmation; real hardware once per program session.
    if (safetyConfirmed_ || ctl_->transportName() == QLatin1String("Simulation")) return true;

    QDialog dlg(this);
    dlg.setWindowTitle(tr("Enable drive – safety check"));
    auto* l = new QVBoxLayout(&dlg);
    auto* intro = new QLabel(tr("The motor will start turning. Confirm each point before the first enable:"));
    intro->setWordWrap(true);
    l->addWidget(intro);
    // Space is reserved for STOP, so the boxes get Alt+1..4 for keyboard use.
    const QString items[] = {
        tr("&1  The power supply is current-limited."),
        tr("&2  The motor shaft can turn freely; nothing and nobody is in the way."),
        tr("&3  The safe-state button is within reach."),
        tr("&4  The supply voltage is being watched (braking can raise it)."),
    };
    std::vector<QCheckBox*> boxes;
    for (const QString& s : items) {
        auto* cb = new QCheckBox(s);
        l->addWidget(cb);
        boxes.push_back(cb);
    }
    auto* bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    bb->button(QDialogButtonBox::Ok)->setText(tr("Enable"));
    bb->button(QDialogButtonBox::Ok)->setEnabled(false);
    bb->button(QDialogButtonBox::Cancel)->setDefault(true);
    auto update = [bb, boxes] {
        bool all = true;
        for (auto* b : boxes) all = all && b->isChecked();
        bb->button(QDialogButtonBox::Ok)->setEnabled(all);
    };
    for (auto* b : boxes) connect(b, &QCheckBox::toggled, &dlg, update);
    connect(bb, &QDialogButtonBox::accepted, &dlg, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, &dlg, &QDialog::reject);
    l->addWidget(bb);
    if (dlg.exec() != QDialog::Accepted) return false;
    safetyConfirmed_ = true;
    return true;
}
