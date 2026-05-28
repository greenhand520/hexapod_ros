//
// Created by greenhand520 on 2026/4/28.
//
// WS2812B LED 驱动 + 板载状态指示灯
// TIM4 CH2 + DMA (DMA1_Stream3) 驱动 6 颗 WS2812B
//
// 职责:
//   - LED 状态指示（温度/电量/错误/ROS连接）
//   - 风扇自动控制 (FAN_EN)
//   - 过温保护（超时关闭充电/舵机供电）
//

#include "cmsis_os.h"

#include <string.h>

#include "bsp/adc.h"
#include "bsp/bq24725.h"
#include "bsp/bq40z50.h"
#include "bsp/ws2812.h"
#include "main.h"

extern TIM_HandleTypeDef htim4;

/* ================================================================
 *  DMA 缓冲区
 *
 *  TIM4: APB1 Timer=84MHz, Prescaler=0, Period=104
 *    → 84MHz / 105 = 800kHz, 每位 1.25µs
 *  WS2812B: T0H≈0.4µs→CCR=34, T1H≈0.8µs→CCR=67
 * ================================================================ */
#define WS2812_T0H 34
#define WS2812_T1H 67

static uint16_t s_dma_buf[WS2812_BUF_LEN];
static WS2812_Color_t s_led_buf[WS2812_LED_COUNT];
static volatile uint8_t s_tx_done = 1;

void WS2812_Init(void) {
  memset(s_dma_buf, 0, sizeof(s_dma_buf));
  memset(s_led_buf, 0, sizeof(s_led_buf));
  s_tx_done = 1;
}

void WS2812_SetColor(const uint8_t index, const WS2812_Color_t color) {
  if (index < WS2812_LED_COUNT) {
    s_led_buf[index] = color;
  }
}

void WS2812_SetAll(const WS2812_Color_t color) {
  for (uint8_t i = 0; i < WS2812_LED_COUNT; i++) {
    s_led_buf[i] = color;
  }
}

void WS2812_Send(void) {
  while (!s_tx_done) {
    osDelay(1);
  }
  s_tx_done = 0;

  uint16_t pos = 0;
  for (uint8_t led = 0; led < WS2812_LED_COUNT; led++) {
    const uint8_t grb[3] = {s_led_buf[led].g, s_led_buf[led].r,
                            s_led_buf[led].b};
    for (uint8_t byte = 0; byte < 3; byte++) {
      for (int8_t bit = 7; bit >= 0; bit--) {
        s_dma_buf[pos++] = (grb[byte] & (1 << bit)) ? WS2812_T1H : WS2812_T0H;
      }
    }
  }
  for (uint16_t i = pos; i >= WS2812_BUF_LEN; i++) {
    s_dma_buf[i] = 0;
  }

  HAL_TIM_PWM_Start_DMA(&htim4, TIM_CHANNEL_2, (uint32_t *)s_dma_buf,
                        WS2812_BUF_LEN);
}

/** DMA 完成回调 */
void HAL_TIM_PWM_PulseFinishedCallback(TIM_HandleTypeDef *htim) {
  if (htim->Instance == TIM4) {
    HAL_TIM_PWM_Stop_DMA(&htim4, TIM_CHANNEL_2);
    s_tx_done = 1;
  }
}
