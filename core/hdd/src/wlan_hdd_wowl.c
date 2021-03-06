/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

/**
 * @file wlan_hdd_wowl.c
 *
 * @brief wake up on WLAN API file
 */

/* Include Files */

#include <wlan_hdd_includes.h>
#include <wlan_hdd_wowl.h>

/* Preprocessor Definitions and Constants */
#define WOWL_INTER_PTRN_TOKENIZER   ';'
#define WOWL_INTRA_PTRN_TOKENIZER   ':'

/* Type Declarations */

static char *g_hdd_wowl_ptrns[WOWL_MAX_PTRNS_ALLOWED];
static bool g_hdd_wowl_ptrns_debugfs[WOWL_MAX_PTRNS_ALLOWED] = { 0 };

static uint8_t g_hdd_wowl_ptrns_count;

static inline int find_ptrn_len(const char *ptrn)
{
	int len = 0;
	while (*ptrn != '\0' && *ptrn != WOWL_INTER_PTRN_TOKENIZER) {
		len++;
		ptrn++;
	}
	return len;
}

static void hdd_wowl_callback(void *pContext, CDF_STATUS cdf_ret_status)
{
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: Return code = (%d)", __func__, cdf_ret_status);
}

#ifdef WLAN_WAKEUP_EVENTS
static void hdd_wowl_wake_indication_callback(void *pContext,
		tpSirWakeReasonInd wake_reason_ind)
{
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO, "%s: Wake Reason %d",
		  __func__, wake_reason_ind->ulReason);
	hdd_exit_wowl((hdd_adapter_t *) pContext);
}
#endif

/**
 * dump_hdd_wowl_ptrn() - log wow patterns
 * @ptrn: pointer to wow pattern
 *
 * Return: none
 */
static void dump_hdd_wowl_ptrn(struct wow_add_pattern *ptrn)
{
	int i;

	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: Patetrn Id = 0x%x", __func__, ptrn->pattern_id);
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: Pattern Byte Offset = 0x%x", __func__,
		  ptrn->pattern_byte_offset);
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: pattern_size = 0x%x", __func__, ptrn->pattern_size);
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO,
		  "%s: pattern_mask_size = 0x%x", __func__,
		  ptrn->pattern_mask_size);
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO, "%s: Pattern: ",
		  __func__);
	for (i = 0; i < ptrn->pattern_size; i++)
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO, " %02X",
			  ptrn->pattern[i]);
	CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO, "%s: pattern_mask: ",
		  __func__);
	for (i = 0; i < ptrn->pattern_mask_size; i++)
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_INFO, "%02X",
			  ptrn->pattern_mask[i]);
}

