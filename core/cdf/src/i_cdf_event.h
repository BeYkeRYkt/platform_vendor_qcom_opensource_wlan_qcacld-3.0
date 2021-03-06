/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
 *
 * Permission to use, copy, modify, and/or distribute this software for
 * any purpose with or without fee is hereby granted, provided that the
 * above copyright notice and this permission notice appear in all
 * copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL
 * WARRANTIES WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE
 * AUTHOR BE LIABLE FOR ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL
 * DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR
 * PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR OTHER
 * TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
 * PERFORMANCE OF THIS SOFTWARE.
 */

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#if !defined(__I_CDF_EVENT_H)
#define __I_CDF_EVENT_H

/**
 * DOC: i_cdf_event.h
 *
 * Linux-specific definitions for CDF Events
 */

/* Include Files */
#include <cdf_types.h>
#include <linux/completion.h>

/* Preprocessor definitions and constants */
#define LINUX_EVENT_COOKIE 0x12341234

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 14, 0))
#define INIT_COMPLETION(event) reinit_completion(&event)
#endif

#ifdef __cplusplus
extern "C" {
#endif /* __cplusplus */

/* Type declarations */

typedef struct evt {
	struct completion complete;
	uint32_t cookie;
} cdf_event_t;

#ifdef __cplusplus
}
#endif /* __cplusplus */
#endif /* __I_CDF_EVENT_H */
