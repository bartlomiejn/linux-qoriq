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
 * API_DPTX.h
 *
 ******************************************************************************
 */

#ifndef _API_DPTX_H_
#define _API_DPTX_H_

#include "API_General.h"
#include "API_AFE.h"

#define MAX_NUM_OF_EVENTS 4

/* Flags for CDN_API_DPTX_SetDbg */
#define DPTX_DBG_SET_PWR_SKIP_SLEEP (1 << 0)
#define DPTX_DBG_SET_ALT_CIPHER_ADDR (1 << 1)

/**
 * \addtogroup DP_TX_API
 * \{
 */
typedef u8 CDN_API_PWR_MODE;
typedef u32 CDN_EVENT;

/**
 * reply data struct for CDN_API_DPTX_READ_EDID
 * please note, buff will point to internal api buffer, user must copy it for later use
 */
typedef struct {
	u8 *buff;
	u8 size;
	u8 blockNo;
} DPTX_Read_EDID_response;
/**
 *  \brief Cadence API for DP TX to get RX EDID
 *  \param [in] segment - EDID segment to read
 *  \param [in] extension - EDID extension to read
 *  \param [out] resp - pointer to store response
 *   buffer , please note, buff will point to internal api buffer, user must copy it for later use
 *  \return status
 *
 */
CDN_API_STATUS CDN_API_DPTX_Read_EDID(state_struct *state, u8 segment,
				      u8 extension,
				      DPTX_Read_EDID_response *resp);
/**
 * \brief blocking version of #CDN_API_DPTX_Read_EDID
 */
CDN_API_STATUS CDN_API_DPTX_Read_EDID_blocking(state_struct *state, u8 segment,
					       u8 extension,
					       DPTX_Read_EDID_response *resp);

/**
 *  \brief Cadence API for DP TX to set power mode of sink
 *
 *  \param [in] mode  - power mode
 *  \return status
 *
 */
CDN_API_STATUS CDN_API_DPTX_SetPowerMode(state_struct *state,
					 CDN_API_PWR_MODE mode);
/**
 * \brief blocking version of #CDN_API_DPTX_SetPowerMode
 */
CDN_API_STATUS CDN_API_DPTX_SetPowerMode_blocking(state_struct *state,
						  CDN_API_PWR_MODE mode);

/**
 *  \brief Cadence API for DP TX to set Host capabilities
 *
 *  \param [in] maxLinkRate Max link rate-> 0x06=1.62Gbps 0x0A=2.7Gbps 0x14=5.4Gbps 0x1E=8.1Gbps
 *  \param [in] lanesCount_SSC bit 0-3 lanes count, bit 4 SSC
 *  \param [in] maxVoltageSwing - - max supported VoltageSwing
 *  \param [in] maxPreemphasis - max supported Preemphasis
 *  \param [in] testPatternsSupported -which test patern supportrd by phy
 *  \param [in] fastLinkTraining - is it fast link training
 *  \param [in] laneMapping - line mapping for USB type C
 *  \param [in] enchanced - enchanced mode
 *  \return status
 *
 */
CDN_API_STATUS CDN_API_DPTX_SetHostCap(state_struct *state, u8 maxLinkRate,
				       u8 lanesCount_SSC,
				       u8 maxVoltageSwing,
				       u8 maxPreemphasis,
				       u8 testPatternsSupported,
				       u8 fastLinkTraining,
				       u8 laneMapping, u8 enchanced);
/**
 * \brief blocking version of #CDN_API_DPTX_SetHostCap
 */
CDN_API_STATUS CDN_API_DPTX_SetHostCap_blocking(state_struct *state,
						u8 maxLinkRate,
						u8 lanesCount_SSC,
						u8 maxVoltageSwing,
						u8 maxPreemphasis,
						u8 testPatternsSupported,
						u8 fastLinkTraining,
						u8 laneMapping, u8 enchanced);

/**
 * reply data struct for #CDN_API_DPTX_READ_DPCD
 */