/**
 * hdd_add_wowl_ptrn() - Function which will add the WoWL pattern to be
 *			 used when PBM filtering is enabled
 * @pAdapter: pointer to the adapter
 * @ptrn: pointer to the pattern string to be added
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_add_wowl_ptrn(hdd_adapter_t *pAdapter, const char *ptrn)
{
	struct wow_add_pattern localPattern;
	int i, first_empty_slot, len, offset;
	CDF_STATUS cdf_ret_status;
	const char *temp;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	uint8_t sessionId = pAdapter->sessionId;
	hdd_context_t *pHddCtx = pAdapter->pHddCtx;

	len = find_ptrn_len(ptrn);

	/* There has to have at least 1 byte for each field (pattern
	 * size, mask size, pattern, mask) e.g. PP:QQ:RR:SS ==> 11
	 * chars
	 */
	while (len >= 11) {
		/* Detect duplicate pattern */
		for (i = 0; i < pHddCtx->config->maxWoWFilters; i++) {
			if (g_hdd_wowl_ptrns[i] == NULL)
				continue;

			if (!memcmp(ptrn, g_hdd_wowl_ptrns[i], len)) {
				/* Pattern Already configured, skip to
				 * next pattern
				 */
				CDF_TRACE(CDF_MODULE_ID_HDD,
					  CDF_TRACE_LEVEL_ERROR,
					  "Trying to add duplicate WoWL pattern. Skip it!");
				ptrn += len;
				goto next_ptrn;
			}
		}

		first_empty_slot = -1;

		/* Find an empty slot to store the pattern */
		for (i = 0; i < pHddCtx->config->maxWoWFilters; i++) {
			if (g_hdd_wowl_ptrns[i] == NULL) {
				first_empty_slot = i;
				break;
			}
		}

		/* Maximum number of patterns have been configured already */
		if (first_empty_slot == -1) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Cannot add anymore patterns. No free slot!",
				  __func__);
			return false;
		}
		/* Validate the pattern */
		if (ptrn[2] != WOWL_INTRA_PTRN_TOKENIZER ||
		    ptrn[5] != WOWL_INTRA_PTRN_TOKENIZER) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Malformed pattern string. Skip!",
				  __func__);
			ptrn += len;
			goto next_ptrn;
		}
		/* Extract the pattern size */
		localPattern.pattern_size =
			(hex_to_bin(ptrn[0]) * 0x10) +
						hex_to_bin(ptrn[1]);

		/* Extract the pattern mask size */
		localPattern.pattern_mask_size =
			(hex_to_bin(ptrn[3]) * 0x10) +
						hex_to_bin(ptrn[4]);

		if (localPattern.pattern_size > SIR_WOWL_BCAST_PATTERN_MAX_SIZE
		    || localPattern.pattern_mask_size >
		    WOWL_PTRN_MASK_MAX_SIZE) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Invalid length specified. Skip!",
				  __func__);
			ptrn += len;
			goto next_ptrn;
		}
		/* compute the offset of tokenizer after the pattern */
		offset = 5 + 2 * localPattern.pattern_size + 1;
		if ((offset >= len) ||
		    (ptrn[offset] != WOWL_INTRA_PTRN_TOKENIZER)) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Malformed pattern string..skip!",
				  __func__);
			ptrn += len;
			goto next_ptrn;
		}
		/* compute the end of pattern sring */
		offset = offset + 2 * localPattern.pattern_mask_size;
		if (offset + 1 != len) {
			/* offset begins with 0 */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: Malformed pattern string...skip!",
				  __func__);
			ptrn += len;
			goto next_ptrn;
		}

		temp = ptrn;

		/* Now advance to where pattern begins */
		ptrn += 6;

		/* Extract the pattern */
		for (i = 0; i < localPattern.pattern_size; i++) {
			localPattern.pattern[i] =
				(hex_to_bin(ptrn[0]) * 0x10) +
				hex_to_bin(ptrn[1]);
			ptrn += 2;      /* skip to next byte */
		}

		/* Skip over the ':' seperator after the pattern */
		ptrn++;

		/* Extract the pattern Mask */
		for (i = 0; i < localPattern.pattern_mask_size; i++) {
			localPattern.pattern_mask[i] =
				(hex_to_bin(ptrn[0]) * 0x10) +
				hex_to_bin(ptrn[1]);
			ptrn += 2;      /* skip to next byte */
		}

		/* All is good. Store the pattern locally */
		g_hdd_wowl_ptrns[first_empty_slot] =
			kmalloc(len + 1, GFP_KERNEL);
		if (g_hdd_wowl_ptrns[first_empty_slot] == NULL) {
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "%s: kmalloc failure", __func__);
			return false;
		}

		memcpy(g_hdd_wowl_ptrns[first_empty_slot], temp, len);
		g_hdd_wowl_ptrns[first_empty_slot][len] = '\0';
		localPattern.pattern_id = first_empty_slot;
		localPattern.pattern_byte_offset = 0;
		localPattern.session_id = sessionId;

		/* Register the pattern downstream */
		cdf_ret_status =
			sme_wow_add_pattern(hHal, &localPattern,
						   sessionId);
		if (!CDF_IS_STATUS_SUCCESS(cdf_ret_status)) {
			/* Add failed, so invalidate the local storage */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "sme_wowl_add_bcast_pattern failed with error code (%d)",
				  cdf_ret_status);
			kfree(g_hdd_wowl_ptrns[first_empty_slot]);
			g_hdd_wowl_ptrns[first_empty_slot] = NULL;
		}

		dump_hdd_wowl_ptrn(&localPattern);

