#include "App.hpp"

#include <imgui.h>
#include <imgui_internal.h>
#include <implot.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>

using namespace fochost;

namespace {

const int kBitrates[] = {125000, 250000, 500000, 1000000};
const char* kBitrateNames[] = {"125 kbit/s", "250 kbit/s", "500 kbit/s", "1 Mbit/s"};
const char* kModeNames[] = {"Aus", "Moment (iq)", "Drehzahl", "Lage", "Open-Loop I/f"};
const char* kSensorNames[] = {"Hall", "Sensorlos", "Hybrid (Hall + Beobachter)"};
const char* kRefUnit[] = {"", "A", "rpm", "deg", "rpm"};

ImVec4 stateColor(uint16_t st)
{
    switch (st) {
    case FOCP_ST_RUN:   return {0.20f, 0.75f, 0.35f, 1.0f};
    case FOCP_ST_READY: return {0.25f, 0.55f, 0.90f, 1.0f};
    case FOCP_ST_FAULT: return {0.90f, 0.25f, 0.25f, 1.0f};
    case FOCP_ST_CALIB: return {0.95f, 0.65f, 0.15f, 1.0f};
    default:            return {0.55f, 0.55f, 0.55f, 1.0f};
    }
}

void badge(const char* text, ImVec4 col, ImVec2 size)
{
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, col);
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1, 1, 1, 1));
    ImGui::Button(text, size);
    ImGui::PopStyleColor(4);
}

bool coloredButton(const char* label, ImVec4 col, ImVec2 size = {0, 0})
{
    ImGui::PushStyleColor(ImGuiCol_Button, col);
    ImGui::PushStyleColor(ImGuiCol_ButtonHovered, ImVec4(col.x * 1.15f, col.y * 1.15f, col.z * 1.15f, 1));
    ImGui::PushStyleColor(ImGuiCol_ButtonActive, ImVec4(col.x * 0.85f, col.y * 0.85f, col.z * 0.85f, 1));
    const bool r = ImGui::Button(label, size);
    ImGui::PopStyleColor(3);
    return r;
}

void helpMarker(const char* text)
{
    ImGui::SameLine();
    ImGui::TextDisabled("(?)");
    if (ImGui::BeginItemTooltip()) {
        ImGui::PushTextWrapPos(ImGui::GetFontSize() * 30.0f);
        ImGui::TextUnformatted(text);
        ImGui::PopTextWrapPos();
        ImGui::EndTooltip();
    }
}

std::string fmtValue(float v, uint16_t type)
{
    char b[32];
    if (type != FOCP_T_F32) std::snprintf(b, sizeof(b), "%d", int(std::lround(v)));
    else std::snprintf(b, sizeof(b), "%.6g", v);
    return b;
}

} // namespace

// ------------------------------------------------------------------------------------------
App::App() : editBuf_(FOCP_NUM_PARAMS, 0.0f), sigOn_(FOCP_NUM_SIGNALS, false)
{
    for (int i = 0; i < FOCP_NUM_PARAMS; ++i) editBuf_[i] = focp_param_table[i].def;
    sigOn_[FOCP_S_SPEED] = sigOn_[FOCP_S_SPEED_REF] = true;
    sigOn_[FOCP_S_ID] = sigOn_[FOCP_S_IQ] = sigOn_[FOCP_S_IQ_REF] = true;
#ifdef __linux__
    transportSel_ = 0;
#endif
}

