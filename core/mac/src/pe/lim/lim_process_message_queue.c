/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/*
 * This file lim ProcessMessageQueue.cc contains the code
 * for processing LIM message Queue.
 * Author:        Chandra Modumudi
 * Date:          02/11/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#include "cds_api.h"
#include "wni_api.h"
#include "wma_types.h"

#include "wni_cfg.h"
#include "cfg_api.h"
#include "sir_common.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_utils.h"
#include "lim_assoc_utils.h"
#include "lim_prop_exts_utils.h"

#include "lim_admit_control.h"
#include "lim_ibss_peer_mgmt.h"
#include "sch_api.h"
#ifdef WLAN_FEATURE_VOWIFI_11R
#include "lim_ft_defs.h"
#endif
#include "lim_session.h"
#include "lim_send_messages.h"

#if defined WLAN_FEATURE_VOWIFI
#include "rrm_api.h"
#endif

#if defined WLAN_FEATURE_VOWIFI_11R
#include "lim_ft.h"
#endif

#include "cdf_types.h"
#include "cds_packet.h"
#include "cdf_memory.h"

void lim_log_session_states(tpAniSirGlobal pMac);
static void lim_process_normal_hdd_msg(tpAniSirGlobal mac_ctx,
	struct sSirMsgQ *msg, uint8_t rsp_reqd);

/**
 * lim_process_dual_mac_cfg_resp() - Process set dual mac config response
 * @mac: Global MAC pointer
 * @body: Set dual mac config response in sir_dual_mac_config_resp format
 *
 * Process the set dual mac config response and post the message
 * to SME to process this further and release the active
 * command list
 *
 * Return: None
 */
static void lim_process_dual_mac_cfg_resp(tpAniSirGlobal mac, void *body)
{
	struct sir_dual_mac_config_resp *resp, *param;
	uint32_t len, fail_resp = 0;
	tSirMsgQ msg;

	resp = (struct sir_dual_mac_config_resp *)body;
	if (!resp) {
		lim_log(mac, LOGE, FL("Set dual mac cfg param is NULL"));
		fail_resp = 1;
		/* Not returning here. If possible, let us proceed
		 * and send fail response to SME
		 */
	}

	len = sizeof(*param);

	param = cdf_mem_malloc(len);
	if (!param) {
		lim_log(mac, LOGE, FL("Fail to allocate memory"));
		/* Memory allocation for param failed.
		 * Cannot send fail status back to SME
		 */
		return;
	}

	if (fail_resp) {
		lim_log(mac, LOGE, FL("Send fail status to SME"));
		param->status = SET_HW_MODE_STATUS_ECANCELED;
	} else {
		param->status = resp->status;
		/*
		 * TODO: Update this HW mode info in any UMAC params, if needed
		 */
	}

	msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	lim_log(mac, LOG1, FL("Send eWNI_SME_SET_DUAL_MAC_CFG_RESP to SME"));
	lim_sys_process_mmh_msg_api(mac, &msg, ePROT);
	return;
}

/**
 * lim_process_set_hw_mode_resp() - Process set HW mode response
 * @mac: Global MAC pointer
 * @body: Set HW mode response in sir_set_hw_mode_resp format
 *
 * Process the set HW mode response and post the message
 * to SME to process this further and release the active
 * command list
 *
 * Return: None
 */
static void lim_process_set_hw_mode_resp(tpAniSirGlobal mac, void *body)
{
	struct sir_set_hw_mode_resp *resp, *param;
	uint32_t len, i, fail_resp = 0;
	tSirMsgQ msg;

	resp = (struct sir_set_hw_mode_resp *)body;
	if (!resp) {
		lim_log(mac, LOGE, FL("Set HW mode param is NULL"));
		fail_resp = 1;
		/* Not returning here. If possible, let us proceed
		 * and send fail response to SME */
	}

	len = sizeof(*param);

	param = cdf_mem_malloc(len);
	if (!param) {
		lim_log(mac, LOGE, FL("Fail to allocate memory"));
		/* Memory allocation for param failed.
		 * Cannot send fail status back to SME
		 */
		return;
	}

	if (fail_resp) {
		lim_log(mac, LOGE, FL("Send fail status to SME"));
		param->status = SET_HW_MODE_STATUS_ECANCELED;
		param->cfgd_hw_mode_index = 0;
		param->num_vdev_mac_entries = 0;
	} else {
		param->status = resp->status;
		param->cfgd_hw_mode_index = resp->cfgd_hw_mode_index;
		param->num_vdev_mac_entries = resp->num_vdev_mac_entries;

		for (i = 0; i < resp->num_vdev_mac_entries; i++) {
			param->vdev_mac_map[i].vdev_id =
				resp->vdev_mac_map[i].vdev_id;
			param->vdev_mac_map[i].mac_id =
				resp->vdev_mac_map[i].mac_id;
		}
		/*
		 * TODO: Update this HW mode info in any UMAC params, if needed
		 */
	}

	msg.type = eWNI_SME_SET_HW_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	lim_log(mac, LOGE, FL("Send eWNI_SME_SET_HW_MODE_RESP to SME"));
	lim_sys_process_mmh_msg_api(mac, &msg, ePROT);
	return;
}

/**
 * lim_process_hw_mode_trans_ind() - Process set HW mode transition indication
 * @mac: Global MAC pointer
 * @body: Set HW mode response in sir_hw_mode_trans_ind format
 *
 * Process the set HW mode transition indication and post the message
 * to SME to invoke the HDD callback
 * command list
 *
 * Return: None
 */
static void lim_process_hw_mode_trans_ind(tpAniSirGlobal mac, void *body)
{
	struct sir_hw_mode_trans_ind *ind, *param;
	uint32_t len, i;
	tSirMsgQ msg;

	ind = (struct sir_hw_mode_trans_ind *)body;
	if (!ind) {
		lim_log(mac, LOGE, FL("Set HW mode trans ind param is NULL"));
		return;
	}

	len = sizeof(*param);

	param = cdf_mem_malloc(len);
	if (!param) {
		lim_log(mac, LOGE, FL("Fail to allocate memory"));
		return;
	}

	param->old_hw_mode_index = ind->old_hw_mode_index;
	param->new_hw_mode_index = ind->new_hw_mode_index;
	param->num_vdev_mac_entries = ind->num_vdev_mac_entries;

	for (i = 0; i < ind->num_vdev_mac_entries; i++) {
		param->vdev_mac_map[i].vdev_id =
			ind->vdev_mac_map[i].vdev_id;
		param->vdev_mac_map[i].mac_id =
			ind->vdev_mac_map[i].mac_id;
	}

	/* TODO: Update this HW mode info in any UMAC params, if needed */

	msg.type = eWNI_SME_HW_MODE_TRANS_IND;
	msg.bodyptr = param;
	msg.bodyval = 0;
	lim_log(mac, LOGE, FL("Send eWNI_SME_HW_MODE_TRANS_IND to SME"));
	lim_sys_process_mmh_msg_api(mac, &msg, ePROT);
	return;
}

/** -------------------------------------------------------------
   \fn def_msg_decision
   \brief The function decides whether to defer a message or not in limProcessMessage function
   \param   tpAniSirGlobal pMac
   \param       tSirMsgQ  limMsg
   \param       tSirMacTspecIE   *ppInfo
   \return none
   -------------------------------------------------------------*/

uint8_t static def_msg_decision(tpAniSirGlobal pMac, tpSirMsgQ limMsg)
{

/* this function should not changed */
	if (pMac->lim.gLimSmeState == eLIM_SME_OFFLINE_STATE) {
		/* Defer processsing this message */
		if (lim_defer_msg(pMac, limMsg) != TX_SUCCESS) {
			CDF_TRACE(CDF_MODULE_ID_PE, LOGE,
					FL("Unable to Defer Msg"));
			lim_log_session_states(pMac);
			lim_handle_defer_msg_error(pMac, limMsg);
		}
		return true;
	}
	/* When defer is requested then defer all the messages except HAL responses. */
	if ((!lim_is_system_in_scan_state(pMac))
	    && (true != GET_LIM_PROCESS_DEFD_MESGS(pMac))
	    && !pMac->lim.gLimSystemInScanLearnMode) {
		if ((limMsg->type != WMA_ADD_BSS_RSP)
		    && (limMsg->type != WMA_DELETE_BSS_RSP)
		    && (limMsg->type != WMA_ADD_STA_RSP)
		    && (limMsg->type != WMA_DELETE_STA_RSP)
		    && (limMsg->type != WMA_SET_BSSKEY_RSP)
		    && (limMsg->type != WMA_SET_STAKEY_RSP)
		    && (limMsg->type != WMA_SET_STA_BCASTKEY_RSP)
		    && (limMsg->type != WMA_AGGR_QOS_RSP)
		    && (limMsg->type != WMA_SET_MIMOPS_RSP)
		    && (limMsg->type != WMA_SWITCH_CHANNEL_RSP)
		    && (limMsg->type != WMA_P2P_NOA_ATTR_IND)
		    && (limMsg->type != WMA_P2P_NOA_START_IND) &&
#ifdef FEATURE_OEM_DATA_SUPPORT
		    (limMsg->type != WMA_START_OEM_DATA_RSP) &&
#endif
		    (limMsg->type != WMA_ADD_TS_RSP) &&
		    /* Allow processing of RX frames while awaiting reception
		     * of ADD TS response over the air. This logic particularly
		     * handles the case when host sends ADD BA request to FW
		     * after ADD TS request is sent over the air and
		     * ADD TS response received over the air */
		    !(limMsg->type == SIR_BB_XPORT_MGMT_MSG &&
			pMac->lim.gLimAddtsSent)) {
			PELOG1(lim_log
			       (pMac, LOG1,
			       FL
					       ("Defer the current message %s , gLimProcessDefdMsgs is false and system is not in scan/learn mode"),
				       lim_msg_str(limMsg->type));
			       )
			/* Defer processsing this message */
			if (lim_defer_msg(pMac, limMsg) != TX_SUCCESS) {
				CDF_TRACE(CDF_MODULE_ID_PE, LOGE,
					FL("Unable to Defer Msg"));
				lim_log_session_states(pMac);
				lim_handle_defer_msg_error(pMac, limMsg);

			}
			return true;
		}
	}
	return false;
}

