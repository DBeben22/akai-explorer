/*
 * AKAI Explorer - shim that is force-included (-include) into every akaiutil
 * translation unit. It redirects akaiutil's console output macros into a log
 * buffer owned by the application, WITHOUT modifying the akaiutil sources.
 * (commoninclude.h only defines PRINTF_OUT / PRINTF_ERR if they are undefined.)
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef AKX_SHIM_H
#define AKX_SHIM_H

#ifdef __cplusplus
extern "C" {
#endif

int akx_log_out(const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;
int akx_log_err(const char *fmt, ...)
#if defined(__GNUC__)
    __attribute__((format(printf, 1, 2)))
#endif
    ;

#ifdef __cplusplus
}
#endif

#define PRINTF_OUT(...) akx_log_out(__VA_ARGS__)
#define PRINTF_ERR(...) akx_log_err(__VA_ARGS__)

#endif /* AKX_SHIM_H */
