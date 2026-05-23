// App/Inc/app_entry.h
#ifndef __APP_ENTRY_H
#define __APP_ENTRY_H

#ifdef __cplusplus
extern "C" {
#endif

// 系统启动时由 main.c 调用一次，执行对象实例化与驱动自检
void App_Init(void);

// 在 main.c 的 while(1) 中调用，以 10Hz 的频率轮询执行业务逻辑
void App_Loop(void);

// 在 TIM4 中断服务程序（10ms一次）中被调用，用于指示灯动画和按键物理消抖的实时推进
void App_Timer_10ms_ISR(void);

#ifdef __cplusplus
}
#endif

#endif
