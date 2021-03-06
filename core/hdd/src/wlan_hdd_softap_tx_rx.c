/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/* Include files */
#include <linux/semaphore.h>
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <cdf_types.h>
#include <ani_global.h>
#include <cdf_types.h>
#include <net/ieee80211_radiotap.h>
#include <cds_sched.h>
#include <wlan_hdd_napi.h>

#ifdef IPA_OFFLOAD
#include <wlan_hdd_ipa.h>
#endif

/* Preprocessor definitions and constants */
#undef QCA_HDD_SAP_DUMP_SK_BUFF

/* Type declarations */

/* Function definitions and documenation */
#ifdef QCA_HDD_SAP_DUMP_SK_BUFF
/**
 * hdd_softap_dump_sk_buff() - Dump an skb
 * @skb: skb to dump
 *
 * Return: None
 */
static void hdd_softap_dump_sk_buff(struct sk_buff *skb)
{
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: head = %p ", __func__, skb->head);
	/* CDF_TRACE( CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,"%s: data = %p ", __func__, skb->data); */
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: tail = %p ", __func__, skb->tail);
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: end = %p ", __func__, skb->end);
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: len = %d ", __func__, skb->len);
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: data_len = %d ", __func__, skb->data_len);
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: mac_len = %d", __func__, skb->mac_len);

	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x ", skb->data[0],
		  skb->data[1], skb->data[2], skb->data[3], skb->data[4],
		  skb->data[5], skb->data[6], skb->data[7]);
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", skb->data[8],
		  skb->data[9], skb->data[10], skb->data[11], skb->data[12],
		  skb->data[13], skb->data[14], skb->data[15]);
}
#else
static void hdd_softap_dump_sk_buff(struct sk_buff *skb)
{
}
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_softap_tx_resume_timer_expired_handler() - TX Q resume timer handler
 * @adapter_context: pointer to vdev adapter
 *
 * TX Q resume timer handler for SAP and P2P GO interface.  If Blocked
 * OS Q is not resumed during timeout period, to prevent permanent
 * stall, resume OS Q forcefully for SAP and P2P GO interface.
 *
 * Return: None
 */
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context)
{
	hdd_adapter_t *pAdapter = (hdd_adapter_t *) adapter_context;

	if (!pAdapter) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: INV ARG", __func__);
		/* INVALID ARG */
		return;
	}

	hddLog(LOG1, FL("Enabling queues"));
	wlan_hdd_netif_queue_control(pAdapter, WLAN_WAKE_ALL_NETIF_QUEUE,
				   WLAN_CONTROL_PATH);
	return;
}

/**
 * hdd_softap_tx_resume_cb() - Resume OS TX Q.
 * @adapter_context: pointer to vdev apdapter
 * @tx_resume: TX Q resume trigger
 *
 * Q was stopped due to WLAN TX path low resource condition
 *
 * Return: None
 */
void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume)
{
	hdd_adapter_t *pAdapter = (hdd_adapter_t *) adapter_context;

	if (!pAdapter) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: INV ARG", __func__);
		/* INVALID ARG */
		return;
	}

	/* Resume TX  */
	if (true == tx_resume) {
		if (CDF_TIMER_STATE_STOPPED !=
		    cdf_mc_timer_get_current_state(&pAdapter->
						   tx_flow_control_timer)) {
			cdf_mc_timer_stop(&pAdapter->tx_flow_control_timer);
		}

		hddLog(LOG1, FL("Enabling queues"));
		wlan_hdd_netif_queue_control(pAdapter,
					WLAN_WAKE_ALL_NETIF_QUEUE,
					WLAN_DATA_FLOW_CONTROL);
	}
	return;
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

/**
 * hdd_softap_hard_start_xmit() - Transmit a frame
 * @skb: pointer to OS packet (sk_buff)
 * @dev: pointer to network device
 *
 * Function registered with the Linux OS for transmitting
 * packets. This version of the function directly passes
 * the packet to Transport Layer.
 *
 * Return: Always returns NETDEV_TX_OK
 */
