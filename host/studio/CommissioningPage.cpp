#include "CommissioningPage.hpp"

#include "DriveController.hpp"
#include "FaultText.hpp"

#include <QAbstractItemView>
#include <QComboBox>
#include <QFormLayout>
#include <QGroupBox>
#include <QLabel>
#include <QPushButton>
#include <QVBoxLayout>

#include <algorithm>
#include <cmath>

namespace {
QLabel* note(const QString& text)
{
    auto* l = new QLabel(text);
    l->setWordWrap(true);
    l->setEnabled(false);
    return l;
}
} // namespace

CommissioningPage::CommissioningPage(DriveController* ctl, QWidget* parent) : QWidget(parent), ctl_(ctl)
{
    auto* root = new QVBoxLayout(this);

    auto* sensorBox = new QGroupBox(tr("Position sensor"));
    auto* sf = new QFormLayout(sensorBox);
    sensor_ = new QComboBox;
    for (int s = FOCP_SENS_HALL; s <= FOCP_SENS_HYBRID; ++s) sensor_->addItem(DriveTexts::sensor(uint16_t(s)), s);
    sf->addRow(tr("&Sensor:"), sensor_);
    sf->addRow(note(tr("Sensorless operation needs a minimum speed (obs.n_switch). Hybrid starts with Hall and "
                       "switches to the observer above that speed.")));
    root->addWidget(sensorBox);

    auto* calBox = new QGroupBox(tr("Calibration"));
    auto* cl = new QVBoxLayout(calBox);
    offset_ = new QPushButton(tr("Measure &current offset"));
    cl->addWidget(offset_);
    cl->addWidget(note(tr("Power stage off, motor at standstill. Measures the zero point of the current sensors.")));
    hall_ = new QPushButton(tr("Calibrate &Hall offset"));
    cl->addWidget(hall_);
    cl->addWidget(note(tr("The rotor turns slowly in open loop with ol.I, forwards and backwards. The shaft must turn "
                          "freely! The result is written to hall.offset.")));
    hallValue_ = new QLabel;
    cl->addWidget(hallValue_);
    motorId_ = new QPushButton(tr("&Identify motor"));
    cl->addWidget(motorId_);
    cl->addWidget(note(tr("Measures Rs, Ls, flux and inertia (phase P4, not yet available in the firmware).")));
    root->addWidget(calBox);

    state_ = new QLabel;
    state_->setWordWrap(true);
    root->addWidget(state_);
    root->addStretch(1);

    connect(sensor_, &QComboBox::activated, this, [this](int i) {
        ctl_->device().writeParam(FOCP_P_CTRL_POS_SENSOR, float(sensor_->itemData(i).toInt()));
    });
    connect(offset_, &QPushButton::clicked, this, [this] { ctl_->device().calibrate(FOCP_CALIB_CURRENT_OFFSET); });
    connect(hall_, &QPushButton::clicked, this, [this] { ctl_->device().calibrate(FOCP_CALIB_HALL); });
    connect(motorId_, &QPushButton::clicked, this, [this] { ctl_->device().calibrate(FOCP_CALIB_MOTOR_ID); });
    connect(ctl_, &DriveController::updated, this, &CommissioningPage::refresh);
    refresh();
}

void CommissioningPage::refresh()
{
    if (!isVisible()) return;
    const bool conn = ctl_->isConnected();
    const auto& st = ctl_->status();
    const auto& p = ctl_->params();
    const bool ready = conn && st.valid && st.state == FOCP_ST_READY;
    const bool running = conn && st.valid && (st.state == FOCP_ST_RUN || st.state == FOCP_ST_CALIB);

    sensor_->setEnabled(conn && p[FOCP_P_CTRL_POS_SENSOR].valid && !running);
    if (!sensor_->view()->isVisible())
        sensor_->setCurrentIndex(std::clamp(int(std::lround(p[FOCP_P_CTRL_POS_SENSOR].value)), 0, 2));
    for (QPushButton* b : {offset_, hall_, motorId_}) b->setEnabled(ready);
    hallValue_->setText(conn && p[FOCP_P_HALL_OFFSET].valid
                            ? tr("hall.offset = %1°").arg(double(p[FOCP_P_HALL_OFFSET].value), 0, 'f', 1)
                            : QString());
    if (!conn) state_->setText(tr("Not connected."));
    else if (!st.valid) state_->setText(QString());
    else if (st.state == FOCP_ST_CALIB) state_->setText(tr("Calibration running… The result appears in the event log."));
    else if (!ready) state_->setText(tr("Calibrations are only possible in state Ready (now: %1).").arg(DriveTexts::state(st.state)));
    else state_->clear();
}
