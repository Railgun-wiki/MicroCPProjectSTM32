// App/Inc/ILcdDisplay.hpp
#pragma once
#include <stdint.h>

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
     * @brief 绘制 ASCII 字符串。
     */
    virtual void drawString(uint16_t x, uint16_t y, const char* str, uint16_t fc, uint16_t bc, uint8_t size) = 0;
};

} // namespace App
