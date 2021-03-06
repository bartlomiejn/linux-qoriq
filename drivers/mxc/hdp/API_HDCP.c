/******************************************************************************
 *
 * Copyright (C) 2016-2017 Cadence Design Systems, Inc.
 * All rights reserved worldwide.
 *
 * Copyright 2017-2018 NXP
 *
 * SPDX-License-Identifier: BSD-3-Clause
 *
 ******************************************************************************
 *
 * API_HDCP.c
 *
 ******************************************************************************
 */

#include "API_HDCP.h"
#include "util.h"
#include "address.h"
#include "opcodes.h"
#include "hdcp2.h"

CDN_API_STATUS CDN_API_HDCP_TX_CONFIGURATION(state_struct *state, u8 val,
					     CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP_TX_CONFIGURATION, 1, 1, val);
		return CDN_STARTED;
	}
	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;
	state->running = 0;
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP_TX_CONFIGURATION_blocking(state_struct *state,
						      u8 val,
						      CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP_TX_CONFIGURATION
				(state, val, bus_type));
}

CDN_API_STATUS
CDN_API_HDCP2_TX_SET_PUBLIC_KEY_PARAMS(state_struct *state,
				       S_HDCP_TRANS_PUBLIC_KEY_PARAMS *val,
				       CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP2_TX_SET_PUBLIC_KEY_PARAMS, 2,
				      -sizeof(val->N), &val->N, -sizeof(val->E),
				      &val->E);
		return CDN_STARTED;
	}
	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;
	state->running = 0;
	return CDN_OK;
}

CDN_API_STATUS
CDN_API_HDCP2_TX_SET_PUBLIC_KEY_PARAMS_blocking(state_struct *state,
							S_HDCP_TRANS_PUBLIC_KEY_PARAMS *val,
							CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex,
				CDN_API_HDCP2_TX_SET_PUBLIC_KEY_PARAMS
				(state, val, bus_type));
}

CDN_API_STATUS CDN_API_HDCP2_TX_SET_KM_KEY_PARAMS(state_struct *state,
						  S_HDCP_TRANS_KM_KEY_PARAMS *val,
						  CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;

		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP2_TX_SET_KM_KEY_PARAMS, 1,
				      -sizeof(val->KM_KEY), &val->KM_KEY);

		return CDN_STARTED;
	}

	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;

	state->running = 0;

	return CDN_OK;
}

CDN_API_STATUS
CDN_API_HDCP2_TX_SET_KM_KEY_PARAMS_blocking(state_struct *state,
					    S_HDCP_TRANS_KM_KEY_PARAMS *val,
					    CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex,
				CDN_API_HDCP2_TX_SET_KM_KEY_PARAMS
				(state, val, bus_type));
}

CDN_API_STATUS
CDN_API_HDCP2_TX_SET_DEBUG_RANDOM_NUMBERS(state_struct *state,
					  S_HDCP_TRANS_DEBUG_RANDOM_NUMBERS *val,
					  CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP2_TX_SET_DEBUG_RANDOM_NUMBERS, 5,
				      -sizeof(val->KM), &val->KM,
				      -sizeof(val->RN), &val->RN,
				      -sizeof(val->KS), &val->KS,
				      -sizeof(val->RIV), &val->RIV,
				      -sizeof(val->RTX), &val->RTX);
		return CDN_STARTED;
	}
	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;
	state->running = 0;
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP2_TX_SET_DEBUG_RANDOM_NUMBERS_blocking(
				state_struct *state,
				S_HDCP_TRANS_DEBUG_RANDOM_NUMBERS *val,
				CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex,
				CDN_API_HDCP2_TX_SET_DEBUG_RANDOM_NUMBERS
				(state, val, bus_type));
}