void App::frame()
{
    const ImGuiID dock = ImGui::DockSpaceOverViewport(0, nullptr, ImGuiDockNodeFlags_PassthruCentralNode);
    if (!layoutDone_) { dockLayout(dock); layoutDone_ = true; }

    if (ImGui::BeginMainMenuBar()) {
        if (ImGui::BeginMenu("Datei")) {
            if (ImGui::MenuItem("Demo mit virtuellem Antrieb")) startDemo();
            ImGui::Separator();
            if (ImGui::MenuItem("Beenden")) quit_ = true;
            ImGui::EndMenu();
        }
        if (ImGui::BeginMenu("Ansicht")) {
            if (ImGui::MenuItem("Layout zuruecksetzen")) layoutDone_ = false;
            ImGui::MenuItem("ImGui-Demo", nullptr, &showImGuiDemo_);
            ImGui::EndMenu();
        }
        const auto st = dev_.status();
        ImGui::SameLine(ImGui::GetWindowWidth() - 360);
        if (dev_.connected())
            ImGui::TextColored(ImVec4(0.5f, 0.85f, 0.5f, 1), "%s  |  %llu Nachrichten", dev_.transportName().c_str(),
                               static_cast<unsigned long long>(dev_.rxMessages()));
        else
            ImGui::TextDisabled("nicht verbunden");
        (void)st;
        ImGui::EndMainMenuBar();
    }

    if (demo_) demoStep();
    panelConnection();
    panelDrive();
    panelParameters();
    panelScope();
    panelLog();
    if (showImGuiDemo_) ImGui::ShowDemoWindow(&showImGuiDemo_);
}

void App::dockLayout(unsigned int dock)
{
    ImGui::DockBuilderRemoveNode(dock);
    ImGui::DockBuilderAddNode(dock, ImGuiDockNodeFlags_DockSpace);
    ImGui::DockBuilderSetNodeSize(dock, ImGui::GetMainViewport()->WorkSize);
    ImGuiID left, right;
    ImGui::DockBuilderSplitNode(dock, ImGuiDir_Left, 0.27f, &left, &right);
    ImGuiID leftTop, leftBottom;
    ImGui::DockBuilderSplitNode(left, ImGuiDir_Up, 0.22f, &leftTop, &leftBottom);
    ImGuiID rightTop, rightBottom;
    ImGui::DockBuilderSplitNode(right, ImGuiDir_Down, 0.30f, &rightBottom, &rightTop);
    ImGui::DockBuilderDockWindow("Verbindung", leftTop);
    ImGui::DockBuilderDockWindow("Antrieb", leftBottom);
    ImGui::DockBuilderDockWindow("Scope", rightTop);
    ImGui::DockBuilderDockWindow("Parameter", rightBottom);
    ImGui::DockBuilderDockWindow("Ereignisse", rightBottom);
    ImGui::DockBuilderFinish(dock);
}

