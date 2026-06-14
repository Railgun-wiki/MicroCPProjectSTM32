// BSP/Inc/LcdBsp.hpp
#pragma once
#include "ILcdDisplay.hpp"
#include "stm32f1xx_hal.h"

namespace Bsp {

/**
 * @brief 基于 ST7796S 驱动芯片的 4寸 SPI TFT LCD BSP 驱动实现类
 * 
 * 实现了 App::ILcdDisplay 接口，内部只保留底层 SPI 传输和基础绘制 primitive。
 * 物理引脚采用参数化配置，免除由于 CubeMX 生成代码带来的强耦合及硬件冲突问题。
 */
class LcdBsp : public App::ILcdDisplay {
public:
    /**
     * @brief 构造 LCD BSP 驱动对象
     * @param hspi SPI 外设 HAL 句柄指针 (如 &hspi1)
     * @param csPort CS 片选引脚所在的 GPIO 端口 (如 GPIOB)
     * @param csPin CS 片选引脚编号 (参考工程为 GPIO_PIN_9)
     * @param rsPort RS 数据/命令选择引脚所在的 GPIO 端口 (如 GPIOB)
     * @param rsPin RS 引脚编号 (参考工程为 GPIO_PIN_7)
     * @param rstPort RST 复位引脚所在的 GPIO 端口 (如 GPIOB)
     * @param rstPin RST 引脚编号 (参考工程为 GPIO_PIN_8)
     * @param ledPort LED 背光引脚所在的 GPIO 端口 (如 GPIOB)
     * @param ledPin LED 引脚编号 (参考工程为 GPIO_PIN_6)
     */
    LcdBsp(SPI_HandleTypeDef* hspi,
           GPIO_TypeDef* csPort, uint16_t csPin,
           GPIO_TypeDef* rsPort, uint16_t rsPin,
           GPIO_TypeDef* rstPort, uint16_t rstPin,
           GPIO_TypeDef* ledPort, uint16_t ledPin);

    void init() override;
    void clear(uint16_t color) override;
    void drawPixel(uint16_t x, uint16_t y, uint16_t color) override;
    void fillRect(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1, uint16_t color) override;
    uint16_t getWidth() const override { return m_width; }
    uint16_t getHeight() const override { return m_height; }
    void drawString(uint16_t x, uint16_t y, const char* str, uint16_t fc, uint16_t bc, uint8_t size) override;

    // 文本渲染（需要 SPI 寄存器级批量写入优化，保留在硬件层）
    void drawChar(uint16_t x, uint16_t y, char c, uint16_t fc, uint16_t bc, uint8_t size);
    void drawScaledInt(uint16_t x, uint16_t y, int32_t value, uint8_t intDigits,
                       uint8_t fracDigits, uint16_t fc, uint16_t bc, uint8_t size);

private:
    SPI_HandleTypeDef* m_hspi;
    GPIO_TypeDef* m_csPort;
    uint16_t m_csPin;
    GPIO_TypeDef* m_rsPort;
    uint16_t m_rsPin;
    GPIO_TypeDef* m_rstPort;
    uint16_t m_rstPin;
    GPIO_TypeDef* m_ledPort;
    uint16_t m_ledPin;

    // 管理 LCD 的宽度和高度参数 (ST7796S 横屏: 480x320)
    uint16_t m_width{480};
    uint16_t m_height{320};

    // 底层物理信号操作
    void csLow();
    void csHigh();
    inline void rsLow() { HAL_GPIO_WritePin(m_rsPort, m_rsPin, GPIO_PIN_RESET); }
    inline void rsHigh() { HAL_GPIO_WritePin(m_rsPort, m_rsPin, GPIO_PIN_SET); }
    inline void rstLow() { HAL_GPIO_WritePin(m_rstPort, m_rstPin, GPIO_PIN_RESET); }
    inline void rstHigh() { HAL_GPIO_WritePin(m_rstPort, m_rstPin, GPIO_PIN_SET); }
    inline void ledOn() { HAL_GPIO_WritePin(m_ledPort, m_ledPin, GPIO_PIN_SET); }
    inline void ledOff() { HAL_GPIO_WritePin(m_ledPort, m_ledPin, GPIO_PIN_RESET); }

    // 底层命令和数据发送
    void writeCmd(uint8_t cmd);
    void writeData(uint8_t data);
    void writeData16(uint16_t data);
    void setAddressWindow(uint16_t x0, uint16_t y0, uint16_t x1, uint16_t y1);
    void reset();

};

} // namespace Bsp
