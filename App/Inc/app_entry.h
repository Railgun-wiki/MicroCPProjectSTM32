// App/Inc/app_entry.h
#ifndef __APP_ENTRY_H
#define __APP_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

// 系统启动时由 main.c 调用一次，执行对象实例化与驱动自检
void App_Init(void);

// 在 main.c 的 while(1) 中持续调用，消费由 10ms 调度 tick 设置的任务标志
void App_Loop(void);

// 由 SysTick 分频得到的 10ms 应用调度入口，只做轻量级标志设置
void App_Timer_10ms_ISR(void);

// 由触摸 PENIRQ 的 EXTI 中断回调调用，只设置触摸待处理标志
void App_TouchPen_ISR(void);

#ifdef __cplusplus
}
#endif

#endif