int hdd_softap_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
	sme_ac_enum_type ac = SME_AC_BE;
	hdd_adapter_t *pAdapter = (hdd_adapter_t *) netdev_priv(dev);
	hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	struct cdf_mac_addr *pDestMacAddress;
	uint8_t STAId;
	uint8_t proto_type = 0;
#ifdef QCA_PKT_PROTO_TRACE
	hdd_context_t *hddCtxt = (hdd_context_t *) pAdapter->pHddCtx;
#endif /* QCA_PKT_PROTO_TRACE */

	++pAdapter->hdd_stats.hddTxRxStats.txXmitCalled;
	/* Prevent this function from being called during SSR since TL
	 * context may not be reinitialized at this time which may
	 * lead to a crash.
	 */
	if (pHddCtx->isLogpInProgress) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: LOGP in Progress. Ignore!!!", __func__);
		goto drop_pkt;
	}

	/*
	 * If the device is operating on a DFS Channel
	 * then check if SAP is in CAC WAIT state and
	 * drop the packets. In CAC WAIT state device
	 * is expected not to transmit any frames.
	 * SAP starts Tx only after the BSS START is
	 * done.
	 */
	if (pHddApCtx->dfs_cac_block_tx) {
		goto drop_pkt;
	}

	pDestMacAddress = (struct cdf_mac_addr *) skb->data;

	if (cdf_is_macaddr_broadcast(pDestMacAddress) ||
	    cdf_is_macaddr_group(pDestMacAddress)) {
		/* The BC/MC station ID is assigned during BSS
		 * starting phase.  SAP will return the station ID
		 * used for BC/MC traffic.
		 */
		STAId = pHddApCtx->uBCStaId;
	} else {
		if (CDF_STATUS_SUCCESS !=
			 hdd_softap_get_sta_id(pAdapter,
				 pDestMacAddress, &STAId)) {
			CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA,
				  CDF_TRACE_LEVEL_WARN,
				  "%s: Failed to find right station", __func__);
			goto drop_pkt;
		}

		if (STAId == HDD_WLAN_INVALID_STA_ID) {
			CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA,
				  CDF_TRACE_LEVEL_WARN,
				  "%s: Failed to find right station", __func__);
			goto drop_pkt;
		} else if (false == pAdapter->aStaInfo[STAId].isUsed) {
			CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA,
				  CDF_TRACE_LEVEL_WARN,
				  "%s: STA %d is unregistered", __func__,
				  STAId);
			goto drop_pkt;
		}

		if ((ol_txrx_peer_state_conn !=
		     pAdapter->aStaInfo[STAId].tlSTAState)
		    && (ol_txrx_peer_state_auth !=
			pAdapter->aStaInfo[STAId].tlSTAState)) {
			CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA,
				  CDF_TRACE_LEVEL_WARN,
				  "%s: Station not connected yet", __func__);
			goto drop_pkt;
		} else if (ol_txrx_peer_state_conn ==
			   pAdapter->aStaInfo[STAId].tlSTAState) {
			if (ntohs(skb->protocol) != HDD_ETHERTYPE_802_1_X) {
				CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA,
					  CDF_TRACE_LEVEL_WARN,
					  "%s: NON-EAPOL packet in non-Authenticated state",
					  __func__);
				goto drop_pkt;
			}
		}
	}

	hdd_get_tx_resource(pAdapter, STAId,
			WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

	/* Get TL AC corresponding to Qdisc queue index/AC. */
	ac = hdd_qdisc_ac_to_tl_ac[skb->queue_mapping];
	++pAdapter->hdd_stats.hddTxRxStats.txXmitClassifiedAC[ac];

#if defined (IPA_OFFLOAD)
	if (!(NBUF_OWNER_ID(skb) == IPA_NBUF_OWNER_ID)) {
#endif
		/* Check if the buffer has enough header room */
		skb = skb_unshare(skb, GFP_ATOMIC);
		if (!skb)
			goto drop_pkt_accounting;

		if (skb_headroom(skb) < dev->hard_header_len) {
			struct sk_buff *tmp;
			tmp = skb;
			skb = skb_realloc_headroom(tmp, dev->hard_header_len);
			dev_kfree_skb(tmp);
			if (!skb)
				goto drop_pkt_accounting;
		}
#if defined (IPA_OFFLOAD)
	}
#endif

	wlan_hdd_log_eapol(skb,
			   WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED);

#ifdef QCA_PKT_PROTO_TRACE
	if ((hddCtxt->config->gEnableDebugLog & CDS_PKT_TRAC_TYPE_EAPOL) ||
	    (hddCtxt->config->gEnableDebugLog & CDS_PKT_TRAC_TYPE_DHCP)) {
		/* Proto Trace enabled */
		proto_type = cds_pkt_get_proto_type(skb,
						    hddCtxt->config->
						    gEnableDebugLog, 0);
		if (CDS_PKT_TRAC_TYPE_EAPOL & proto_type) {
			cds_pkt_trace_buf_update("HA:T:EPL");
		} else if (CDS_PKT_TRAC_TYPE_DHCP & proto_type) {
			cds_pkt_trace_buf_update("HA:T:DHC");
		}
	}
#endif /* QCA_PKT_PROTO_TRACE */
	pAdapter->stats.tx_bytes += skb->len;
	++pAdapter->stats.tx_packets;

	/* Zero out skb's context buffer for the driver to use */
	cdf_mem_set(skb->cb, sizeof(skb->cb), 0);
	NBUF_SET_PACKET_TRACK(skb, NBUF_TX_PKT_DATA_TRACK);
	NBUF_UPDATE_TX_PKT_COUNT(skb, NBUF_TX_PKT_HDD);

	cdf_dp_trace_set_track(skb);
	DPTRACE(cdf_dp_trace(skb, CDF_DP_TRACE_HDD_PACKET_PTR_RECORD,
				(uint8_t *)skb->data, sizeof(skb->data)));
	DPTRACE(cdf_dp_trace(skb, CDF_DP_TRACE_HDD_PACKET_RECORD,
				(uint8_t *)skb->data, cdf_nbuf_len(skb)));
	if (cdf_nbuf_len(skb) > CDF_DP_TRACE_RECORD_SIZE)
		DPTRACE(cdf_dp_trace(skb, CDF_DP_TRACE_HDD_PACKET_RECORD,
				(uint8_t *)&skb->data[CDF_DP_TRACE_RECORD_SIZE],
				(cdf_nbuf_len(skb)-CDF_DP_TRACE_RECORD_SIZE)));

	if (ol_tx_send_data_frame(STAId, skb,
							  proto_type) != NULL) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_WARN,
			  "%s: Failed to send packet to txrx for staid:%d",
			  __func__, STAId);
		++pAdapter->hdd_stats.hddTxRxStats.txXmitDroppedAC[ac];
		goto drop_pkt;
	}
	dev->trans_start = jiffies;

	return NETDEV_TX_OK;