// ------------------------------------------------------------------------------------------
void App::panelConnection()
{
    if (!ImGui::Begin("Verbindung")) { ImGui::End(); return; }
    const auto kinds = availableTransports();
    const bool conn = dev_.connected();

    ImGui::BeginDisabled(conn);
    if (transportSel_ >= int(kinds.size())) transportSel_ = 0;
    ImGui::SetNextItemWidth(-FLT_MIN);
    if (ImGui::BeginCombo("##transport", transportName(kinds[transportSel_]))) {
        for (int i = 0; i < int(kinds.size()); ++i)
            if (ImGui::Selectable(transportName(kinds[i]), i == transportSel_)) {
                transportSel_ = i;
                std::strcpy(channel_, kinds[i] == TransportKind::PCAN ? "USB1" : kinds[i] == TransportKind::GsUsb ? "" : "can0");
            }
        ImGui::EndCombo();
    }
    const TransportKind kind = kinds[transportSel_];
    if (kind != TransportKind::Simulation) {
        ImGui::SetNextItemWidth(100);
        ImGui::InputText("Kanal", channel_, sizeof(channel_));
        if (kind == TransportKind::PCAN || kind == TransportKind::GsUsb) {   // gs_usb: empty channel = first adapter
            ImGui::SameLine();
            ImGui::SetNextItemWidth(110);
            ImGui::Combo("##br", &bitrateSel_, kBitrateNames, IM_ARRAYSIZE(kBitrateNames));
        }
    }
    ImGui::SetNextItemWidth(100);
    ImGui::InputInt("Knoten", &node_);
    node_ = std::clamp(node_, 0, 15);
    ImGui::EndDisabled();

    if (!conn) {
        if (coloredButton("Verbinden", ImVec4(0.2f, 0.5f, 0.3f, 1), ImVec2(-FLT_MIN, 0))) {
            TransportConfig cfg;
            cfg.kind = kind;
            cfg.channel = channel_;
            cfg.bitrate = kBitrates[bitrateSel_];
            connError_.clear();
            if (dev_.connect(cfg, uint16_t(node_), connError_)) applyScopeConfig();
        }
        if (!connError_.empty()) ImGui::TextColored(ImVec4(1, 0.4f, 0.4f, 1), "%s", connError_.c_str());
    } else {
        if (ImGui::Button("Trennen", ImVec2(-FLT_MIN, 0))) { demo_ = false; dev_.disconnect(); }
        const auto inf = dev_.info();
        if (inf.valid)
            ImGui::TextDisabled("FW %d.%d  Board 0x%02X  Protokoll v%d", inf.fwMajor, inf.fwMinor, inf.board, inf.protocol);
        bool hb = dev_.heartbeat;
        if (ImGui::Checkbox("Heartbeat senden", &hb)) dev_.heartbeat = hb;
        helpMarker("Das Tool sendet alle 100 ms ein Lebenszeichen. Bleibt es laenger als "
                   "comm.timeout_ms aus, schaltet der Antrieb im Betrieb ab (Fehler COMM).");
    }
    ImGui::End();
}

