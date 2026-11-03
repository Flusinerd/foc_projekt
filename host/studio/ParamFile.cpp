#include "ParamFile.hpp"

#include <QCoreApplication>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <cmath>

/// Translation context for the messages below.
class ParamFile {
    Q_DECLARE_TR_FUNCTIONS(ParamFile)
};

namespace {
const char* const kFormat = "focstudio-params";
constexpr int kFormatVersion = 1;

QString hexId(uint16_t id) { return QStringLiteral("0x%1").arg(id, 4, 16, QLatin1Char('0')); }
} // namespace

QByteArray paramsToJson(const std::vector<fochost::ParamState>& params, const ParamFileMeta& meta)
{
    QJsonArray list;
    for (int i = 0; i < FOCP_NUM_PARAMS && i < int(params.size()); ++i) {
        const auto& info = focp_param_table[i];
        if (!params[size_t(i)].valid) continue;
        if ((info.flags & FOCP_F_SIM) && !meta.includeSim) continue;
        QJsonObject o;
        o[QStringLiteral("id")] = hexId(info.id);
        o[QStringLiteral("name")] = QString::fromUtf8(info.name);
        o[QStringLiteral("value")] = double(params[size_t(i)].value);
        o[QStringLiteral("unit")] = QString::fromUtf8(info.unit);
        if (info.flags & FOCP_F_RO) o[QStringLiteral("readonly")] = true;
        list.append(o);
    }
    QJsonObject drive;
    if (meta.drive.valid) {
        drive[QStringLiteral("firmware")] = QStringLiteral("%1.%2").arg(meta.drive.fwMajor).arg(meta.drive.fwMinor);
        drive[QStringLiteral("board")] = meta.drive.board;
        drive[QStringLiteral("protocol_version")] = meta.drive.protocol;
    }
    QJsonObject root;
    root[QStringLiteral("format")] = QString::fromLatin1(kFormat);
    root[QStringLiteral("format_version")] = kFormatVersion;
    root[QStringLiteral("protocol_version")] = int(FOCP_PROTOCOL_VERSION);
    root[QStringLiteral("param_table_size")] = int(FOCP_NUM_PARAMS);
    root[QStringLiteral("program_version")] = meta.programVersion;
    root[QStringLiteral("created")] = QDateTime::currentDateTime().toString(Qt::ISODate);
    root[QStringLiteral("drive")] = drive;
    root[QStringLiteral("params")] = list;
    return QJsonDocument(root).toJson(QJsonDocument::Indented);
}

ParamFileImport paramsFromJson(const QByteArray& json, bool allowSim)
{
    ParamFileImport r;
    QJsonParseError pe{};
    const QJsonDocument doc = QJsonDocument::fromJson(json, &pe);
    if (doc.isNull() || !doc.isObject()) {
        r.error = ParamFile::tr("The file is not valid JSON (%1).").arg(pe.errorString());
        return r;
    }
    const QJsonObject root = doc.object();
    if (root.value(QStringLiteral("format")).toString() != QLatin1String(kFormat)) {
        r.error = ParamFile::tr("The file is not a FOC Studio parameter file.");
        return r;
    }
    const int fv = root.value(QStringLiteral("format_version")).toInt(-1);
    if (fv < 1 || fv > kFormatVersion) {
        r.error = ParamFile::tr("File format version %1 is not supported by this program.").arg(fv);
        return r;
    }
    const int pv = root.value(QStringLiteral("protocol_version")).toInt(-1);
    if (pv != int(FOCP_PROTOCOL_VERSION)) {
        r.error = ParamFile::tr("The file was written for protocol version %1, this program uses version %2.")
                      .arg(pv).arg(FOCP_PROTOCOL_VERSION);
        return r;
    }

    const QJsonArray list = root.value(QStringLiteral("params")).toArray();
    for (const QJsonValue& v : list) {
        const QJsonObject o = v.toObject();
        const QString idText = o.value(QStringLiteral("id")).toString();
        const QString name = o.value(QStringLiteral("name")).toString();
        bool idOk = false;
        const uint idNum = idText.toUInt(&idOk, 0);
        const int idx = idOk && idNum <= 0xFFFF ? focp_param_find(uint16_t(idNum)) : -1;
        if (idx < 0) {
            r.warnings << ParamFile::tr("%1 (%2): unknown parameter, skipped.").arg(name, idText);
            continue;
        }
        const auto& info = focp_param_table[idx];
        if (name != QString::fromUtf8(info.name)) {
            r.warnings << ParamFile::tr("%1: name in file is \"%2\", expected \"%3\"; skipped.")
                              .arg(idText, name, QString::fromUtf8(info.name));
            continue;
        }
        if (info.flags & FOCP_F_RO) continue;   // reference only
        if ((info.flags & FOCP_F_SIM) && !allowSim) continue;
        const QJsonValue val = o.value(QStringLiteral("value"));
        if (!val.isDouble()) {
            r.warnings << ParamFile::tr("%1: value missing, skipped.").arg(name);
            continue;
        }
        const double d = val.toDouble();
        if (!std::isfinite(d) || d < double(info.min) || d > double(info.max)) {
            r.warnings << ParamFile::tr("%1 = %2 is outside %3 … %4, skipped.").arg(name).arg(d).arg(double(info.min)).arg(double(info.max));
            continue;
        }
        r.values.emplace_back(idx, float(d));
    }
    r.ok = true;
    return r;
}