#ifdef FEATURE_WLAN_EXTSCAN
static void
__lim_pno_match_fwd_bcn_probepsp(tpAniSirGlobal pmac, uint8_t *rx_pkt_info,
				tSirProbeRespBeacon *frame, uint32_t ie_len,
				uint32_t msg_type)
{
	struct pno_match_found  *result;
	uint8_t                 *body;
	tSirMsgQ                mmh_msg;
	tpSirMacMgmtHdr         hdr;
	uint32_t num_results = 1, len, i;

	/* Upon receiving every matched beacon, bss info is forwarded to the
	 * the upper layer, hence num_results is set to 1 */
	len = sizeof(*result) + (num_results * sizeof(tSirWifiScanResult)) +
		ie_len;

	result = cdf_mem_malloc(len);
	if (NULL == result) {
		lim_log(pmac, LOGE, FL("Memory allocation failed"));
		return;
	}
	hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	cdf_mem_zero(result, sizeof(*result) + ie_len);

	/* Received frame does not have request id, hence set 0 */
	result->request_id = 0;
	result->more_data = 0;
	result->num_results = num_results;

	for (i = 0; i < result->num_results; i++) {
		result->ap[i].ts = cdf_mc_timer_get_system_time();
		result->ap[i].beaconPeriod = frame->beaconInterval;
		result->ap[i].capability =
			lim_get_u16((uint8_t *) &frame->capabilityInfo);
		result->ap[i].channel = WMA_GET_RX_CH(rx_pkt_info);
		result->ap[i].rssi = WMA_GET_RX_RSSI_DB(rx_pkt_info);
		result->ap[i].rtt = 0;
		result->ap[i].rtt_sd = 0;
		result->ap[i].ieLength = ie_len;
		cdf_mem_copy((uint8_t *) &result->ap[i].ssid[0],
			(uint8_t *) frame->ssId.ssId, frame->ssId.length);
		result->ap[i].ssid[frame->ssId.length] = '\0';
		cdf_mem_copy((uint8_t *) &result->ap[i].bssid,
				(uint8_t *) hdr->bssId,
				sizeof(tSirMacAddr));
		/* Copy IE fields */
		cdf_mem_copy((uint8_t *) &result->ap[i].ieData,
				body + SIR_MAC_B_PR_SSID_OFFSET, ie_len);
	}

	mmh_msg.type = msg_type;
	mmh_msg.bodyptr = result;
	mmh_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(pmac, &mmh_msg, ePROT);
}


static void
__lim_ext_scan_forward_bcn_probe_rsp(tpAniSirGlobal pmac, uint8_t *rx_pkt_info,
					tSirProbeRespBeacon *frame,
					uint32_t ie_len,
					uint32_t msg_type)
{
	tpSirWifiFullScanResultEvent result;
	uint8_t                     *body;
	tSirMsgQ                     mmh_msg;
	tpSirMacMgmtHdr              hdr;

	result = cdf_mem_malloc(sizeof(*result) + ie_len);
	if (NULL == result) {
		lim_log(pmac, LOGE, FL("Memory allocation failed"));
		return;
	}
	hdr = WMA_GET_RX_MAC_HEADER(rx_pkt_info);
	body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
	cdf_mem_zero(result, sizeof(*result) + ie_len);

	/* Received frame does not have request id, hence set 0 */
	result->requestId = 0;

	result->moreData = 0;
	result->ap.ts = cdf_mc_timer_get_system_time();
	result->ap.beaconPeriod = frame->beaconInterval;
	result->ap.capability =
			lim_get_u16((uint8_t *) &frame->capabilityInfo);
	result->ap.channel = WMA_GET_RX_CH(rx_pkt_info);
	result->ap.rssi = WMA_GET_RX_RSSI_DB(rx_pkt_info);
	result->ap.rtt = 0;
	result->ap.rtt_sd = 0;
	result->ap.ieLength = ie_len;

	cdf_mem_copy((uint8_t *) &result->ap.ssid[0],
			(uint8_t *) frame->ssId.ssId, frame->ssId.length);
	result->ap.ssid[frame->ssId.length] = '\0';
	cdf_mem_copy((uint8_t *) &result->ap.bssid.bytes,
			(uint8_t *) hdr->bssId,
			CDF_MAC_ADDR_SIZE);
	/* Copy IE fields */
	cdf_mem_copy((uint8_t *) &result->ap.ieData,
			body + SIR_MAC_B_PR_SSID_OFFSET, ie_len);

	mmh_msg.type = msg_type;
	mmh_msg.bodyptr = result;
	mmh_msg.bodyval = 0;
	lim_sys_process_mmh_msg_api(pmac, &mmh_msg, ePROT);
}

static void
__lim_process_ext_scan_beacon_probe_rsp(tpAniSirGlobal pmac,
					uint8_t *rx_pkt_info,
					uint8_t sub_type)
{
	tSirProbeRespBeacon  *frame;
	uint8_t              *body;
	uint32_t             frm_len;
	tSirRetStatus        status;

	frm_len = WMA_GET_RX_PAYLOAD_LEN(rx_pkt_info);
	if (frm_len <= SIR_MAC_B_PR_SSID_OFFSET) {
		lim_log(pmac, LOGP,
			FL("RX packet has invalid length %d"), frm_len);
		return;
	}

	frame = cdf_mem_malloc(sizeof(*frame));
	if (NULL == frame) {
		lim_log(pmac, LOGE, FL("Memory allocation failed"));
		return;
	}

	if (sub_type == SIR_MAC_MGMT_BEACON) {
		lim_log(pmac, LOG2, FL("Beacon due to ExtScan/epno"));
		status = sir_convert_beacon_frame2_struct(pmac,
						(uint8_t *)rx_pkt_info,
						frame);
	} else if (sub_type == SIR_MAC_MGMT_PROBE_RSP) {
		lim_log(pmac, LOG2, FL("Probe Rsp due to ExtScan/epno"));
		body = WMA_GET_RX_MPDU_DATA(rx_pkt_info);
		status = sir_convert_probe_frame2_struct(pmac, body,
							frm_len, frame);
	} else {
		cdf_mem_free(frame);
		return;
	}

	if (status != eSIR_SUCCESS) {
		lim_log(pmac, LOGE, FL("Frame parsing failed"));
		cdf_mem_free(frame);
		return;
	}

	if (WMA_IS_EXTSCAN_SCAN_SRC(rx_pkt_info))
		__lim_ext_scan_forward_bcn_probe_rsp(pmac, rx_pkt_info, frame,
					(frm_len - SIR_MAC_B_PR_SSID_OFFSET),
					eWNI_SME_EXTSCAN_FULL_SCAN_RESULT_IND);

	if (WMA_IS_EPNO_SCAN_SRC(rx_pkt_info))
		__lim_pno_match_fwd_bcn_probepsp(pmac, rx_pkt_info, frame,
					(frm_len - SIR_MAC_B_PR_SSID_OFFSET),
					eWNI_SME_EPNO_NETWORK_FOUND_IND);

	cdf_mem_free(frame);
}
#endif

/*
 * Beacon Handling Cases:
 * during scanning, when no session is active:
 *    handled by lim_handle_frames_in_scan_state before __lim_handle_beacon call is invoked.
 * during scanning, when any session is active, but beacon/Pr does not belong to that session, psessionEntry will be null.
 *    handled by lim_handle_frames_in_scan_state before __lim_handle_beacon call is invoked.
 * during scanning, when any session is active, and beacon/Pr belongs to one of the session, psessionEntry will not be null.
 *    handled by lim_handle_frames_in_scan_state before __lim_handle_beacon call is invoked.
 * Not scanning, no session:
 *    there should not be any beacon coming, if coming, should be dropped.
 * Not Scanning,
 */
static void
__lim_handle_beacon(tpAniSirGlobal pMac, tpSirMsgQ pMsg,
		    tpPESession psessionEntry)
{
	/* checking for global SME state... */
	uint8_t *pRxPacketInfo;
	lim_get_b_dfrom_rx_packet(pMac, pMsg->bodyptr,
				  (uint32_t * *) &pRxPacketInfo);

	/* This function should not be called if beacon is received in scan state. */
	/* So not doing any checks for the global state. */

	if (psessionEntry == NULL) {
		sch_beacon_process(pMac, pRxPacketInfo, NULL);
	} else if ((psessionEntry->limSmeState == eLIM_SME_LINK_EST_STATE) ||
		   (psessionEntry->limSmeState == eLIM_SME_NORMAL_STATE)) {
		sch_beacon_process(pMac, pRxPacketInfo, psessionEntry);
	} else
		lim_process_beacon_frame(pMac, pRxPacketInfo, psessionEntry);

	return;
}

/**
 * lim_defer_msg()
 *
 ***FUNCTION:
 * This function is called to defer the messages received
 * during Learn mode
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMsg of type tSirMsgQ - Pointer to the message structure
 * @return None
 */

uint32_t lim_defer_msg(tpAniSirGlobal pMac, tSirMsgQ *pMsg)
{
	uint32_t retCode = TX_SUCCESS;

	retCode = lim_write_deferred_msg_q(pMac, pMsg);

	if (retCode == TX_SUCCESS) {
		MTRACE(mac_trace_msg_rx
			       (pMac, NO_SESSION,
			       LIM_TRACE_MAKE_RXMSG(pMsg->type, LIM_MSG_DEFERRED));
		       )
	} else {
		lim_log(pMac, LOGE, FL("Dropped lim message (0x%X)"),
			pMsg->type);
		MTRACE(mac_trace_msg_rx
			       (pMac, NO_SESSION,
			       LIM_TRACE_MAKE_RXMSG(pMsg->type, LIM_MSG_DROPPED));
		       )
	}

	return retCode;
} /*** end lim_defer_msg() ***/

/**
 * lim_handle_unknown_a2_index_frames() - This function handles Unknown Unicast
 *                                        (A2 Index) packets
 * @mac_ctx:          Pointer to the Global Mac Context.
 * @rx_pkt_buffer:    Pointer to the packet Buffer descriptor.
 * @session_entry:    Pointer to the PE Session Entry.
 *
 * This routine will handle public action frames.
 *
 * Return:      None.
 */
