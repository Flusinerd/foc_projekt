// Parameter file (JSON): round trip and version / ID / range checks.
#include "check.hpp"

#include <ParamFile.hpp>

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

#include <algorithm>

using namespace fochost;

namespace {

std::vector<ParamState> defaults()
{
    std::vector<ParamState> p(FOCP_NUM_PARAMS);
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) {
        p[size_t(i)].value = focp_param_table[i].def;
        p[size_t(i)].valid = true;
    }
    return p;
}

QJsonObject parse(const QByteArray& j) { return QJsonDocument::fromJson(j).object(); }

bool hasValue(const ParamFileImport& r, int idx, float v)
{
    return std::any_of(r.values.begin(), r.values.end(), [&](const auto& e) { return e.first == idx && e.second == v; });
}

bool hasIdx(const ParamFileImport& r, int idx)
{
    return std::any_of(r.values.begin(), r.values.end(), [&](const auto& e) { return e.first == idx; });
}

} // namespace

int main()
{
    std::puts("round trip");
    auto p = defaults();
    p[FOCP_P_HALL_OFFSET].value = 23.2f;
    p[FOCP_P_SPD_KP].value = 0.01f;
    p[FOCP_P_CUR_KI].valid = false;   // never read -> not exported
    ParamFileMeta meta;
    meta.programVersion = QStringLiteral("0.1.0");
    const QByteArray json = paramsToJson(p, meta);
    const QJsonObject root = parse(json);
    CHECK(root.value(QStringLiteral("format")).toString() == QStringLiteral("focstudio-params"));
    CHECK(root.value(QStringLiteral("protocol_version")).toInt() == int(FOCP_PROTOCOL_VERSION));

    ParamFileImport r = paramsFromJson(json, false);
    CHECK(r.ok);
    CHECK(r.warnings.isEmpty());
    CHECK(hasValue(r, FOCP_P_HALL_OFFSET, 23.2f));
    CHECK(hasValue(r, FOCP_P_SPD_KP, 0.01f));
    CHECK(!hasIdx(r, FOCP_P_CUR_KI));
    CHECK(!hasIdx(r, FOCP_P_INV_F_PWM));      // read-only: exported for reference only
    CHECK(!hasIdx(r, FOCP_P_SIM_T_LOAD));     // sim.* not exported without includeSim

    std::puts("sim parameters only when allowed");
    meta.includeSim = true;
    const QByteArray withSim = paramsToJson(p, meta);
    CHECK(hasIdx(paramsFromJson(withSim, true), FOCP_P_SIM_T_LOAD));
    CHECK(!hasIdx(paramsFromJson(withSim, false), FOCP_P_SIM_T_LOAD));

    std::puts("version checks");
    {
        QJsonObject o = root;
        o[QStringLiteral("protocol_version")] = int(FOCP_PROTOCOL_VERSION) + 1;
        const auto x = paramsFromJson(QJsonDocument(o).toJson(), false);
        CHECK(!x.ok && !x.error.isEmpty());
        std::printf("  -> %s\n", qPrintable(x.error));
    }
    {
        QJsonObject o = root;
        o[QStringLiteral("format_version")] = 99;
        CHECK(!paramsFromJson(QJsonDocument(o).toJson(), false).ok);
        o = root;
        o[QStringLiteral("format")] = QStringLiteral("something-else");
        CHECK(!paramsFromJson(QJsonDocument(o).toJson(), false).ok);
        CHECK(!paramsFromJson("not json", false).ok);
    }

    std::puts("entry checks: unknown id, renamed, out of range");
    {
        QJsonObject o = root;
        QJsonArray list = o.value(QStringLiteral("params")).toArray();
        QJsonObject unknown;
        unknown[QStringLiteral("id")] = QStringLiteral("0x7777");
        unknown[QStringLiteral("name")] = QStringLiteral("does.not_exist");
        unknown[QStringLiteral("value")] = 1.0;
        list.append(unknown);
        for (int k = 0; k < list.size(); ++k) {
            QJsonObject e = list[k].toObject();
            const QString n = e.value(QStringLiteral("name")).toString();
            if (n == QLatin1String("motor.pole_pairs")) e[QStringLiteral("name")] = QStringLiteral("motor.poles");
            if (n == QLatin1String("spd.Kp")) e[QStringLiteral("value")] = 1e9;
            list[k] = e;
        }
        o[QStringLiteral("params")] = list;
        const auto x = paramsFromJson(QJsonDocument(o).toJson(), false);
        CHECK(x.ok);
        CHECK(x.warnings.size() == 3);
        CHECK(!hasIdx(x, FOCP_P_MOTOR_POLE_PAIRS));
        CHECK(!hasIdx(x, FOCP_P_SPD_KP));
        CHECK(hasValue(x, FOCP_P_HALL_OFFSET, 23.2f));
        for (const QString& w : x.warnings) std::printf("  -> %s\n", qPrintable(w));
    }

    return TEST_RESULT();
}
