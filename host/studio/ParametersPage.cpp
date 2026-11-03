#include "ParametersPage.hpp"

#include "DriveController.hpp"
#include "ParamFile.hpp"

#include <QApplication>
#include <QDate>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QStandardPaths>
#include <QTreeWidget>
#include <QVBoxLayout>

#include <cmath>

namespace {
enum Col { kName, kValue, kUnit, kRange, kId, kCols };

QString categoryTitle(const QString& prefix)
{
    static const std::map<QString, const char*> titles = {
        {QStringLiteral("motor"), QT_TRANSLATE_NOOP("ParametersPage", "Motor")},
        {QStringLiteral("inv"), QT_TRANSLATE_NOOP("ParametersPage", "Power stage")},
        {QStringLiteral("ctrl"), QT_TRANSLATE_NOOP("ParametersPage", "Control")},
        {QStringLiteral("cur"), QT_TRANSLATE_NOOP("ParametersPage", "Current controller")},
        {QStringLiteral("spd"), QT_TRANSLATE_NOOP("ParametersPage", "Speed controller")},
        {QStringLiteral("pos"), QT_TRANSLATE_NOOP("ParametersPage", "Position controller")},
        {QStringLiteral("ol"), QT_TRANSLATE_NOOP("ParametersPage", "Open loop")},
        {QStringLiteral("hall"), QT_TRANSLATE_NOOP("ParametersPage", "Hall sensors")},
        {QStringLiteral("obs"), QT_TRANSLATE_NOOP("ParametersPage", "Observer (sensorless)")},
        {QStringLiteral("comm"), QT_TRANSLATE_NOOP("ParametersPage", "Communication")},
        {QStringLiteral("sim"), QT_TRANSLATE_NOOP("ParametersPage", "Virtual drive")},
        {QStringLiteral("sys"), QT_TRANSLATE_NOOP("ParametersPage", "System")},
    };
    const auto it = titles.find(prefix);
    return it != titles.end() ? QCoreApplication::translate("ParametersPage", it->second) : prefix;
}

QString statusText(uint16_t s)
{
    switch (s) {
    case FOCP_PS_UNKNOWN_ID:   return ParametersPage::tr("The drive does not know this parameter.");
    case FOCP_PS_READ_ONLY:    return ParametersPage::tr("Read-only parameter.");
    case FOCP_PS_OUT_OF_RANGE: return ParametersPage::tr("Value outside the allowed range; the drive kept the old value.");
    case FOCP_PS_LOCKED:       return ParametersPage::tr("Can only be changed while the drive is not running.");
    case FOCP_PS_BAD_REQUEST:  return ParametersPage::tr("Request rejected by the drive.");
    default:                   return QString();
    }
}

const QColor kPending(0xff, 0xf3, 0xc4);
const QColor kRejected(0xc6, 0x2b, 0x2b);
} // namespace