drop_pkt:

	DPTRACE(cdf_dp_trace(skb, CDF_DP_TRACE_DROP_PACKET_RECORD,
				(uint8_t *)skb->data, cdf_nbuf_len(skb)));
	if (cdf_nbuf_len(skb) > CDF_DP_TRACE_RECORD_SIZE)
		DPTRACE(cdf_dp_trace(skb, CDF_DP_TRACE_DROP_PACKET_RECORD,
				(uint8_t *)&skb->data[CDF_DP_TRACE_RECORD_SIZE],
				(cdf_nbuf_len(skb)-CDF_DP_TRACE_RECORD_SIZE)));
	kfree_skb(skb);

drop_pkt_accounting:
	++pAdapter->stats.tx_dropped;
	++pAdapter->hdd_stats.hddTxRxStats.txXmitDropped;

	return NETDEV_TX_OK;
}

/**
 * __hdd_softap_tx_timeout() - TX timeout handler
 * @dev: pointer to network device
 *
 * This function is registered as a netdev ndo_tx_timeout method, and
 * is invoked by the kernel if the driver takes too long to transmit a
 * frame.
 *
 * Return: None
 */
static void __hdd_softap_tx_timeout(struct net_device *dev)
{
	hdd_adapter_t *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	hdd_context_t *hdd_ctx;

	DPTRACE(cdf_dp_trace(NULL, CDF_DP_TRACE_HDD_SOFTAP_TX_TIMEOUT,
				NULL, 0));
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
		  "%s: Transmission timeout occurred", __func__);
	/* Getting here implies we disabled the TX queues for too
	 * long. Queues are disabled either because of disassociation
	 * or low resource scenarios. In case of disassociation it is
	 * ok to ignore this. But if associated, we have do possible
	 * recovery here
	 */
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (hdd_ctx->isLogpInProgress) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			 "%s: LOGP in Progress. Ignore!!!", __func__);
		return;
	}
}

