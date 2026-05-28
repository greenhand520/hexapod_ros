//
// Created by greenhand520 on 2026/5/13.
//
// USB CDC 日志模块实现
// 使用 FreeRTOS 消息队列 + 专用任务实现异步发送
//

#include "log.h"

#if LOG_ENABLED

#include "FreeRTOS.h"
#include "cmsis_os2.h"
#include "queue.h"
#include "task.h"
#include "usb_device.h"
#include "usbd_cdc_if.h"

#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static QueueHandle_t s_log_queue = NULL;
static uint8_t s_log_initialized = 0;

void LOG_Init(void) {
  /* 创建消息队列：每个元素是一条日志字符串（固定长度） */
  s_log_queue = xQueueCreate(LOG_QUEUE_LENGTH, LOG_MSG_MAX_LEN);
  configASSERT(s_log_queue != NULL);
  s_log_initialized = 1;
}

void LOG_Send(const char *str) {
  if (!s_log_initialized || s_log_queue == NULL) {
    return;
  }

  /* 复制到固定长度缓冲区（队列要求每条消息等长） */
  char buf[LOG_MSG_MAX_LEN];
  strncpy(buf, str, LOG_MSG_MAX_LEN - 1);
  buf[LOG_MSG_MAX_LEN - 1] = '\0';

  /* 非阻塞入队，队列满则丢弃 */
  xQueueSend(s_log_queue, buf, 0);
}

void LOG_Printf(const char *fmt, ...) {
  if (!s_log_initialized || s_log_queue == NULL) {
    return;
  }

  char buf[LOG_MSG_MAX_LEN];
  va_list args;
  va_start(args, fmt);
  const int len = vsnprintf(buf, LOG_MSG_MAX_LEN, fmt, args);
  va_end(args);

  if (len <= 0) {
    return;
  }

  /* 确保 null 结尾 */
  if (len >= LOG_MSG_MAX_LEN) {
    buf[LOG_MSG_MAX_LEN - 1] = '\0';
  }

  xQueueSend(s_log_queue, buf, 0);
}

/**
 * @brief USB CDC 发送任务入口
 *       专用发送任务：从队列取消息 → USB CDC
 *       发送，使用低优先级，不影响其他关键任务
 * @param argument
 */
void LOG_CDC_TxTask(void *argument) {
  (void)argument;

  char buf[LOG_MSG_MAX_LEN];

  for (;;) {
    /* 阻塞等待队列消息 */
    if (xQueueReceive(s_log_queue, buf, portMAX_DELAY) == pdTRUE) {
      uint16_t len = (uint16_t)strlen(buf);

      /* 等待 USB CDC 就绪并发送 */
      uint32_t tickstart = HAL_GetTick();
      while (CDC_Transmit_FS((uint8_t *)buf, len) == USBD_BUSY) {
        /* 超时 200ms 放弃 */
        if ((HAL_GetTick() - tickstart) > 200) {
          break;
        }
        taskYIELD();
      }
    }
  }
}

void LOG_Write(int file, const char *ptr, const int len) {
#if LOG_ENABLED
  /* 通过日志队列发送到 USB CDC（异步，不阻塞调用者） */
  /* 逐行发送，避免超长输出截断 */
  int sent = 0;
  while (sent < len) {
    const int remaining = len - sent;
    const int chunk =
        (remaining < LOG_MSG_MAX_LEN) ? remaining : (LOG_MSG_MAX_LEN - 1);
    char buf[LOG_MSG_MAX_LEN];
    /* 在换行符处截断 */
    int i;
    for (i = 0; i < chunk; i++) {
      buf[i] = ptr[sent + i];
      if (ptr[sent + i] == '\n') {
        i++;
        break;
      }
    }
    buf[i] = '\0';
    LOG_Send(buf);
    sent += i;
  }
#else
  (void)ptr;
  (void)len;
#endif
}

void Task_CreateLogCDCTask(void) {
  const osThreadAttr_t task_attr = {
    .name = "LOG_TxTask",
    .stack_size = LOG_TX_TASK_STACK * 4,
    .priority = (osPriority_t) osPriorityLow,
  };
  osThreadNew(LOG_CDC_TxTask, NULL, &task_attr);
}

#else /* LOG_ENABLED == 0 */

/* 生产模式：空实现 */
void LOG_Init(void) {}
void LOG_Send(const char *str) { (void)str; }
void LOG_CDC_TxTask(void *argument) { (void)argument; }
void Task_CreateLogCDCTask(void) {}

#endif /* LOG_ENABLED */