static void lim_handle_unknown_a2_index_frames(tpAniSirGlobal mac_ctx,
	void *rx_pkt_buffer, tpPESession session_entry)
{
#ifdef FEATURE_WLAN_TDLS
	tpSirMacDataHdr3a mac_hdr;
#endif
	if (LIM_IS_P2P_DEVICE_ROLE(session_entry))
		lim_process_action_frame_no_session(mac_ctx,
			(uint8_t *) rx_pkt_buffer);
#ifdef FEATURE_WLAN_TDLS
	mac_hdr = WMA_GET_RX_MPDUHEADER3A(rx_pkt_buffer);

	if (lim_is_group_addr(mac_hdr->addr2)) {
		CDF_TRACE(CDF_MODULE_ID_PE, CDF_TRACE_LEVEL_INFO_HIGH,
			FL("Ignoring A2 Invalid Packet received for MC/BC:"));
		lim_print_mac_addr(mac_ctx, mac_hdr->addr2, LOG2);
		return;
	}
	CDF_TRACE(CDF_MODULE_ID_PE, CDF_TRACE_LEVEL_INFO,
			FL("type=0x%x, subtype=0x%x"),
		mac_hdr->fc.type, mac_hdr->fc.subType);
	/* Currently only following type and subtype are handled.
	 * If there are more combinations, then add switch-case
	 * statements.
	 */
	if (LIM_IS_STA_ROLE(session_entry) &&
		(mac_hdr->fc.type == SIR_MAC_MGMT_FRAME) &&
		(mac_hdr->fc.subType == SIR_MAC_MGMT_ACTION))
		lim_process_action_frame(mac_ctx, rx_pkt_buffer, session_entry);
#endif
	return;
}

/**
 * lim_check_mgmt_registered_frames() - This function handles registered
 *                                      management frames.
 *
 * @mac_ctx:          Pointer to the Global Mac Context.
 * @buff_desc:        Pointer to the packet Buffer descriptor.
 * @session_entry:    Pointer to the PE Session Entry.
 *
 * This function is called to process to check if received frame match with
 * any of the registered frame from HDD. If yes pass this frame to SME.
 *
 * Return:      True or False for Match or Mismatch respectively.
 */
static bool
lim_check_mgmt_registered_frames(tpAniSirGlobal mac_ctx, uint8_t *buff_desc,
				 tpPESession session_entry)
{
	tSirMacFrameCtl fc;
	tpSirMacMgmtHdr hdr;
	uint8_t *body;
	struct mgmt_frm_reg_info *mgmt_frame = NULL;
	struct mgmt_frm_reg_info *next_frm = NULL;
	uint16_t frm_type;
	uint16_t frm_len;
	uint8_t type, sub_type;
	bool match = false;
	CDF_STATUS cdf_status;

	hdr = WMA_GET_RX_MAC_HEADER(buff_desc);
	fc = hdr->fc;
	frm_type = (fc.type << 2) | (fc.subType << 4);
	body = WMA_GET_RX_MPDU_DATA(buff_desc);
	frm_len = WMA_GET_RX_PAYLOAD_LEN(buff_desc);

	cdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
	cdf_list_peek_front(&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
			    (cdf_list_node_t **) &mgmt_frame);
	cdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);

	while (mgmt_frame != NULL) {
		type = (mgmt_frame->frameType >> 2) & 0x03;
		sub_type = (mgmt_frame->frameType >> 4) & 0x0f;
		if ((type == SIR_MAC_MGMT_FRAME)
		    && (fc.type == SIR_MAC_MGMT_FRAME)
		    && (sub_type == SIR_MAC_MGMT_RESERVED15)) {
			CDF_TRACE(CDF_MODULE_ID_PE, CDF_TRACE_LEVEL_INFO_HIGH,
				FL
				("rcvd frm match for SIR_MAC_MGMT_RESERVED15"));
			match = true;
			break;
		}
		if (mgmt_frame->frameType == frm_type) {
			if (mgmt_frame->matchLen <= 0) {
				match = true;
				break;
			}
			if (mgmt_frame->matchLen <= frm_len &&
				cdf_mem_compare(mgmt_frame->matchData, body,
				mgmt_frame->matchLen)) {
				/* found match! */
				match = true;
				break;
			}
		}

		cdf_mutex_acquire(&mac_ctx->lim.lim_frame_register_lock);
		cdf_status =
			cdf_list_peek_next(
			&mac_ctx->lim.gLimMgmtFrameRegistratinQueue,
			(cdf_list_node_t *) mgmt_frame,
			(cdf_list_node_t **) &next_frm);
		cdf_mutex_release(&mac_ctx->lim.lim_frame_register_lock);
		mgmt_frame = next_frm;
		next_frm = NULL;
	}

	if (match) {
		CDF_TRACE(CDF_MODULE_ID_PE, CDF_TRACE_LEVEL_INFO,
			FL("rcvd frame match with registered frame params"));
		/* Indicate this to SME */
		lim_send_sme_mgmt_frame_ind(mac_ctx, hdr->fc.subType,
			(uint8_t *) hdr,
			WMA_GET_RX_PAYLOAD_LEN(buff_desc) +
			sizeof(tSirMacMgmtHdr), mgmt_frame->sessionId,
			WMA_GET_RX_CH(buff_desc), session_entry, 0);

		if ((type == SIR_MAC_MGMT_FRAME)
		    && (fc.type == SIR_MAC_MGMT_FRAME)
		    && (sub_type == SIR_MAC_MGMT_RESERVED15))
			/* These packets needs to be processed by PE/SME
			 * as well as HDD.If it returns true here,
			 * the packet is forwarded to HDD only.
			 */
			match = false;
	}

	return match;
}

/**
 * lim_handle80211_frames()
 *
 ***FUNCTION:
 * This function is called to process 802.11 frames
 * received by LIM.
 *
 ***LOGIC:
 * NA
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @param  pMsg of type tSirMsgQ - Pointer to the message structure
 * @return None
 */

static void
lim_handle80211_frames(tpAniSirGlobal pMac, tpSirMsgQ limMsg, uint8_t *pDeferMsg)
{
	uint8_t *pRxPacketInfo = NULL;
	tSirMacFrameCtl fc;
	tpSirMacMgmtHdr pHdr = NULL;
	tpPESession psessionEntry = NULL;
	uint8_t sessionId;
	tAniBool isFrmFt = false;

	*pDeferMsg = false;
	lim_get_b_dfrom_rx_packet(pMac, limMsg->bodyptr,
				  (uint32_t * *) &pRxPacketInfo);

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);
	isFrmFt = WMA_GET_RX_FT_DONE(pRxPacketInfo);
	fc = pHdr->fc;

#ifdef WLAN_DUMP_MGMTFRAMES
	lim_log(pMac, LOGE,
		FL("ProtVersion %d, Type %d, Subtype %d rateIndex=%d"),
		fc.protVer, fc.type, fc.subType,
		WMA_GET_RX_MAC_RATE_IDX(pRxPacketInfo));
	CDF_TRACE_HEX_DUMP(CDF_MODULE_ID_PE, CDF_TRACE_LEVEL_ERROR, pHdr,
			   WMA_GET_RX_MPDU_HEADER_LEN(pRxPacketInfo));
#endif
	if (pMac->fEnableDebugLog & 0x1) {
		if ((fc.type == SIR_MAC_MGMT_FRAME) &&
		    (fc.subType != SIR_MAC_MGMT_PROBE_REQ) &&
		    (fc.subType != SIR_MAC_MGMT_PROBE_RSP) &&
		    (fc.subType != SIR_MAC_MGMT_BEACON)) {
			lim_log(pMac, LOGE,
				FL("RX MGMT - Type %hu, SubType %hu, seq num[%d]"),
				   fc.type,
				   fc.subType,
				   ((pHdr->seqControl.seqNumHi <<
				   HIGH_SEQ_NUM_OFFSET) |
				   pHdr->seqControl.seqNumLo));
		}
	}
#ifdef FEATURE_WLAN_EXTSCAN
	if (WMA_IS_EXTSCAN_SCAN_SRC(pRxPacketInfo) ||
		WMA_IS_EPNO_SCAN_SRC(pRxPacketInfo)) {
		if (fc.subType == SIR_MAC_MGMT_BEACON ||
		    fc.subType == SIR_MAC_MGMT_PROBE_RSP) {
			__lim_process_ext_scan_beacon_probe_rsp(pMac,
								pRxPacketInfo,
								fc.subType);
		} else {
			lim_log(pMac, LOGE,
				FL("Wrong frameType %d, Subtype %d for %d"),
				fc.type, fc.subType,
				WMA_GET_SCAN_SRC(pRxPacketInfo));
		}
		goto end;
	}
#endif
	if (WMA_GET_OFFLOADSCANLEARN(pRxPacketInfo)) {
		if (fc.subType == SIR_MAC_MGMT_BEACON) {
			lim_log(pMac, LOG2, FL("Learning scan beacon"));
			__lim_handle_beacon(pMac, limMsg, NULL);
		} else if (fc.subType == SIR_MAC_MGMT_PROBE_RSP) {
			lim_log(pMac, LOG2, FL("Learning scan probe rsp"));
			lim_process_probe_rsp_frame_no_session(pMac, pRxPacketInfo);
		} else {
			lim_log(pMac, LOGE,
				FL("Wrong frame Type %d, Subtype %d for LFR"),
				fc.type, fc.subType);
		}
		goto end;
	}
	/* Added For BT-AMP Support */
	if ((psessionEntry =
		     pe_find_session_by_bssid(pMac, pHdr->bssId,
					      &sessionId)) == NULL) {
#ifdef WLAN_FEATURE_VOWIFI_11R
		if (fc.subType == SIR_MAC_MGMT_AUTH) {
#ifdef WLAN_FEATURE_VOWIFI_11R_DEBUG
			lim_log(pMac, LOG1,
				FL
					("ProtVersion %d, Type %d, Subtype %d rateIndex=%d"),
				fc.protVer, fc.type, fc.subType,
				WMA_GET_RX_MAC_RATE_IDX(pRxPacketInfo));
			lim_print_mac_addr(pMac, pHdr->bssId, LOG1);
#endif
			if (lim_process_auth_frame_no_session
				    (pMac, pRxPacketInfo,
				    limMsg->bodyptr) == eSIR_SUCCESS) {
				lim_pkt_free(pMac, TXRX_FRM_802_11_MGMT,
					     pRxPacketInfo, limMsg->bodyptr);
				return;
			}
		}
#endif
		if ((fc.subType != SIR_MAC_MGMT_PROBE_RSP) &&
		    (fc.subType != SIR_MAC_MGMT_BEACON) &&
		    (fc.subType != SIR_MAC_MGMT_PROBE_REQ)
		    && (fc.subType != SIR_MAC_MGMT_ACTION)      /* Public action frame can be received from non-associated stations. */
		    ) {

			if ((psessionEntry =
				     pe_find_session_by_peer_sta(pMac, pHdr->sa,
								 &sessionId)) == NULL) {
				lim_log(pMac, LOG1,
					FL
						("session does not exist for given bssId"));
				lim_pkt_free(pMac, TXRX_FRM_802_11_MGMT,
					     pRxPacketInfo, limMsg->bodyptr);
				return;
			} else
				lim_log(pMac, LOG1,
					"SessionId:%d Session Exist for given Bssid",
					psessionEntry->peSessionId);
		}
		/*  For p2p resp frames search for valid session with DA as */
		/*  BSSID will be SA and session will be present with DA only */
		if (fc.subType == SIR_MAC_MGMT_ACTION) {
			psessionEntry =
				pe_find_session_by_bssid(pMac, pHdr->da, &sessionId);
		}
	}

	/* Check if frame is registered by HDD */
	if (lim_check_mgmt_registered_frames(pMac, pRxPacketInfo, psessionEntry)) {
		lim_log(pMac, LOG1, FL("Received frame is passed to SME"));
		lim_pkt_free(pMac, TXRX_FRM_802_11_MGMT, pRxPacketInfo,
			     limMsg->bodyptr);
		return;
	}

	if (fc.protVer != SIR_MAC_PROTOCOL_VERSION) {   /* Received Frame with non-zero Protocol Version */
		lim_log(pMac, LOGE,
			FL("Unexpected frame with protVersion %d received"),
			fc.protVer);
		lim_pkt_free(pMac, TXRX_FRM_802_11_MGMT, pRxPacketInfo,
			     (void *)limMsg->bodyptr);
#ifdef WLAN_DEBUG
		pMac->lim.numProtErr++;
#endif
		return;
	}