/**
 * hdd_softap_tx_timeout() - SSR wrapper for __hdd_softap_tx_timeout
 * @dev: pointer to net_device
 *
 * Return: none
 */
void hdd_softap_tx_timeout(struct net_device *dev)
{
	cds_ssr_protect(__func__);
	__hdd_softap_tx_timeout(dev);
	cds_ssr_unprotect(__func__);
}

/**
 * @hdd_softap_init_tx_rx() - Initialize Tx/RX module
 * @pAdapter: pointer to adapter context
 *
 * Return: CDF_STATUS_E_FAILURE if any errors encountered,
 *	   CDF_STATUS_SUCCESS otherwise
 */
CDF_STATUS hdd_softap_init_tx_rx(hdd_adapter_t *pAdapter)
{
	CDF_STATUS status = CDF_STATUS_SUCCESS;

	uint8_t STAId = 0;

	cdf_mem_zero(&pAdapter->stats, sizeof(struct net_device_stats));

	spin_lock_init(&pAdapter->staInfo_lock);

	for (STAId = 0; STAId < WLAN_MAX_STA_COUNT; STAId++) {
		cdf_mem_zero(&pAdapter->aStaInfo[STAId],
			     sizeof(hdd_station_info_t));
	}

	return status;
}

/**
 * @hdd_softap_deinit_tx_rx() - Deinitialize Tx/RX module
 * @pAdapter: pointer to adapter context
 *
 * Return: CDF_STATUS_E_FAILURE if any errors encountered,
 *	   CDF_STATUS_SUCCESS otherwise
 */
CDF_STATUS hdd_softap_deinit_tx_rx(hdd_adapter_t *pAdapter)
{
	CDF_STATUS status = CDF_STATUS_SUCCESS;

	return status;
}

/**
 * hdd_softap_init_tx_rx_sta() - Initialize tx/rx for a softap station
 * @pAdapter: pointer to adapter context
 * @STAId: Station ID to initialize
 * @pmacAddrSTA: pointer to the MAC address of the station
 *
 * Return: CDF_STATUS_E_FAILURE if any errors encountered,
 *	   CDF_STATUS_SUCCESS otherwise
 */
CDF_STATUS hdd_softap_init_tx_rx_sta(hdd_adapter_t *pAdapter, uint8_t STAId,
				     struct cdf_mac_addr *pmacAddrSTA)
{
	spin_lock_bh(&pAdapter->staInfo_lock);
	if (pAdapter->aStaInfo[STAId].isUsed) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Reinit station %d", __func__, STAId);
		spin_unlock_bh(&pAdapter->staInfo_lock);
		return CDF_STATUS_E_FAILURE;
	}

	cdf_mem_zero(&pAdapter->aStaInfo[STAId], sizeof(hdd_station_info_t));

	pAdapter->aStaInfo[STAId].isUsed = true;
	pAdapter->aStaInfo[STAId].isDeauthInProgress = false;
	cdf_copy_macaddr(&pAdapter->aStaInfo[STAId].macAddrSTA, pmacAddrSTA);

	spin_unlock_bh(&pAdapter->staInfo_lock);
	return CDF_STATUS_SUCCESS;
}

/**
 * hdd_softap_deinit_tx_rx_sta() - Deinitialize tx/rx for a softap station
 * @pAdapter: pointer to adapter context
 * @STAId: Station ID to deinitialize
 *
 * Return: CDF_STATUS_E_FAILURE if any errors encountered,
 *	   CDF_STATUS_SUCCESS otherwise
 */
