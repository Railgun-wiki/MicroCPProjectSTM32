// SYSTEM/sys.hpp
#pragma once

#include <stdint.h>

// 1. 系统时钟与周期配置
#define SYS_CPU_FREQ_HZ         72000000U
#define SYS_MAIN_LOOP_PERIOD_MS 100U

// 2. 双传感器物理 I2C 地址 (7位)
#define SYS_I2C_ADDR_AHT20      0x38U
#define SYS_I2C_ADDR_BMP280     0x76U

// 3. 项目组号显示配置
#define SYS_GROUP_NUMBER        0U

// 4. 调试输出日志开关
#define SYS_DEBUG_ENABLED       1

#if SYS_DEBUG_ENABLED
    #include <stdio.h>
    #define SYS_LOG(format, ...) printf("[SYS LOG] " format "\r\n", ##__VA_ARGS__)
#else
    #define SYS_LOG(format, ...) ((void)0)
#endif

// 5. 全局核心命名空间与通用枚举
namespace Sys {

// 系统底层通信或自检错误代码
enum class Status : uint8_t {
    OK = 0,
    ERROR_INIT,
    ERROR_TIMEOUT,
    ERROR_BUSY,
    ERROR_COM_FAIL,
    ERROR_CRC
};

// 环境报警状态定义
enum class AlarmState : uint8_t {
    NORMAL = 0,
    WARNING_TEMP,
    WARNING_PRES,
    MUTED
};

// 系统全局临界区中断锁
inline void EnterCritical() {
    __asm volatile ("cpsid i" : : : "memory");
}

inline void ExitCritical() {
    __asm volatile ("cpsie i" : : : "memory");
}

} // namespace Sys