/* Chance of crashing : to be done BT-AMP ........happens when broadcast probe req is received */

#ifdef WLAN_DEBUG
	pMac->lim.numMAC[fc.type][fc.subType]++;
#endif

	switch (fc.type) {
	case SIR_MAC_MGMT_FRAME:
	{
		/* Received Management frame */
		switch (fc.subType) {
		case SIR_MAC_MGMT_ASSOC_REQ:
			/* Make sure the role supports Association */
			if (LIM_IS_BT_AMP_AP_ROLE(psessionEntry) ||
			    LIM_IS_AP_ROLE(psessionEntry))
				lim_process_assoc_req_frame(pMac,
							    pRxPacketInfo,
							    LIM_ASSOC,
							    psessionEntry);
			else {
				/* Unwanted messages - Log error */
				lim_log(pMac, LOGE,
					FL
						("unexpected message received %X"),
					limMsg->type);
				lim_print_msg_name(pMac, LOGE,
						   limMsg->type);
			}
			break;

		case SIR_MAC_MGMT_ASSOC_RSP:
			lim_process_assoc_rsp_frame(pMac, pRxPacketInfo,
						    LIM_ASSOC,
						    psessionEntry);
			break;

		case SIR_MAC_MGMT_REASSOC_REQ:
			/* Make sure the role supports Reassociation */
			if (LIM_IS_BT_AMP_AP_ROLE(psessionEntry) ||
			    LIM_IS_AP_ROLE(psessionEntry)) {
				lim_process_assoc_req_frame(pMac,
							    pRxPacketInfo,
							    LIM_REASSOC,
							    psessionEntry);
			} else {
				/* Unwanted messages - Log error */
				lim_log(pMac, LOGE,
					FL
						("unexpected message received %X"),
					limMsg->type);
				lim_print_msg_name(pMac, LOGE,
					limMsg->type);
			}
			break;

		case SIR_MAC_MGMT_REASSOC_RSP:
			lim_process_assoc_rsp_frame(pMac, pRxPacketInfo,
						    LIM_REASSOC,
						    psessionEntry);
			break;

		case SIR_MAC_MGMT_PROBE_REQ:
			lim_process_probe_req_frame_multiple_bss(pMac,
								 pRxPacketInfo,
								 psessionEntry);
			break;

		case SIR_MAC_MGMT_PROBE_RSP:
			if (psessionEntry == NULL)
				lim_process_probe_rsp_frame_no_session(pMac,
								       pRxPacketInfo);
			else
				lim_process_probe_rsp_frame(pMac,
							    pRxPacketInfo,
							    psessionEntry);
			break;

		case SIR_MAC_MGMT_BEACON:
			__lim_handle_beacon(pMac, limMsg, psessionEntry);
			break;

		case SIR_MAC_MGMT_DISASSOC:
			lim_process_disassoc_frame(pMac, pRxPacketInfo,
						   psessionEntry);
			break;

		case SIR_MAC_MGMT_AUTH:
			lim_process_auth_frame(pMac, pRxPacketInfo,
					       psessionEntry);
			break;

		case SIR_MAC_MGMT_DEAUTH:
			lim_process_deauth_frame(pMac, pRxPacketInfo,
						 psessionEntry);
			break;

		case SIR_MAC_MGMT_ACTION:
			if (psessionEntry == NULL)
				lim_process_action_frame_no_session(pMac,
								    pRxPacketInfo);
			else {
				if (WMA_GET_RX_UNKNOWN_UCAST
					    (pRxPacketInfo))
					lim_handle_unknown_a2_index_frames
						(pMac, pRxPacketInfo,
						psessionEntry);
				else
					lim_process_action_frame(pMac,
								 pRxPacketInfo,
								 psessionEntry);
			}
			break;
		default:
			/* Received Management frame of 'reserved' subtype */
			break;
		} /* switch (fc.subType) */

	}
	break;
	case SIR_MAC_DATA_FRAME:
	{
	}
	break;
	default:
		/* Received frame of type 'reserved' */
		break;

	} /* switch (fc.type) */

end:
	lim_pkt_free(pMac, TXRX_FRM_802_11_MGMT, pRxPacketInfo,
		     (void *)limMsg->bodyptr);
	return;
} /*** end lim_handle80211_frames() ***/

/**
 * lim_send_stop_scan_offload_req()
 *
 ***FUNCTION:
 * This function will be called to abort the ongoing offloaded scan
 * request.
 *
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @return CDF_STATUS_SUCCESS or CDF_STATUS_E_FAILURE
 */
CDF_STATUS lim_send_stop_scan_offload_req(tpAniSirGlobal pMac,
	uint8_t SessionId, uint32_t scan_id)
{
	tSirMsgQ msg;
	tSirRetStatus rc = eSIR_SUCCESS;
	tAbortScanParams *pAbortScanParams;

	pAbortScanParams = cdf_mem_malloc(sizeof(tAbortScanParams));
	if (NULL == pAbortScanParams) {
		lim_log(pMac, LOGP,
			FL("Memory allocation failed for AbortScanParams"));
		return CDF_STATUS_E_NOMEM;
	}

	pAbortScanParams->SessionId = SessionId;
	pAbortScanParams->scan_id = scan_id;
	msg.type = WMA_STOP_SCAN_OFFLOAD_REQ;
	msg.bodyptr = pAbortScanParams;
	msg.bodyval = 0;

	rc = wma_post_ctrl_msg(pMac, &msg);
	if (rc != eSIR_SUCCESS) {
		lim_log(pMac, LOGE, FL("wma_post_ctrl_msg() return failure"));
		cdf_mem_free(pAbortScanParams);
		return CDF_STATUS_E_FAILURE;
	}

	lim_log(pMac, LOG1, FL("Abort ongoing offload scan."));
	return CDF_STATUS_SUCCESS;

}

/**
 * lim_process_abort_scan_ind()
 *
 ***FUNCTION:
 * This function is called from HDD to abort the scan which is presently being run
 *
 *
 ***NOTE:
 *
 * @param  pMac      Pointer to Global MAC structure
 * @param  *pMsgBuf  A pointer to the SME message buffer
 * @return None
 */
void lim_process_abort_scan_ind(tpAniSirGlobal mac_ctx,
	uint8_t session_id, uint32_t scan_id)
{
#ifdef FEATURE_WLAN_DIAG_SUPPORT_LIM
	lim_diag_event_report(mac_ctx, WLAN_PE_DIAG_SCAN_ABORT_IND_EVENT, NULL, 0, 0);
#endif

	lim_log(mac_ctx, LOG2, FL("Processing AbortScan Ind scan_id %d"),
				 scan_id);

	/* send stop scan cmd to fw if scan offload is enabled. */
	lim_send_stop_scan_offload_req(mac_ctx, session_id, scan_id);
	return;
}

/**
 * lim_message_processor() - Process messages from LIM.
 *
 * @mac_ctx:          Pointer to the Global Mac Context.
 * @msg:              Received LIM message.
 *
 * Wrapper function for lim_process_messages when handling messages received by
 * LIM.Could either defer messages or process them.
 *
 * Return:  None.
 */
void lim_message_processor(tpAniSirGlobal mac_ctx, tpSirMsgQ msg)
{
	if (eLIM_MLM_OFFLINE_STATE == mac_ctx->lim.gLimMlmState) {
		pe_free_msg(mac_ctx, msg);
		return;
	}

	if (!def_msg_decision(mac_ctx, msg)) {
		lim_process_messages(mac_ctx, msg);
		/* process deferred message queue if allowed */
		if ((!(mac_ctx->lim.gLimAddtsSent)) &&
			(!(lim_is_system_in_scan_state(mac_ctx))) &&
			(true == GET_LIM_PROCESS_DEFD_MESGS(mac_ctx)))
			lim_process_deferred_message_queue(mac_ctx);
	}
}

#ifdef FEATURE_OEM_DATA_SUPPORT

void lim_oem_data_rsp_handle_resume_link_rsp(tpAniSirGlobal pMac, CDF_STATUS status,
					     uint32_t *mlmOemDataRsp)
{
	if (status != CDF_STATUS_SUCCESS) {
		lim_log(pMac, LOGE,
			FL
				("OEM Data Rsp failed to get the response for resume link"));
	}

	if (NULL != pMac->lim.gpLimMlmOemDataReq) {
		cdf_mem_free(pMac->lim.gpLimMlmOemDataReq);
		pMac->lim.gpLimMlmOemDataReq = NULL;
	}
	/* "Failure" status doesn't mean that Oem Data Rsp did not happen */
	/* and hence we need to respond to upper layers. Only Resume link is failed, but */
	/* we got the oem data response already. */
	/* Post the meessage to MLM */
	lim_post_sme_message(pMac, LIM_MLM_OEM_DATA_CNF,
			     (uint32_t *) (mlmOemDataRsp));

	return;
}