CDF_STATUS hdd_softap_deinit_tx_rx_sta(hdd_adapter_t *pAdapter, uint8_t STAId)
{
	CDF_STATUS status = CDF_STATUS_SUCCESS;
	hdd_hostapd_state_t *pHostapdState;

	pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pAdapter);

	spin_lock_bh(&pAdapter->staInfo_lock);

	if (false == pAdapter->aStaInfo[STAId].isUsed) {
		spin_unlock_bh(&pAdapter->staInfo_lock);
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Deinit station not inited %d", __func__, STAId);
		return CDF_STATUS_E_FAILURE;
	}

	pAdapter->aStaInfo[STAId].isUsed = false;
	pAdapter->aStaInfo[STAId].isDeauthInProgress = false;

	spin_unlock_bh(&pAdapter->staInfo_lock);
	return status;
}

/**
 * hdd_softap_rx_packet_cbk() - Receive packet handler
 * @cds_context: pointer to CDS context
 * @rxBuf: pointer to rx cdf_nbuf
 * @staId: Station Id
 *
 * Receive callback registered with TL.  TL will call this to notify
 * the HDD when one or more packets were received for a registered
 * STA.
 *
 * Return: CDF_STATUS_E_FAILURE if any errors encountered,
 *	   CDF_STATUS_SUCCESS otherwise
 */
CDF_STATUS hdd_softap_rx_packet_cbk(void *cds_context,
				    cdf_nbuf_t rxBuf, uint8_t staId)
{
	hdd_adapter_t *pAdapter = NULL;
	int rxstat;
	unsigned int cpu_index;
	struct sk_buff *skb = NULL;
	hdd_context_t *pHddCtx = NULL;
#ifdef QCA_PKT_PROTO_TRACE
	uint8_t proto_type;
#endif /* QCA_PKT_PROTO_TRACE */

	/* Sanity check on inputs */
	if ((NULL == cds_context) || (NULL == rxBuf)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Null params being passed", __func__);
		return CDF_STATUS_E_FAILURE;
	}

	pHddCtx = cds_get_context(CDF_MODULE_ID_HDD);
	if (NULL == pHddCtx) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: HDD context is Null", __func__);
		return CDF_STATUS_E_FAILURE;
	}

	pAdapter = pHddCtx->sta_to_adapter[staId];
	if ((NULL == pAdapter) || (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic)) {
		hddLog(LOGE,
			FL("invalid adapter %p or adapter has invalid magic"),
			pAdapter);
		return CDF_STATUS_E_FAILURE;
	}

	/* walk the chain until all are processed */
	skb = (struct sk_buff *)rxBuf;

	hdd_softap_dump_sk_buff(skb);

	skb->dev = pAdapter->dev;

	if (skb->dev == NULL) {

		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: ERROR!!Invalid netdevice", __func__);
		return CDF_STATUS_E_FAILURE;
	}
	cpu_index = wlan_hdd_get_cpu();
	++pAdapter->hdd_stats.hddTxRxStats.rxPackets[cpu_index];
	++pAdapter->stats.rx_packets;
	pAdapter->stats.rx_bytes += skb->len;

	wlan_hdd_log_eapol(skb, WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);

#ifdef QCA_PKT_PROTO_TRACE
	if ((pHddCtx->config->gEnableDebugLog & CDS_PKT_TRAC_TYPE_EAPOL) ||
	    (pHddCtx->config->gEnableDebugLog & CDS_PKT_TRAC_TYPE_DHCP)) {
		proto_type = cds_pkt_get_proto_type(skb,
						    pHddCtx->config->
						    gEnableDebugLog, 0);
		if (CDS_PKT_TRAC_TYPE_EAPOL & proto_type) {
			cds_pkt_trace_buf_update("HA:R:EPL");
		} else if (CDS_PKT_TRAC_TYPE_DHCP & proto_type) {
			cds_pkt_trace_buf_update("HA:R:DHC");
		}
	}
#endif /* QCA_PKT_PROTO_TRACE */

	skb->protocol = eth_type_trans(skb, skb->dev);
#ifdef WLAN_FEATURE_HOLD_RX_WAKELOCK
	cdf_wake_lock_timeout_acquire(&pHddCtx->rx_wake_lock,
				      HDD_WAKE_LOCK_DURATION,
				      WIFI_POWER_EVENT_WAKELOCK_HOLD_RX);
