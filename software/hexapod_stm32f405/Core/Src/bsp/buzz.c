//
// Created by greenhand520 on 2026/4/28.
//
// 蜂鸣器驱动 (BUZZ 引脚, PA10, TIM1 CH3)
// TIM1 配置: Prescaler=31, APB2 Timer Clock=168MHz
//   → Tick = 168MHz / 32 = 5.25MHz
//   → ARR = 5250000 / freq - 1
//

#include "cmsis_os.h"

#include "bsp/buzz.h"
#include "main.h"

extern TIM_HandleTypeDef htim1;

//  168MHz / (Prescaler+1)
#define BUZZ_TIM_TICK_HZ 5250000UL
#define BUZZ_CHANNEL TIM_CHANNEL_3

void BUZZ_Init(void) {
  /* TIM1 已由 CubeMX 初始化，此处仅停止 PWM */
  HAL_TIM_PWM_Stop(&htim1, BUZZ_CHANNEL);
  htim1.Instance->CCR3 = 0;
}

void BUZZ_Tone(const uint32_t freq_hz, const uint32_t duration_ms) {
  if (freq_hz == 0 || duration_ms == 0) {
    return;
  }

  /* 计算 ARR 和 CCR (50% 占空比) */
  uint32_t arr = BUZZ_TIM_TICK_HZ / freq_hz;
  if (arr > 0)
    arr--;
  if (arr > 0xFFFF)
    arr = 0xFFFF;

  htim1.Instance->ARR = (uint16_t)arr;
  htim1.Instance->CCR3 = (uint16_t)(arr / 2);

  /* 启动 PWM */
  HAL_TIM_PWM_Start(&htim1, BUZZ_CHANNEL);

  /* 等待指定时间 */
  osDelay(duration_ms);
}

void BUZZ_Stop(void) {
  HAL_TIM_PWM_Stop(&htim1, BUZZ_CHANNEL);
  htim1.Instance->CCR3 = 0;
}

/**
 * R2-D2 风格"苏醒"音效：
 *   1. 快速上升音 (低→高)
 *   2. 短促高频嘟嘟 × 3
 *   3. 下滑音
 *   4. 欢快上升音 × 2
 *   5. 结束长音
 */
void BUZZ_StartupSound(void) {
  /* ---- 阶段1: 快速上升滑音 ---- */
  for (uint32_t f = 800; f <= 3000; f += 200) {
    BUZZ_Tone(f, 12);
  }

  /* ---- 阶段2: 短促高频嘟嘟 × 3 ---- */
  BUZZ_Tone(3500, 30);
  BUZZ_Tone(0, 15); /* 短静音 */
  BUZZ_Tone(4000, 30);
  BUZZ_Tone(0, 15);
  BUZZ_Tone(3200, 30);
  BUZZ_Tone(0, 20);

  /* ---- 阶段3: 下滑音（模拟叹气） ---- */
  for (uint32_t f = 2500; f >= 600; f -= 150) {
    BUZZ_Tone(f, 10);
  }

  /* ---- 阶段4: 欢快上升音 × 2 ---- */
  BUZZ_Tone(0, 30); /* 间隔 */
  for (uint32_t f = 1000; f <= 2800; f += 300) {
    BUZZ_Tone(f, 15);
  }
  BUZZ_Tone(0, 20);
  for (uint32_t f = 1200; f <= 3500; f += 300) {
    BUZZ_Tone(f, 15);
  }

  /* ---- 阶段5: 结束长音（确认音） ---- */
  BUZZ_Tone(0, 20);
  BUZZ_Tone(2500, 60);
  BUZZ_Tone(3000, 80);

  /* 停止 */
  BUZZ_Stop();
}