// ------------------------------------------------------------------------------------------
void App::panelDrive()
{
    if (!ImGui::Begin("Antrieb")) { ImGui::End(); return; }
    const bool conn = dev_.connected();
    const auto st = dev_.status();
    const auto params = dev_.params();
    const bool running = st.state == FOCP_ST_RUN || st.state == FOCP_ST_CALIB;
    const float w = ImGui::GetContentRegionAvail().x;

    // --- status --------------------------------------------------------------------------
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%s", conn && st.valid ? focp_state_name(st.state) : "---");
    if (mono_) ImGui::PushFont(mono_);
    badge(buf, conn && st.valid ? stateColor(st.state) : ImVec4(0.3f, 0.3f, 0.3f, 1), ImVec2(w, 40));
    if (mono_) ImGui::PopFont();

    if (ImGui::BeginTable("stat", 2, ImGuiTableFlags_SizingStretchSame)) {
        auto row = [](const char* k, const char* v) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::TextDisabled("%s", k);
            ImGui::TableNextColumn(); ImGui::TextUnformatted(v);
        };
        char v[48];
        std::snprintf(v, sizeof(v), "%.2f V", st.udc);              row("Zwischenkreis", st.valid ? v : "-");
        std::snprintf(v, sizeof(v), "%.1f degC", st.temp);          row("Temperatur", st.valid ? v : "-");
        row("Regelmodus", st.valid ? focp_mode_name(st.mode) : "-");
        row("Lagegeber", st.valid && st.sensor < 3 ? kSensorNames[st.sensor] : "-");
        std::snprintf(v, sizeof(v), "%.1f %%", params[FOCP_P_SYS_ISR_LOAD].value);
        row("ISR-Last", params[FOCP_P_SYS_ISR_LOAD].valid ? v : "-");
        ImGui::EndTable();
    }

    if (st.faults) {
        ImGui::Spacing();
        for (uint16_t b = 1; b; b <<= 1)
            if (st.faults & b) ImGui::TextColored(ImVec4(1, 0.35f, 0.35f, 1), "! %s", focp_fault_name(b));
    }

    ImGui::SeparatorText("Steuerung");
    ImGui::BeginDisabled(!conn);
    const float bw = (w - ImGui::GetStyle().ItemSpacing.x) * 0.5f;
    ImGui::BeginDisabled(st.state != FOCP_ST_READY);
    if (coloredButton("Freigeben", ImVec4(0.18f, 0.55f, 0.28f, 1), ImVec2(bw, 34))) {
        dev_.setReference(ref_[std::clamp<int>(int(params[FOCP_P_CTRL_MODE].value), 0, 4)]);
        dev_.enable();
    }
    ImGui::EndDisabled();
    ImGui::SameLine();
    if (coloredButton("STOPP", ImVec4(0.75f, 0.15f, 0.15f, 1), ImVec2(bw, 34))) dev_.disable();
    ImGui::BeginDisabled(st.state != FOCP_ST_FAULT);
    if (ImGui::Button("Fehler quittieren", ImVec2(-FLT_MIN, 0))) dev_.ackFault();
    ImGui::EndDisabled();

    // --- mode + sensor -------------------------------------------------------------------
    int mode = std::clamp(int(std::lround(params[FOCP_P_CTRL_MODE].value)), 0, 4);
    int sens = std::clamp(int(std::lround(params[FOCP_P_CTRL_POS_SENSOR].value)), 0, 2);
    ImGui::BeginDisabled(running);
    ImGui::SetNextItemWidth(-90);
    if (ImGui::Combo("Modus", &mode, kModeNames, IM_ARRAYSIZE(kModeNames))) dev_.writeParam(FOCP_P_CTRL_MODE, float(mode));
    ImGui::SetNextItemWidth(-90);
    if (ImGui::Combo("Geber", &sens, kSensorNames, IM_ARRAYSIZE(kSensorNames))) dev_.writeParam(FOCP_P_CTRL_POS_SENSOR, float(sens));
    ImGui::EndDisabled();

    // --- reference -----------------------------------------------------------------------
    if (mode != FOCP_MODE_OFF) {
        float lo = -1, hi = 1;
        const float nmax = params[FOCP_P_MOTOR_N_MAX].value, iqmax = params[FOCP_P_SPD_IQ_MAX].value;
        switch (mode) {
        case FOCP_MODE_TORQUE:   lo = -iqmax; hi = iqmax; break;
        case FOCP_MODE_SPEED:    lo = -nmax; hi = nmax; break;
        case FOCP_MODE_POSITION: lo = -1080; hi = 1080; break;
        case FOCP_MODE_OPENLOOP: lo = -nmax; hi = nmax; break;
        }
        ImGui::Spacing();
        ImGui::Text("Sollwert [%s]", kRefUnit[mode]);
        ImGui::SetNextItemWidth(-FLT_MIN);
        bool changed = ImGui::SliderFloat("##refslider", &ref_[mode], lo, hi, "%.1f");
        ImGui::SetNextItemWidth(w * 0.5f);
        changed |= ImGui::InputFloat("##refin", &ref_[mode], 0, 0, "%.2f", ImGuiInputTextFlags_EnterReturnsTrue);
        ImGui::SameLine();
        if (ImGui::Button("0")) { ref_[mode] = 0; changed = true; }
        ImGui::SameLine();
        ImGui::Checkbox("live", &refLive_);
        ImGui::SameLine();
        const bool sendNow = ImGui::Button("Senden");
        if ((changed && refLive_) || sendNow) dev_.setReference(ref_[mode]);
        if (mode == FOCP_MODE_POSITION)
            ImGui::TextDisabled("Hall-Aufloesung: %.1f deg mech.", 360.0f / (6.0f * std::max(1.0f, params[FOCP_P_MOTOR_POLE_PAIRS].value)));
    }

    // --- commissioning -------------------------------------------------------------------
    ImGui::SeparatorText("Inbetriebnahme");
    ImGui::BeginDisabled(st.state != FOCP_ST_READY);
    if (ImGui::Button("Stromoffset", ImVec2(-FLT_MIN, 0))) dev_.calibrate(FOCP_CALIB_CURRENT_OFFSET);
    if (ImGui::Button("Hall-Offset kalibrieren", ImVec2(-FLT_MIN, 0))) dev_.calibrate(FOCP_CALIB_HALL);
    ImGui::SetItemTooltip("Rotor dreht im Open-Loop langsam mit ol.I. Die Hall-Flanken werden gegen den "
                          "Open-Loop-Winkel ausgewertet, das Ergebnis landet in hall.offset. Welle muss frei drehen koennen!");
    if (ImGui::Button("Motoridentifikation", ImVec2(-FLT_MIN, 0))) dev_.calibrate(FOCP_CALIB_MOTOR_ID);
    ImGui::SetItemTooltip("Rs, Ls, Psi, J messen (Phase P4, noch nicht implementiert)");
    ImGui::EndDisabled();
    ImGui::Text("hall.offset = %.1f deg", params[FOCP_P_HALL_OFFSET].value);
    ImGui::EndDisabled();
    ImGui::End();
}