next_ptrn:
		if (*ptrn == WOWL_INTER_PTRN_TOKENIZER) {
			/* move past the tokenizer */
			ptrn += 1;
			len = find_ptrn_len(ptrn);
			continue;
		} else
			break;
	}

	return true;
}

/**
 * hdd_del_wowl_ptrn() - Function which will remove a WoWL pattern
 * @pAdapter: pointer to the adapter
 * @ptrn: pointer to the pattern string to be removed
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_del_wowl_ptrn(hdd_adapter_t *pAdapter, const char *ptrn)
{
	struct wow_delete_pattern delPattern;
	unsigned char id;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	bool patternFound = false;
	CDF_STATUS cdf_ret_status;
	uint8_t sessionId = pAdapter->sessionId;
	hdd_context_t *pHddCtx = pAdapter->pHddCtx;

	/* Detect pattern */
	for (id = 0;
	     id < pHddCtx->config->maxWoWFilters
	     && g_hdd_wowl_ptrns[id] != NULL; id++) {
		if (!strcmp(ptrn, g_hdd_wowl_ptrns[id])) {
			patternFound = true;
			break;
		}
	}

	/* If pattern present, remove it from downstream */
	if (patternFound) {
		delPattern.pattern_id = id;
		delPattern.session_id = sessionId;
		cdf_ret_status =
			sme_wow_delete_pattern(hHal, &delPattern,
						   sessionId);
		if (CDF_IS_STATUS_SUCCESS(cdf_ret_status)) {
			/* Remove from local storage as well */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "Deleted pattern with id %d [%s]", id,
				  g_hdd_wowl_ptrns[id]);

			kfree(g_hdd_wowl_ptrns[id]);
			g_hdd_wowl_ptrns[id] = NULL;
			return true;
		}
	}
	return false;
}

