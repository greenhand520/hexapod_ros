//
// Created by greenhand520 on 2026/4/28.
//
// 蜂鸣器驱动 (BUZZ 引脚, PA10, TIM1 CH3)
// 使用 TIM1 CH3 输出 PWM 方波驱动无源蜂鸣器
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/**
 * @brief 初始化蜂鸣器（需在 MX_TIM1_Init() 之后调用）
 */
void BUZZ_Init(void);

/**
 * @brief 播放指定频率和持续时间的音调
 * @param freq_hz  频率 (Hz), 0 = 静音
 * @param duration_ms  持续时间 (ms)
 */
void BUZZ_Tone(uint32_t freq_hz, uint32_t duration_ms);

/**
 * @brief 停止蜂鸣器
 */
void BUZZ_Stop(void);

/**
 * @brief 播放 R2-D2 风格开机音效
 *        一系列快速升降调的嘟嘟声，听起来像机器人"苏醒"
 *        阻塞执行，约 600ms 完成
 */
void BUZZ_StartupSound(void);

#ifdef __cplusplus
}
#endif