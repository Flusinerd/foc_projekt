#include "EventsPage.hpp"

#include "EventLog.hpp"

#include <QComboBox>
#include <QDesktopServices>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollBar>
#include <QStandardPaths>
#include <QTableWidget>
#include <QTextStream>
#include <QUrl>
#include <QVBoxLayout>

namespace {
const QColor kWarn(0xb2, 0x6b, 0x00);
const QColor kError(0xc6, 0x2b, 0x2b);
}

EventsPage::EventsPage(EventLog* log, QWidget* parent) : QWidget(parent), log_(log)
{
    auto* root = new QVBoxLayout(this);
    auto* bar = new QHBoxLayout;
    filter_ = new QComboBox;
    filter_->addItem(tr("All events"), 0);
    filter_->addItem(tr("Warnings and errors"), 1);
    filter_->addItem(tr("Errors only"), 2);
    auto* show = new QLabel(tr("&Show:"));
    show->setBuddy(filter_);
    auto* exportBtn = new QPushButton(tr("E&xport…"));
    auto* clear = new QPushButton(tr("C&lear view"));
    clear->setToolTip(tr("Clears this list. The log file is kept."));
    auto* folder = new QPushButton(tr("Open log &folder"));
    bar->addWidget(show);
    bar->addWidget(filter_);
    bar->addStretch(1);
    bar->addWidget(exportBtn);
    bar->addWidget(clear);
    bar->addWidget(folder);
    root->addLayout(bar);

    table_ = new QTableWidget(0, 3);
    table_->setHorizontalHeaderLabels({tr("Time"), tr("Level"), tr("Message")});
    table_->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setSectionResizeMode(1, QHeaderView::ResizeToContents);
    table_->horizontalHeader()->setStretchLastSection(true);
    table_->verticalHeader()->hide();
    table_->setEditTriggers(QAbstractItemView::NoEditTriggers);
    table_->setSelectionBehavior(QAbstractItemView::SelectRows);
    table_->setWordWrap(true);
    table_->setAlternatingRowColors(true);
    root->addWidget(table_, 1);

    fileLabel_ = new QLabel(tr("Log file: %1").arg(QDir::toNativeSeparators(log_->logFile())));
    fileLabel_->setTextInteractionFlags(Qt::TextSelectableByMouse);
    fileLabel_->setEnabled(false);
    root->addWidget(fileLabel_);

    connect(filter_, &QComboBox::currentIndexChanged, this, &EventsPage::rebuild);
    connect(exportBtn, &QPushButton::clicked, this, &EventsPage::exportFile);
    connect(clear, &QPushButton::clicked, log_, &EventLog::clearView);
    connect(folder, &QPushButton::clicked, this, [this] { QDesktopServices::openUrl(QUrl::fromLocalFile(log_->logDir())); });
    connect(log_, &EventLog::added, this, &EventsPage::append);
    connect(log_, &EventLog::cleared, this, &EventsPage::rebuild);
    rebuild();
}

bool EventsPage::passes(int level) const { return level >= filter_->currentData().toInt(); }

void EventsPage::addRow(int i)
{
    const EventRecord& r = log_->records()[size_t(i)];
    if (!passes(r.level)) return;
    const int row = table_->rowCount();
    table_->insertRow(row);
    auto* t = new QTableWidgetItem(r.when.toString(QStringLiteral("HH:mm:ss.zzz")));
    t->setToolTip(r.when.toString(Qt::ISODateWithMs));
    auto* l = new QTableWidgetItem(EventLog::levelText(r.level));
    auto* m = new QTableWidgetItem(r.text);
    m->setToolTip(r.text);
    if (r.level >= 1) {
        const QBrush b(r.level >= 2 ? kError : kWarn);
        l->setForeground(b);
        m->setForeground(b);
    }
    table_->setItem(row, 0, t);
    table_->setItem(row, 1, l);
    table_->setItem(row, 2, m);
}

void EventsPage::rebuild()
{
    table_->setRowCount(0);
    for (int i = 0; i < int(log_->records().size()); ++i) addRow(i);
    table_->scrollToBottom();
}

void EventsPage::append(int count)
{
    QScrollBar* sb = table_->verticalScrollBar();
    const bool atBottom = sb->value() >= sb->maximum() - 2;
    const int n = int(log_->records().size());
    // The collector drops the oldest records beyond its limit; keep the table in step.
    const int maxRows = n;
    for (int i = std::max(0, n - count); i < n; ++i) addRow(i);
    while (table_->rowCount() > maxRows) table_->removeRow(0);
    if (atBottom) table_->scrollToBottom();
}

void EventsPage::exportFile()
{
    const QString name = QFileDialog::getSaveFileName(
        this, tr("Export events"),
        QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) +
            QStringLiteral("/focstudio_events_%1.csv").arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss"))),
        tr("CSV files (*.csv);;Text files (*.txt)"));
    if (name.isEmpty()) return;
    QFile f(name);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate | QIODevice::Text)) {
        QMessageBox::warning(this, tr("Export events"), tr("Cannot write %1:\n%2").arg(name, f.errorString()));
        return;
    }
    QTextStream out(&f);
    out.setEncoding(QStringConverter::Utf8);
    out.setGenerateByteOrderMark(true);   // Excel recognises UTF-8 only with BOM
    out << tr("Time") << ';' << tr("Level") << ';' << tr("Message") << '\n';
    for (const EventRecord& r : log_->records()) {
        QString text = r.text;
        text.replace(QLatin1Char('"'), QStringLiteral("\"\""));
        out << r.when.toString(Qt::ISODateWithMs) << ';' << EventLog::levelText(r.level) << ";\"" << text << "\"\n";
    }
}
