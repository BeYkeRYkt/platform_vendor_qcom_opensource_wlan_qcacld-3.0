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
#if !defined(__CDS_API_H)
#define __CDS_API_H

/**
 * DOC:  cds_api.h
 *
 * Connectivity driver services public API
 *
 */

#include <cdf_types.h>
#include <cdf_status.h>
#include <cdf_memory.h>
#include <cdf_list.h>
#include <cdf_trace.h>
#include <cdf_event.h>
#include <cdf_lock.h>
#include <cds_reg_service.h>
#include <cds_mq.h>
#include <cds_packet.h>
#include <cds_sched.h>
#include <cdf_threads.h>
#include <cdf_mc_timer.h>
#include <cds_pack_align.h>

/* Amount of time to wait for WMA to perform an asynchronous activity.
 * This value should be larger than the timeout used by WMI to wait for
 * a response from target
 */
#define CDS_WMA_TIMEOUT  (15000)

CDF_STATUS cds_alloc_global_context(v_CONTEXT_t *p_cds_context);

CDF_STATUS cds_free_global_context(v_CONTEXT_t *p_cds_context);

CDF_STATUS cds_pre_enable(v_CONTEXT_t cds_context);

CDF_STATUS cds_open(v_CONTEXT_t *p_cds_context, uint32_t hddContextSize);

CDF_STATUS cds_enable(v_CONTEXT_t cds_context);

CDF_STATUS cds_disable(v_CONTEXT_t cds_context);

CDF_STATUS cds_close(v_CONTEXT_t cds_context);

CDF_STATUS cds_shutdown(v_CONTEXT_t cds_context);

void cds_core_return_msg(void *pVContext, p_cds_msg_wrapper pMsgWrapper);

void *cds_get_context(CDF_MODULE_ID moduleId);

v_CONTEXT_t cds_get_global_context(void);

uint8_t cds_is_logp_in_progress(void);
void cds_set_logp_in_progress(uint8_t value);

uint8_t cds_is_load_unload_in_progress(void);
uint8_t cds_is_unload_in_progress(void);

void cds_set_load_unload_in_progress(uint8_t value);

CDF_STATUS cds_alloc_context(void *p_cds_context, CDF_MODULE_ID moduleID,
			     void **ppModuleContext, uint32_t size);

CDF_STATUS cds_free_context(void *p_cds_context, CDF_MODULE_ID moduleID,
			    void *pModuleContext);

CDF_STATUS cds_get_vdev_types(tCDF_CON_MODE mode, uint32_t *type,
			      uint32_t *subType);

void cds_flush_work(void *work);
void cds_flush_delayed_work(void *dwork);

bool cds_is_packet_log_enabled(void);

uint64_t cds_get_monotonic_boottime(void);

void cds_trigger_recovery(void);

void cds_set_wakelock_logging(bool value);
bool cds_is_wakelock_enabled(void);
void cds_set_ring_log_level(uint32_t ring_id, uint32_t log_level);
enum wifi_driver_log_level cds_get_ring_log_level(uint32_t ring_id);
void cds_set_multicast_logging(uint8_t value);
uint8_t cds_is_multicast_logging(void);
CDF_STATUS cds_set_log_completion(uint32_t is_fatal,
		uint32_t type,
		uint32_t sub_type);
void cds_get_log_completion(uint32_t *is_fatal,
		uint32_t *type,
		uint32_t *sub_type);
bool cds_is_log_report_in_progress(void);
void cds_init_log_completion(void);
void cds_deinit_log_completion(void);
CDF_STATUS cds_flush_logs(uint32_t is_fatal,
		uint32_t indicator,
		uint32_t reason_code);
void cds_logging_set_fw_flush_complete(void);
#endif /* if !defined __CDS_API_H */
