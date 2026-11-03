#pragma once
#include <fochost/Device.hpp>

#include <QDateTime>
#include <QObject>
#include <QString>

#include <deque>

class DriveController;

struct EventRecord {
    QDateTime when;
    int level = 0;   ///< 0 info, 1 warning, 2 error
    QString text;    ///< translated
};

/// Collects the drive's log entries (also while the page is hidden), phrases them in the
/// UI language and appends them to a daily log file in the user profile.
class EventLog : public QObject {
    Q_OBJECT
public:
    explicit EventLog(DriveController* ctl, QObject* parent = nullptr);

    const std::deque<EventRecord>& records() const { return records_; }
    void clearView();
    QString logDir() const { return dir_; }
    QString logFile() const;

    static QString describe(const fochost::LogEntry& e);
    static QString levelText(int level);

Q_SIGNALS:
    void added(int count);
    void cleared();

private:
    void poll();
    void writeFile(const std::deque<EventRecord>::const_iterator& from);

    DriveController* ctl_;
    std::deque<EventRecord> records_;
    uint64_t lastSeq_ = 0;
    QString dir_;
};