#endif

	/* Remove SKB from internal tracking table before submitting
	 * it to stack
	 */
	cdf_net_buf_debug_release_skb(rxBuf);

	if (hdd_napi_enabled(HDD_NAPI_ANY))
		rxstat = netif_receive_skb(skb);
	else
		rxstat = netif_rx_ni(skb);
	if (NET_RX_SUCCESS == rxstat) {
		++pAdapter->hdd_stats.hddTxRxStats.rxDelivered[cpu_index];
	} else {
		++pAdapter->hdd_stats.hddTxRxStats.rxRefused[cpu_index];
	}

	pAdapter->dev->last_rx = jiffies;

	return CDF_STATUS_SUCCESS;
}

/**
 * hdd_softap_deregister_sta(hdd_adapter_t *pAdapter, uint8_t staId)
 * @pAdapter: pointer to adapter context
 * @staId: Station ID to deregister
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_* on error
 */
CDF_STATUS hdd_softap_deregister_sta(hdd_adapter_t *pAdapter, uint8_t staId)
{
	CDF_STATUS cdf_status = CDF_STATUS_SUCCESS;
	hdd_context_t *pHddCtx;

	if (NULL == pAdapter) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: pAdapter is NULL", __func__);
		return CDF_STATUS_E_INVAL;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != pAdapter->magic) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid pAdapter magic", __func__);
		return CDF_STATUS_E_INVAL;
	}

	pHddCtx = (hdd_context_t *) (pAdapter->pHddCtx);
	/* Clear station in TL and then update HDD data
	 * structures. This helps to block RX frames from other
	 * station to this station.
	 */
	cdf_status = ol_txrx_clear_peer(staId);
	if (!CDF_IS_STATUS_SUCCESS(cdf_status)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "ol_txrx_clear_peer() failed to for staID %d.  "
			  "Status= %d [0x%08X]", staId, cdf_status, cdf_status);
	}

	if (pAdapter->aStaInfo[staId].isUsed) {
		spin_lock_bh(&pAdapter->staInfo_lock);
		cdf_mem_zero(&pAdapter->aStaInfo[staId],
			     sizeof(hdd_station_info_t));
		spin_unlock_bh(&pAdapter->staInfo_lock);
	}
	pHddCtx->sta_to_adapter[staId] = NULL;

	return cdf_status;
}

/**
 * hdd_softap_register_sta() - Register a SoftAP STA
 * @pAdapter: pointer to adapter context
 * @fAuthRequired: is additional authentication required?
 * @fPrivacyBit: should 802.11 privacy bit be set?
 * @staId: station ID assigned to this station
 * @ucastSig: unicast security signature
 * @bcastSig: broadcast security signature
 * @pPeerMacAddress: station MAC address
 * @fWmmEnabled: is WMM enabled for this STA?
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_* on error
 */