ParametersPage::ParametersPage(DriveController* ctl, QWidget* parent) : QWidget(parent), ctl_(ctl)
{
    auto* root = new QVBoxLayout(this);
    auto* bar = new QHBoxLayout;
    readAll_ = new QPushButton(tr("&Read all"));
    apply_ = new QPushButton(tr("A&pply"));
    discard_ = new QPushButton(tr("Disc&ard"));
    export_ = new QPushButton(tr("E&xport…"));
    import_ = new QPushButton(tr("&Import…"));
    save_ = new QPushButton(tr("Sa&ve in drive"));
    defaults_ = new QPushButton(tr("&Factory defaults…"));
    apply_->setToolTip(tr("Write all changed values to the drive."));
    save_->setToolTip(tr("Store the current parameters permanently in the drive."));
    filter_ = new QLineEdit;
    filter_->setPlaceholderText(tr("Filter (name or description)"));
    filter_->setClearButtonEnabled(true);
    for (QPushButton* b : {readAll_, apply_, discard_, export_, import_, save_, defaults_}) bar->addWidget(b);
    bar->addStretch(1);
    bar->addWidget(filter_, 1);
    root->addLayout(bar);

    tree_ = new QTreeWidget;
    tree_->setColumnCount(kCols);
    tree_->setHeaderLabels({tr("Parameter"), tr("Value"), tr("Unit"), tr("Range"), tr("ID")});
    tree_->setAlternatingRowColors(true);
    tree_->setUniformRowHeights(true);
    tree_->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                           QAbstractItemView::AnyKeyPressed);
    root->addWidget(tree_, 1);

    info_ = new QLabel;
    info_->setWordWrap(true);
    root->addWidget(info_);

    build();

    connect(readAll_, &QPushButton::clicked, this, [this] { ctl_->device().readAllParams(); });
    connect(apply_, &QPushButton::clicked, this, &ParametersPage::apply);
    connect(discard_, &QPushButton::clicked, this, &ParametersPage::discard);
    connect(export_, &QPushButton::clicked, this, &ParametersPage::exportFile);
    connect(import_, &QPushButton::clicked, this, &ParametersPage::importFile);
    connect(save_, &QPushButton::clicked, this, &ParametersPage::saveInDrive);
    connect(defaults_, &QPushButton::clicked, this, &ParametersPage::loadDefaults);
    connect(filter_, &QLineEdit::textChanged, this, &ParametersPage::applyFilter);
    connect(tree_, &QTreeWidget::itemChanged, this, &ParametersPage::onItemChanged);
    connect(ctl_, &DriveController::updated, this, &ParametersPage::refresh);
    connect(ctl_, &DriveController::expertModeChanged, this, &ParametersPage::onExpertMode);
    onExpertMode(ctl_->expertMode());
}

void ParametersPage::build()
{
    updating_ = true;
    items_.assign(FOCP_NUM_PARAMS, nullptr);
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) {
        const auto& info = focp_param_table[i];
        const QString name = QString::fromUtf8(info.name);
        const QString prefix = name.section(QLatin1Char('.'), 0, 0);
        QTreeWidgetItem*& group = groups_[prefix];
        if (!group) {
            group = new QTreeWidgetItem(tree_, {categoryTitle(prefix)});
            group->setFirstColumnSpanned(true);
            group->setFlags(Qt::ItemIsEnabled);
            QFont f = group->font(0);
            f.setBold(true);
            group->setFont(0, f);
            group->setExpanded(true);
        }
        auto* it = new QTreeWidgetItem(group);
        it->setText(kName, name);
        it->setText(kUnit, QString::fromUtf8(info.unit));
        it->setText(kRange, QStringLiteral("%1 … %2").arg(formatValue(i, info.min), formatValue(i, info.max)));
        it->setText(kId, QStringLiteral("0x%1").arg(info.id, 4, 16, QLatin1Char('0')));
        it->setData(kName, Qt::UserRole, i);
        QString tip = QString::fromUtf8(info.desc);
        if (info.flags & FOCP_F_LOCK) tip += QLatin1Char('\n') + tr("Can only be changed while the drive is not running.");
        if (info.flags & FOCP_F_RO) tip += QLatin1Char('\n') + tr("Read-only.");
        tip += QLatin1Char('\n') + tr("Default: %1 %2").arg(formatValue(i, info.def), QString::fromUtf8(info.unit));
        for (int c = 0; c < kCols; ++c) it->setToolTip(c, tip);
        it->setTextAlignment(kValue, Qt::AlignRight | Qt::AlignVCenter);
        items_[size_t(i)] = it;
    }
    tree_->header()->setSectionResizeMode(kName, QHeaderView::ResizeToContents);
    tree_->header()->setSectionResizeMode(kValue, QHeaderView::Interactive);
    tree_->setColumnWidth(kValue, 140);
    updating_ = false;
}

QString ParametersPage::formatValue(int idx, float v) const
{
    if (focp_param_table[idx].type != FOCP_T_F32) return QString::number(std::lround(v));
    return QString::number(double(v), 'g', 6);
}

bool ParametersPage::editable(int idx) const
{
    const auto& info = focp_param_table[idx];
    const auto& st = ctl_->status();
    const bool running = st.valid && (st.state == FOCP_ST_RUN || st.state == FOCP_ST_CALIB);
    return ctl_->expertMode() && ctl_->isConnected() && !(info.flags & FOCP_F_RO) &&
           !((info.flags & FOCP_F_LOCK) && running);
}

