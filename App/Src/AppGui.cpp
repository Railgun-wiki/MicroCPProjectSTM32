#include "AppGui.hpp"
#include <stdio.h>

namespace App {

namespace {

constexpr uint16_t kColorBlack = 0x0000;
constexpr uint16_t kColorWhite = 0xFFFF;
constexpr uint16_t kColorBlue = 0x001F;
constexpr uint16_t kColorRed = 0xF800;
constexpr uint16_t kColorGreen = 0x07E0;
constexpr uint16_t kColorYellow = 0xFFE0;
constexpr uint16_t kColorGray = 0x8430;

constexpr uint16_t kGraphX0 = 20;
constexpr uint16_t kGraphY0 = 60;
constexpr uint16_t kGraphX1 = 460;
constexpr uint16_t kGraphY1 = 170;
constexpr uint16_t kSettingsButtonX0 = 20;
constexpr uint16_t kSettingsButtonX1 = 220;
constexpr uint16_t kSettingsButtonY0 = 245;
constexpr uint16_t kSettingsButtonY1 = 300;
constexpr uint16_t kPageToggleMarginY = 230;
constexpr uint16_t kThresholdRowHeight = 42;
constexpr uint8_t kThresholdRowCount = 4;
constexpr uint16_t kThresholdRowXMinus = 360;
constexpr uint16_t kThresholdRowXPlus = 400;
constexpr uint16_t kThresholdControlY0 = 250;
constexpr uint16_t kThresholdControlY1 = 295;
constexpr uint16_t kConfirmButtonX0 = 40;
constexpr uint16_t kConfirmButtonX1 = 190;
constexpr uint16_t kCancelButtonX0 = 300;
constexpr uint16_t kCancelButtonX1 = 450;
constexpr uint16_t kBandHeight = 8;

static void formatScaledInt(char* dest, const char* prefix, int32_t val, const char* suffix, int divisor)
{
    int32_t absVal = val < 0 ? -val : val;
    int32_t intPart = absVal / divisor;
    int32_t fracPart = absVal % divisor;

    if (divisor == 10) {
        sprintf(dest, "%s%s%ld.%ld%s", prefix, val < 0 ? "-" : "", (long)intPart, (long)fracPart, suffix);
    } else if (divisor == 100) {
        sprintf(dest, "%s%s%ld.%02ld%s", prefix, val < 0 ? "-" : "", (long)intPart, (long)fracPart, suffix);
    } else {
        sprintf(dest, "%s%ld%s", prefix, (long)val, suffix);
    }
}

static void formatPressureKpa(char* dest, const char* prefix, uint32_t pressurePa, const char* suffix)
{
    const uint32_t whole = pressurePa / 1000U;
    const uint32_t fract = (pressurePa % 1000U) / 100U;
    sprintf(dest, "%s%lu.%01lu%s", prefix, (unsigned long)whole, (unsigned long)fract, suffix);
}

} // namespace

AppGui::AppGui(ILcdDisplay& lcd)
    : m_lcd(lcd)
    , m_gui(lcd)
{
}

void AppGui::setModel(const Model& model)
{
    const bool first = !m_hasModel;
    const bool pageChanged = !first && (model.currentViewPage != m_model.currentViewPage);
    const Model previous = (m_stage != RenderStage::Idle) ? m_planModel :
        (m_hasDrawnModel ? m_drawnModel : m_model);

    m_model = model;
    m_hasModel = true;
    markDirtyForChange(previous, m_model, first, pageChanged);

    if (first || pageChanged) {
        m_stage = RenderStage::Idle;
    }

    if (m_stage == RenderStage::Idle) {
        startPlan();
    }
}

void AppGui::renderTick()
{
    if (!m_hasModel) {
        return;
    }

    if (m_stage == RenderStage::Idle) {
        startPlan();
        if (m_stage == RenderStage::Idle) {
            return;
        }
    }

    switch (m_stage) {
        case RenderStage::ClearFull:
            if (!renderBand(0, 0, static_cast<uint16_t>(m_lcd.getWidth() - 1U),
                            static_cast<uint16_t>(m_lcd.getHeight() - 1U), kColorBlack)) {
                m_stage = RenderStage::Header;
                m_textStep = 0;
            }
            break;
        case RenderStage::Header:
            drawHeaderStep();
            break;
        case RenderStage::GraphClear:
            if (!renderBand(kGraphX0, kGraphY0, kGraphX1, kGraphY1, kColorBlack)) {
                m_stage = RenderStage::GraphBorder;
            }
            break;
        case RenderStage::GraphBorder:
            m_gui.drawRectBorder(kGraphX0, kGraphY0, kGraphX1, kGraphY1, kColorGray);
            m_stage = RenderStage::GraphWaitingOrLine;
            m_graphIndex = 1;
            break;
        case RenderStage::GraphWaitingOrLine:
            drawGraphWaitingOrLine();
            break;
        case RenderStage::DataPanelClear:
            if (!renderBand(20, 190, 460, 248, kColorBlack)) {
                m_stage = RenderStage::DataPanelText;
                m_textStep = 0;
            }
            break;
        case RenderStage::DataPanelText:
            drawDataPanelTextStep();
            break;
        case RenderStage::FooterClear:
            if (!renderBand(20, 220, 460, 312, kColorBlack)) {
                m_stage = RenderStage::FooterText;
                m_textStep = 0;
            }
            break;
        case RenderStage::FooterText:
            drawFooterTextStep();
            break;
        case RenderStage::SettingsClear:
            if (!renderBand(18, 58, 462, 226, kColorBlack)) {
                m_stage = RenderStage::SettingsRows;
                m_settingsRow = 0;
                m_textStep = 0;
            }
            break;
        case RenderStage::SettingsRows:
            drawSettingsRowStep();
            break;
        case RenderStage::SettingsControls:
            drawSettingsControlsStep();
            break;
        case RenderStage::Done:
            finishPlan();
            break;
        default:
            break;
    }
}

void AppGui::renderBootScan(uint32_t groupNumber, bool aht20Connected, bool bmp280Connected)
{
    char lineBuf[48];
    m_lcd.clear(kColorBlack);
    m_lcd.drawString(20, 30, "MicroCP Sensor Monitor", kColorYellow, kColorBlack, 16);
    m_lcd.drawString(20, 55, "Scanning I2C2 Bus...", kColorWhite, kColorBlack, 16);
    sprintf(lineBuf, "GROUP: %lu", (unsigned long)groupNumber);
    m_lcd.drawString(20, 85, lineBuf, kColorGreen, kColorBlack, 16);
    sprintf(lineBuf, "AHT20: %s", aht20Connected ? "Connected" : "Unconnected");
    m_lcd.drawString(20, 120, lineBuf, aht20Connected ? kColorGreen : kColorRed, kColorBlack, 16);
    sprintf(lineBuf, "BMP280: %s", bmp280Connected ? "Connected" : "Unconnected");
    m_lcd.drawString(20, 150, lineBuf, bmp280Connected ? kColorGreen : kColorRed, kColorBlack, 16);
}

AppGui::Command AppGui::hitTest(const TouchPoint& point) const
{
    Command command{};
    if (!point.valid) {
        return command;
    }

    if (m_model.currentViewPage != 2) {
        if (point.x >= kSettingsButtonX0 && point.x <= kSettingsButtonX1 &&
            point.y >= kSettingsButtonY0 && point.y <= kSettingsButtonY1) {
            command.type = Command::Type::EnterSettings;
            return command;
        }
        if (point.x > (m_lcd.getWidth() / 2U) && point.y < kPageToggleMarginY) {
            command.type = Command::Type::TogglePage;
        }
        return command;
    }

    for (uint8_t row = 0; row < kThresholdRowCount; ++row) {
        const uint16_t rowY0 = static_cast<uint16_t>(60U + row * kThresholdRowHeight);
        const uint16_t rowY1 = static_cast<uint16_t>(rowY0 + 32U);
        if (point.y < rowY0 || point.y > rowY1) {
            continue;
        }
        if (point.x >= kThresholdRowXMinus && point.x <= (kThresholdRowXMinus + 28U)) {
            command.type = Command::Type::AdjustThreshold;
            command.field = row;
            command.increase = false;
            return command;
        }
        if (point.x >= kThresholdRowXPlus && point.x <= (kThresholdRowXPlus + 28U)) {
            command.type = Command::Type::AdjustThreshold;
            command.field = row;
            command.increase = true;
            return command;
        }
    }

    if (point.y >= kThresholdControlY0 && point.y <= kThresholdControlY1) {
        if (point.x >= kConfirmButtonX0 && point.x <= kConfirmButtonX1) {
            command.type = Command::Type::Confirm;
        } else if (point.x >= kCancelButtonX0 && point.x <= kCancelButtonX1) {
            command.type = Command::Type::Cancel;
        }
    }

    return command;
}

void AppGui::markDirtyForChange(const Model& previous, const Model& next, bool first, bool pageChanged)
{
    if (first || pageChanged) {
        m_dirtyFlags |= kDirtyFullPage | kDirtyHeader | kDirtyGraph | kDirtyDataPanel | kDirtyFooter | kDirtySettings;
        return;
    }

    if (next.currentViewPage == 2) {
        if (settingsChanged(previous, next)) {
            m_dirtyFlags |= kDirtySettings;
        }
        return;
    }

    const bool pressure = next.currentViewPage == 1;
    if (pressure) {
        if (needsGraphFull(previous, next, true) || canAppendGraphSegment(previous, next, true)) {
            m_dirtyFlags |= kDirtyGraph;
        }
        if (pressDataPanelChanged(previous, next)) {
            m_dirtyFlags |= kDirtyDataPanel;
        }
    } else {
        if (needsGraphFull(previous, next, false) || canAppendGraphSegment(previous, next, false)) {
            m_dirtyFlags |= kDirtyGraph;
        }
        if (tempDataPanelChanged(previous, next)) {
            m_dirtyFlags |= kDirtyDataPanel;
        }
    }

    if (footerChanged(previous, next)) {
        m_dirtyFlags |= kDirtyFooter;
    }
}

void AppGui::startPlan()
{
    if (m_dirtyFlags == kDirtyNone) {
        return;
    }

    m_planFlags = m_dirtyFlags;
    m_dirtyFlags = kDirtyNone;
    m_planModel = m_model;
    m_planPage = m_model.currentViewPage;
    m_planFull = (m_planFlags & kDirtyFullPage) != 0U;
    m_bandY = 0;
    m_textStep = 0;
    m_graphIndex = 1;
    m_settingsRow = 0;

    if (m_planFull) {
        m_stage = RenderStage::ClearFull;
    } else if ((m_planFlags & kDirtyHeader) != 0U) {
        m_stage = RenderStage::Header;
    } else if (m_planPage == 2 && (m_planFlags & kDirtySettings) != 0U) {
        m_stage = RenderStage::SettingsClear;
        m_bandY = 58;
    } else if ((m_planFlags & kDirtyGraph) != 0U) {
        const bool pressure = m_planPage == 1;
        const bool append = canAppendGraphSegment(m_drawnModel, m_planModel, pressure) &&
            !needsGraphFull(m_drawnModel, m_planModel, pressure);
        m_stage = append ? RenderStage::GraphWaitingOrLine : RenderStage::GraphClear;
        m_bandY = kGraphY0;
    } else if ((m_planFlags & kDirtyDataPanel) != 0U) {
        m_stage = RenderStage::DataPanelClear;
        m_bandY = 190;
    } else if ((m_planFlags & kDirtyFooter) != 0U) {
        m_stage = RenderStage::FooterClear;
        m_bandY = 220;
    }
}

void AppGui::finishPlan()
{
    if (m_dirtyFlags == kDirtyNone) {
            m_drawnModel = m_planModel;
        m_hasDrawnModel = true;
    }
    m_stage = RenderStage::Idle;
    startPlan();
}

bool AppGui::renderBand(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color)
{
    if (m_bandY == 0 || m_bandY < y0) {
        m_bandY = y0;
    }
    if (m_bandY > y1) {
        return false;
    }

    const uint16_t bandY1 = static_cast<uint16_t>((m_bandY + kBandHeight - 1U > y1) ? y1 : m_bandY + kBandHeight - 1U);
    m_lcd.fillRect(x0, m_bandY, x1, bandY1, color);
    m_bandY = static_cast<uint16_t>(bandY1 + 1U);
    return m_bandY <= y1;
}

void AppGui::advanceAfterHeader()
{
    if (m_planPage == 2 && ((m_planFlags & kDirtySettings) != 0U || m_planFull)) {
        m_stage = RenderStage::SettingsClear;
        m_bandY = 58;
    } else if ((m_planFlags & kDirtyGraph) != 0U || m_planFull) {
        m_stage = RenderStage::GraphClear;
        m_bandY = kGraphY0;
    } else if ((m_planFlags & kDirtyDataPanel) != 0U) {
        m_stage = RenderStage::DataPanelClear;
        m_bandY = 190;
    } else if ((m_planFlags & kDirtyFooter) != 0U) {
        m_stage = RenderStage::FooterClear;
        m_bandY = 220;
    } else {
        m_stage = RenderStage::Done;
    }
}

void AppGui::advanceAfterGraph()
{
    if ((m_planFlags & kDirtyDataPanel) != 0U || m_planFull) {
        m_stage = RenderStage::DataPanelClear;
        m_bandY = 190;
    } else if ((m_planFlags & kDirtyFooter) != 0U || m_planFull) {
        m_stage = RenderStage::FooterClear;
        m_bandY = 220;
    } else {
        m_stage = RenderStage::Done;
    }
}

void AppGui::advanceAfterDataPanel()
{
    if ((m_planFlags & kDirtyFooter) != 0U || m_planFull) {
        m_stage = RenderStage::FooterClear;
        m_bandY = 220;
    } else {
        m_stage = RenderStage::Done;
    }
}

void AppGui::advanceAfterFooter()
{
    m_stage = RenderStage::Done;
}

void AppGui::drawHeaderStep()
{
    char buf[32];
    switch (m_textStep++) {
        case 0:
            m_lcd.drawString(20, 15, "MicroCP Sensor Monitor", kColorYellow, kColorBlack, 16);
            break;
        case 1:
            m_lcd.drawString(20, 35, "--------------------------------", kColorGray, kColorBlack, 16);
            break;
        case 2:
            sprintf(buf, "GROUP: %lu", (unsigned long)SYS_GROUP_NUMBER);
            m_lcd.drawString(250, 15, buf, kColorGreen, kColorBlack, 16);
            break;
        case 3:
            sprintf(buf, "PAGE: %d/3", m_planModel.currentViewPage + 1);
            m_lcd.drawString(390, 15, buf, kColorYellow, kColorBlack, 16);
            break;
        default:
            advanceAfterHeader();
            break;
    }
}

void AppGui::drawGraphWaitingOrLine()
{
    const bool pressure = m_planPage == 1;
    if (pressure) {
        if (!m_planModel.pressureConnected || m_planModel.historyCount < 2) {
            m_lcd.drawString(140, 110, "PRESSURE GRAPH WAITING", kColorGray, kColorBlack, 16);
            advanceAfterGraph();
            return;
        }
    } else if (!m_planModel.tempHumConnected || m_planModel.historyCount < 2) {
        m_lcd.drawString(136, 110, "TEMP/HUMI GRAPH WAITING", kColorGray, kColorBlack, 16);
        advanceAfterGraph();
        return;
    }

    if (!m_planFull && canAppendGraphSegment(m_drawnModel, m_planModel, pressure) &&
        !needsGraphFull(m_drawnModel, m_planModel, pressure)) {
        drawGraphAppend();
        advanceAfterGraph();
        return;
    }

    if (m_graphIndex >= m_planModel.historyCount) {
        advanceAfterGraph();
        return;
    }

    uint16_t x0 = graphXForIndex(m_graphIndex - 1U, kGraphX0 + 1U, kGraphX1 - 1U);
    uint16_t x1 = graphXForIndex(m_graphIndex, kGraphX0 + 1U, kGraphX1 - 1U);
    if (pressure) {
        uint32_t minValue = 0;
        uint32_t maxValue = 0;
        pressRange(m_planModel, minValue, maxValue);
        uint16_t y0 = graphYForValue(static_cast<int32_t>(m_planModel.pressHistory[m_graphIndex - 1U]),
                                     static_cast<int32_t>(minValue), static_cast<int32_t>(maxValue),
                                     kGraphY0 + 1U, kGraphY1 - 1U);
        uint16_t y1 = graphYForValue(static_cast<int32_t>(m_planModel.pressHistory[m_graphIndex]),
                                     static_cast<int32_t>(minValue), static_cast<int32_t>(maxValue),
                                     kGraphY0 + 1U, kGraphY1 - 1U);
        m_gui.drawLine(x0, y0, x1, y1, kColorBlue);
    } else {
        int32_t minValue = 0;
        int32_t maxValue = 0;
        tempRange(m_planModel, minValue, maxValue);
        uint16_t y0 = graphYForValue(m_planModel.tempHistory[m_graphIndex - 1U], minValue, maxValue,
                                     kGraphY0 + 1U, kGraphY1 - 1U);
        uint16_t y1 = graphYForValue(m_planModel.tempHistory[m_graphIndex], minValue, maxValue,
                                     kGraphY0 + 1U, kGraphY1 - 1U);
        m_gui.drawLine(x0, y0, x1, y1, kColorRed);
    }
    ++m_graphIndex;
}

void AppGui::drawGraphAppend()
{
    const bool pressure = m_planPage == 1;
    const uint16_t index = static_cast<uint16_t>(m_planModel.historyCount - 1U);
    uint16_t x0 = graphXForIndex(index - 1U, kGraphX0 + 1U, kGraphX1 - 1U);
    uint16_t x1 = graphXForIndex(index, kGraphX0 + 1U, kGraphX1 - 1U);
    if (pressure) {
        uint32_t minValue = 0;
        uint32_t maxValue = 0;
        pressRange(m_planModel, minValue, maxValue);
        uint16_t y0 = graphYForValue(static_cast<int32_t>(m_planModel.pressHistory[index - 1U]),
                                     static_cast<int32_t>(minValue), static_cast<int32_t>(maxValue),
                                     kGraphY0 + 1U, kGraphY1 - 1U);
        uint16_t y1 = graphYForValue(static_cast<int32_t>(m_planModel.pressHistory[index]),
                                     static_cast<int32_t>(minValue), static_cast<int32_t>(maxValue),
                                     kGraphY0 + 1U, kGraphY1 - 1U);
        m_gui.drawLine(x0, y0, x1, y1, kColorBlue);
        return;
    }

    int32_t minValue = 0;
    int32_t maxValue = 0;
    tempRange(m_planModel, minValue, maxValue);
    uint16_t y0 = graphYForValue(m_planModel.tempHistory[index - 1U], minValue, maxValue,
                                 kGraphY0 + 1U, kGraphY1 - 1U);
    uint16_t y1 = graphYForValue(m_planModel.tempHistory[index], minValue, maxValue,
                                 kGraphY0 + 1U, kGraphY1 - 1U);
    m_gui.drawLine(x0, y0, x1, y1, kColorRed);
}

void AppGui::drawDataPanelTextStep()
{
    char buf[64];
    if (m_planPage == 1) {
        if (!m_planModel.pressureConnected) {
            m_lcd.drawString(140, 190, "PRESSURE: Unconnected", kColorRed, kColorBlack, 16);
            advanceAfterDataPanel();
            return;
        }
        switch (m_textStep++) {
            case 0:
                formatPressureKpa(buf, "PRES: ", m_planModel.pressure, " kPa");
                m_lcd.drawString(20, 190, buf, kColorWhite, kColorBlack, 16);
                break;
            case 1:
                formatScaledInt(buf, "ALTI: ", m_planModel.altitude, " m", 10);
                m_lcd.drawString(20, 210, buf, kColorWhite, kColorBlack, 16);
                break;
            case 2: {
                char lowBuf[32];
                char highBuf[32];
                formatPressureKpa(lowBuf, "PRES LIMIT: ", m_planModel.pressLowLimit, " ~ ");
                formatPressureKpa(highBuf, "", m_planModel.pressHighLimit, " kPa");
                sprintf(buf, "%s%s", lowBuf, highBuf);
                m_lcd.drawString(20, 232, buf, kColorGray, kColorBlack, 16);
                break;
            }
            default:
                advanceAfterDataPanel();
                break;
        }
        return;
    }

    if (!m_planModel.tempHumConnected) {
        m_lcd.drawString(140, 190, "TEMP/HUMI: Unconnected", kColorRed, kColorBlack, 16);
        advanceAfterDataPanel();
        return;
    }
    switch (m_textStep++) {
        case 0:
            formatScaledInt(buf, "TEMP: ", m_planModel.temperature, " C", 10);
            m_lcd.drawString(20, 190, buf, kColorWhite, kColorBlack, 16);
            break;
        case 1:
            formatScaledInt(buf, "HUMI: ", m_planModel.humidity, " %", 10);
            m_lcd.drawString(20, 210, buf, kColorWhite, kColorBlack, 16);
            break;
        case 2: {
            char lowBuf[20];
            char highBuf[20];
            formatScaledInt(lowBuf, "", m_planModel.tempLowLimit, "", 10);
            formatScaledInt(highBuf, "", m_planModel.tempHighLimit, "", 10);
            sprintf(buf, "TEMP LIMIT: %s ~ %s C", lowBuf, highBuf);
            m_lcd.drawString(20, 232, buf, kColorGray, kColorBlack, 16);
            break;
        }
        default:
            advanceAfterDataPanel();
            break;
    }
}

void AppGui::drawFooterTextStep()
{
    char buf[48];
    const char* stateStr = "UNKNOWN";
    uint16_t stateColor = kColorWhite;
    switch (m_planModel.alarmState) {
        case Sys::AlarmState::NORMAL:
            stateStr = "NORMAL";
            stateColor = kColorGreen;
            break;
        case Sys::AlarmState::WARNING_TEMP:
            stateStr = "WARNING TEMP";
            stateColor = kColorRed;
            break;
        case Sys::AlarmState::WARNING_PRES:
            stateStr = "WARNING PRES";
            stateColor = kColorRed;
            break;
        case Sys::AlarmState::MUTED:
            stateStr = "MUTED ALARM";
            stateColor = kColorYellow;
            break;
        default:
            break;
    }

    switch (m_textStep++) {
        case 0:
            m_lcd.drawString(20, 220, "--------------------------------", kColorGray, kColorBlack, 16);
            break;
        case 1:
            sprintf(buf, "SYS STATUS: %-15s", stateStr);
            m_lcd.drawString(20, 245, buf, stateColor, kColorBlack, 16);
            break;
        case 2:
            sprintf(buf, "MUTE: %s", m_planModel.isMuted ? "ON " : "OFF");
            m_lcd.drawString(300, 245, buf, m_planModel.isMuted ? kColorYellow : kColorGray, kColorBlack, 16);
            break;
        case 3:
            m_gui.drawRectBorder(kSettingsButtonX0, 276, kSettingsButtonX1, 312, kColorWhite);
            break;
        case 4:
            m_lcd.drawString(30, 284, "SETTINGS", kColorWhite, kColorBlack, 16);
            break;
        default:
            advanceAfterFooter();
            break;
    }
}

void AppGui::drawSettingsRowStep()
{
    char buf[64];
    const char* labels[4] = {"TEMP HIGH", "TEMP LOW", "PRESS HIGH", "PRESS LOW"};
    if (m_settingsRow >= kThresholdRowCount) {
        m_stage = RenderStage::SettingsControls;
        m_textStep = 0;
        return;
    }

    const uint16_t rowY0 = static_cast<uint16_t>(60U + m_settingsRow * kThresholdRowHeight);
    const uint16_t rowY1 = static_cast<uint16_t>(rowY0 + 32U);
    switch (m_textStep++) {
        case 0:
            m_gui.drawRectBorder(20, rowY0, 460, rowY1, kColorGray);
            break;
        case 1:
            if (m_planModel.selectedThresholdField == m_settingsRow) {
                m_gui.drawRectBorder(18, rowY0 - 2U, 462, rowY1 + 2U, kColorYellow);
            }
            break;
        case 2:
            m_lcd.drawString(24, rowY0 + 8U, labels[m_settingsRow], kColorWhite, kColorBlack, 16);
            break;
        case 3:
            if (m_settingsRow == 0) {
                formatScaledInt(buf, "", m_planModel.pendingTempHighLimit, " C", 10);
            } else if (m_settingsRow == 1) {
                formatScaledInt(buf, "", m_planModel.pendingTempLowLimit, " C", 10);
            } else if (m_settingsRow == 2) {
                formatPressureKpa(buf, "", m_planModel.pendingPressHighLimit, " kPa");
            } else {
                formatPressureKpa(buf, "", m_planModel.pendingPressLowLimit, " kPa");
            }
            m_lcd.drawString(220, rowY0 + 8U, buf, kColorWhite, kColorBlack, 16);
            break;
        case 4:
            m_lcd.fillRect(360, rowY0, 388, rowY1, kColorBlue);
            m_gui.drawRectBorder(360, rowY0, 388, rowY1, kColorWhite);
            m_lcd.drawString(370, rowY0 + 8U, "-", kColorWhite, kColorBlue, 16);
            break;
        case 5:
            m_lcd.fillRect(400, rowY0, 428, rowY1, kColorBlue);
            m_gui.drawRectBorder(400, rowY0, 428, rowY1, kColorWhite);
            m_lcd.drawString(410, rowY0 + 8U, "+", kColorWhite, kColorBlue, 16);
            break;
        default:
            ++m_settingsRow;
            m_textStep = 0;
            break;
    }
}

void AppGui::drawSettingsControlsStep()
{
    switch (m_textStep++) {
        case 0:
            m_lcd.fillRect(40, 250, 190, 288, kColorBlack);
            m_gui.drawRectBorder(40, 250, 190, 288, kColorGreen);
            m_lcd.drawString(50, 258, "CONFIRM", kColorGreen, kColorBlack, 16);
            break;
        case 1:
            m_lcd.fillRect(300, 250, 450, 288, kColorRed);
            m_gui.drawRectBorder(300, 250, 450, 288, kColorWhite);
            m_lcd.drawString(310, 258, "CANCEL", kColorWhite, kColorRed, 16);
            break;
        default:
            m_stage = RenderStage::Done;
            break;
    }
}

bool AppGui::needsGraphFull(const Model& previous, const Model& next, bool pressure) const
{
    if (pressure) {
        return previous.pressureConnected != next.pressureConnected ||
            previous.pressHighLimit != next.pressHighLimit ||
            previous.pressLowLimit != next.pressLowLimit ||
            pressGraphRangeChanged(previous, next) ||
            (historyChanged(previous, next) && !canAppendGraphSegment(previous, next, true));
    }
    return previous.tempHumConnected != next.tempHumConnected ||
        previous.tempHighLimit != next.tempHighLimit ||
        previous.tempLowLimit != next.tempLowLimit ||
        tempGraphRangeChanged(previous, next) ||
        (historyChanged(previous, next) && !canAppendGraphSegment(previous, next, false));
}

bool AppGui::canAppendGraphSegment(const Model& previous, const Model& next, bool pressure) const
{
    if (next.historyCount < 2 || next.historyCount != previous.historyCount + 1U) {
        return false;
    }
    for (uint16_t i = 0; i < previous.historyCount; ++i) {
        if (previous.tempHistory[i] != next.tempHistory[i] ||
            previous.pressHistory[i] != next.pressHistory[i]) {
            return false;
        }
    }
    return pressure ? (next.pressureConnected && !pressGraphRangeChanged(previous, next))
                    : (next.tempHumConnected && !tempGraphRangeChanged(previous, next));
}

bool AppGui::historyChanged(const Model& a, const Model& b) const
{
    if (a.historyCount != b.historyCount) {
        return true;
    }
    for (uint16_t i = 0; i < a.historyCount; ++i) {
        if (a.tempHistory[i] != b.tempHistory[i] || a.pressHistory[i] != b.pressHistory[i]) {
            return true;
        }
    }
    return false;
}

bool AppGui::tempDataPanelChanged(const Model& a, const Model& b) const
{
    return a.temperature != b.temperature || a.humidity != b.humidity ||
        a.tempHighLimit != b.tempHighLimit || a.tempLowLimit != b.tempLowLimit ||
        a.tempHumConnected != b.tempHumConnected;
}

bool AppGui::pressDataPanelChanged(const Model& a, const Model& b) const
{
    return a.pressure != b.pressure || a.altitude != b.altitude ||
        a.pressHighLimit != b.pressHighLimit || a.pressLowLimit != b.pressLowLimit ||
        a.pressureConnected != b.pressureConnected;
}

bool AppGui::footerChanged(const Model& a, const Model& b) const
{
    return a.alarmState != b.alarmState || a.isMuted != b.isMuted;
}

bool AppGui::settingsChanged(const Model& a, const Model& b) const
{
    return a.selectedThresholdField != b.selectedThresholdField ||
        a.pendingTempHighLimit != b.pendingTempHighLimit ||
        a.pendingTempLowLimit != b.pendingTempLowLimit ||
        a.pendingPressHighLimit != b.pendingPressHighLimit ||
        a.pendingPressLowLimit != b.pendingPressLowLimit;
}

bool AppGui::tempGraphRangeChanged(const Model& previous, const Model& next) const
{
    if (previous.historyCount < 2 || next.historyCount < 2) {
        return previous.historyCount != next.historyCount;
    }
    int32_t prevMin = 0;
    int32_t prevMax = 0;
    int32_t nextMin = 0;
    int32_t nextMax = 0;
    tempRange(previous, prevMin, prevMax);
    tempRange(next, nextMin, nextMax);
    return prevMin != nextMin || prevMax != nextMax;
}

bool AppGui::pressGraphRangeChanged(const Model& previous, const Model& next) const
{
    if (previous.historyCount < 2 || next.historyCount < 2) {
        return previous.historyCount != next.historyCount;
    }
    uint32_t prevMin = 0;
    uint32_t prevMax = 0;
    uint32_t nextMin = 0;
    uint32_t nextMax = 0;
    pressRange(previous, prevMin, prevMax);
    pressRange(next, nextMin, nextMax);
    return prevMin != nextMin || prevMax != nextMax;
}

void AppGui::tempRange(const Model& model, int32_t& minValue, int32_t& maxValue) const
{
    minValue = model.tempHistory[0];
    maxValue = model.tempHistory[0];
    for (uint16_t i = 1; i < model.historyCount; ++i) {
        minValue = (model.tempHistory[i] < minValue) ? model.tempHistory[i] : minValue;
        maxValue = (model.tempHistory[i] > maxValue) ? model.tempHistory[i] : maxValue;
    }
    minValue = (model.tempLowLimit < minValue) ? model.tempLowLimit : minValue;
    maxValue = (model.tempHighLimit > maxValue) ? model.tempHighLimit : maxValue;
    if (maxValue - minValue < 20) {
        maxValue = minValue + 20;
    }
}

void AppGui::pressRange(const Model& model, uint32_t& minValue, uint32_t& maxValue) const
{
    minValue = model.pressHistory[0];
    maxValue = model.pressHistory[0];
    for (uint16_t i = 1; i < model.historyCount; ++i) {
        minValue = (model.pressHistory[i] < minValue) ? model.pressHistory[i] : minValue;
        maxValue = (model.pressHistory[i] > maxValue) ? model.pressHistory[i] : maxValue;
    }
    minValue = (model.pressLowLimit < minValue) ? model.pressLowLimit : minValue;
    maxValue = (model.pressHighLimit > maxValue) ? model.pressHighLimit : maxValue;
    if (maxValue <= minValue) {
        maxValue = minValue + 1000U;
    }
}

uint16_t AppGui::graphXForIndex(uint16_t index, uint16_t x0, uint16_t x1) const
{
    return static_cast<uint16_t>(x0 + (static_cast<uint32_t>(x1 - x0) * index) / (kHistorySize - 1U));
}

uint16_t AppGui::graphYForValue(int32_t value, int32_t minValue, int32_t maxValue,
                                uint16_t y0, uint16_t y1) const
{
    int32_t range = maxValue - minValue;
    if (range <= 0) {
        range = 1;
    }
    const uint16_t height = static_cast<uint16_t>(y1 - y0);
    int64_t scaled = static_cast<int64_t>(value - minValue) * height / range;
    if (scaled < 0) {
        scaled = 0;
    } else if (scaled > height) {
        scaled = height;
    }
    return static_cast<uint16_t>(y1 - scaled);
}

} // namespace App