CDF_STATUS hdd_softap_register_sta(hdd_adapter_t *pAdapter,
				   bool fAuthRequired,
				   bool fPrivacyBit,
				   uint8_t staId,
				   uint8_t ucastSig,
				   uint8_t bcastSig,
				   struct cdf_mac_addr *pPeerMacAddress,
				   bool fWmmEnabled)
{
	CDF_STATUS cdf_status = CDF_STATUS_E_FAILURE;
	struct ol_txrx_desc_type staDesc = { 0 };
	hdd_context_t *pHddCtx = pAdapter->pHddCtx;

	/*
	 * Clean up old entry if it is not cleaned up properly
	 */
	if (pAdapter->aStaInfo[staId].isUsed) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
			  "clean up old entry for STA %d", staId);
		hdd_softap_deregister_sta(pAdapter, staId);
	}
	/* Get the Station ID from the one saved during the assocation. */

	staDesc.sta_id = staId;

	/*Save the pAdapter Pointer for this staId */
	pHddCtx->sta_to_adapter[staId] = pAdapter;

	cdf_status =
		hdd_softap_init_tx_rx_sta(pAdapter, staId,
					  pPeerMacAddress);

	staDesc.is_qos_enabled = fWmmEnabled;
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
		  "HDD SOFTAP register TL QoS_enabled=%d",
		  staDesc.is_qos_enabled);


	cdf_status =
		ol_txrx_register_peer(hdd_softap_rx_packet_cbk,
					   &staDesc);
	if (!CDF_IS_STATUS_SUCCESS(cdf_status)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "SOFTAP ol_txrx_register_peer() failed to register.  Status= %d [0x%08X]",
			  cdf_status, cdf_status);
		return cdf_status;
	}

	/* if ( WPA ), tell TL to go to 'connected' and after keys come to the
	 * driver then go to 'authenticated'.  For all other authentication
	 * types (those that do not require upper layer authentication) we can
	 * put TL directly into 'authenticated' state
	 */

	pAdapter->aStaInfo[staId].ucSTAId = staId;
	pAdapter->aStaInfo[staId].isQosEnabled = fWmmEnabled;

	if (!fAuthRequired) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
			  "open/shared auth StaId= %d.  Changing TL state to AUTHENTICATED at Join time",
			  pAdapter->aStaInfo[staId].ucSTAId);

		/* Connections that do not need Upper layer auth,
		 * transition TL directly to 'Authenticated' state.
		 */
		cdf_status = hdd_change_peer_state(pAdapter, staDesc.sta_id,
						ol_txrx_peer_state_auth, false);

		pAdapter->aStaInfo[staId].tlSTAState = ol_txrx_peer_state_auth;
		pAdapter->sessionCtx.ap.uIsAuthenticated = true;
	} else {

		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
			  "ULA auth StaId= %d.  Changing TL state to CONNECTED at Join time",
			  pAdapter->aStaInfo[staId].ucSTAId);

		cdf_status = hdd_change_peer_state(pAdapter, staDesc.sta_id,
						ol_txrx_peer_state_conn, false);
		pAdapter->aStaInfo[staId].tlSTAState = ol_txrx_peer_state_conn;

		pAdapter->sessionCtx.ap.uIsAuthenticated = false;

	}

	/* Enable Tx queue */
	hddLog(LOG1, FL("Enabling queues"));
	wlan_hdd_netif_queue_control(pAdapter,
				   WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
				   WLAN_CONTROL_PATH);

	return cdf_status;
}

/**
 * hdd_softap_register_bc_sta() - Register the SoftAP broadcast STA
 * @pAdapter: pointer to adapter context
 * @fPrivacyBit: should 802.11 privacy bit be set?
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_* on error
 */
CDF_STATUS hdd_softap_register_bc_sta(hdd_adapter_t *pAdapter,
				      bool fPrivacyBit)
{
	CDF_STATUS cdf_status = CDF_STATUS_E_FAILURE;
	hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
	struct cdf_mac_addr broadcastMacAddr =
					CDF_MAC_ADDR_BROADCAST_INITIALIZER;
	hdd_ap_ctx_t *pHddApCtx;

	pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pAdapter);

	pHddCtx->sta_to_adapter[WLAN_RX_BCMC_STA_ID] = pAdapter;
#ifdef WLAN_FEATURE_MBSSID
	pHddCtx->sta_to_adapter[pHddApCtx->uBCStaId] = pAdapter;
#else
	pHddCtx->sta_to_adapter[WLAN_RX_SAP_SELF_STA_ID] = pAdapter;
#endif
	cdf_status =
		hdd_softap_register_sta(pAdapter, false, fPrivacyBit,
					(WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->
					uBCStaId, 0, 1, &broadcastMacAddr, 0);

	return cdf_status;
}

/**
 * hdd_softap_deregister_bc_sta() - Deregister the SoftAP broadcast STA
 * @pAdapter: pointer to adapter context
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_* on error
 */
