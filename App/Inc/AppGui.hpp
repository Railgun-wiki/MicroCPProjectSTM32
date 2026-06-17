#pragma once
#include "GuiEngine.hpp"
#include "ITouch.hpp"
#include "sys.hpp"
#include <stdint.h>

namespace App {

class AppGui {
public:
    static constexpr uint16_t kHistorySize = 300;

    struct Model {
        int32_t temperature{0};
        int32_t humidity{0};
        uint32_t pressure{0};
        int32_t altitude{0};

        int32_t tempHighLimit{290};
        int32_t tempLowLimit{100};
        uint32_t pressHighLimit{103000};
        uint32_t pressLowLimit{98000};

        Sys::AlarmState alarmState{Sys::AlarmState::NORMAL};
        uint8_t currentViewPage{0};
        bool isMuted{false};

        bool tempHumConnected{false};
        bool pressureConnected{false};

        int32_t tempHistory[kHistorySize]{};
        uint32_t pressHistory[kHistorySize]{};
        uint16_t historyCount{0};

        uint8_t selectedThresholdField{0};
        int32_t pendingTempHighLimit{290};
        int32_t pendingTempLowLimit{100};
        uint32_t pendingPressHighLimit{103000};
        uint32_t pendingPressLowLimit{98000};
    };

    struct Command {
        enum class Type : uint8_t {
            None,
            TogglePage,
            EnterSettings,
            AdjustThreshold,
            Confirm,
            Cancel
        };

        Type type{Type::None};
        uint8_t field{0};
        bool increase{false};
    };

    explicit AppGui(ILcdDisplay& lcd);

    void setModel(const Model& model);
    void renderTick();
    void renderBootScan(uint32_t groupNumber, bool aht20Connected, bool bmp280Connected);
    Command hitTest(const TouchPoint& point) const;

private:
    enum DirtyFlags : uint8_t {
        kDirtyNone = 0,
        kDirtyGraph = 1U << 0,
        kDirtyDataPanel = 1U << 1,
        kDirtyFooter = 1U << 2,
        kDirtySettings = 1U << 3,
        kDirtyHeader = 1U << 4,
        kDirtyFullPage = 1U << 5
    };

    enum class RenderStage : uint8_t {
        Idle,
        ClearFull,
        Header,
        GraphClear,
        GraphBorder,
        GraphWaitingOrLine,
        DataPanelClear,
        DataPanelText,
        FooterClear,
        FooterText,
        SettingsClear,
        SettingsRows,
        SettingsControls,
        Done
    };

    ILcdDisplay& m_lcd;
    GuiEngine m_gui;
    Model m_model{};
    Model m_drawnModel{};
    Model m_planModel{};
    bool m_hasModel{false};
    bool m_hasDrawnModel{false};

    RenderStage m_stage{RenderStage::Idle};
    uint16_t m_bandY{0};
    uint16_t m_graphIndex{1};
    uint8_t m_settingsRow{0};
    uint8_t m_textStep{0};
    uint8_t m_dirtyFlags{kDirtyNone};
    uint8_t m_planFlags{kDirtyNone};
    uint8_t m_planPage{0};
    bool m_planFull{false};

    void markDirtyForChange(const Model& previous, const Model& next, bool first, bool pageChanged);
    void startPlan();
    void finishPlan();
    bool renderBand(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color);
    void advanceAfterHeader();
    void advanceAfterGraph();
    void advanceAfterDataPanel();
    void advanceAfterFooter();

    void drawHeaderStep();
    void drawGraphWaitingOrLine();
    void drawGraphAppend();
    void drawDataPanelTextStep();
    void drawFooterTextStep();
    void drawSettingsRowStep();
    void drawSettingsControlsStep();

    bool needsGraphFull(const Model& previous, const Model& next, bool pressure) const;
    bool canAppendGraphSegment(const Model& previous, const Model& next, bool pressure) const;
    bool historyChanged(const Model& a, const Model& b) const;
    bool tempDataPanelChanged(const Model& a, const Model& b) const;
    bool pressDataPanelChanged(const Model& a, const Model& b) const;
    bool footerChanged(const Model& a, const Model& b) const;
    bool settingsChanged(const Model& a, const Model& b) const;
    bool tempGraphRangeChanged(const Model& previous, const Model& next) const;
    bool pressGraphRangeChanged(const Model& previous, const Model& next) const;
    void tempRange(const Model& model, int32_t& minValue, int32_t& maxValue) const;
    void pressRange(const Model& model, uint32_t& minValue, uint32_t& maxValue) const;
    uint16_t graphXForIndex(uint16_t index, uint16_t x0, uint16_t x1) const;
    uint16_t graphYForValue(int32_t value, int32_t minValue, int32_t maxValue,
                            uint16_t y0, uint16_t y1) const;
};

} // namespace App
