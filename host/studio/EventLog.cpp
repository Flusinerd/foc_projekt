#include "EventLog.hpp"

#include "DriveController.hpp"
#include "FaultText.hpp"

#include <QDir>
#include <QFile>
#include <QStandardPaths>
#include <QTextStream>

using fochost::LogEntry;
using fochost::LogKind;

namespace {
constexpr size_t kMaxRecords = 10000;

QString calibName(int c)
{
    switch (c) {
    case FOCP_CALIB_CURRENT_OFFSET: return EventLog::tr("current offset");
    case FOCP_CALIB_HALL:           return EventLog::tr("Hall offset");
    case FOCP_CALIB_MOTOR_ID:       return EventLog::tr("motor identification");
    default:                        return EventLog::tr("calibration");
    }
}
} // namespace

EventLog::EventLog(DriveController* ctl, QObject* parent) : QObject(parent), ctl_(ctl)
{
    dir_ = QStandardPaths::writableLocation(QStandardPaths::AppLocalDataLocation) + QStringLiteral("/logs");
    QDir().mkpath(dir_);
    connect(ctl_, &DriveController::updated, this, &EventLog::poll);
}

QString EventLog::logFile() const
{
    return dir_ + QStringLiteral("/focstudio_%1.log").arg(QDate::currentDate().toString(Qt::ISODate));
}

QString EventLog::levelText(int level)
{
    return level >= 2 ? tr("Error") : level == 1 ? tr("Warning") : tr("Info");
}

QString EventLog::describe(const LogEntry& e)
{
    switch (e.kind) {
    case LogKind::Connected:
        return tr("Connected via %1, node %2.").arg(QString::fromStdString(e.detail)).arg(e.code);
    case LogKind::Disconnected:  return tr("Disconnected.");
    case LogKind::CalibStarted:  return tr("Calibration started: %1.").arg(calibName(e.code));
    case LogKind::CalibDone:     return tr("Calibration finished: %1.").arg(calibName(e.code));
    case LogKind::CalibFailed:   return tr("Calibration failed or not available: %1.").arg(calibName(e.code));
    case LogKind::State:         return tr("State: %1").arg(DriveTexts::state(uint16_t(e.code)));
    case LogKind::FaultSet: {
        const auto f = DriveTexts::faults(uint16_t(e.code));
        return f.empty() ? tr("Fault") : tr("Fault: %1. %2").arg(f.front().title, f.front().action);
    }
    case LogKind::FaultCleared: {
        const auto f = DriveTexts::faults(uint16_t(e.code));
        return tr("Acknowledged: %1").arg(f.empty() ? QString() : f.front().title);
    }
    case LogKind::ParamsSaved:   return tr("Parameters saved in the drive.");
    case LogKind::ParamRejected: {
        QString why;
        switch (e.code) {
        case FOCP_PS_READ_ONLY:    why = tr("read-only"); break;
        case FOCP_PS_OUT_OF_RANGE: why = tr("value out of range"); break;
        case FOCP_PS_LOCKED:       why = tr("locked while running"); break;
        case FOCP_PS_UNKNOWN_ID:   why = tr("unknown to the drive"); break;
        default:                   why = tr("request rejected"); break;
        }
        return tr("Parameter %1 not changed: %2.").arg(QString::fromStdString(e.detail), why);
    }
    case LogKind::UnknownParam:
        return tr("Answer for unknown parameter ID 0x%1 (drive firmware newer than this program?).")
            .arg(e.code, 4, 16, QLatin1Char('0'));
    case LogKind::VersionMismatch:
        return tr("Protocol or table version of drive and program differ. Update firmware or FOC Studio.");
    case LogKind::Text:
    default:
        return QString::fromStdString(e.text);
    }
}

void EventLog::poll()
{
    const auto entries = ctl_->device().logSince(lastSeq_);
    if (entries.empty()) return;
    const QDateTime now = QDateTime::currentDateTime();
    const size_t before = records_.size();
    for (const LogEntry& e : entries) {
        records_.push_back({now, e.level, describe(e)});
        lastSeq_ = e.seq;
    }
    writeFile(records_.begin() + std::ptrdiff_t(before));
    const int n = int(entries.size());
    while (records_.size() > kMaxRecords) records_.pop_front();
    Q_EMIT added(n);
}

void EventLog::writeFile(const std::deque<EventRecord>::const_iterator& from)
{
    QFile f(logFile());
    if (!f.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) return;   // logging must never block operation
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    for (auto it = from; it != records_.cend(); ++it)
        out << it->when.toString(Qt::ISODateWithMs) << '\t' << levelText(it->level) << '\t' << it->text << '\n';
}

void EventLog::clearView()
{
    records_.clear();
    Q_EMIT cleared();
}