typedef struct {
    /** buffer where data will be stored, will become invalid after next call to API */
	u8 *buff;
	int addr;
	int size;
} DPTX_Read_DPCD_response;
/**
 *  \brief Cadence API for DP TX to read DPCD
 *
 *  \param [in] numOfBytes - num of bytes to read
 *  \param [in] addr - address to read from
 *  \param [out] resp - pointer to store response
 *  \return status
 *
 */
CDN_API_STATUS CDN_API_DPTX_Read_DPCD(state_struct *state, int numOfBytes,
				      int addr, DPTX_Read_DPCD_response *resp,
				      CDN_BUS_TYPE bus_type);
/**
 * blocking version of #CDN_API_DPTX_READ_DPCD
 */
CDN_API_STATUS CDN_API_DPTX_Read_DPCD_blocking(state_struct *state,
					       int numOfBytes, int addr,
					       DPTX_Read_DPCD_response *resp,
					       CDN_BUS_TYPE bus_type);

/**
 * reply data struct for CDN_API_DPTX_WRITE_DPCD
 */
typedef struct {
	int addr;
	int size;
} DPTX_Write_DPCD_response;
/**
 *  \brief Cadence API for DP TX to write DPCD
 *
 *  \param [in] numOfBytes - num of bytes to write
 *  \param [in] addr - address to write
 *  \param [in] buff - with the data to write
 *  \param [out] resp - pointer to store response
 *  \return status
 *
 */
CDN_API_STATUS CDN_API_DPTX_Write_DPCD(state_struct *state, u32 numOfBytes,
				       u32 addr, u8 *buff,
				       DPTX_Write_DPCD_response *resp,
				       CDN_BUS_TYPE bus_type);
/**
 * blocking version of #CDN_API_DPTX_WRITE_DPCD
 */
CDN_API_STATUS CDN_API_DPTX_Write_DPCD_blocking(state_struct *state,
						u32 numOfBytes, u32 addr,
						u8 *buff,
						DPTX_Write_DPCD_response *resp,
						CDN_BUS_TYPE bus_type);

/**
 * DPTX_Read_Register response struct
 */
typedef struct {
	u8 base;
	u8 regNo;
	u32 val;
} DPTX_Read_Register_response;
/**
 * \brief Cadence API for DP TX to read register
 *
 * \param [in] base - bank numeber (MSB of bank base address)
 * \param [in] regNo - register number
 * \param [out] resp - pointer to store response
 * \return status
 *
 * this function will return #CDN_ERR if value of base is incorrect
 */
CDN_API_STATUS CDN_API_DPTX_Read_Register(state_struct *state, u8 base,
					  u8 regNo,
					  DPTX_Read_Register_response *resp);
/**
 * blocking version of #CDN_API_DPTX_READ_REGISTER
 */
CDN_API_STATUS CDN_API_DPTX_Read_Register_blocking(state_struct *state,
						   u8 base, u8 regNo,
						   DPTX_Read_Register_response *
						   resp);

/**
 * \brief Cadence API for DP TX write register
 *
 * \param [in] base - bank numeber (MSB of bank base address)
 * \param [in] regNo - register number
 * \param [in] val - value to write
 * \return status
 *
 * this function will return #CDN_ERR if value of base is incorrect
 */
CDN_API_STATUS CDN_API_DPTX_Write_Register(state_struct *state, u8 base,
					   u8 regNo, u32 val);
/**
 * blocking version of #CDN_API_DPTX_Write_Register
 */
CDN_API_STATUS CDN_API_DPTX_Write_Register_blocking(state_struct *state,
						    u8 base, u8 regNo, u32 val);

/**
 * \brief Cadence API for DP TX write register
 *
 * \param [in] base - bank numeber (MSB of bank base address)
 * \param [in] regNo - register number
 * \param [in] startBit - first bit to write
 * \param [in] bitsNo - number of bits to write
 * \param [in] val - value to write
 * \return status
 *
 * this function will return #CDN_ERR if value of base is incorrect
 */
CDN_API_STATUS CDN_API_DPTX_Write_Field(state_struct *state, u8 base, u8 regNo,
					u8 startBit, u8 bitsNo, u32 val);
