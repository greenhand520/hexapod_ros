//
// Created by greenhand520 on 2026/4/28.
//
// WS2812B LED 驱动 + 板载状态指示灯
// 使用 TIM4 CH2 + DMA (DMA1_Stream3) 驱动 6 颗 WS2812B
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>

/** 板载 6 颗 WS2812B */
#define WS2812_LED_COUNT 6
/** GRB: 8+8+8 */
#define WS2812_BITS_PER_LED 24
/** 复位脉冲 (>50µs) */
#define WS2812_RESET_LEN 60
#define WS2812_BUF_LEN                                                         \
  (WS2812_LED_COUNT * WS2812_BITS_PER_LED + WS2812_RESET_LEN)

/** 颜色结构体 (GRB 顺序) */
typedef struct {
  uint8_t g;
  uint8_t r;
  uint8_t b;
} WS2812_Color_t;

#define WS2812_COLOR_OFF ((WS2812_Color_t){0, 0, 0})
#define WS2812_COLOR_GREEN ((WS2812_Color_t){255, 0, 0})
#define WS2812_COLOR_RED ((WS2812_Color_t){0, 255, 0})
/** #FF4757 警告红 */
#define WS2812_COLOR_WARN_RED ((WS2812_Color_t){71, 255, 87})
#define WS2812_COLOR_ORANGE ((WS2812_Color_t){100, 255, 0})
#define WS2812_COLOR_LIME ((WS2812_Color_t){200, 50, 0})
#define WS2812_COLOR_BLUE ((WS2812_Color_t){0, 0, 255})

/**
 * @brief 初始化 WS2812B 驱动（DMA + TIM4 CH2）
 */
void WS2812_Init(void);

/**
 * @brief 设置单颗 LED 颜色
 */
void WS2812_SetColor(uint8_t index, WS2812_Color_t color);

/**
 * @brief 设置全部 LED 为同一颜色
 */
void WS2812_SetAll(WS2812_Color_t color);

/**
 * @brief 通过 DMA 发送数据到 WS2812B
 */
void WS2812_Send(void);

/**
 * @brief WS2812 状态指示灯任务入口
 *        同时负责风扇控制、过温保护（关闭充电/舵机供电）
 */
void WS2812_IndicatorTask(const void *argument);

#ifdef __cplusplus
}
#endif