void ParametersPage::onExpertMode(bool on)
{
    for (QPushButton* b : {import_, save_, defaults_, apply_, discard_}) b->setVisible(on);
    tree_->setColumnHidden(kId, !on);
    if (!on) discard();
    applyFilter();   // sim.* only in expert mode
    refresh();
}

void ParametersPage::applyFilter()
{
    const QString f = filter_->text().trimmed();
    for (auto& [prefix, group] : groups_) {
        bool any = false;
        const bool hiddenGroup = prefix == QLatin1String("sim") && !ctl_->expertMode();
        for (int k = 0; k < group->childCount(); ++k) {
            QTreeWidgetItem* it = group->child(k);
            const int idx = it->data(kName, Qt::UserRole).toInt();
            const bool match = f.isEmpty() || it->text(kName).contains(f, Qt::CaseInsensitive) ||
                               QString::fromUtf8(focp_param_table[idx].desc).contains(f, Qt::CaseInsensitive);
            it->setHidden(hiddenGroup || !match);
            any = any || !it->isHidden();
        }
        group->setHidden(!any);
    }
}

void ParametersPage::refresh()
{
    // Never overwrite a value while its editor (a child of the viewport) is open.
    if (QWidget* fw = QApplication::focusWidget(); fw && tree_->viewport()->isAncestorOf(fw)) return;
    const bool conn = ctl_->isConnected();
    const auto& params = ctl_->params();
    updating_ = true;
    int rejected = 0;
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) {
        QTreeWidgetItem* it = items_[size_t(i)];
        const auto& p = params[size_t(i)];
        const auto pend = pending_.find(i);
        QString text;
        if (pend != pending_.end()) text = formatValue(i, pend->second);
        else if (conn && p.valid) text = formatValue(i, p.value);
        else text = QStringLiteral("–");
        if (p.pending) text += QStringLiteral(" …");
        if (it->text(kValue) != text) it->setText(kValue, text);

        const bool isRejected = conn && p.valid && p.status != FOCP_PS_OK && pend == pending_.end();
        rejected += isRejected ? 1 : 0;
        it->setBackground(kValue, pend != pending_.end() ? QBrush(kPending) : QBrush());
        it->setForeground(kValue, isRejected ? QBrush(kRejected) : QBrush());
        const QString base = it->toolTip(kName);
        it->setToolTip(kValue, isRejected ? statusText(p.status) + QLatin1Char('\n') + base : base);
        Qt::ItemFlags fl = Qt::ItemIsEnabled | Qt::ItemIsSelectable;
        if (editable(i)) fl |= Qt::ItemIsEditable;
        if (it->flags() != fl) it->setFlags(fl);
    }
    updating_ = false;

    const bool expert = ctl_->expertMode();
    readAll_->setEnabled(conn);
    export_->setEnabled(conn);
    apply_->setEnabled(conn && !pending_.empty());
    apply_->setText(pending_.empty() ? tr("A&pply") : tr("A&pply (%1)").arg(pending_.size()));
    discard_->setEnabled(!pending_.empty());
    import_->setEnabled(conn);
    save_->setEnabled(conn);
    const auto& st = ctl_->status();
    defaults_->setEnabled(conn && st.valid && st.state != FOCP_ST_RUN && st.state != FOCP_ST_CALIB);

    QString msg;
    if (!conn) msg = tr("Not connected. The table shows the parameter list of this program.");
    else if (!expert) msg = tr("Values are read-only. Switch to expert mode (View menu) to change them.");
    else if (!pending_.empty()) msg = tr("%n change(s) not yet written. Apply writes them to the drive.", nullptr, int(pending_.size()));
    else msg = tr("Double-click a value to change it. Changes take effect after Apply.");
    if (rejected) msg += QLatin1Char(' ') + tr("Red values were rejected by the drive (see tooltip).");
    info_->setText(msg);
}