CDF_STATUS hdd_softap_deregister_bc_sta(hdd_adapter_t *pAdapter)
{
	return hdd_softap_deregister_sta(pAdapter,
					 (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->
					 uBCStaId);
}

/**
 * hdd_softap_stop_bss() - Stop the BSS
 * @pAdapter: pointer to adapter context
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_* on error
 */
CDF_STATUS hdd_softap_stop_bss(hdd_adapter_t *pAdapter)
{
	CDF_STATUS cdf_status = CDF_STATUS_E_FAILURE;
	uint8_t staId = 0;
	hdd_context_t *pHddCtx;
	pHddCtx = WLAN_HDD_GET_CTX(pAdapter);

	/* bss deregister is not allowed during wlan driver loading or
	 * unloading
	 */
	if ((pHddCtx->isLoadInProgress) || (pHddCtx->isUnloadInProgress)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s:Loading_unloading in Progress. Ignore!!!",
			  __func__);
		return CDF_STATUS_E_PERM;
	}

	cdf_status = hdd_softap_deregister_bc_sta(pAdapter);

	if (!CDF_IS_STATUS_SUCCESS(cdf_status)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Failed to deregister BC sta Id %d", __func__,
			  (WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->uBCStaId);
	}

	for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++) {
		/* This excludes BC sta as it is already deregistered */
		if (pAdapter->aStaInfo[staId].isUsed) {
			cdf_status = hdd_softap_deregister_sta(pAdapter, staId);
			if (!CDF_IS_STATUS_SUCCESS(cdf_status)) {
				CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA,
					  CDF_TRACE_LEVEL_ERROR,
					  "%s: Failed to deregister sta Id %d",
					  __func__, staId);
			}
		}
	}
	return cdf_status;
}

/**
 * hdd_softap_change_sta_state() - Change the state of a SoftAP station
 * @pAdapter: pointer to adapter context
 * @pDestMacAddress: MAC address of the station
 * @state: new state of the station
 *
 * Return: CDF_STATUS_SUCCESS on success, CDF_STATUS_E_* on error
 */
CDF_STATUS hdd_softap_change_sta_state(hdd_adapter_t *pAdapter,
				       struct cdf_mac_addr *pDestMacAddress,
				       enum ol_txrx_peer_state state)
{
	uint8_t ucSTAId = WLAN_MAX_STA_COUNT;
	CDF_STATUS cdf_status = CDF_STATUS_SUCCESS;

	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
		  "%s: enter", __func__);

	if (CDF_STATUS_SUCCESS !=
		 hdd_softap_get_sta_id(pAdapter,
			 pDestMacAddress, &ucSTAId)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Failed to find right station", __func__);
		return CDF_STATUS_E_FAILURE;
	}

	if (false ==
	    cdf_is_macaddr_equal(&pAdapter->aStaInfo[ucSTAId].macAddrSTA,
				 pDestMacAddress)) {
		CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_ERROR,
			  "%s: Station MAC address does not matching",
			  __func__);
		return CDF_STATUS_E_FAILURE;
	}

	cdf_status =
		hdd_change_peer_state(pAdapter, ucSTAId, state, false);
	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
		  "%s: change station to state %d succeed", __func__, state);

	if (CDF_STATUS_SUCCESS == cdf_status) {
		pAdapter->aStaInfo[ucSTAId].tlSTAState =
			ol_txrx_peer_state_auth;
	}

	CDF_TRACE(CDF_MODULE_ID_HDD_SAP_DATA, CDF_TRACE_LEVEL_INFO,
		  "%s exit", __func__);

	return cdf_status;
}

/*
 * hdd_softap_get_sta_id() - Find station ID from MAC address
 * @pAdapter: pointer to adapter context
 * @pDestMacAddress: MAC address of the destination
 * @staId: Station ID associated with the MAC address
 *
 * Return: CDF_STATUS_SUCCESS if a match was found, in which case
 *	   staId is populated, CDF_STATUS_E_FAILURE if a match is
 *	   not found
 */
CDF_STATUS hdd_softap_get_sta_id(hdd_adapter_t *pAdapter,
				 struct cdf_mac_addr *pMacAddress,
				 uint8_t *staId)
{
	uint8_t i;

	for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
		if (cdf_mem_compare
			(&pAdapter->aStaInfo[i].macAddrSTA, pMacAddress,
			CDF_MAC_ADDR_SIZE) && pAdapter->aStaInfo[i].isUsed) {
			*staId = i;
			return CDF_STATUS_SUCCESS;
		}
	}

	return CDF_STATUS_E_FAILURE;
}