CDN_API_STATUS CDN_API_HDCP2_TX_RESPOND_KM(state_struct *state,
					   S_HDCP_TRANS_PAIRING_DATA *val,
					   CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		/* pairing info found in storage */
		if (val != NULL)
			internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
					      HDCP2_TX_RESPOND_KM, 4,
					      -sizeof(val->Receiver_ID),
					      &val->Receiver_ID,
					      -sizeof(val->m), &val->m,
					      -sizeof(val->KM), &val->KM,
					      -sizeof(val->EKH), &val->EKH);
		else
			/* no pairing info found in storage */
			internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
					      HDCP2_TX_RESPOND_KM, 0);
		return CDN_STARTED;
	}
	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;
	state->running = 0;
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP2_TX_RESPOND_KM_blocking(state_struct *state,
						    S_HDCP_TRANS_PAIRING_DATA *val,
							CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP2_TX_RESPOND_KM
				(state, val, bus_type));
}

CDN_API_STATUS
CDN_API_HDCP1_TX_SEND_KEYS(state_struct *state,
			   S_HDCP_TX_MAIL_BOX_CMD_HDCP1_TX_SEND_KEYS *val,
			   CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP1_TX_SEND_KEYS, 2, -sizeof(val->AKSV),
				      &val->AKSV, -sizeof(val->KSV), &val->KSV);
		return CDN_STARTED;
	}
	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;
	state->running = 0;
	return CDN_OK;
}

CDN_API_STATUS
CDN_API_HDCP1_TX_SEND_KEYS_blocking(state_struct *state,
				    S_HDCP_TX_MAIL_BOX_CMD_HDCP1_TX_SEND_KEYS *val,
					CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP1_TX_SEND_KEYS
				(state, val, bus_type));
}

CDN_API_STATUS CDN_API_HDCP1_TX_SEND_RANDOM_AN(state_struct *state, u8 An[8],
					       CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP1_TX_SEND_RANDOM_AN, 1, -8, An);
		return CDN_STARTED;
	}
	if (state->txEnable && !internal_mbox_tx_process(state).txend)
		return CDN_BSY;
	state->running = 0;
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP1_TX_SEND_RANDOM_AN_blocking(state_struct *state,
							u8 An[8], CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP1_TX_SEND_RANDOM_AN
				(state, An, bus_type));
}

CDN_API_STATUS CDN_API_HDCP_TX_STATUS_REQ(state_struct *state,
					u8 *resp, CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		state->rxEnable = 1;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP_TX_STATUS_CHANGE, 0);
		return CDN_STARTED;
	}
	internal_process_messages(state);
	internal_opcode_match_or_return(state);
	internal_readmsg(state, 1, -5, resp);
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP_TX_STATUS_REQ_blocking(state_struct *state,
						   u8 resp[5],
						   CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP_TX_STATUS_REQ
				(state, resp, bus_type));
}

CDN_API_STATUS CDN_API_HDCP2_TX_IS_KM_STORED_REQ(state_struct *state,
						 u8 resp[5],
						 CDN_BUS_TYPE bus_type)
{
	printk(KERN_DEBUG "_debug: 0\n");
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		printk(KERN_DEBUG "_debug: 1\n");
		state->bus_type = bus_type;
		state->rxEnable = 1;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP2_TX_IS_KM_STORED, 0);
		printk(KERN_DEBUG "_debug: 2\n");
		return CDN_STARTED;
	}
	printk(KERN_DEBUG "_debug: 3\n");
	internal_process_messages(state);
	printk(KERN_DEBUG "_debug: 4\n");
	internal_opcode_match_or_return(state);
	printk(KERN_DEBUG "_debug: 5\n");
	internal_readmsg(state, 1, -5, resp);
	printk(KERN_DEBUG "_debug: 6\n");
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP2_TX_IS_KM_STORED_REQ_blocking(state_struct *state,
							  u8 resp[5],
							  CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP2_TX_IS_KM_STORED_REQ
				(state, resp, bus_type));
}

