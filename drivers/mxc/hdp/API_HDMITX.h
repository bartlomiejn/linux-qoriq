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
 * API_HDMITX.h
 *
 ******************************************************************************
 */

#ifndef _API_HDMITX_H_
#define _API_HDMITX_H_

#include "API_General.h"
#include "hdmi.h"

/**
 * \addtogroup HDMI_TX_API
 * \{
 */

# define HDMI_TX_EVENT_CODE_HPD_HIGH                  0x01
# define HDMI_TX_EVENT_CODE_HPD_LOW                   0x02
# define HDMI_TX_EVENT_CODE_HPD_STATE_LOW             0x00
# define HDMI_TX_EVENT_CODE_HPD_STATE_HIGH            0x08
typedef struct {
    /** if used to return data, this pointer is set (instead of being a destination to copy data to */
	u8 *buff;
	HDMI_I2C_STATUS status;
	u16 len;
	u8 slave;
	u8 offset;
} HDMITX_TRANS_DATA;

typedef enum {
	HDMI_TX_MODE_DVI,
	HDMI_TX_MODE_HDMI_1_4,
	HDMI_TX_MODE_HDMI_2_0,
} HDMI_TX_MAIL_HANDLER_PROTOCOL_TYPE;

/**
 * \brief I2C read transaction
 * \param [in]  data_in  - fields used: len, slave, offset
 * \param [out] data_out - fields used: all
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_DDC_READ(state_struct *state,
				       HDMITX_TRANS_DATA *data_in,
				       HDMITX_TRANS_DATA *data_out);
CDN_API_STATUS CDN_API_HDMITX_DDC_READ_blocking(state_struct *state,
						HDMITX_TRANS_DATA *data_in,
						HDMITX_TRANS_DATA *data_out);

/**
 * \brief I2C write transaction
 * \param [in]  data_in  - fields used: len, slave, offset, buff
 * \param [out] data_out - fields used: status, len, slave, offset
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_DDC_WRITE(state_struct *state,
					HDMITX_TRANS_DATA *data_in,
					HDMITX_TRANS_DATA *data_out);
CDN_API_STATUS CDN_API_HDMITX_DDC_WRITE_blocking(state_struct *state,
						 HDMITX_TRANS_DATA *data_in,
						 HDMITX_TRANS_DATA *data_out);

/**
 * \brief I2C update read
 * \param [out] data_out - fields used: status, buff
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_DDC_UPDATE_READ(state_struct *state,
					      HDMITX_TRANS_DATA *data_out);
CDN_API_STATUS CDN_API_HDMITX_DDC_UPDATE_READ_blocking(state_struct *state,
						       HDMITX_TRANS_DATA *
						       data_out);

/**
 * \brief I2C read edid
 * \param [in]  block    - EDID block
 * \pram  [in]  segment  - EDID segment
 * \param [out] data_out - fields used: status, buff, slave (as block), offset (as segment), len
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_READ_EDID(state_struct *state, u8 block,
					u8 segment,
					HDMITX_TRANS_DATA *data_out);
CDN_API_STATUS CDN_API_HDMITX_READ_EDID_blocking(state_struct *state, u8 block,
						 u8 segment,
						 HDMITX_TRANS_DATA *data_out);

/**
 * \brief set hdmi protocol type (DVI,1.x,2.x) (send scrambler command over scdc and set bits in controller)
 * \param [in]  protocol    - type
 * \returns status
 */
CDN_API_STATUS
CDN_API_HDMITX_Set_Mode_blocking(state_struct *state,
				 HDMI_TX_MAIL_HANDLER_PROTOCOL_TYPE protocol,
				 u32 character_rate);

/**
 * \brief init hdmi registers
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_Init_blocking(state_struct *state);

/**
 * \brief change to vid id vicMode
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_SetVic_blocking(state_struct *state,
					      struct drm_display_mode *mode,
					      int bpp,
					      VIC_PXL_ENCODING_FORMAT format);

/**
 * \brief option to force color depth in the gcp or not force (HW mode)
 * \returns status
 */
CDN_API_STATUS CDN_API_HDMITX_ForceColorDepth_blocking(state_struct *state,
						       u8 force, u8 val);
CDN_API_STATUS CDN_API_HDMITX_ReadEvents(state_struct *state, u32 *events);
CDN_API_STATUS CDN_API_HDMITX_ReadEvents_blocking(state_struct *state,
						  u32 *events);
CDN_API_STATUS CDN_API_HDMITX_GetHpdStatus(state_struct *state, u8 *hpd_sts);
CDN_API_STATUS CDN_API_HDMITX_GetHpdStatus_blocking(state_struct *state,
						    u8 *hpd_sts);
#endif
