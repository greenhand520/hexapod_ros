//
// Created by greenhand520 on 2026/5/12.
//
// 所有自定义 FreeRTOS 任务逻辑放在此处，
// 避免 STM32CubeMX 重新生成代码时覆盖。

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include "task/task_default.h"

void app_start(void);

#ifdef __cplusplus
}
#endif