void ParametersPage::onItemChanged(QTreeWidgetItem* item, int column)
{
    if (updating_ || column != kValue) return;
    const int idx = item->data(kName, Qt::UserRole).toInt();
    const auto& info = focp_param_table[idx];
    bool ok = false;
    QString t = item->text(kValue).trimmed();
    t.replace(QLatin1Char(','), QLatin1Char('.'));   // accept German decimal comma
    double v = t.toDouble(&ok);
    if (ok && info.type != FOCP_T_F32) v = std::round(v);
    if (!ok || v < double(info.min) || v > double(info.max)) {
        QMessageBox::warning(this, tr("Invalid value"),
                             tr("%1 must be a number between %2 and %3.")
                                 .arg(QString::fromUtf8(info.name), formatValue(idx, info.min), formatValue(idx, info.max)));
    } else if (std::fabs(v - double(ctl_->params()[size_t(idx)].value)) < 1e-12 && ctl_->params()[size_t(idx)].valid) {
        pending_.erase(idx);
    } else {
        pending_[idx] = float(v);
    }
    refresh();
}

void ParametersPage::apply()
{
    for (const auto& [idx, v] : pending_) ctl_->device().writeParam(idx, v);
    pending_.clear();
    refresh();
}

void ParametersPage::discard()
{
    pending_.clear();
    refresh();
}

void ParametersPage::exportFile()
{
    const QString dir = QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation);
    const QString name = QFileDialog::getSaveFileName(
        this, tr("Export parameters"),
        dir + QStringLiteral("/focstudio_params_%1.json").arg(QDate::currentDate().toString(Qt::ISODate)),
        tr("Parameter files (*.json)"));
    if (name.isEmpty()) return;
    ParamFileMeta meta;
    meta.programVersion = QCoreApplication::applicationVersion();
    meta.drive = ctl_->info();
    meta.includeSim = ctl_->expertMode();
    QFile f(name);
    if (!f.open(QIODevice::WriteOnly | QIODevice::Truncate) || f.write(paramsToJson(ctl_->params(), meta)) < 0) {
        QMessageBox::warning(this, tr("Export parameters"), tr("Cannot write %1:\n%2").arg(name, f.errorString()));
        return;
    }
    info_->setText(tr("Parameters exported to %1.").arg(QDir::toNativeSeparators(name)));
}

void ParametersPage::importFile()
{
    const QString name = QFileDialog::getOpenFileName(this, tr("Import parameters"),
                                                      QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation),
                                                      tr("Parameter files (*.json)"));
    if (name.isEmpty()) return;
    QFile f(name);
    if (!f.open(QIODevice::ReadOnly)) {
        QMessageBox::warning(this, tr("Import parameters"), tr("Cannot read %1:\n%2").arg(name, f.errorString()));
        return;
    }
    const ParamFileImport r = paramsFromJson(f.readAll(), true);
    if (!r.ok) {
        QMessageBox::warning(this, tr("Import parameters"), r.error);
        return;
    }
    int changed = 0;
    for (const auto& [idx, v] : r.values) {
        const auto& p = ctl_->params()[size_t(idx)];
        if (p.valid && p.value == v) continue;
        pending_[idx] = v;
        ++changed;
    }
    QString msg = tr("%n value(s) differ from the drive and are marked. Check them and press Apply.", nullptr, changed);
    if (!r.warnings.isEmpty())
        msg += QStringLiteral("\n\n") + tr("Skipped:") + QStringLiteral("\n• ") + r.warnings.join(QStringLiteral("\n• "));
    QMessageBox::information(this, tr("Import parameters"), msg);
    refresh();
}

void ParametersPage::saveInDrive()
{
    if (!pending_.empty() &&
        QMessageBox::question(this, tr("Save in drive"),
                              tr("There are changes that have not been applied. Save the drive's current values anyway?")) !=
            QMessageBox::Yes)
        return;
    ctl_->device().saveParams();
    info_->setText(tr("Save command sent. The drive reports the result in the event log."));
}

void ParametersPage::loadDefaults()
{
    if (QMessageBox::warning(this, tr("Factory defaults"),
                             tr("All parameters of the drive are reset to their defaults, including motor data and "
                                "calibration results. Continue?"),
                             QMessageBox::Yes | QMessageBox::Cancel, QMessageBox::Cancel) != QMessageBox::Yes)
        return;
    pending_.clear();
    ctl_->device().loadDefaults();
    ctl_->device().readAllParams();
}
