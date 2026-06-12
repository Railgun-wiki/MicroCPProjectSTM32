// App/Inc/ILcdDisplay.hpp
#pragma once
#include "sys.hpp"

namespace App {

/**
 * @brief LCD 抽象显示接口类
 * 
 * 按照项目架构规范，应用逻辑层（App）仅依赖于该抽象接口进行屏幕刷新，
 * 彻底隔离底层的硬件 SPI 接口和屏幕引脚细节。
 */
class ILcdDisplay {
public:
    virtual ~ILcdDisplay() = default;

    /**
     * @brief 初始化 LCD 硬件配置并亮屏
     */
    virtual void init() = 0;

    /**
     * @brief 全屏使用指定颜色填充
     * @param color 16 位 RGB565 颜色值
     */
    virtual void clear(uint16_t color) = 0;

    /**
     * @brief 绘制单个像素
     */
    virtual void drawPixel(uint16_t x, uint16_t y, uint16_t color) = 0;

    /**
     * @brief 填充矩形区域
     */
    virtual void fillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) = 0;

    /**
     * @brief 获取 LCD 宽度
     */
    virtual uint16_t getWidth() const = 0;

    /**
     * @brief 获取 LCD 高度
     */
    virtual uint16_t getHeight() const = 0;

    /**
     * @brief LCD 渲染的解耦遥测数据包
     */
    struct RenderData {
        int32_t temperature;      ///< 实时环境温度 (放大10倍)
        int32_t humidity;         ///< 实时环境湿度 (放大10倍)
        uint32_t pressure;        ///< 实时大气压强 (Pa)
        int32_t altitude;         ///< 测算海拔高度 (放大10倍)

        int32_t tempHighLimit;    ///< 温度报警上限值 (放大10倍)
        int32_t tempLowLimit;     ///< 温度报警下限值 (放大10倍)
        uint32_t pressHighLimit;  ///< 气压报警上限值 (Pa)
        uint32_t pressLowLimit;   ///< 气压报警下限值 (Pa)

        Sys::AlarmState alarmState; ///< 系统当前的报警状态
        uint8_t currentViewPage;  ///< 当前活动的分页页码 (0 或 1)
        bool isMuted;             ///< 告警展示是否已被确认/抑制

        bool tempHumConnected;    ///< 温湿度传感器是否已连接
        bool pressureConnected;   ///< 气压传感器是否已连接
    };

    /**
     * @brief 定期更新和刷新 LCD 显示内容
     * @param data 最新获取的遥测和系统状态数据
     */
    virtual void update(const RenderData& data) = 0;
};

} // namespace App