// ------------------------------------------------------------------------------------------
void App::panelParameters()
{
    if (!ImGui::Begin("Parameter")) { ImGui::End(); return; }
    const bool conn = dev_.connected();
    const auto params = dev_.params();
    const auto st = dev_.status();
    const bool running = st.state == FOCP_ST_RUN || st.state == FOCP_ST_CALIB;

    ImGui::BeginDisabled(!conn);
    if (ImGui::Button("Alle lesen")) dev_.readAllParams();
    ImGui::SameLine();
    if (ImGui::Button("Im Antrieb speichern")) dev_.saveParams();
    ImGui::SameLine();
    ImGui::BeginDisabled(running);
    if (ImGui::Button("Werkseinstellungen")) ImGui::OpenPopup("defaults?");
    ImGui::EndDisabled();
    ImGui::EndDisabled();
    if (ImGui::BeginPopupModal("defaults?", nullptr, ImGuiWindowFlags_AlwaysAutoResize)) {
        ImGui::Text("Alle Parameter im Antrieb auf Standardwerte setzen?");
        if (ImGui::Button("Ja", ImVec2(120, 0))) { dev_.loadDefaults(); ImGui::CloseCurrentPopup(); }
        ImGui::SameLine();
        if (ImGui::Button("Abbrechen", ImVec2(120, 0))) ImGui::CloseCurrentPopup();
        ImGui::EndPopup();
    }
    ImGui::SameLine();
    ImGui::SetNextItemWidth(180);
    ImGui::InputTextWithHint("##filter", "Filter (z. B. spd.)", paramFilter_, sizeof(paramFilter_));
    ImGui::SameLine();
    ImGui::Checkbox("Simulation", &showSimParams_);

    const ImGuiTableFlags tf = ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY |
                               ImGuiTableFlags_Resizable | ImGuiTableFlags_SizingStretchProp;
    if (ImGui::BeginTable("params", 5, tf)) {
        ImGui::TableSetupScrollFreeze(0, 1);
        ImGui::TableSetupColumn("Parameter", ImGuiTableColumnFlags_WidthStretch, 1.2f);
        ImGui::TableSetupColumn("Wert", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Einheit", ImGuiTableColumnFlags_WidthStretch, 0.5f);
        ImGui::TableSetupColumn("Bereich", ImGuiTableColumnFlags_WidthStretch, 1.0f);
        ImGui::TableSetupColumn("Beschreibung", ImGuiTableColumnFlags_WidthStretch, 2.4f);
        ImGui::TableHeadersRow();

        for (int i = 0; i < FOCP_NUM_PARAMS; ++i) {
            const auto& info = focp_param_table[i];
            const auto& p = params[i];
            if (!showSimParams_ && (info.flags & FOCP_F_SIM)) continue;
            if (paramFilter_[0] && !std::strstr(info.name, paramFilter_) && !std::strstr(info.desc, paramFilter_)) continue;

            ImGui::TableNextRow();
            ImGui::PushID(i);
            ImGui::TableNextColumn();
            ImVec4 nameCol = ImGui::GetStyleColorVec4(ImGuiCol_Text);
            if (info.flags & FOCP_F_SIM) nameCol = ImVec4(0.6f, 0.6f, 0.75f, 1);
            if (p.status != FOCP_PS_OK) nameCol = ImVec4(1, 0.45f, 0.4f, 1);
            if (p.pending) nameCol = ImVec4(1, 0.85f, 0.3f, 1);
            if (mono_) ImGui::PushFont(mono_);
            ImGui::TextColored(nameCol, "%s", info.name);
            if (mono_) ImGui::PopFont();
            if (p.status != FOCP_PS_OK) ImGui::SetItemTooltip("Letzter Schreibversuch abgelehnt (Status %d)", p.status);

            ImGui::TableNextColumn();
            const bool ro = (info.flags & FOCP_F_RO) || ((info.flags & FOCP_F_LOCK) && running) || !conn;
            if (ImGui::GetActiveID() != ImGui::GetID("##v")) editBuf_[i] = p.value;   // not being edited
            ImGui::SetNextItemWidth(-FLT_MIN);
            if (ro) {
                ImGui::TextDisabled("%s%s", fmtValue(p.value, info.type).c_str(), p.valid ? "" : " ?");
                if ((info.flags & FOCP_F_LOCK) && running) ImGui::SetItemTooltip("Nur im Stillstand aenderbar");
            } else {
                const char* fmt = info.type == FOCP_T_F32 ? "%.6g" : "%.0f";
                ImGui::InputFloat("##v", &editBuf_[i], 0, 0, fmt);
                if (ImGui::IsItemDeactivatedAfterEdit()) dev_.writeParam(i, editBuf_[i]);   // Enter or focus loss
            }
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s", info.unit);
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%s .. %s", fmtValue(info.min, info.type).c_str(), fmtValue(info.max, info.type).c_str());
            ImGui::TableNextColumn();
            ImGui::TextUnformatted(info.desc);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    ImGui::End();
}

// ------------------------------------------------------------------------------------------
void App::applyScopeConfig()
{
    std::vector<int> sel;
    for (int i = 0; i < FOCP_NUM_SIGNALS && sel.size() < FOCP_TELEM_SLOTS; ++i)
        if (sigOn_[i]) sel.push_back(i);
    dev_.configureScope(sel, uint16_t(std::max(1, scopeDiv_)));
    dev_.runScope(true);
}

void App::panelScope()
{
    if (!ImGui::Begin("Scope")) { ImGui::End(); return; }
    const bool conn = dev_.connected();

    // --- signal selection (left column) ---------------------------------------------------
    ImGui::BeginChild("sigs", ImVec2(210, 0), ImGuiChildFlags_Borders | ImGuiChildFlags_ResizeX);
    int nOn = int(std::count(sigOn_.begin(), sigOn_.end(), true));
    bool cfgChanged = false;
    ImGui::TextDisabled("Signale (%d/%d)", nOn, int(FOCP_TELEM_SLOTS));
    for (int i = 0; i < FOCP_NUM_SIGNALS; ++i) {
        const auto& s = focp_signal_table[i];
        bool on = sigOn_[i];
        ImGui::BeginDisabled(!on && nOn >= int(FOCP_TELEM_SLOTS));
        char label[64];
        std::snprintf(label, sizeof(label), "%s [%s]", s.name, s.unit);
        if (ImGui::Checkbox(label, &on)) { sigOn_[i] = on; cfgChanged = true; nOn += on ? 1 : -1; }
        ImGui::SetItemTooltip("%s", s.desc);
        ImGui::EndDisabled();
    }
    ImGui::Separator();
    const double fbase = dev_.telemetryBaseHz();
    ImGui::SetNextItemWidth(80);
    if (ImGui::InputInt("Teiler", &scopeDiv_, 1, 10, ImGuiInputTextFlags_EnterReturnsTrue)) cfgChanged = true;
    scopeDiv_ = std::clamp(scopeDiv_, 1, 10000);
    ImGui::TextDisabled("= %.0f Hz Abtastung", fbase / scopeDiv_);
    helpMarker("Bei CAN begrenzt die Buslast die Abtastrate: jede Abtastung braucht ceil(n/3) Frames. "
               "Bei 500 kbit/s sind etwa 3000 Frames/s realistisch.");
    if (cfgChanged && conn) applyScopeConfig();

    ImGui::SetNextItemWidth(80);
    ImGui::DragFloat("Fenster [s]", &window_, 0.1f, 0.05f, 120.0f, "%.2f");
    ImGui::Checkbox("Mitlaufen", &follow_);
    ImGui::Checkbox("Pause", &paused_);
    if (ImGui::Button("Loeschen")) dev_.clearScope();
    ImGui::SetNextItemWidth(-FLT_MIN);
    ImGui::InputText("##csv", csvPath_, sizeof(csvPath_));
    if (ImGui::Button("CSV exportieren")) {
        std::string err;
        csvMsg_ = dev_.exportCsv(csvPath_, err) ? std::string("gespeichert: ") + csvPath_ : err;
    }
    if (!csvMsg_.empty()) ImGui::TextWrapped("%s", csvMsg_.c_str());
    ImGui::TextDisabled("verloren: %llu", static_cast<unsigned long long>(dev_.lostSamples()));
    ImGui::EndChild();
    ImGui::SameLine();

    // --- plots --------------------------------------------------------------------------
    ImGui::BeginChild("plots");
    std::vector<int> slotSig;
    double tLast = 0;
    if (!paused_) {
        dev_.withScope([&](const ScopeBuffer& s) {
            tLast = s.tLast();
            slotSig = s.signals;
            s.copyWindow(tLast - window_, 20000, tWin_, yWin_);
        });
        tLastShown_ = tLast;
        slotSigShown_ = slotSig;
    } else {
        tLast = tLastShown_;
        slotSig = slotSigShown_;
    }

    // group slots by unit -> one plot per unit
    std::map<std::string, std::vector<int>> groups;
    std::vector<std::string> order;
    for (int k = 0; k < int(slotSig.size()); ++k) {
        if (slotSig[k] < 0 || k >= int(yWin_.size())) continue;
        std::string u = focp_signal_table[slotSig[k]].unit;
        if (u.empty()) u = "-";
        if (!groups.count(u)) order.push_back(u);
        groups[u].push_back(k);
    }
    if (order.empty()) {
        ImGui::TextDisabled(conn ? "Signale links auswaehlen." : "Nicht verbunden. Datei -> Demo startet den virtuellen Antrieb.");
    } else {
        const float h = std::max(120.0f, (ImGui::GetContentRegionAvail().y - ImGui::GetStyle().ItemSpacing.y * (order.size() - 1)) / order.size());
        static double xMin = 0, xMax = 1;
        if (follow_) { xMin = tLast - window_; xMax = tLast; }
        for (const auto& u : order) {
            if (ImPlot::BeginPlot(("##" + u).c_str(), ImVec2(-1, h), ImPlotFlags_NoMenus)) {
                ImPlot::SetupAxes(nullptr, u.c_str(), ImPlotAxisFlags_None, ImPlotAxisFlags_AutoFit);
                ImPlot::SetupAxisLinks(ImAxis_X1, &xMin, &xMax);
                if (follow_) ImPlot::SetupAxisLimits(ImAxis_X1, xMin, xMax, ImGuiCond_Always);
                ImPlot::SetupLegend(ImPlotLocation_NorthWest, ImPlotLegendFlags_None);
                for (int k : groups[u]) {
                    const auto& s = focp_signal_table[slotSig[k]];
                    ImPlotSpec spec;
                    spec.LineWeight = 1.5f;
                    ImPlot::PlotLine(s.name, tWin_.data(), yWin_[k].data(), int(tWin_.size()), spec);
                }
                ImPlot::EndPlot();
            }
        }
    }
    ImGui::EndChild();
    ImGui::End();
}

// ------------------------------------------------------------------------------------------
void App::panelLog()
{
    if (!ImGui::Begin("Ereignisse")) { ImGui::End(); return; }
    if (ImGui::Button("Leeren")) dev_.clearLog();
    const auto log = dev_.log();
    if (ImGui::BeginTable("log", 2, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY | ImGuiTableFlags_SizingFixedFit)) {
        ImGui::TableSetupColumn("t [s]");
        ImGui::TableSetupColumn("Meldung", ImGuiTableColumnFlags_WidthStretch);
        for (const auto& e : log) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled("%8.3f", e.t);
            ImGui::TableNextColumn();
            const ImVec4 c = e.level == 2 ? ImVec4(1, 0.4f, 0.4f, 1) : e.level == 1 ? ImVec4(1, 0.8f, 0.3f, 1) : ImGui::GetStyleColorVec4(ImGuiCol_Text);
            ImGui::TextColored(c, "%s", e.text.c_str());
        }
        if (ImGui::GetScrollY() >= ImGui::GetScrollMaxY() - 5) ImGui::SetScrollHereY(1.0f);
        ImGui::EndTable();
    }
    ImGui::End();
}

// ------------------------------------------------------------------------------------------
void App::startDemo()
{
    if (dev_.connected()) dev_.disconnect();
    std::string err;
    TransportConfig cfg;
    cfg.kind = TransportKind::Simulation;
    if (!dev_.connect(cfg, 1, err)) { connError_ = err; return; }
    std::fill(sigOn_.begin(), sigOn_.end(), false);
    for (int s : {FOCP_S_SPEED, FOCP_S_SPEED_REF, FOCP_S_IQ, FOCP_S_IQ_REF, FOCP_S_ID, FOCP_S_THETA_EL, FOCP_S_THETA_HALL, FOCP_S_TORQUE})
        sigOn_[s] = true;
    window_ = 6.0f;
    applyScopeConfig();
    demo_ = true;
    demoPhase_ = -1;
    demoT0_ = ImGui::GetTime();
}

void App::demoStep()
{
    if (!dev_.connected()) { demo_ = false; return; }
    const double t = ImGui::GetTime() - demoT0_;
    const auto st = dev_.status();
    auto phase = [&](int p, double at) { return demoPhase_ < p && t >= at; };

    if (phase(0, 0.6) && st.state == FOCP_ST_READY) { dev_.calibrate(FOCP_CALIB_HALL); demoPhase_ = 0; }
    if (phase(1, 3.4) && st.state == FOCP_ST_READY) {
        dev_.writeParam(FOCP_P_CTRL_MODE, FOCP_MODE_SPEED);
        ref_[FOCP_MODE_SPEED] = 1500;
        dev_.setReference(1500);
        dev_.enable();
        demoPhase_ = 1;
    }
    if (phase(2, 5.5)) { dev_.writeParam(FOCP_P_SIM_T_LOAD, 0.3f); demoPhase_ = 2; }
    if (phase(3, 7.0)) { ref_[FOCP_MODE_SPEED] = 3000; dev_.setReference(3000); demoPhase_ = 3; }
    if (phase(4, 9.0)) { dev_.writeParam(FOCP_P_SIM_T_LOAD, 0.0f); ref_[FOCP_MODE_SPEED] = -1000; dev_.setReference(-1000); demoPhase_ = 4; }
    if (phase(5, 11.5)) { demo_ = false; demoPhase_ = 5; }
}