void lim_process_oem_data_rsp(tpAniSirGlobal pMac, uint32_t *body)
{
	tpLimMlmOemDataRsp mlmOemDataRsp = NULL;

	/* Process all the messages for the lim queue */
	SET_LIM_PROCESS_DEFD_MESGS(pMac, true);

	mlmOemDataRsp = (tpLimMlmOemDataRsp) body;

	PELOG1(lim_log
		       (pMac, LOG1, FL("%s: sending oem data response msg to sme"),
		       __func__);
	       )
	lim_post_sme_message(pMac, LIM_MLM_OEM_DATA_CNF,
			     (uint32_t *) (mlmOemDataRsp));

	return;
}

#endif

/**
 * lim_process_messages() - Process messages from upper layers.
 *
 * @mac_ctx:          Pointer to the Global Mac Context.
 * @msg:              Received message.
 *
 * Return:  None.
 */
void lim_process_messages(tpAniSirGlobal mac_ctx, tpSirMsgQ msg)
{
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	uint8_t vdev_id = 0;
	tUpdateBeaconParams beacon_params;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	uint8_t i;
	uint8_t p2p_go_exists = 0;
	tpPESession session_entry = NULL;
	uint8_t defer_msg = false;
	tLinkStateParams *link_state_param;
	uint16_t pkt_len = 0;
	cds_pkt_t *body_ptr = NULL;
	CDF_STATUS cdf_status;
	tSirMsgQ new_msg;
	tSirSmeScanAbortReq *req_msg = NULL;
	uint8_t session_id;
	uint32_t scan_id;

#ifdef FEATURE_WLAN_TDLS
	tSirTdlsInd *tdls_ind = NULL;
	tpDphHashNode sta_ds = NULL;
	tTdlsLinkEstablishParams *tdls_link_params = NULL;
#endif
	tSirMbMsgP2p *p2p_msg = NULL;
	if (ANI_DRIVER_TYPE(mac_ctx) == eDRIVER_TYPE_MFG) {
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		return;
	}
	if (msg == NULL) {
		lim_log(mac_ctx, LOGE, FL("Message pointer is Null"));
		CDF_ASSERT(0);
		return;
	}
#ifdef WLAN_DEBUG
	mac_ctx->lim.numTot++;
#endif
	MTRACE(mac_trace_msg_rx(mac_ctx, NO_SESSION,
		LIM_TRACE_MAKE_RXMSG(msg->type, LIM_MSG_PROCESSED));)

	switch (msg->type) {

	case SIR_LIM_UPDATE_BEACON:
		lim_update_beacon(mac_ctx);
		break;
	case SIR_CFG_PARAM_UPDATE_IND:
		if (!lim_is_system_in_scan_state(mac_ctx)) {
			lim_handle_cf_gparam_update(mac_ctx, msg->bodyval);
			break;
		}
		/* System is in DFS (Learn) mode.
		 * Defer processsing this message
		 */
		if (lim_defer_msg(mac_ctx, msg) != TX_SUCCESS) {
			if (!(mac_ctx->lim.deferredMsgCnt & 0xF))
				CDF_TRACE(CDF_MODULE_ID_PE, LOGE,
					FL("Unable to Defer Msg"));
			lim_log_session_states(mac_ctx);
			lim_print_msg_name(mac_ctx, LOGE, msg->type);
		}
		break;
#ifdef FEATURE_OEM_DATA_SUPPORT
	case WMA_START_OEM_DATA_RSP:
		lim_process_oem_data_rsp(mac_ctx, msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#endif
	case WMA_SWITCH_CHANNEL_RSP:
		lim_process_switch_channel_rsp(mac_ctx, msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#ifdef ANI_SIR_IBSS_PEER_CACHING
	case WMA_IBSS_STA_ADD:
		lim_ibss_sta_add(mac_ctx, msg->bodyptr);
		break;
#endif
	case SIR_BB_XPORT_MGMT_MSG:
		/* These messages are from Peer MAC entity. */
#ifdef WLAN_DEBUG
		mac_ctx->lim.numBbt++;
#endif
		/* The original msg which we were deferring have the
		 * bodyPointer point to 'BD' instead of 'cds pkt'. If we
		 * don't make a copy of msg, then overwrite the
		 * msg->bodyPointer and next time when we try to
		 * process the msg, we will try to use 'BD' as
		 * 'cds Pkt' which will cause a crash
		 */
		if (msg->bodyptr == NULL) {
			lim_log(mac_ctx, LOGE, FL("Message bodyptr is Null"));
			CDF_ASSERT(0);
			break;
		}
		cdf_mem_copy((uint8_t *) &new_msg,
			(uint8_t *) msg, sizeof(tSirMsgQ));
		body_ptr = (cds_pkt_t *) new_msg.bodyptr;
		cds_pkt_get_packet_length(body_ptr, &pkt_len);

		cdf_status = wma_ds_peek_rx_packet_info(body_ptr,
			(void **) &new_msg.bodyptr, false);

		if (!CDF_IS_STATUS_SUCCESS(cdf_status)) {
			cds_pkt_return_packet(body_ptr);
			break;
		}

                if (WMA_GET_ROAMCANDIDATEIND(new_msg.bodyptr))
                        lim_log(mac_ctx, LOG1, FL("roamCandidateInd %d"),
                                WMA_GET_ROAMCANDIDATEIND(new_msg.bodyptr));

                if (WMA_GET_OFFLOADSCANLEARN(new_msg.bodyptr))
                        lim_log(mac_ctx, LOG1, FL("offloadScanLearn %d"),
                                 WMA_GET_OFFLOADSCANLEARN(new_msg.bodyptr));

		lim_handle80211_frames(mac_ctx, &new_msg, &defer_msg);

		if (defer_msg == true) {
			CDF_TRACE(CDF_MODULE_ID_PE, LOG1,
					FL("Defer Msg type=%x"), msg->type);
			if (lim_defer_msg(mac_ctx, msg) != TX_SUCCESS) {
				CDF_TRACE(CDF_MODULE_ID_PE, LOGE,
						FL("Unable to Defer Msg"));
				lim_log_session_states(mac_ctx);
				cds_pkt_return_packet(body_ptr);
			}
		} else
			/* PE is not deferring this 802.11 frame so we need to
			 * call cds_pkt_return. Asumption here is when Rx mgmt
			 * frame processing is done, cds packet could be
			 * freed here.
			 */
			cds_pkt_return_packet(body_ptr);
		break;
	case eWNI_SME_SCAN_REQ:
	case eWNI_SME_REMAIN_ON_CHANNEL_REQ:
	case eWNI_SME_DISASSOC_REQ:
	case eWNI_SME_DEAUTH_REQ:
#ifdef FEATURE_OEM_DATA_SUPPORT
	case eWNI_SME_OEM_DATA_REQ:
#endif
#ifdef FEATURE_WLAN_TDLS
	case eWNI_SME_TDLS_SEND_MGMT_REQ:
	case eWNI_SME_TDLS_ADD_STA_REQ:
	case eWNI_SME_TDLS_DEL_STA_REQ:
	case eWNI_SME_TDLS_LINK_ESTABLISH_REQ:
#endif
	case eWNI_SME_RESET_AP_CAPS_CHANGED:
	case eWNI_SME_SET_HW_MODE_REQ:
	case eWNI_SME_SET_DUAL_MAC_CFG_REQ:
		/* These messages are from HDD. Need to respond to HDD */
		lim_process_normal_hdd_msg(mac_ctx, msg, true);
		break;

	case eWNI_SME_SCAN_ABORT_IND:
		req_msg = msg->bodyptr;
		if (req_msg) {
			session_id = req_msg->sessionId;
			scan_id = req_msg->scan_id;
			lim_process_abort_scan_ind(mac_ctx, session_id,
				scan_id);
			cdf_mem_free((void *)msg->bodyptr);
			msg->bodyptr = NULL;
		}
		break;
	case eWNI_SME_SYS_READY_IND:
	case eWNI_SME_JOIN_REQ:
	case eWNI_SME_REASSOC_REQ:
	case eWNI_SME_START_BSS_REQ:
	case eWNI_SME_STOP_BSS_REQ:
	case eWNI_SME_SWITCH_CHL_IND:
	case eWNI_SME_SETCONTEXT_REQ:
	case eWNI_SME_DISASSOC_CNF:
	case eWNI_SME_DEAUTH_CNF:
	case eWNI_SME_ASSOC_CNF:
	case eWNI_SME_ADDTS_REQ:
	case eWNI_SME_DELTS_REQ:
	case eWNI_SME_GET_ASSOC_STAS_REQ:
	case eWNI_SME_TKIP_CNTR_MEAS_REQ:
	case eWNI_SME_UPDATE_APWPSIE_REQ:
	case eWNI_SME_HIDE_SSID_REQ:
	case eWNI_SME_GET_WPSPBC_SESSION_REQ:
	case eWNI_SME_SET_APWPARSNIEs_REQ:
	case eWNI_SME_CHNG_MCC_BEACON_INTERVAL:
#if defined WLAN_FEATURE_VOWIFI
	case eWNI_SME_NEIGHBOR_REPORT_REQ_IND:
	case eWNI_SME_BEACON_REPORT_RESP_XMIT_IND:
#endif
#if defined FEATURE_WLAN_ESE
	case eWNI_SME_ESE_ADJACENT_AP_REPORT:
#endif
#ifdef WLAN_FEATURE_VOWIFI_11R
	case eWNI_SME_FT_UPDATE_KEY:
	case eWNI_SME_FT_PRE_AUTH_REQ:
	case eWNI_SME_FT_AGGR_QOS_REQ:
#endif
	case eWNI_SME_REGISTER_MGMT_FRAME_REQ:
	case eWNI_SME_UPDATE_NOA:
	case eWNI_SME_CLEAR_DFS_CHANNEL_LIST:
	case eWNI_SME_GET_STATISTICS_REQ:
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
	case eWNI_SME_GET_TSM_STATS_REQ:
#endif  /* FEATURE_WLAN_ESE && FEATURE_WLAN_ESE_UPLOAD */
	case eWNI_SME_EXT_CHANGE_CHANNEL:
	/* These messages are from HDD.No need to respond to HDD */
		lim_process_normal_hdd_msg(mac_ctx, msg, false);
		break;

	case eWNI_PMC_SMPS_STATE_IND:
		if (msg->bodyptr) {
			cdf_mem_free(msg->bodyptr);
			msg->bodyptr = NULL;
		}
		break;
	case eWNI_SME_SEND_ACTION_FRAME_IND:
		lim_send_p2p_action_frame(mac_ctx, msg);
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_ABORT_REMAIN_ON_CHAN_IND:
		p2p_msg = (tSirMbMsgP2p *) msg->bodyptr;
		lim_abort_remain_on_chan(mac_ctx, p2p_msg->sessionId,
			p2p_msg->scan_id);
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_HAL_P2P_NOA_START_IND:
		session_entry = &mac_ctx->lim.gpSession[0];
		lim_log(mac_ctx, LOG1, "LIM received NOA start %x", msg->type);

		/* Since insert NOA is done and NOA start msg received,
		 * we should deactivate the Insert NOA timer
		 */
		lim_deactivate_and_change_timer(mac_ctx,
			eLIM_INSERT_SINGLESHOT_NOA_TIMER);

		for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
			session_entry = &mac_ctx->lim.gpSession[i];
			if ((session_entry != NULL) && (session_entry->valid) &&
				(session_entry->pePersona == CDF_P2P_GO_MODE)) {
				/* Save P2P NOA start attribute for Go persona*/
				p2p_go_exists = 1;
				cdf_mem_copy(&session_entry->p2pGoPsNoaStartInd,
					msg->bodyptr, sizeof(tSirP2PNoaStart));
				cdf_status =
					session_entry->p2pGoPsNoaStartInd.status;
				if (cdf_status != CDF_STATUS_SUCCESS)
					CDF_TRACE(CDF_MODULE_ID_PE, LOGW,
						FL(
						"GO NOA start status %d by FW"),
						cdf_status);
				break;
			}
		}

		if (p2p_go_exists == 0)
			CDF_TRACE(CDF_MODULE_ID_PE, LOGW,
				FL(
				"GO is removed by the time NOA start recvd"));

		/* We received the NOA start indication. Now we can send down
		 * the SME request which requires off-channel operation */
		lim_process_regd_defd_sme_req_after_noa_start(mac_ctx);
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#ifdef FEATURE_WLAN_TDLS
	case SIR_HAL_TDLS_IND:
		tdls_ind = (tpSirTdlsInd) msg->bodyptr;
		session_entry = pe_find_session_by_sta_id(mac_ctx,
			tdls_ind->staIdx, &session_id);
		if (session_entry == NULL) {
				lim_log(mac_ctx, LOG1,
					FL("No session exist for given bssId"));
				cdf_mem_free(msg->bodyptr);
				msg->bodyptr = NULL;
				return;
			}
		sta_ds = dph_get_hash_entry(mac_ctx, tdls_ind->assocId,
			&session_entry->dph.dphHashTable);
			if (sta_ds == NULL) {
				lim_log(mac_ctx, LOG1,
					FL("No sta_ds exist for given staId"));
				cdf_mem_free(msg->bodyptr);
				msg->bodyptr = NULL;
				return;
			}

			if (STA_ENTRY_TDLS_PEER == sta_ds->staType) {
				lim_log(mac_ctx, LOGE,
					FL("rcvd TDLS IND from FW with RC %d "),
					tdls_ind->reasonCode);
				lim_send_sme_tdls_del_sta_ind(mac_ctx, sta_ds,
					session_entry, tdls_ind->reasonCode);
			}
			cdf_mem_free(msg->bodyptr);
			msg->bodyptr = NULL;
		break;
#endif
	case SIR_HAL_P2P_NOA_ATTR_IND:
		session_entry = &mac_ctx->lim.gpSession[0];
		lim_log(mac_ctx, LOG1, FL("Received message Noa_ATTR %x"),
			msg->type);
		for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
			session_entry = &mac_ctx->lim.gpSession[i];
			if ((session_entry != NULL) && (session_entry->valid)
				&& (session_entry->pePersona ==
				CDF_P2P_GO_MODE)) { /* Save P2P attr for Go */
					cdf_mem_copy(
						&session_entry->p2pGoPsUpdate,
						msg->bodyptr,
						sizeof(tSirP2PNoaAttr));
					lim_log(mac_ctx, LOG2,
						FL("bssId"
						MAC_ADDRESS_STR
						" ctWin=%d oppPsFlag=%d"),
						MAC_ADDR_ARRAY(
							session_entry->bssId),
						session_entry->p2pGoPsUpdate.ctWin,
						session_entry->p2pGoPsUpdate.oppPsFlag);
					lim_log(mac_ctx, LOG2,
						FL
						(" uNoa1IntervalCnt=%d uNoa1Duration=%d uNoa1Interval=%d uNoa1StartTime=%d"),
						session_entry->p2pGoPsUpdate.uNoa1IntervalCnt,
						session_entry->p2pGoPsUpdate.uNoa1Duration,
						session_entry->p2pGoPsUpdate.uNoa1Interval,
						session_entry->p2pGoPsUpdate.uNoa1StartTime);
					break;
			}
		}
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_MISSED_BEACON_IND:
		lim_ps_offload_handle_missed_beacon_ind(mac_ctx, msg);
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case WMA_ROAM_OFFLOAD_SYNCH_IND:
		lim_roam_offload_synch_ind(mac_ctx, msg);
		/* bodyPtr is freed after handling
		 * eWNI_SME_ROAM_OFFLOAD_SYNCH_IND in sme_ProcessMsg */
		break;
#endif
	case SIR_LIM_ADDTS_RSP_TIMEOUT:
		lim_process_sme_req_messages(mac_ctx, msg);
		break;
#ifdef FEATURE_WLAN_ESE
	case SIR_LIM_ESE_TSM_TIMEOUT:
#ifndef FEATURE_WLAN_ESE_UPLOAD
		limProcessTsmTimeoutHandler(mac_ctx, msg);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
		break;
	case WMA_TSM_STATS_RSP:
#ifdef FEATURE_WLAN_ESE_UPLOAD
		lim_send_sme_pe_ese_tsm_rsp(mac_ctx,
			(tAniGetTsmStatsRsp *) msg->bodyptr);
#else
		limProcessHalEseTsmRsp(mac_ctx, msg);
#endif /* FEATURE_WLAN_ESE_UPLOAD */
		break;
#endif
	case WMA_ADD_TS_RSP:
		lim_process_hal_add_ts_rsp(mac_ctx, msg);
		break;
	case SIR_LIM_DEL_TS_IND:
		lim_process_del_ts_ind(mac_ctx, msg);
		break;
	case SIR_LIM_BEACON_GEN_IND:
		if (mac_ctx->lim.gLimSystemRole != eLIM_AP_ROLE)
			sch_process_pre_beacon_ind(mac_ctx, msg);
		break;
	case SIR_LIM_DELETE_STA_CONTEXT_IND:
		lim_delete_sta_context(mac_ctx, msg);
		break;
	case SIR_LIM_PERIODIC_PROBE_REQ_TIMEOUT:
	case SIR_LIM_JOIN_FAIL_TIMEOUT:
	case SIR_LIM_PERIODIC_JOIN_PROBE_REQ_TIMEOUT:
	case SIR_LIM_AUTH_FAIL_TIMEOUT:
	case SIR_LIM_AUTH_RSP_TIMEOUT:
	case SIR_LIM_ASSOC_FAIL_TIMEOUT:
	case SIR_LIM_REASSOC_FAIL_TIMEOUT:
#ifdef WLAN_FEATURE_VOWIFI_11R
	case SIR_LIM_FT_PREAUTH_RSP_TIMEOUT:
#endif
	case SIR_LIM_REMAIN_CHN_TIMEOUT:
	case SIR_LIM_INSERT_SINGLESHOT_NOA_TIMEOUT:
	case SIR_LIM_DISASSOC_ACK_TIMEOUT:
	case SIR_LIM_DEAUTH_ACK_TIMEOUT:
	case SIR_LIM_CONVERT_ACTIVE_CHANNEL_TO_PASSIVE:
		/* These timeout messages are handled by MLM sub module */
		lim_process_mlm_req_messages(mac_ctx, msg);
		break;
	case SIR_LIM_HEART_BEAT_TIMEOUT:
		/** check if heart beat failed, even if one Beacon
		 * is rcvd within the Heart Beat interval continue
		 * normal processing
		 */
		if (NULL == msg->bodyptr)
			lim_log(mac_ctx, LOGE,
				FL("Can't Process HB TO - bodyptr is Null"));
		else {
			session_entry = (tpPESession) msg->bodyptr;
			lim_log(mac_ctx, LOGE,
				FL
				("SIR_LIM_HEART_BEAT_TIMEOUT, Session %d"),
				((tpPESession) msg->bodyptr)->peSessionId);
			limResetHBPktCount(session_entry);
			lim_handle_heart_beat_timeout_for_session(mac_ctx,
							session_entry);
		}
		break;
	case SIR_LIM_PROBE_HB_FAILURE_TIMEOUT:
		lim_handle_heart_beat_failure_timeout(mac_ctx);
		break;
	case SIR_LIM_HASH_MISS_THRES_TIMEOUT:
		mac_ctx->lim.gLimDisassocFrameCredit = 0;
		break;
	case SIR_LIM_CNF_WAIT_TIMEOUT:
		/* Does not receive CNF or dummy packet */
		lim_handle_cnf_wait_timeout(mac_ctx, (uint16_t) msg->bodyval);
		break;
	case SIR_LIM_RETRY_INTERRUPT_MSG:
		/* Message from ISR upon TFP's max retry limit interrupt */
		break;
	case SIR_LIM_INV_KEY_INTERRUPT_MSG:
		/* Message from ISR upon SP's Invalid session key interrupt */
		break;
	case SIR_LIM_KEY_ID_INTERRUPT_MSG:
		/* Message from ISR upon SP's Invalid key ID interrupt */
		break;
	case SIR_LIM_REPLAY_THRES_INTERRUPT_MSG:
		/* Message from ISR upon SP's Replay threshold interrupt */
		break;
	case SIR_LIM_CHANNEL_SWITCH_TIMEOUT:
		lim_process_channel_switch_timeout(mac_ctx);
		break;
	case SIR_LIM_QUIET_TIMEOUT:
		lim_process_quiet_timeout(mac_ctx);
		break;
	case SIR_LIM_QUIET_BSS_TIMEOUT:
		lim_process_quiet_bss_timeout(mac_ctx);
		break;
	case SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT:
		lim_handle_update_olbc_cache(mac_ctx);
		break;
#ifdef FEATURE_WLAN_TDLS
	case SIR_HAL_TDLS_SHOULD_DISCOVER:
	case SIR_HAL_TDLS_SHOULD_TEARDOWN:
	case SIR_HAL_TDLS_PEER_DISCONNECTED:
		CDF_TRACE(CDF_MODULE_ID_PE, CDF_TRACE_LEVEL_INFO,
			("%s received tdls event: 0x%x"), __func__, msg->type);
		lim_send_sme_tdls_event_notify(mac_ctx, msg->type,
					(void *)msg->bodyptr);
		break;
#endif
	case WMA_ADD_BSS_RSP:
		lim_process_mlm_add_bss_rsp(mac_ctx, msg);
		break;
	case WMA_ADD_STA_RSP:
		lim_process_add_sta_rsp(mac_ctx, msg);
		break;
	case WMA_DELETE_STA_RSP:
		lim_process_mlm_del_sta_rsp(mac_ctx, msg);
		break;
	case WMA_DELETE_BSS_RSP:
		lim_handle_delete_bss_rsp(mac_ctx, msg);
		break;
	case WMA_CSA_OFFLOAD_EVENT:
		lim_handle_csa_offload_msg(mac_ctx, msg);
		break;
	case WMA_SET_BSSKEY_RSP:
	case WMA_SET_STA_BCASTKEY_RSP:
		lim_process_mlm_set_bss_key_rsp(mac_ctx, msg);
		break;
	case WMA_SET_STAKEY_RSP:
		lim_process_mlm_set_sta_key_rsp(mac_ctx, msg);
		break;
	case WMA_GET_STATISTICS_RSP:
		lim_send_sme_pe_statistics_rsp(mac_ctx, msg->type,
					     (void *)msg->bodyptr);
		break;
	case WMA_SET_MIMOPS_RSP:
	case WMA_SET_TX_POWER_RSP:
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_SET_MAX_TX_POWER_RSP:
#if defined WLAN_FEATURE_VOWIFI
		rrm_set_max_tx_power_rsp(mac_ctx, msg);
#endif
		if (msg->bodyptr != NULL) {
			cdf_mem_free((void *)msg->bodyptr);
			msg->bodyptr = NULL;
		}
		break;
	case SIR_LIM_ADDR2_MISS_IND:
		lim_log(mac_ctx, LOGE,
			FL("Addr2 mismatch interrupt received %X"), msg->type);
		/* message from HAL indicating addr2 mismatch interrupt occurred
		 * msg->bodyptr contains only pointer to 48-bit addr2 field
		 */
		cdf_mem_free((void *)(msg->bodyptr));
		msg->bodyptr = NULL;
		break;
#ifdef WLAN_FEATURE_VOWIFI_11R
	case WMA_AGGR_QOS_RSP:
		lim_process_ft_aggr_qo_s_rsp(mac_ctx, msg);
		break;
#endif
	case WMA_SET_LINK_STATE_RSP:
		link_state_param = (tLinkStateParams *) msg->bodyptr;
#if defined WLAN_FEATURE_VOWIFI_11R
		session_entry = link_state_param->session;
		if (link_state_param->ft
#if defined WLAN_FEATURE_ROAM_OFFLOAD
			&& !session_entry->bRoamSynchInProgress
#endif
		)
			lim_send_reassoc_req_with_ft_ies_mgmt_frame(mac_ctx,
				session_entry->pLimMlmReassocReq,
				session_entry);
#endif
		if (link_state_param->callback)
			link_state_param->callback(mac_ctx,
				link_state_param->callbackArg,
				link_state_param->status);
		cdf_mem_free((void *)(msg->bodyptr));
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_SET_BCN_FILTER_REQ:
		session_id = (uint8_t) msg->bodyval;
		session_entry = &mac_ctx->lim.gpSession[session_id];
		if ((session_entry != NULL) &&
			(lim_send_beacon_filter_info(mac_ctx, session_entry) !=
			 eSIR_SUCCESS))
			lim_log(mac_ctx, LOGE,
				FL("Failied to send Beacon Filter Info "));
		cdf_mem_free((void *)(msg->bodyptr));
		msg->bodyptr = NULL;
		break;
#ifdef FEATURE_WLAN_TDLS
	case WMA_SET_TDLS_LINK_ESTABLISH_REQ_RSP:
		tdls_link_params = (tTdlsLinkEstablishParams *) msg->bodyptr;
		session_entry = pe_find_session_by_sta_id(mac_ctx,
			tdls_link_params->staIdx, &session_id);
		if (session_entry == NULL) {
			lim_log(mac_ctx, LOGE,
				FL("session %u does not exist"), session_id);
			/* Still send the eWNI_SME_TDLS_LINK_ESTABLISH_RSP
			 * message to SME with session id as zero and status
			 * as FAILURE so, that message queued in SME queue
			 * can be freed to prevent the SME cmd buffer leak
			 */
			lim_send_sme_tdls_link_establish_req_rsp(mac_ctx, 0,
				NULL, NULL, eSIR_FAILURE);
		} else {
			lim_send_sme_tdls_link_establish_req_rsp(mac_ctx,
				session_entry->smeSessionId, NULL, NULL,
				tdls_link_params->status);
		}
		cdf_mem_free((void *)(msg->bodyptr));
		msg->bodyptr = NULL;
		break;
#endif
	case WMA_RX_SCAN_EVENT:
		lim_process_rx_scan_event(mac_ctx, msg->bodyptr);
		break;
	case WMA_IBSS_PEER_INACTIVITY_IND:
		lim_process_ibss_peer_inactivity(mac_ctx, msg->bodyptr);
		cdf_mem_free((void *)(msg->bodyptr));
		msg->bodyptr = NULL;
		break;
	case WMA_DFS_RADAR_IND:
		lim_send_sme_dfs_event_notify(mac_ctx, msg->type,
				(void *)msg->bodyptr);
		/* msg->bodyptr will be freed up by SME/CSR */
		break;
	case WMA_DFS_BEACON_TX_SUCCESS_IND:
		lim_process_beacon_tx_success_ind(mac_ctx, msg->type,
				(void *)msg->bodyptr);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case WMA_DISASSOC_TX_COMP:
		lim_disassoc_tx_complete_cnf(mac_ctx, msg->bodyval);
		break;
	case WMA_DEAUTH_TX_COMP:
		lim_deauth_tx_complete_cnf(mac_ctx, msg->bodyval);
		break;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	case WMA_UPDATE_Q2Q_IE_IND:
		cdf_mem_zero(&beacon_params, sizeof(tUpdateBeaconParams));
		beacon_params.paramChangeBitmap = 0;
		for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
			vdev_id = ((uint8_t *)msg->bodyptr)[i];
			session_entry = pe_find_session_by_sme_session_id(
				mac_ctx, vdev_id);
			if (session_entry == NULL)
				continue;
			session_entry->sap_advertise_avoid_ch_ie =
				(uint8_t)msg->bodyval;
			/*
			 * if message comes for DFS channel, no need to update:
			 * 1) We wont have MCC with DFS channels. so no need to
			 *    add Q2Q IE
			 * 2) We cannot end up in DFS channel SCC by channel
			 *    switch from non DFS MCC scenario, so no need to
			 *    remove Q2Q IE
			 * 3) There is however a case where device start MCC and
			 *    then user modifies hostapd.conf and does SAP
			 *    restart, in such a case, beacon params will be
			 *    reset and thus will not contain Q2Q IE, by default
			 */
			if (cds_get_channel_state(
					session_entry->currentOperChannel)
					!= CHANNEL_STATE_DFS) {
				beacon_params.bssIdx = session_entry->bssIdx;
				beacon_params.beaconInterval =
					session_entry->beaconParams.beaconInterval;
				beacon_params.paramChangeBitmap |=
					PARAM_BCN_INTERVAL_CHANGED;
				sch_set_fixed_beacon_fields(mac_ctx,
					session_entry);
				lim_send_beacon_params(mac_ctx, &beacon_params,
					session_entry);
			}
		}
		cdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	case eWNI_SME_NSS_UPDATE_REQ:
	case eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_CHANNEL_CHANGE_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_START_BEACON_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_UPDATE_ADDITIONAL_IES:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_MODIFY_ADDITIONAL_IES:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#ifdef QCA_HT_2040_COEX
	case eWNI_SME_SET_HT_2040_MODE:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
#endif
	case SIR_HAL_SOC_SET_HW_MODE_RESP:
		lim_process_set_hw_mode_resp(mac_ctx, msg->bodyptr);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_HAL_SOC_HW_MODE_TRANS_IND:
		lim_process_hw_mode_trans_ind(mac_ctx, msg->bodyptr);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case SIR_HAL_SOC_DUAL_MAC_CFG_RESP:
		lim_process_dual_mac_cfg_resp(mac_ctx, msg->bodyptr);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	case eWNI_SME_SET_IE_REQ:
		lim_process_sme_req_messages(mac_ctx, msg);
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		break;
	default:
		cdf_mem_free((void *)msg->bodyptr);
		msg->bodyptr = NULL;
		/* Unwanted messages */
		/* Log error */
		lim_log(mac_ctx, LOGE,
			FL("Discarding unexpected message received %X"),
			msg->type);
		lim_print_msg_name(mac_ctx, LOGE, msg->type);
		break;

	} /* switch (msg->type) */
} /*** end lim_process_messages() ***/