/**
 * blocking version of #CDN_API_DPTX_Write_Field
 */
CDN_API_STATUS CDN_API_DPTX_Write_Field_blocking(state_struct *state, u8 base,
						 u8 regNo,
						 u8 startBit,
						 u8 bitsNo, u32 val);

/* TODO doxygen of DPTX_CONTROL API */
CDN_API_STATUS CDN_API_DPTX_Control(state_struct *state, u32 mode);
/**
 * blocking version of #CDN_API_DPTX_Control
 */
CDN_API_STATUS CDN_API_DPTX_Control_blocking(state_struct *state, u32 mode);

/**
  * \brief Performs Fast Link Training, using LINK_RATE_SET DPCD register.
  * \param [in] mode - 0 to stop training, 1 to start it, 2 to restart it.
  * \param [in] linkRate - Link Rate to be used for training.
  * \param [in] rateId - index of selected Link Rate in DPCd registers.
  *
  * Performs Fast Link Training, selecting Link Rate using LINK_RATE_SET DPCD
  * register, characteristic to Embedded DisplayPort (eDP) v1.4 standard.
  * If requested link rate is not supported by DPTX, function will return error
  * code CDN_ERROR_NOT_SUPPORTED, and will take no further action.
  * rateId is used to select, which Link Rate supported by sink (enumerated in
  * SUPPORTED_LINK_RATES DPCD registers) is equal to rate requested. This value
  * will be written to first 3 bits of LINK_RATE_SET DPCD registers. Allowed
  * range is 0-7. If it is not known beforehand, SUPPORTED_LINK_RATES DPCD
  * registers may be read by an upper layer, and then used to determine the
  * correct value to use.
  */
CDN_API_STATUS CDN_API_DPTX_EDP_Training(state_struct *state, u8 mode,
					 ENUM_AFE_LINK_RATE linkRate,
					 u8 rateId);

/**
 * blocking version of #CDN_API_DPTX_EDP_Training
 */
CDN_API_STATUS CDN_API_DPTX_EDP_Training_blocking(state_struct *state, u8 mode,
						  ENUM_AFE_LINK_RATE linkRate,
						  u8 rateId);

/**
  * \brief send DPX_ENABLE_EVENT command
  */
CDN_API_STATUS CDN_API_DPTX_EnableEvent(state_struct *state, bool hpd,
					bool training);

/**
 * blocking version of #CDN_API_DPTX_EnableEvent
 */
CDN_API_STATUS CDN_API_DPTX_EnableEvent_blocking(state_struct *state, bool hpd,
						 bool training);

/**
 * \brief send DPTX_READ_EVENT_REQUEST command
 */
CDN_API_STATUS CDN_API_DPTX_ReadEvent(state_struct *state, u8 *LinkeventId,
				      u8 *HPDevents);

/**
 * blocking version of #CDN_API_DPTX_ReadEvent
 */
CDN_API_STATUS CDN_API_DPTX_ReadEvent_blocking(state_struct *state,
					       u8 *LinkeventId,
					       u8 *HPDevents);

/**
 * \brief set vic mode according to vic table, the input are video parameters
 */
CDN_API_STATUS CDN_API_DPTX_Set_VIC(state_struct *state,
				    const struct drm_display_mode *mode,
				    int bitsPerPixel,
				    VIC_NUM_OF_LANES NumOfLanes,
				    VIC_SYMBOL_RATE rate,
				    VIC_PXL_ENCODING_FORMAT pxlencformat,
				    STEREO_VIDEO_ATTR steroVidAttr,
				    BT_TYPE bt_type, int TU);

/**
 * blocking version of #CDN_API_DPTX_Set_VIC
 */
CDN_API_STATUS CDN_API_DPTX_Set_VIC_blocking(state_struct *state,
					     const struct drm_display_mode *mode,
					     int bitsPerPixel,
					     VIC_NUM_OF_LANES NumOfLanes,
					     VIC_SYMBOL_RATE rate,
					     VIC_PXL_ENCODING_FORMAT
					     pxlencformat,
					     STEREO_VIDEO_ATTR steroVidAttr,
					     BT_TYPE bt_type, int TU);

