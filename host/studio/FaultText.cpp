#include "FaultText.hpp"

#include <focp/focp.h>

QString DriveTexts::state(uint16_t state)
{
    switch (state) {
    case FOCP_ST_INIT:       return tr("Starting");
    case FOCP_ST_OFFSET_CAL: return tr("Calibrating current offset");
    case FOCP_ST_READY:      return tr("Ready");
    case FOCP_ST_RUN:        return tr("Running");
    case FOCP_ST_FAULT:      return tr("Fault");
    case FOCP_ST_CALIB:      return tr("Calibration running");
    default:                 return tr("Unknown state %1").arg(state);
    }
}

QColor DriveTexts::stateColor(uint16_t state)
{
    switch (state) {
    case FOCP_ST_RUN:   return QColor(0x2e, 0x9e, 0x4f);
    case FOCP_ST_READY: return QColor(0x2f, 0x6f, 0xc4);
    case FOCP_ST_FAULT: return QColor(0xc6, 0x2b, 0x2b);
    case FOCP_ST_CALIB:
    case FOCP_ST_OFFSET_CAL: return QColor(0xd9, 0x8a, 0x10);
    default:            return QColor(0x6b, 0x6b, 0x6b);
    }
}

QString DriveTexts::mode(uint16_t mode)
{
    switch (mode) {
    case FOCP_MODE_OFF:      return tr("Off");
    case FOCP_MODE_TORQUE:   return tr("Torque");
    case FOCP_MODE_SPEED:    return tr("Speed");
    case FOCP_MODE_POSITION: return tr("Position");
    case FOCP_MODE_OPENLOOP: return tr("Open loop I/f");
    default:                 return QString::number(mode);
    }
}

QString DriveTexts::sensor(uint16_t sensor)
{
    switch (sensor) {
    case FOCP_SENS_HALL:       return tr("Hall");
    case FOCP_SENS_SENSORLESS: return tr("Sensorless");
    case FOCP_SENS_HYBRID:     return tr("Hybrid (Hall + observer)");
    default:                   return QString::number(sensor);
    }
}

std::vector<FaultText> DriveTexts::faults(uint16_t faults)
{
    std::vector<FaultText> out;
    for (uint16_t b = 1; b; b = uint16_t(b << 1)) {
        if (!(faults & b)) continue;
        switch (b) {
        case FOCP_FAULT_OC_HW:
            out.push_back({b, tr("Hardware overcurrent trip"),
                           tr("Check motor cable and phases for short circuits. Acknowledge only after the cause is found.")});
            break;
        case FOCP_FAULT_OC_SW:
            out.push_back({b, tr("Overcurrent"),
                           tr("Phase current exceeded the limit. Reduce load or acceleration; if it repeats, have the current controller checked.")});
            break;
        case FOCP_FAULT_UNDERVOLTAGE:
            out.push_back({b, tr("Supply voltage too low"),
                           tr("Check the supply voltage and the current limit of the power supply, then acknowledge.")});
            break;
        case FOCP_FAULT_OVERVOLTAGE:
            out.push_back({b, tr("Supply voltage too high"),
                           tr("Check the supply voltage. Braking also feeds energy back into the supply, which a lab power supply cannot absorb: brake more gently or use a supply that can.")});
            break;
        case FOCP_FAULT_OVERTEMP:
            out.push_back({b, tr("Power stage too hot"),
                           tr("Let the drive cool down and check cooling and load before acknowledging.")});
            break;
        case FOCP_FAULT_HALL:
            out.push_back({b, tr("Hall sensor error"),
                           tr("Check the Hall sensor cable and its supply. Afterwards run the Hall calibration.")});
            break;
        case FOCP_FAULT_OBSERVER:
            out.push_back({b, tr("Sensorless operation lost the rotor position"),
                           tr("Speed too low for sensorless operation or motor data wrong. Use Hall or hybrid mode.")});
            break;
        case FOCP_FAULT_COMM:
            out.push_back({b, tr("Connection to the PC interrupted"),
                           tr("The drive missed the heartbeat. Check CAN cable, bitrate and node number, then acknowledge.")});
            break;
        case FOCP_FAULT_SAFE_INPUT:
            out.push_back({b, tr("Safe-state button pressed"),
                           tr("Release the safe-state button and acknowledge the fault.")});
            break;
        case FOCP_FAULT_PARAM:
            out.push_back({b, tr("Inconsistent parameters"),
                           tr("The parameter set is not valid. Load a known parameter file or the factory defaults.")});
            break;
        case FOCP_FAULT_OVERSPEED:
            out.push_back({b, tr("Overspeed"),
                           tr("Check the speed setpoint and whether the motor is being driven externally.")});
            break;
        default:
            out.push_back({b, tr("Unknown fault (bit 0x%1)").arg(b, 4, 16, QLatin1Char('0')),
                           tr("The drive firmware is newer than this program. Update FOC Studio.")});
            break;
        }
    }
    return out;
}