/**
 * lim_process_deferred_message_queue
 *
 ***FUNCTION:
 * This function is called by LIM while exiting from Learn
 * mode. This function fetches messages posted to the LIM
 * deferred message queue limDeferredMsgQ.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  pMac - Pointer to Global MAC structure
 * @return None
 */

void lim_process_deferred_message_queue(tpAniSirGlobal pMac)
{
	tSirMsgQ limMsg = { 0, 0, 0 };

	tSirMsgQ *readMsg;
	uint16_t size;

	/*
	** check any deferred messages need to be processed
	**/
	size = pMac->lim.gLimDeferredMsgQ.size;
	if (size > 0) {
		while ((readMsg = lim_read_deferred_msg_q(pMac)) != NULL) {
			cdf_mem_copy((uint8_t *) &limMsg,
				     (uint8_t *) readMsg, sizeof(tSirMsgQ));
			size--;
			lim_process_messages(pMac, &limMsg);

			if ((lim_is_system_in_scan_state(pMac))
			    || (true != GET_LIM_PROCESS_DEFD_MESGS(pMac))
			    || (pMac->lim.gLimSystemInScanLearnMode)
			    ||  pMac->lim.gLimAddtsSent)
				break;
		}
	}
} /*** end lim_process_deferred_message_queue() ***/