/**
 * \brief turn on or off the video
 */
CDN_API_STATUS CDN_API_DPTX_SetVideo(state_struct *state, u8 mode);

/**
 * \brief blocking version of  CDN_API_DPTX_SetVideo
 */
CDN_API_STATUS CDN_API_DPTX_SetVideo_blocking(state_struct *state, u8 mode);
/**
 * \brief blocking version of #CDN_API_DPTX_SetAudio
 */
CDN_API_STATUS CDN_API_DPTX_SetAudio_blocking(state_struct *state, u8 mode);

typedef struct {
	u8 rate;
	u8 lanes;
	u8 swing[4];
	u8 preemphasis[4];
} S_LINK_STAT;

/**
 * \brief get current link status (rate, num of lanes etc), user may read it after get link finish event
 */
CDN_API_STATUS CDN_API_DPTX_ReadLinkStat(state_struct *state,
					 S_LINK_STAT *stat);

/**
 * \brief blocking version of #CDN_API_DPTX_ReadLinkStat
 */
CDN_API_STATUS CDN_API_DPTX_ReadLinkStat_blocking(state_struct *state,
						  S_LINK_STAT *stat);

/**
 * \brief start link training
 */
CDN_API_STATUS CDN_API_DPTX_TrainingControl(state_struct *state, u8 val);
/**
 * \brief blocking version of #CDN_API_DPTX_TrainingControl
 */
CDN_API_STATUS CDN_API_DPTX_TrainingControl_blocking(state_struct *state,
						     u8 val);

/**
 * \brief check if last auxilary transaction succedd
 */
CDN_API_STATUS CDN_API_DPTX_GetLastAuxStatus(state_struct *state, u8 *resp);
/**
 * \brief blocking version of #CDN_API_DPTX_GetLastAuxStatus
 */
CDN_API_STATUS CDN_API_DPTX_GetLastAuxStatus_blocking(state_struct *state,
						      u8 *resp);

/**
 * \brief get current hpd status
 */

CDN_API_STATUS CDN_API_DPTX_GetHpdStatus(state_struct *state, u8 *resp);
/**
 * \brief blocking version of #CDN_API_DPTX_GetHpdStatus
 */
CDN_API_STATUS CDN_API_DPTX_GetHpdStatus_blocking(state_struct *state,
						  u8 *resp);

/**
 * \brief force the lanes to specific swing or preemphasis, with SSc or without to pattern (0=PRBS7 or 1=D10.2) for CTS or debug phy purpose
 */
CDN_API_STATUS CDN_API_DPTX_ForceLanes(state_struct *state, u8 linkRate,
				       u8 numOfLanes,
				       u8 voltageSwing_l0,
				       u8 preemphasis_l0,
				       u8 voltageSwing_l1,
				       u8 preemphasis_l1,
				       u8 voltageSwing_l2,
				       u8 preemphasis_l2,
				       u8 voltageSwing_l3,
				       u8 preemphasis_l3, u8 pattern, u8 ssc);

/**
 * \brief blocking version of #CDN_API_DPTX_ForceLanes
 */
CDN_API_STATUS CDN_API_DPTX_ForceLanes_blocking(state_struct *state,
						u8 linkRate, u8 numOfLanes,
						u8 voltageSwing_l0,
						u8 preemphasis_l0,
						u8 voltageSwing_l1,
						u8 preemphasis_l1,
						u8 voltageSwing_l2,
						u8 preemphasis_l2,
						u8 voltageSwing_l3,
						u8 preemphasis_l3, u8 pattern,
						u8 ssc);

/**
 * \brief Sets DP TX debug related features.
 */
CDN_API_STATUS CDN_API_DPTX_SetDbg(state_struct *state, u32 dbg_cfg);

/**
 * \brief blocking version of #CDN_API_DPTX_SetDbg
 */
CDN_API_STATUS CDN_API_DPTX_SetDbg_blocking(state_struct *state, u32 dbg_cfg);

#endif
