#pragma once
#include <fochost/Device.hpp>

#include <string>
#include <vector>

struct ImFont;

/// FOC Studio - all UI state and panels. Rendering backend lives in main.cpp.
class App {
public:
    App();
    void setFont(ImFont* mono) { mono_ = mono; }
    void frame();                         ///< build one ImGui frame
    void startDemo();                     ///< connect to the virtual drive and run a demo profile
    bool wantsQuit() const { return quit_; }

private:
    void dockLayout(unsigned int dockId);
    void panelConnection();
    void panelDrive();
    void panelParameters();
    void panelScope();
    void panelLog();
    void applyScopeConfig();
    void demoStep();

    fochost::Device dev_;

    // connection
    int transportSel_ = 0;
    char channel_[32] = "can0";
    int bitrateSel_ = 2;
    int node_ = 1;
    std::string connError_;

    // drive
    float ref_[5] = {0, 2.0f, 1000.0f, 0.0f, 300.0f};   ///< per mode: off, torque, speed, position, open loop
    bool refLive_ = true;

    // parameters
    char paramFilter_[64] = "";
    bool showSimParams_ = true;
    std::vector<float> editBuf_;

    // scope
    std::vector<bool> sigOn_;
    int scopeDiv_ = 20;
    float window_ = 5.0f;
    bool follow_ = true;
    bool paused_ = false;
    char csvPath_[256] = "scope.csv";
    std::string csvMsg_;
    std::vector<double> tWin_;
    double tLastShown_ = 0.0;
    std::vector<int> slotSigShown_;
    std::vector<std::vector<double>> yWin_;

    // demo
    bool demo_ = false;
    double demoT0_ = 0.0;
    int demoPhase_ = -1;

    bool layoutDone_ = false;
    bool quit_ = false;
    bool showImGuiDemo_ = false;
    ImFont* mono_ = nullptr;
};
