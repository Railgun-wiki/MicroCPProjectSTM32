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
     * @brief LCD 渲染的解耦遥测数据包
     */
    struct RenderData {
        float temperature;        ///< 实时环境温度
        float humidity;           ///< 实时环境湿度
        float pressure;           ///< 实时大气压强
        float altitude;           ///< 测算海拔高度

        float tempHighLimit;      ///< 温度报警上限值
        float tempLowLimit;       ///< 温度报警下限值
        float pressHighLimit;     ///< 气压报警上限值
        float pressLowLimit;      ///< 气压报警下限值

        Sys::AlarmState alarmState; ///< 系统当前的报警状态
        uint8_t currentViewPage;  ///< 当前活动的分页页码 (0 或 1)
        bool isMuted;             ///< 警报是否已静音

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