/**
 * lim_process_normal_hdd_msg() - Process the message and defer if needed
 * @mac_ctx :     Pointer to Global MAC structure
 * @msg     :     The message need to be processed
 * @rsp_reqd:     whether return result to hdd
 *
 * This function checks the current lim state and decide whether the message
 * passed will be deferred or not.
 *
 * Return: None
 */
static void lim_process_normal_hdd_msg(tpAniSirGlobal mac_ctx, tSirMsgQ *msg,
	uint8_t rsp_reqd)
{
	bool defer_msg = true;

	/* Added For BT-AMP Support */
	if ((mac_ctx->lim.gLimSystemRole == eLIM_AP_ROLE)
		|| (mac_ctx->lim.gLimSystemRole == eLIM_BT_AMP_AP_ROLE)
		|| (mac_ctx->lim.gLimSystemRole == eLIM_BT_AMP_STA_ROLE)
		|| (mac_ctx->lim.gLimSystemRole == eLIM_UNKNOWN_ROLE)) {
		/*
		 * This check is required only for the AP and in 2 cases.
		 * 1. If we are in learn mode and we receive any of these
		 * messages, you have to come out of scan and process the
		 * message, hence dont defer the message here. In handler,
		 * these message could be defered till we actually come out of
		 * scan mode.
		 * 2. If radar is detected, you might have to defer all of
		 * these messages except Stop BSS request/ Switch channel
		 * request. This decision is also made inside its handler.
		 *
		 * Please be careful while using the flag defer_msg. Possibly
		 * you might end up in an infinite loop.
		 */
		if ((msg->type == eWNI_SME_START_BSS_REQ) ||
			(msg->type == eWNI_SME_STOP_BSS_REQ) ||
			(msg->type == eWNI_SME_SWITCH_CHL_IND))
			defer_msg = false;
	}

	if (((mac_ctx->lim.gLimAddtsSent) ||
		(lim_is_system_in_scan_state(mac_ctx))) && defer_msg) {
		/*
		 * System is in DFS (Learn) mode or awaiting addts response or
		 * if radar is detected, Defer processsing this message
		 */
		if (lim_defer_msg(mac_ctx, msg) != TX_SUCCESS) {
#ifdef WLAN_DEBUG
			mac_ctx->lim.numSme++;
#endif
			lim_log_session_states(mac_ctx);
			/* Release body */
			cdf_mem_free(msg->bodyptr);
			msg->bodyptr = NULL;
		}
	} else {
		/*
		 * These messages are from HDD.Since these requests may also be
		 * generated internally within LIM module, need to distinquish
		 * and send response to host
		 */
		if (rsp_reqd)
			mac_ctx->lim.gLimRspReqd = true;
#ifdef WLAN_DEBUG
		mac_ctx->lim.numSme++;
#endif
		if (lim_process_sme_req_messages(mac_ctx, msg)) {
			/*
			 * Release body. limProcessSmeReqMessage consumed the
			 * buffer. We can free it.
			 */
			cdf_mem_free(msg->bodyptr);
			msg->bodyptr = NULL;
		}
	}
}