/**
 * hdd_add_wowl_ptrn_debugfs() - Function which will add a WoW pattern
 *				 sent from debugfs interface
 * @pAdapter: pointer to the adapter
 * @pattern_idx: index of the pattern to be added
 * @pattern_offset: offset of the pattern in the frame payload
 * @pattern_buf: pointer to the pattern hex string to be added
 * @pattern_mask: pointer to the pattern mask hex string
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_add_wowl_ptrn_debugfs(hdd_adapter_t *pAdapter, uint8_t pattern_idx,
			       uint8_t pattern_offset, char *pattern_buf,
			       char *pattern_mask)
{
	struct wow_add_pattern localPattern;
	CDF_STATUS cdf_ret_status;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	uint8_t session_id = pAdapter->sessionId;
	uint16_t pattern_len, mask_len, i;

	if (pattern_idx > (WOWL_MAX_PTRNS_ALLOWED - 1)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: WoW pattern index %d is out of range (0 ~ %d).",
			  __func__, pattern_idx, WOWL_MAX_PTRNS_ALLOWED - 1);

		return false;
	}

	pattern_len = strlen(pattern_buf);

	/* Since the pattern is a hex string, 2 characters represent 1 byte. */
	if (pattern_len % 2) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Malformed WoW pattern!", __func__);

		return false;
	} else
		pattern_len >>= 1;

	if (!pattern_len || pattern_len > WOWL_PTRN_MAX_SIZE) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: WoW pattern length %d is out of range (1 ~ %d).",
			  __func__, pattern_len, WOWL_PTRN_MAX_SIZE);

		return false;
	}

	localPattern.pattern_id = pattern_idx;
	localPattern.pattern_byte_offset = pattern_offset;
	localPattern.pattern_size = pattern_len;
	localPattern.session_id = session_id;

	if (localPattern.pattern_size > SIR_WOWL_BCAST_PATTERN_MAX_SIZE) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: WoW pattern size (%d) greater than max (%d)",
			  __func__, localPattern.pattern_size,
			  SIR_WOWL_BCAST_PATTERN_MAX_SIZE);
		return false;
	}
	/* Extract the pattern */
	for (i = 0; i < localPattern.pattern_size; i++) {
		localPattern.pattern[i] =
			(hex_to_bin(pattern_buf[0]) << 4) +
			hex_to_bin(pattern_buf[1]);

		/* Skip to next byte */
		pattern_buf += 2;
	}

	/* Get pattern mask size by pattern length */
	localPattern.pattern_mask_size = pattern_len >> 3;
	if (pattern_len % 8)
		localPattern.pattern_mask_size += 1;

	mask_len = strlen(pattern_mask);
	if ((mask_len % 2)
	    || (localPattern.pattern_mask_size != (mask_len >> 1))) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: Malformed WoW pattern mask!", __func__);

		return false;
	}
	if (localPattern.pattern_mask_size > WOWL_PTRN_MASK_MAX_SIZE) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: WoW pattern mask size (%d) greater than max (%d)",
			  __func__, localPattern.pattern_mask_size,
			  WOWL_PTRN_MASK_MAX_SIZE);
		return false;
	}
	/* Extract the pattern mask */
	for (i = 0; i < localPattern.pattern_mask_size; i++) {
		localPattern.pattern_mask[i] =
			(hex_to_bin(pattern_mask[0]) << 4) +
			hex_to_bin(pattern_mask[1]);

		/* Skip to next byte */
		pattern_mask += 2;
	}

	/* Register the pattern downstream */
	cdf_ret_status =
		sme_wow_add_pattern(hHal, &localPattern, session_id);

	if (!CDF_IS_STATUS_SUCCESS(cdf_ret_status)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: sme_wowl_add_bcast_pattern failed with error code (%d).",
			  __func__, cdf_ret_status);

		return false;
	}

	/* All is good. */
	if (!g_hdd_wowl_ptrns_debugfs[pattern_idx]) {
		g_hdd_wowl_ptrns_debugfs[pattern_idx] = 1;
		g_hdd_wowl_ptrns_count++;
	}

	dump_hdd_wowl_ptrn(&localPattern);

	return true;
}

