//
// Created by greenhand520 on 2026/5/13.
//
// USB CDC 日志模块
// 使用 FreeRTOS 消息队列 + 专用任务实现异步发送
// 通过 LOG_ENABLED 宏区分调试/生产模式
//

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

/* ================================================================
 *  调试/生产模式切换
 *    定义 LOG_ENABLED = 1  → 调试模式，printf 输出到 USB CDC
 *    定义 LOG_ENABLED = 0  → 生产模式，printf 静默（不输出）
 * ================================================================ */

#ifndef LOG_ENABLED
#define LOG_ENABLED 0
#endif

/* ================================================================
 *  ANSI 颜色定义（适用于支持 VT100 的终端）
 *    PuTTY / minicom / picocom / screen / VS Code 终端 均支持
 *    如果终端不支持，显示的是原始转义字符，不影响功能
 * ================================================================ */

#define CLR_RESET "\033[0m"
#define CLR_BOLD "\033[1m"

#define CLR_RED "\033[1;31m"    /* 粗体红 — 错误 */
#define CLR_YELLOW "\033[1;33m" /* 粗体黄 — 警告 */
#define CLR_GREEN "\033[1;32m"  /* 粗体绿 — 成功 */
#define CLR_CYAN "\033[1;36m"   /* 粗体青 — 调试 */
#define CLR_WHITE "\033[1;37m"  /* 粗体白 — 普通信息 */
#define CLR_GRAY "\033[0;37m"   /* 暗灰   — 详细跟踪 */

/* ================================================================
 *  日志队列参数
 * ================================================================ */

#define LOG_QUEUE_LENGTH 16     /* 队列深度（消息条数） */
#define LOG_MSG_MAX_LEN 256     /* 单条消息最大长度（字节） */
#define LOG_TX_TASK_STACK (512) /* 发送任务栈大小（字） */
#define LOG_TX_TASK_PRIO 2      /* 发送任务优先级（低优先级） */

/**
 * @brief 初始化日志模块（创建队列 + 启动发送任务）
 *        应在 osKernelInitialize() 之后、osKernelStart() 之前调用
 */
void LOG_Init(void);

/**
 * @brief 将字符串放入日志队列（非阻塞）
 *        如果队列满则丢弃
 * @param str  以 '\0' 结尾的字符串
 */
void LOG_Send(const char *str);

/**
 * @brief 格式化输出到日志队列（类似 printf，无颜色无标签）
 *        最多 LOG_MSG_MAX_LEN 字节，超出截断
 */
#if LOG_ENABLED
void LOG_Printf(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
#else
static inline void LOG_Printf(const char *fmt, ...) { (void)fmt; }
#endif

void LOG_Write(int file, const char *ptr, int len);

void Task_CreateLogCDCTask(void);

/* ================================================================
 *  分级日志宏（带颜色标签）
 *
 *  用法示例:
 *    LOG_INFO("Sensor ready, ID=0x%02X", id);
 *    LOG_WARN("Battery low: %d%%", level);
 *    LOG_ERR("Init failed: %d", ret);
 *    LOG_DBG("raw adc = %lu", adc_val);
 *
 *  输出效果（终端渲染颜色）:
 *    [INFO] Sensor ready, ID=0x3A
 *    [WARN] Battery low: 12%
 *    [ERR ] Init failed: -1
 *    [DBG ] raw adc = 2048
 * ================================================================ */

#if LOG_ENABLED

/* 内部宏：拼接颜色 + 标签 + 格式串 */
#define LOG_AT_LEVEL(color, tag, fmt, ...)                                     \
  LOG_Printf(color "[" tag "] " fmt CLR_RESET, ##__VA_ARGS__)

#define LOG_INFO(fmt, ...) LOG_AT_LEVEL(CLR_WHITE, "INFO", fmt, ##__VA_ARGS__)
#define LOG_WARN(fmt, ...) LOG_AT_LEVEL(CLR_YELLOW, "WARN", fmt, ##__VA_ARGS__)
#define LOG_ERR(fmt, ...) LOG_AT_LEVEL(CLR_RED, "ERR ", fmt, ##__VA_ARGS__)
#define LOG_DBG(fmt, ...) LOG_AT_LEVEL(CLR_CYAN, "DBG ", fmt, ##__VA_ARGS__)
#define LOG_OK(fmt, ...) LOG_AT_LEVEL(CLR_GREEN, " OK ", fmt, ##__VA_ARGS__)
#define LOG_TRACE(fmt, ...) LOG_AT_LEVEL(CLR_GRAY, "TRCE", fmt, ##__VA_ARGS__)

#else

#define LOG_INFO(fmt, ...) ((void)0)
#define LOG_WARN(fmt, ...) ((void)0)
#define LOG_ERR(fmt, ...) ((void)0)
#define LOG_DBG(fmt, ...) ((void)0)
#define LOG_OK(fmt, ...) ((void)0)
#define LOG_TRACE(fmt, ...) ((void)0)

#endif /* LOG_ENABLED */

#ifdef __cplusplus
}
#endif
