#pragma once
// Parameter files (JSON) with a version check against the protocol tables. Qt Core only.
#include <fochost/Device.hpp>

#include <QByteArray>
#include <QString>
#include <QStringList>

#include <utility>
#include <vector>

struct ParamFileMeta {
    QString programVersion;
    fochost::DriveInfo drive;
    bool includeSim = false;   ///< also write sim.* (virtual drive) parameters
};

/// Writes all valid parameters. Read-only ones are included for reference and marked.
QByteArray paramsToJson(const std::vector<fochost::ParamState>& params, const ParamFileMeta& meta);

struct ParamFileImport {
    bool ok = false;
    QString error;                                ///< set when the file is unusable as a whole
    QStringList warnings;                         ///< skipped entries
    std::vector<std::pair<int, float>> values;    ///< (focp_param_idx_t, value) to write
};

/// Checks format, protocol version, IDs/names and ranges. Read-only and (unless allowed)
/// sim.* parameters are skipped.
ParamFileImport paramsFromJson(const QByteArray& json, bool allowSim);