/**
 * hdd_del_wowl_ptrn_debugfs() - Function which will remove a WoW pattern
 *				 sent from debugfs interface
 * @pAdapter: pointer to the adapter
 * @pattern_idx: index of the pattern to be removed
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_del_wowl_ptrn_debugfs(hdd_adapter_t *pAdapter, uint8_t pattern_idx)
{
	struct wow_delete_pattern delPattern;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	CDF_STATUS cdf_ret_status;
	uint8_t sessionId = pAdapter->sessionId;

	if (pattern_idx > (WOWL_MAX_PTRNS_ALLOWED - 1)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: WoW pattern index %d is not in the range (0 ~ %d).",
			  __func__, pattern_idx, WOWL_MAX_PTRNS_ALLOWED - 1);

		return false;
	}

	if (!g_hdd_wowl_ptrns_debugfs[pattern_idx]) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: WoW pattern %d is not in the table.",
			  __func__, pattern_idx);

		return false;
	}

	delPattern.pattern_id = pattern_idx;
	delPattern.session_id = sessionId;
	cdf_ret_status = sme_wow_delete_pattern(hHal, &delPattern,
						    sessionId);

	if (!CDF_IS_STATUS_SUCCESS(cdf_ret_status)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "%s: sme_wowl_del_bcast_pattern failed with error code (%d).",
			  __func__, cdf_ret_status);

		return false;
	}

	g_hdd_wowl_ptrns_debugfs[pattern_idx] = 0;
	g_hdd_wowl_ptrns_count--;

	return true;
}

/**
 * hdd_enter_wowl() - Function which will enable WoWL. At least one
 *		      of MP and PBM must be enabled
 * @pAdapter: pointer to the adapter
 * @enable_mp: Whether to enable magic packet WoWL mode
 * @enable_pbm: Whether to enable pattern byte matching WoWL mode
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_enter_wowl(hdd_adapter_t *pAdapter, bool enable_mp, bool enable_pbm)
{
	tSirSmeWowlEnterParams wowParams;
	CDF_STATUS cdf_ret_status;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);

	cdf_mem_zero(&wowParams, sizeof(tSirSmeWowlEnterParams));

	wowParams.ucPatternFilteringEnable = enable_pbm;
	wowParams.ucMagicPktEnable = enable_mp;
	wowParams.sessionId = pAdapter->sessionId;
	if (enable_mp) {
		cdf_copy_macaddr((struct cdf_mac_addr *) &(wowParams.magicPtrn),
				 &(pAdapter->macAddressCurrent));
	}
#ifdef WLAN_WAKEUP_EVENTS
	wowParams.ucWoWEAPIDRequestEnable = true;
	wowParams.ucWoWEAPOL4WayEnable = true;
	wowParams.ucWowNetScanOffloadMatch = true;
	wowParams.ucWowGTKRekeyError = true;
	wowParams.ucWoWBSSConnLoss = true;
#endif /* WLAN_WAKEUP_EVENTS */

	/* Request to put Libra into WoWL */
	cdf_ret_status = sme_enter_wowl(hHal, hdd_wowl_callback, pAdapter,
#ifdef WLAN_WAKEUP_EVENTS
					hdd_wowl_wake_indication_callback,
					pAdapter,
#endif /* WLAN_WAKEUP_EVENTS */
					&wowParams, pAdapter->sessionId);

	if (!CDF_IS_STATUS_SUCCESS(cdf_ret_status)) {
		if (CDF_STATUS_PMC_PENDING != cdf_ret_status) {
			/* We failed to enter WoWL */
			CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
				  "sme_enter_wowl failed with error code (%d)",
				  cdf_ret_status);
			return false;
		}
	}
	return true;
}

/**
 * hdd_exit_wowl() - Function which will disable WoWL
 * @pAdapter: pointer to the adapter
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_exit_wowl(hdd_adapter_t *pAdapter)
{
	tSirSmeWowlExitParams wowParams;
	tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pAdapter);
	CDF_STATUS cdf_ret_status;

	wowParams.sessionId = pAdapter->sessionId;

	cdf_ret_status = sme_exit_wowl(hHal, &wowParams);
	if (!CDF_IS_STATUS_SUCCESS(cdf_ret_status)) {
		CDF_TRACE(CDF_MODULE_ID_HDD, CDF_TRACE_LEVEL_ERROR,
			  "sme_exit_wowl failed with error code (%d)",
			  cdf_ret_status);
		return false;
	}

	return true;
}

/**
 * hdd_init_wowl() - Init function which will initialize the WoWL module
 *		     and perform any required initial configuration
 * @pAdapter: pointer to the adapter
 *
 * Return: false if any errors encountered, true otherwise
 */
bool hdd_init_wowl(hdd_adapter_t *pAdapter)
{
	hdd_context_t *pHddCtx = NULL;
	pHddCtx = pAdapter->pHddCtx;

	memset(g_hdd_wowl_ptrns, 0, sizeof(g_hdd_wowl_ptrns));
	g_hdd_wowl_ptrns_count = 0;

	/* Add any statically configured patterns */
	hdd_add_wowl_ptrn(pAdapter, pHddCtx->config->wowlPattern);

	return true;
}
