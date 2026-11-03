#pragma once
#include <QColor>
#include <QCoreApplication>
#include <QString>

#include <cstdint>
#include <vector>

struct FaultText {
    uint16_t bit;
    QString title;   ///< what happened
    QString action;  ///< what the user should do
};

/// Plain-language texts for drive states, modes and faults (translatable, context "DriveTexts").
class DriveTexts {
    Q_DECLARE_TR_FUNCTIONS(DriveTexts)
public:
    static QString state(uint16_t state);
    static QColor stateColor(uint16_t state);
    static QString mode(uint16_t mode);
    static QString sensor(uint16_t sensor);
    /// One entry per set bit, in bit order. Unknown bits get a generic text.
    static std::vector<FaultText> faults(uint16_t faults);
};
