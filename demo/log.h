//
// Copyright (c) 2021 BiliBili Inc. All rights reserved.
// Author : liushuai
// Created on 2/19/21.
//

/**
 * Features & Usages:
 *
 * 1. 设置TAG
 *  默认TAG为"LOG"，可通过预定义LOG_TAG修改，对模块设置。
 *  如CMake: add_definitions(-DLOG_TAG="TAG")
 *  也可为源文件单独设置:
 *  #undef  LOG_TAG
 *  #define LOG_TAG "TAG"
 *
 * 2. 通过编译参数(LOG_LEVEL)控制日志输出级别
 *  如CMake: add_definitions(-DLOG_LEVEL=LOG_LEVEL_DEBUG)
 *  默认规则(依据C标准的NDEBUG宏):
 *  Release编译时，只打开LOG_LEVEL_INFO和以上级别。
 *  Debug编译时，打开了所有输出（包括VERBOSE）。
 *
 * 3. 兼容Android和其他平台
 *  安卓使用：__android_log_print
 *  其他平台：printf
 */

#pragma once

#define LOG_LEVEL_VERBOSE   6
#define LOG_LEVEL_DEBUG     5
#define LOG_LEVEL_INFO      4
#define LOG_LEVEL_WARN      3
#define LOG_LEVEL_ERROR     2
#define LOG_LEVEL_FATAL     1
#define LOG_LEVEL_OFF       0

#ifndef LOG_LEVEL
#ifndef NDEBUG
#define LOG_LEVEL           LOG_LEVEL_VERBOSE
#else
#define LOG_LEVEL           LOG_LEVEL_INFO
#endif
#endif

#ifndef LOG_TAG
#define LOG_TAG             "LOG"
#endif

#ifdef  LOG_LINE_END_CRLF
#define LOG_LINE_END        "\r\n"
#else
#define LOG_LINE_END        "\n"
#endif

#if __ANDROID__
#include <android/log.h>
#define LOG_PRINTF_IMPL(level, tag, fmt, ...)   __android_log_print(level, tag, fmt, ##__VA_ARGS__)
#define LOG_PRIORITY_VERBOSE ANDROID_LOG_VERBOSE
#define LOG_PRIORITY_DEBUG   ANDROID_LOG_DEBUG
#define LOG_PRIORITY_INFO    ANDROID_LOG_INFO
#define LOG_PRIORITY_WARN    ANDROID_LOG_WARN
#define LOG_PRIORITY_ERROR   ANDROID_LOG_ERROR
#define LOG_PRIORITY_FATAL   ANDROID_LOG_FATAL
#else
#include <stdio.h>
#define LOG_PRINTF_IMPL(level, tag, fmt, ...)   printf(level "/" tag ":" fmt LOG_LINE_END, ##__VA_ARGS__)
#define LOG_PRIORITY_VERBOSE "V"
#define LOG_PRIORITY_DEBUG   "D"
#define LOG_PRIORITY_INFO    "I"
#define LOG_PRIORITY_WARN    "W"
#define LOG_PRIORITY_ERROR   "E"
#define LOG_PRIORITY_FATAL   "A"
#endif

#if LOG_LEVEL >= LOG_LEVEL_VERBOSE
#define LOGV(...)     LOG_PRINTF_IMPL(LOG_PRIORITY_VERBOSE, LOG_TAG, __VA_ARGS__)
#else
#define LOGV(...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_DEBUG
#define LOGD(...)     LOG_PRINTF_IMPL(LOG_PRIORITY_DEBUG, LOG_TAG, __VA_ARGS__)
#else
#define LOGD(...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_INFO
#define LOGI(...)     LOG_PRINTF_IMPL(LOG_PRIORITY_INFO, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_WARN
#define LOGW(...)     LOG_PRINTF_IMPL(LOG_PRIORITY_WARN, LOG_TAG, __VA_ARGS__)
#else
#define LOGW(...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_ERROR
#define LOGE(...)     LOG_PRINTF_IMPL(LOG_PRIORITY_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGE(...)     ((void)0)
#endif

#if LOG_LEVEL >= LOG_LEVEL_FATAL
#define LOGF(...)     LOG_PRINTF_IMPL(LOG_PRIORITY_FATAL, LOG_TAG, __VA_ARGS__)
#else
#define LOGF(...)     ((void)0)
#endif
