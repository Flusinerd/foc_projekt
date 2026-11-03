#pragma once
#include <QWidget>

class EventLog;
class QComboBox;
class QLabel;
class QTableWidget;

/// Event log: time, level, message; filter, export, link to the log file.
class EventsPage : public QWidget {
    Q_OBJECT
public:
    explicit EventsPage(EventLog* log, QWidget* parent = nullptr);

private:
    void rebuild();
    void append(int count);
    void addRow(int recordIndex);
    bool passes(int level) const;
    void exportFile();

    EventLog* log_;
    QTableWidget* table_ = nullptr;
    QComboBox* filter_ = nullptr;
    QLabel* fileLabel_ = nullptr;
};