void
handle_ht_capabilityand_ht_info(struct sAniSirGlobal *pMac,
				tpPESession psessionEntry)
{
	tSirMacHTCapabilityInfo macHTCapabilityInfo;
	tSirMacHTParametersInfo macHTParametersInfo;
	tSirMacHTInfoField1 macHTInfoField1;
	tSirMacHTInfoField2 macHTInfoField2;
	tSirMacHTInfoField3 macHTInfoField3;
	uint32_t cfgValue;
	uint8_t *ptr;

	if (wlan_cfg_get_int(pMac, WNI_CFG_HT_CAP_INFO, &cfgValue) !=
	    eSIR_SUCCESS) {
		lim_log(pMac, LOGP,
			FL("Fail to retrieve WNI_CFG_HT_CAP_INFO value"));
		return;
	}
	ptr = (uint8_t *) &macHTCapabilityInfo;
	*((uint16_t *) ptr) = (uint16_t) (cfgValue & 0xffff);
	pMac->lim.gHTLsigTXOPProtection =
		(uint8_t) macHTCapabilityInfo.lsigTXOPProtection;
	pMac->lim.gHTMIMOPSState =
		(tSirMacHTMIMOPowerSaveState) macHTCapabilityInfo.mimoPowerSave;
	pMac->lim.gHTGreenfield = (uint8_t) macHTCapabilityInfo.greenField;
	pMac->lim.gHTMaxAmsduLength =
		(uint8_t) macHTCapabilityInfo.maximalAMSDUsize;
	pMac->lim.gHTShortGI20Mhz = (uint8_t) macHTCapabilityInfo.shortGI20MHz;
	pMac->lim.gHTShortGI40Mhz = (uint8_t) macHTCapabilityInfo.shortGI40MHz;
	pMac->lim.gHTPSMPSupport = (uint8_t) macHTCapabilityInfo.psmp;
	pMac->lim.gHTDsssCckRate40MHzSupport =
		(uint8_t) macHTCapabilityInfo.dsssCckMode40MHz;

	if (wlan_cfg_get_int(pMac, WNI_CFG_HT_AMPDU_PARAMS, &cfgValue) !=
	    eSIR_SUCCESS) {
		lim_log(pMac, LOGP,
			FL("Fail to retrieve WNI_CFG_HT_PARAM_INFO value"));
		return;
	}
	ptr = (uint8_t *) &macHTParametersInfo;
	*ptr = (uint8_t) (cfgValue & 0xff);
	pMac->lim.gHTAMpduDensity = (uint8_t) macHTParametersInfo.mpduDensity;
	pMac->lim.gHTMaxRxAMpduFactor =
		(uint8_t) macHTParametersInfo.maxRxAMPDUFactor;

	/* Get HT IE Info */
	if (wlan_cfg_get_int(pMac, WNI_CFG_HT_INFO_FIELD1, &cfgValue) !=
	    eSIR_SUCCESS) {
		lim_log(pMac, LOGP,
			FL("Fail to retrieve WNI_CFG_HT_INFO_FIELD1 value"));
		return;
	}
	ptr = (uint8_t *) &macHTInfoField1;
	*((uint8_t *) ptr) = (uint8_t) (cfgValue & 0xff);
	pMac->lim.gHTServiceIntervalGranularity =
		(uint8_t) macHTInfoField1.serviceIntervalGranularity;
	pMac->lim.gHTControlledAccessOnly =
		(uint8_t) macHTInfoField1.controlledAccessOnly;
	pMac->lim.gHTRifsMode = (uint8_t) macHTInfoField1.rifsMode;

	if (wlan_cfg_get_int(pMac, WNI_CFG_HT_INFO_FIELD2, &cfgValue) !=
	    eSIR_SUCCESS) {
		lim_log(pMac, LOGP,
			FL("Fail to retrieve WNI_CFG_HT_INFO_FIELD2 value"));
		return;
	}
	ptr = (uint8_t *) &macHTInfoField2;
	*((uint16_t *) ptr) = (uint16_t) (cfgValue & 0xffff);
	pMac->lim.gHTOperMode = (tSirMacHTOperatingMode) macHTInfoField2.opMode;

	if (wlan_cfg_get_int(pMac, WNI_CFG_HT_INFO_FIELD3, &cfgValue) !=
	    eSIR_SUCCESS) {
		lim_log(pMac, LOGP,
			FL("Fail to retrieve WNI_CFG_HT_INFO_FIELD3 value"));
		return;
	}
	ptr = (uint8_t *) &macHTInfoField3;
	*((uint16_t *) ptr) = (uint16_t) (cfgValue & 0xffff);
	pMac->lim.gHTPCOActive = (uint8_t) macHTInfoField3.pcoActive;
	pMac->lim.gHTPCOPhase = (uint8_t) macHTInfoField3.pcoPhase;
	pMac->lim.gHTSecondaryBeacon =
		(uint8_t) macHTInfoField3.secondaryBeacon;
	pMac->lim.gHTDualCTSProtection =
		(uint8_t) macHTInfoField3.dualCTSProtection;
	pMac->lim.gHTSTBCBasicMCS = (uint8_t) macHTInfoField3.basicSTBCMCS;

	/* The lim globals for channelwidth and secondary chnl have been removed and should not be used during no session;
	 * instead direct cfg is read and used when no session for transmission of mgmt frames (same as old);
	 * For now, we might come here during init and join with sessionEntry = NULL; in that case just fill the globals which exist
	 * Sessionized entries values will be filled in join or add bss req. The ones which are missed in join are filled below
	 */
	if (psessionEntry != NULL) {
		psessionEntry->htCapability =
			IS_DOT11_MODE_HT(psessionEntry->dot11mode);
		psessionEntry->beaconParams.fLsigTXOPProtectionFullSupport =
			(uint8_t) macHTInfoField3.lsigTXOPProtectionFullSupport;
	}
}

void lim_log_session_states(tpAniSirGlobal mac_ctx)
{
#ifdef WLAN_DEBUG
	int i;

	for (i = 0; i < mac_ctx->lim.maxBssId; i++) {
		if (mac_ctx->lim.gpSession[i].valid) {
			CDF_TRACE(CDF_MODULE_ID_PE, LOG1,
				FL("sysRole(%d) Session (%d)"),
				mac_ctx->lim.gLimSystemRole, i);
			CDF_TRACE(CDF_MODULE_ID_PE, LOG1,
				FL("SME: Curr %s,Prev %s,MLM: Curr %s,Prev %s"),
				lim_sme_state_str(
				mac_ctx->lim.gpSession[i].limSmeState),
				lim_sme_state_str(
				mac_ctx->lim.gpSession[i].limPrevSmeState),
				lim_mlm_state_str(
				mac_ctx->lim.gpSession[i].limMlmState),
				lim_mlm_state_str(
				mac_ctx->lim.gpSession[i].limPrevMlmState));
		}
	}
#endif
}