CDN_API_STATUS CDN_API_HDCP2_TX_STORE_KM_REQ(state_struct *state,
					     S_HDCP_TRANS_PAIRING_DATA *resp,
					     CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		state->rxEnable = 1;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP2_TX_STORE_KM, 0);
		return CDN_STARTED;
	}
	internal_process_messages(state);
	internal_opcode_match_or_return(state);
	internal_readmsg(state, 4,
			 -sizeof(resp->Receiver_ID), &resp->Receiver_ID,
			 -sizeof(resp->m), &resp->m,
			 -sizeof(resp->KM), &resp->KM,
			 -sizeof(resp->EKH), &resp->EKH);
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP2_TX_STORE_KM_REQ_blocking(state_struct *state,
						      S_HDCP_TRANS_PAIRING_DATA
						      *resp,
						      CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP2_TX_STORE_KM_REQ
				(state, resp, bus_type));
}

CDN_API_STATUS CDN_API_HDCP_TX_IS_RECEIVER_ID_VALID_REQ(state_struct *state,
							u8 *num, u8 *id,
							CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		state->rxEnable = 1;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP_TX_IS_RECEIVER_ID_VALID, 0);
		return CDN_STARTED;
	}
	internal_process_messages(state);
	internal_opcode_match_or_return(state);
	internal_readmsg(state, 1, 1, num);
	internal_readmsg(state, 2, 1, NULL, -5 * *num, id);
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP_TX_IS_RECEIVER_ID_VALID_REQ_blocking(
								state_struct *state,
								 u8 *num,
								 u8 *id,
								 CDN_BUS_TYPE
								 bus_type)
{
	internal_block_function(&state->mutex,
				CDN_API_HDCP_TX_IS_RECEIVER_ID_VALID_REQ
				(state, num, id, bus_type));
}

CDN_API_STATUS CDN_API_HDCP_TX_RESPOND_RECEIVER_ID_VALID(
							state_struct *state,
							 u8 valid,
							 CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP_TX_RESPOND_RECEIVER_ID_VALID, 1, 1,
				      valid);
		return CDN_STARTED;
	}
	internal_process_messages(state);
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP_TX_RESPOND_RECEIVER_ID_VALID_blocking(
								state_struct *state,
								u8 valid,
								CDN_BUS_TYPE
								bus_type)
{
	internal_block_function(&state->mutex,
				CDN_API_HDCP_TX_RESPOND_RECEIVER_ID_VALID
				(state, valid, bus_type));
}

CDN_API_STATUS CDN_API_HDCP_GENERAL_2_SET_LC(state_struct *state, u8 *lc,
					     CDN_BUS_TYPE bus_type)
{
	internal_macro_command_tx(state, MB_MODULE_ID_HDCP_GENERAL,
				  HDCP_GENERAL_SET_LC_128, bus_type, 1, -16, lc);
	return CDN_OK;
}

CDN_API_STATUS CDN_API_HDCP_GENERAL_2_SET_LC_blocking(state_struct *state,
						      u8 *lc,
						      CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_HDCP_GENERAL_2_SET_LC
				(state, lc, bus_type));
}

CDN_API_STATUS CDN_API_TEST_KEYS(state_struct *state, u8 test_type, u8 resp[1],
				 CDN_BUS_TYPE bus_type)
{
	if (!state->running) {
		if (!internal_apb_available(state))
			return CDN_BSY;
		state->bus_type = bus_type;
		state->rxEnable = 1;
		internal_tx_mkfullmsg(state, MB_MODULE_ID_HDCP_TX,
				      HDCP_TX_TEST_KEYS, 1, 1, test_type);
		return CDN_STARTED;
	}
	internal_process_messages(state);
	internal_opcode_match_or_return(state);
	internal_readmsg(state, 1, -1, resp);
	return CDN_OK;
}

CDN_API_STATUS CDN_API_TEST_KEYS_blocking(state_struct *state, u8 test_type,
					  u8 resp[1], CDN_BUS_TYPE bus_type)
{
	internal_block_function(&state->mutex, CDN_API_TEST_KEYS
				(state, test_type, resp, bus_type));
}
