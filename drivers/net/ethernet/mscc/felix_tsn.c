// SPDX-License-Identifier: (GPL-2.0 OR MIT)
/* Felix Switch TSN driver
 *
 * Copyright 2018-2019 NXP
 */

#include <linux/io.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/sys_soc.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/phy.h>
#include <linux/iopoll.h>
#include "ocelot.h"
#include "felix_tsn.h"

#define MSCC_NUM_OUT_PORT 4 /* Number of physical output ports */
#define SE_IX_PORT 64

/* MSCC TSN parameters limited */
#define NUM_MSCC_QOS_PRIO 8
#define MSCC_PSFP_SFID_NUM 176
#define MSCC_FRER_SSID_NUM 128

/* Using the max number of MSCC_PSFP_SFID_NUM and MSCC_FRER_SSID_NUM */
#define MSCC_STREAM_HANDLE_NUM	MSCC_PSFP_SFID_NUM

int streamhandle_map[MSCC_STREAM_HANDLE_NUM] = {0};
static struct mscc_switch_capa capa __ro_after_init = {
	.num_tas_gcl	= 64,
	.tas_ct_min	= 100,
	.tas_ct_max	= 1000000000,
	.tas_cte_max	= 999999999,
	.tas_it_max	= 999999999,
	.tas_it_min	= 1000,
	.num_hsch	= 72,
	.num_psfp_sfid	= MSCC_PSFP_SFID_NUM,
	.num_psfp_sgid	= 184,
	.psfp_fmi_max	= 246,
	.psfp_fmi_min	= 63,
	.num_sgi_gcl	= 4,
	.sgi_ct_min	= 5000,
	.sgi_ct_max	= 1000000000,
	.sgi_cte_max	= 999999999,
	.qos_pol_max	= 383,
	/* Maximum allowed value of committed burst size(CBS) is 240 KB */
	.pol_cbs_max	= 60,
	/* Maximum allowed value of excess burst size(EBS) is 240 KB */
	.pol_pbs_max	= 60,
	.num_frer_ssid  = MSCC_FRER_SSID_NUM,
	.frer_seq_len_min = 1,
	.frer_seq_len_max = 28,
	.frer_his_len_min = 1,
	.frer_his_len_max = 32,
	.qos_dscp_max	= 63,
	.qos_cos_max	= NUM_MSCC_QOS_PRIO - 1,
	.qos_dp_max	= 1,
};

u32 felix_tsn_get_cap(struct net_device *ndev)
{
	u32 cap;

	cap = (TSN_CAP_QBV | TSN_CAP_QCI | TSN_CAP_QBU | TSN_CAP_CBS |
	       TSN_CAP_CB | TSN_CAP_TBS | TSN_CAP_CTH);

	return cap;
}

static int qos_port_tas_gcl_set(struct net_device *ndev,
				struct ocelot *ocelot, const u8 gcl_ix,
				struct tsn_qbv_entry *control_list)
{
	if (gcl_ix >= capa.num_tas_gcl) {
		netdev_info(ndev, "Invalid gcl ix %u\n", gcl_ix);
		return -EINVAL;
	}
	if (control_list->time_interval < capa.tas_it_min ||
	    control_list->time_interval > capa.tas_it_max) {
		netdev_info(ndev, "Invalid time_interval %u\n",
			    control_list->time_interval);

		return -EINVAL;
	}

	ocelot_write(ocelot,
		     QSYS_GCL_CFG_REG_1_GCL_ENTRY_NUM(gcl_ix) |
		     QSYS_GCL_CFG_REG_1_GATE_STATE(control_list->gate_state),
		     QSYS_GCL_CFG_REG_1);

	ocelot_write(ocelot,
		     control_list->time_interval,
		     QSYS_GCL_CFG_REG_2);

	return 0;
}

static u32 tas_read_status(struct ocelot *ocelot)
{
	u32 val;

	val = ocelot_read(ocelot, QSYS_TAS_PARAM_CFG_CTRL);

	return val;
}

int felix_qbv_set(struct net_device *ndev,
		  struct tsn_qbv_conf *shaper_config)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	struct tsn_qbv_basic *admin_basic = &shaper_config->admin;
	struct tsn_qbv_entry *control_list = admin_basic->control_list;
	u32 base_time_nsec = admin_basic->base_time % 1000000000;
	u64 base_time_sec = admin_basic->base_time / 1000000000;
	u64 cur_time;
	u32 val;
	u8 speed;
	int i;
	int ret;

	if (admin_basic->control_list_length > capa.num_tas_gcl) {
		netdev_info(ndev, "Invalid admin_control_list_length %u\n",
			    admin_basic->control_list_length);
		return -EINVAL;
	}

	if ((admin_basic->cycle_time < capa.tas_ct_min ||
	     admin_basic->cycle_time > capa.tas_ct_max) &&
	    shaper_config->gate_enabled) {
		netdev_info(ndev, "Invalid admin_cycle_time %u ns\n",
			    admin_basic->cycle_time);
		return -EINVAL;
	}
	if (admin_basic->cycle_time_extension > capa.tas_cte_max) {
		netdev_info(ndev,
			    "Invalid admin_cycle_time_extension %u\n",
			    admin_basic->cycle_time_extension);
		return -EINVAL;
	}

	cur_time = ocelot_read(ocelot, PTP_CUR_SEC_MSB);
	cur_time = cur_time << 32;
	cur_time += ocelot_read(ocelot, PTP_CUR_SEC_LSB);

	if (base_time_sec < cur_time) {
		base_time_sec = cur_time;
		base_time_nsec = ocelot_read(ocelot, PTP_CUR_NSEC);
	}

	/* Select port */
	ocelot_rmw(ocelot,
		   QSYS_TAS_PARAM_CFG_CTRL_PORT_NUM(port->chip_port),
		   QSYS_TAS_PARAM_CFG_CTRL_PORT_NUM_M,
		   QSYS_TAS_PARAM_CFG_CTRL);

	val = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_8);
	if (val & QSYS_PARAM_STATUS_REG_8_CONFIG_PENDING) {
		ocelot_rmw_rix(ocelot, 0, QSYS_TAG_CONFIG_ENABLE,
			       QSYS_TAG_CONFIG, port->chip_port);
	}

	if (!shaper_config->gate_enabled)
		admin_basic->gate_states = 0xff;

	speed = OCELOT_SPEED_1000;
	if (ndev->phydev) {
		switch (ndev->phydev->speed) {
		case SPEED_10:
			speed = OCELOT_SPEED_10;
			break;
		case SPEED_100:
			speed = OCELOT_SPEED_100;
			break;
		case SPEED_1000:
			speed = OCELOT_SPEED_1000;
			break;
		case SPEED_2500:
			speed = OCELOT_SPEED_2500;
			break;
		}
	}

	ocelot_rmw_rix(ocelot,
		       (shaper_config->gate_enabled ?
			QSYS_TAG_CONFIG_ENABLE : 0) |
		       QSYS_TAG_CONFIG_INIT_GATE_STATE(admin_basic->gate_states) |
		       QSYS_TAG_CONFIG_SCH_TRAFFIC_QUEUES(0xff) |
		       QSYS_TAG_CONFIG_LINK_SPEED(speed),
		       QSYS_TAG_CONFIG_ENABLE |
		       QSYS_TAG_CONFIG_INIT_GATE_STATE_M |
		       QSYS_TAG_CONFIG_SCH_TRAFFIC_QUEUES_M |
		       QSYS_TAG_CONFIG_LINK_SPEED_M,
		       QSYS_TAG_CONFIG,
		       port->chip_port);

	ocelot_write_rix(ocelot, shaper_config->maxsdu,
			 QSYS_PORT_MAX_SDU, port->chip_port);
	/* TODO: add queue max SDU set */

	if (shaper_config->gate_enabled) {
		ocelot_write(ocelot, base_time_nsec,
			     QSYS_PARAM_CFG_REG_1);

		ocelot_write(ocelot, base_time_sec & GENMASK(31, 0),
			     QSYS_PARAM_CFG_REG_2);

		ocelot_write(ocelot,
			     QSYS_PARAM_CFG_REG_3_BASE_TIME_SEC_MSB(base_time_sec >> 32) |
			     QSYS_PARAM_CFG_REG_3_LIST_LENGTH(admin_basic->control_list_length),
			     QSYS_PARAM_CFG_REG_3);

		ocelot_write(ocelot, admin_basic->cycle_time,
			     QSYS_PARAM_CFG_REG_4);

		ocelot_write(ocelot, admin_basic->cycle_time_extension,
			     QSYS_PARAM_CFG_REG_5);

		for (i = 0; i < admin_basic->control_list_length; i++) {
			qos_port_tas_gcl_set(ndev, ocelot, i, control_list);
			control_list++;
		}

		/* Start configuration change */
		ocelot_rmw(ocelot,
			   QSYS_TAS_PARAM_CFG_CTRL_CONFIG_CHANGE,
			   QSYS_TAS_PARAM_CFG_CTRL_CONFIG_CHANGE,
			   QSYS_TAS_PARAM_CFG_CTRL);

		ret = readx_poll_timeout(tas_read_status, ocelot, val,
					 !(QSYS_TAS_PARAM_CFG_CTRL_CONFIG_CHANGE
					 & val), 10, 100000);
		return ret;
	}

	return 0;
}

int felix_qbv_get(struct net_device *ndev,
		  struct tsn_qbv_conf *shaper_config)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 val, reg;
	int i;
	u8 p_num = port->chip_port;
	u32 base_timel;
	u32 base_timeh;
	struct tsn_qbv_basic *admin = &shaper_config->admin;
	struct tsn_qbv_entry *list;

	ocelot_field_write(ocelot,
			   QSYS_TAS_PARAM_CFG_CTRL_PORT_NUM_0, p_num);

	val = ocelot_read(ocelot, QSYS_TAG_CONFIG);
	shaper_config->gate_enabled = (val & QSYS_TAG_CONFIG_ENABLE);
	admin->gate_states = QSYS_TAG_CONFIG_INIT_GATE_STATE_X(val);

	base_timel = ocelot_read(ocelot, QSYS_PARAM_CFG_REG_1);
	base_timeh = ocelot_read(ocelot, QSYS_PARAM_CFG_REG_2);
	reg = ocelot_read(ocelot, QSYS_PARAM_CFG_REG_3);
	admin->base_time = base_timeh |
		(((u64)QSYS_PARAM_CFG_REG_3_BASE_TIME_SEC_MSB(reg)) << 32);

	admin->base_time = (admin->base_time * 1000000000) + base_timel;

	admin->control_list_length =
		QSYS_PARAM_CFG_REG_3_LIST_LENGTH_X(reg);

	admin->cycle_time = ocelot_read(ocelot, QSYS_PARAM_CFG_REG_4);
	admin->cycle_time_extension =
		ocelot_read(ocelot, QSYS_PARAM_CFG_REG_5);

	list = kmalloc_array(admin->control_list_length,
			     sizeof(struct tsn_qbv_entry), GFP_KERNEL);
	admin->control_list = list;

	for (i = 0; i < admin->control_list_length; i++) {
		ocelot_field_write(ocelot,
				   QSYS_GCL_CFG_REG_1_GCL_ENTRY_NUM_0, i);
		list->time_interval =
			ocelot_read(ocelot, QSYS_GCL_CFG_REG_2);

		reg = ocelot_read(ocelot, QSYS_GCL_CFG_REG_1);
		list->gate_state = QSYS_GCL_CFG_REG_1_GATE_STATE_X(reg);

		list++;
	}

	return 0;
}

static int qbv_get_gatelist(struct ocelot *ocelot,
			    struct tsn_qbv_basic *oper)
{
	u32 base_timel;
	u32 base_timeh;
	u32 val;
	struct tsn_qbv_entry *glist;
	int i;

	base_timel = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_1);
	base_timeh = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_2);
	val = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_3);
	oper->base_time = base_timeh;
	oper->base_time +=
		((u64)QSYS_PARAM_STATUS_REG_3_BASE_TIME_SEC_MSB(val)) <<
		32;
	oper->base_time = (oper->base_time * 1000000000) + base_timel;

	oper->control_list_length =
		QSYS_PARAM_STATUS_REG_3_LIST_LENGTH_X(val);

	oper->cycle_time = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_4);
	oper->cycle_time_extension = ocelot_read(ocelot,
						 QSYS_PARAM_STATUS_REG_5);

	val = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_8);
	oper->gate_states = QSYS_PARAM_STATUS_REG_8_OPER_GATE_STATE_X(val);

	glist = kmalloc_array(oper->control_list_length,
			      sizeof(struct tsn_qbv_entry), GFP_KERNEL);

	oper->control_list = glist;

	for (i = 0; i < oper->control_list_length; i++) {
		ocelot_field_write(ocelot,
				   QSYS_GCL_STATUS_REG_1_GCL_ENTRY_NUM_0,
				   i);

		val = ocelot_read(ocelot, QSYS_GCL_STATUS_REG_2);
		glist->time_interval = val;
		val = ocelot_read(ocelot, QSYS_GCL_STATUS_REG_1);
		glist->gate_state =
			QSYS_GCL_STATUS_REG_1_GATE_STATE_X(val);

		glist++;
	}

	return 0;
}

int felix_qbv_get_status(struct net_device *ndev,
			 struct tsn_qbv_status *qbvstatus)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	struct tsn_qbv_basic *oper = &qbvstatus->oper;
	u8 p_num = port->chip_port;
	u32 val;
	ptptime_t cur_time;

	ocelot_field_write(ocelot,
			   QSYS_TAS_PARAM_CFG_CTRL_PORT_NUM_0,
			   p_num);

	qbvstatus->supported_list_max = capa.num_tas_gcl;

	val = ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_8);
	qbvstatus->config_pending =
		(val & QSYS_PARAM_STATUS_REG_8_CONFIG_PENDING) ? 1 : 0;

	qbvstatus->config_change_time =
		ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_7);

	qbvstatus->config_change_time +=
		((u64)QSYS_PARAM_STATUS_REG_8_CFG_CHG_TIME_SEC_MSB(val)) <<
		32;

	qbvstatus->config_change_time =
		(qbvstatus->config_change_time * 1000000000) +
		ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_6);

	qbvstatus->config_change_error =
		ocelot_read(ocelot, QSYS_PARAM_STATUS_REG_9);

	cur_time = ocelot_read(ocelot, PTP_CUR_SEC_MSB);
	cur_time = cur_time << 32;
	cur_time += ocelot_read(ocelot, PTP_CUR_SEC_LSB);
	cur_time = (cur_time * 1000000000) +
		   ocelot_read(ocelot, PTP_CUR_NSEC);

	qbvstatus->current_time = cur_time;
	qbv_get_gatelist(ocelot, oper);

	return 0;
}

int felix_cut_thru_set(struct net_device *ndev, u8 cut_thru)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;

	ocelot_write_rix(ocelot, cut_thru, ANA_CUT_THRU_CFG,
			 ocelot_port->chip_port);

	return 0;
}

static int qos_shaper_conf_set(struct net_device *ndev, u32 port_ix,
			       u8 percent)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;
	u32 cbs = 0;
	u32 cir = 0;

	if (percent > 100) {
		netdev_info(ndev, "percentage %d larger than 100\n",
			    percent);
		return -EINVAL;
	}
	if (port_ix >= capa.num_hsch) {
		netdev_info(ndev,
			    "CIR_CFG: id %d is exceed num of HSCH instance\n",
			    port_ix);
		return -EINVAL;
	}

	if (ndev->phydev) {
		switch (ndev->phydev->speed) {
		case SPEED_10:
			cir = 10000;
			break;
		case SPEED_100:
			cir = 100000;
			break;
		case SPEED_1000:
			cir = 1000000;
			break;
		case SPEED_2500:
			cir = 2500000;
			break;
		}
	} else {
		cir = 1000000;
	}

	cir = cir * percent / 100;
	cir = DIV_ROUND_UP(cir, 100);  /* Rate unit is 100 kbps */
	cir = (cir ? cir : 1);              /* Avoid using zero rate */
	cbs = DIV_ROUND_UP(cbs, 4096); /* Burst unit is 4kB */
	cbs = (cbs ? cbs : 1);		/* Avoid using zero burst size */
	cir = min_t(u32, GENMASK(15, 0), cir);
	cbs = min_t(u32, GENMASK(6, 0), cbs);
	ocelot_write_gix(ocelot,
			 QSYS_CIR_CFG_CIR_RATE(cir) |
			 QSYS_CIR_CFG_CIR_BURST(cbs),
			 QSYS_CIR_CFG,
			 port_ix);

	return 0;
}

static int qos_shaper_conf_get(struct net_device *ndev, u32 port_ix)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;
	u32 val;
	u32 bandwidth = 0;
	u32 cir = 0;
	int percentage;

	if (port_ix >= capa.num_hsch) {
		netdev_info(ndev,
			    "CIR_CFG: id %d is exceed num of HSCH instance\n",
			    port_ix);
		return -EINVAL;
	}

	if (ndev->phydev) {
		switch (ndev->phydev->speed) {
		case SPEED_10:
			bandwidth = 10000;
			break;
		case SPEED_100:
			bandwidth = 100000;
			break;
		case SPEED_1000:
			bandwidth = 1000000;
			break;
		case SPEED_2500:
			bandwidth = 2500000;
			break;
		}
	} else {
		bandwidth = 1000000;
	}

	val = ocelot_read_gix(ocelot, QSYS_CIR_CFG, port_ix);

	cir = QSYS_CIR_CFG_CIR_RATE_X(val);
	cir *= 100;
	percentage = cir * 100 / bandwidth;

	return percentage;
}

int felix_cbs_set(struct net_device *ndev, u8 tc, u8 bw)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;

	if (tc > capa.qos_cos_max) {
		netdev_info(ndev, "Invalid tc: %u\n", tc);
		return -EINVAL;
	}

	qos_shaper_conf_set(ndev, ocelot_port->chip_port * 8 + tc, bw);

	ocelot_rmw_gix(ocelot,
		       QSYS_SE_CFG_SE_AVB_ENA,
		       QSYS_SE_CFG_SE_AVB_ENA,
		       QSYS_SE_CFG,
		       ocelot_port->chip_port * 8 + tc);

	return 0;
}

int felix_cbs_get(struct net_device *ndev, u8 tc)
{
	int ret;
	struct ocelot_port *ocelot_port = netdev_priv(ndev);

	if (tc > capa.qos_cos_max) {
		netdev_info(ndev, "Invalid tc: %u\n", tc);
		return -EINVAL;
	}

	ret = qos_shaper_conf_get(ndev, ocelot_port->chip_port * 8 + tc);

	return ret;
}

int felix_qbu_set(struct net_device *ndev, u8 preemptible)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;

	ocelot_port_rmwl(ocelot_port,
			 DEV_GMII_MM_CONFIG_ENABLE_CONFIG_MM_RX_ENA |
			 DEV_GMII_MM_CONFIG_ENABLE_CONFIG_MM_TX_ENA,
			 DEV_GMII_MM_CONFIG_ENABLE_CONFIG_MM_RX_ENA |
			 DEV_GMII_MM_CONFIG_ENABLE_CONFIG_MM_TX_ENA,
			 DEV_GMII_MM_CONFIG_ENABLE_CONFIG);

	ocelot_rmw_rix(ocelot,
		       QSYS_PREEMPTION_CFG_P_QUEUES(preemptible),
		       QSYS_PREEMPTION_CFG_P_QUEUES_M,
		       QSYS_PREEMPTION_CFG,
		       ocelot_port->chip_port);

	return 0;
}

int felix_qbu_get(struct net_device *ndev, struct tsn_preempt_status *c)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;
	u32 val;

	val = ocelot_read_rix(ocelot,
			      QSYS_PREEMPTION_CFG,
			      ocelot_port->chip_port);

	c->admin_state = QSYS_PREEMPTION_CFG_P_QUEUES(val);
	c->hold_advance = QSYS_PREEMPTION_CFG_HOLD_ADVANCE_X(val);

	val = ocelot_port_readl(ocelot_port,
				DEV_GMII_MM_STATISTICS_MM_STATUS);
	c->preemption_active =
		DEV_GMII_MM_STATISTICS_MM_STATUS_PRMPT_ACTIVE_STATUS & val;

	return 0;
}

int felix_cb_streamid_counters_get(struct net_device *ndev, u32 index,
				   struct tsn_cb_streamid_counters *s_counters)
{
	return 0;
}

int felix_cb_streamid_get(struct net_device *ndev, u32 index,
			  struct tsn_cb_streamid *streamid)
{
	struct ocelot_port *ocelot_port = netdev_priv(ndev);
	struct ocelot *ocelot = ocelot_port->ocelot;
	u32 m_index;
	u32 bucket;
	u32 val, dst, reg;
	u64 dmac;
	u32 ldmac, hdmac;

	if (index >= MSCC_STREAM_HANDLE_NUM) {
		netdev_info(ndev, "Invalid stream handle %u, maximum:%u\n",
			    index, MSCC_STREAM_HANDLE_NUM - 1);
		return -EINVAL;
	}

	index = streamhandle_map[index];
	m_index = index / 4;
	bucket =  index % 4;
	streamid->type = 1;
	regmap_field_write(ocelot->regfields[ANA_TABLES_MACTINDX_BUCKET],
			   bucket);
	regmap_field_write(ocelot->regfields[ANA_TABLES_MACTINDX_M_INDEX],
			   m_index);

	/*READ command MACACCESS.VALID(11 bit) must be 0 */
	ocelot_write(ocelot,
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
		     ANA_TABLES_MACACCESS);

	val = ocelot_read(ocelot, ANA_TABLES_MACACCESS);
	dst = ANA_TABLES_MACACCESS_DEST_IDX_X(val);
	reg = ocelot_read_rix(ocelot, ANA_PGID_PGID, dst);
	streamid->ofac_oport = ANA_PGID_PGID_PGID(reg);

	/*Get the entry's MAC address and VLAN id*/
	ldmac = ocelot_read(ocelot, ANA_TABLES_MACLDATA);
	val = ocelot_read(ocelot, ANA_TABLES_MACHDATA);
	val &= 0x1fffffff;
	hdmac = val & 0xffff;
	dmac = hdmac;
	dmac = (dmac << 32) | ldmac;
	streamid->para.nid.dmac = dmac;

	streamid->para.nid.vid = ANA_TABLES_MACHDATA_VID_X(val);

	val = ocelot_read(ocelot, ANA_TABLES_STREAMDATA);
	if (!(val & ANA_TABLES_STREAMDATA_SFID_VALID))
		return -EINVAL;

	streamid->handle = ANA_TABLES_STREAMDATA_SFID(val);

	return 0;
}

int lookup_mactable(struct ocelot *ocelot, u16 vid, u64 mac)
{
	u32 mach, macl;
	u32 reg1, reg2;
	u32 index, bucket;

	macl = mac & 0xffffffff;
	mach = (mac >> 32) & 0xffff;
	ocelot_write(ocelot, macl, ANA_TABLES_MACLDATA);
	ocelot_write(ocelot, ANA_TABLES_MACHDATA_VID(vid) |
		     ANA_TABLES_MACHDATA_MACHDATA(mach),
		     ANA_TABLES_MACHDATA);

	ocelot_write(ocelot, ANA_TABLES_MACACCESS_VALID |
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
		     ANA_TABLES_MACACCESS);

	reg1 = ocelot_read(ocelot, ANA_TABLES_MACLDATA);
	reg2 = ocelot_read(ocelot, ANA_TABLES_MACHDATA);
	if (reg1 == 0 && reg2 == 0)
		return -1;

	regmap_field_read(ocelot->regfields[ANA_TABLES_MACTINDX_BUCKET],
			  &bucket);
	regmap_field_read(ocelot->regfields[ANA_TABLES_MACTINDX_M_INDEX],
			  &index);

	index = index * 4 + bucket;

	return index;
}

int felix_cb_streamid_set(struct net_device *ndev, u32 index, bool enable,
			  struct tsn_cb_streamid *streamid)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	struct regmap_field *rf;
	u16 vid;
	u64 mac;
	u32 macl, mach;
	u32 dst_idx;
	int idx;
	u32 reg;
	int sfid, ssid;
	u32 m_index, bucket;

	if (!enable) {
		if (index >= MSCC_STREAM_HANDLE_NUM) {
			netdev_info(ndev,
				    "Invalid index %u, maximum:%u\n",
				    index, MSCC_STREAM_HANDLE_NUM - 1);
			return -EINVAL;
		}
		m_index = streamhandle_map[index] / 4;
		bucket =  streamhandle_map[index] % 4;
		rf = ocelot->regfields[ANA_TABLES_MACTINDX_BUCKET];
		regmap_field_write(rf, bucket);
		rf = ocelot->regfields[ANA_TABLES_MACTINDX_M_INDEX];
		regmap_field_write(rf, m_index);

		/*READ command MACACCESS.VALID(11 bit) must be 0 */
		ocelot_write(ocelot,
			     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
			     ANA_TABLES_MACACCESS);

		ocelot_write(ocelot,
			     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_FORGET),
			     ANA_TABLES_MACACCESS);

		streamhandle_map[index] = 0;

		return 0;
	}

	if (streamid->type != 1) {
		netdev_info(ndev, "Invalid stream type\n");
		return -EINVAL;
	}

	if (streamid->handle >= MSCC_STREAM_HANDLE_NUM) {
		netdev_info(ndev, "Invalid stream handle %u, maximum:%u\n",
			    streamid->handle, MSCC_STREAM_HANDLE_NUM - 1);
		return -EINVAL;
	}

	sfid = streamid->handle;
	ssid = (streamid->handle < MSCC_FRER_SSID_NUM ?
		streamid->handle : (MSCC_FRER_SSID_NUM - 1));

	mac = streamid->para.nid.dmac;
	macl = mac & 0xffffffff;
	mach = (mac >> 32) & 0xffff;
	vid = streamid->para.nid.vid;

	idx = lookup_mactable(ocelot, vid, mac);

	if (idx < 0) {
		ocelot_write(ocelot, macl, ANA_TABLES_MACLDATA);
		ocelot_write(ocelot, ANA_TABLES_MACHDATA_VID(vid) |
			     ANA_TABLES_MACHDATA_MACHDATA(mach),
			     ANA_TABLES_MACHDATA);

		ocelot_write(ocelot,
			     ANA_TABLES_STREAMDATA_SFID_VALID |
			     ANA_TABLES_STREAMDATA_SFID(sfid) |
			     ANA_TABLES_STREAMDATA_SSID_VALID |
			     ANA_TABLES_STREAMDATA_SSID(ssid),
			     ANA_TABLES_STREAMDATA);

		dst_idx = port->chip_port;
		ocelot_write(ocelot, ANA_TABLES_MACACCESS_VALID |
			     ANA_TABLES_MACACCESS_ENTRYTYPE(1) |
			     ANA_TABLES_MACACCESS_DEST_IDX(dst_idx) |
			     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_LEARN),
			     ANA_TABLES_MACACCESS);

		ocelot_write(ocelot, ANA_TABLES_MACACCESS_VALID |
			     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
			     ANA_TABLES_MACACCESS);

		regmap_field_read(ocelot->regfields[ANA_TABLES_MACTINDX_BUCKET],
				  &bucket);
		regmap_field_read(ocelot->regfields[ANA_TABLES_MACTINDX_M_INDEX],
				  &m_index);

		m_index = m_index * 4 + bucket;
		streamhandle_map[streamid->handle] = m_index;

		return 0;
	}

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMDATA_SFID_VALID |
		     ANA_TABLES_STREAMDATA_SFID(sfid) |
		     ANA_TABLES_STREAMDATA_SSID_VALID |
		     ANA_TABLES_STREAMDATA_SSID(ssid),
		     ANA_TABLES_STREAMDATA);

	reg = ocelot_read(ocelot, ANA_TABLES_MACACCESS);
	dst_idx = ANA_TABLES_MACACCESS_DEST_IDX_X(reg);
	ocelot_write(ocelot, ANA_TABLES_MACACCESS_VALID |
		     ANA_TABLES_MACACCESS_ENTRYTYPE(1) |
		     ANA_TABLES_MACACCESS_DEST_IDX(dst_idx) |
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_WRITE),
		     ANA_TABLES_MACACCESS);

	streamhandle_map[streamid->handle] = idx;

	return 0;
}

static int streamid_multi_forward_set(struct ocelot *ocelot, u32 index,
				      u8 fwdmask)
{
	u32 m_index;
	u32 bucket;
	u32 val;
	int m, n, i;
	u8 pgid_val, fwdport;
	u32 dst_idx;

	m_index = index / 4;
	bucket =  index % 4;

	regmap_field_write(ocelot->regfields[ANA_TABLES_MACTINDX_BUCKET],
			   bucket);
	regmap_field_write(ocelot->regfields[ANA_TABLES_MACTINDX_M_INDEX],
			   m_index);

	/*READ command MACACCESS.VALID(11 bit) must be 0 */
	ocelot_write(ocelot,
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_READ),
		     ANA_TABLES_MACACCESS);

	val = ocelot_read(ocelot, ANA_TABLES_MACACCESS);
	fwdport = ANA_TABLES_MACACCESS_DEST_IDX_X(val);

	if (fwdport >= MSCC_NUM_OUT_PORT) {
		dst_idx = fwdport;
		return 0;
	}

	fwdmask |= (1 << fwdport);

	m = ocelot->num_phys_ports - 1;
	for (i = m; i >= MSCC_NUM_OUT_PORT; i--) {
		if (fwdmask & (1 << i)) {
			dst_idx = PGID_MCRED +
				  (m - i) * MSCC_NUM_OUT_PORT +
				  fwdport;

			pgid_val = (1 << i) | (1 << fwdport);
			break;
		}
	}

	if (i < MSCC_NUM_OUT_PORT) {
		m = PGID_MCRED +
		    (ocelot->num_phys_ports - MSCC_NUM_OUT_PORT) *
		    MSCC_NUM_OUT_PORT;

		for (; i > 0; i--) {
			if (fwdmask & (1 << i))
				break;

			m = m + (1 << i) - 1;
		}
		n = fwdmask & ((1 << i) - 1);
		if (n) {
			dst_idx = m + n;
			pgid_val = fwdmask & ((1 << MSCC_NUM_OUT_PORT) - 1);
		} else {
			dst_idx = fwdport;
		}
	}

	if (dst_idx < PGID_MCRED)
		return 0;

	ocelot_write(ocelot, ANA_TABLES_MACACCESS_VALID |
		     ANA_TABLES_MACACCESS_ENTRYTYPE(1) |
		     ANA_TABLES_MACACCESS_DEST_IDX(dst_idx) |
		     ANA_TABLES_MACACCESS_MAC_TABLE_CMD(MACACCESS_CMD_WRITE),
		     ANA_TABLES_MACACCESS);

	ocelot_write_rix(ocelot, pgid_val, ANA_PGID_PGID, dst_idx);

	return 0;
}

int felix_qci_max_cap_get(struct net_device *ndev,
			  struct tsn_qci_psfp_stream_param *stream_para)
{
	/* MaxStreamFilterInstances */
	stream_para->max_sf_instance = capa.num_psfp_sfid;
	/* MaxStreamGateInstances */
	stream_para->max_sg_instance = capa.num_psfp_sgid;
	/* MaxFlowMeterInstances */
	stream_para->max_fm_instance = capa.psfp_fmi_max -
		capa.psfp_fmi_min + 1;
	/* SupportedListMax */
	stream_para->supported_list_max = capa.num_sgi_gcl;

	return 0;
}

int felix_qci_sfi_get(struct net_device *ndev, u32 index,
		      struct tsn_qci_psfp_sfi_conf *sfi)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 val, reg, fmeter_id, max_sdu;
	u32 sfid = index;

	if (sfid >= capa.num_psfp_sfid) {
		netdev_info(ndev, "Invalid index %u, maximum:%u\n",
			    sfid, capa.num_psfp_sfid);
		return -EINVAL;
	}

	/* In switch, streamhandle and sfid are the same parameter */
	sfi->stream_handle_spec = (s32)sfid;

	ocelot_field_write(ocelot, ANA_TABLES_SFIDTIDX_SFID_INDEX_0, sfid);

	ocelot_write(ocelot,
		     ANA_TABLES_SFIDACCESS_SFID_TBL_CMD(SFIDACCESS_CMD_READ),
		     ANA_TABLES_SFIDACCESS);

	val = ocelot_read(ocelot, ANA_TABLES_SFIDTIDX);
	if (!(val & ANA_TABLES_SFIDTIDX_SGID_VALID))
		return -EINVAL;

	sfi->stream_gate_instance_id = ANA_TABLES_SFIDTIDX_SGID_X(val);
	fmeter_id = ANA_TABLES_SFIDTIDX_POL_IDX_X(val);
	sfi->stream_filter.flow_meter_instance_id = fmeter_id;

	reg = ocelot_read(ocelot, ANA_TABLES_SFIDACCESS);
	max_sdu = ANA_TABLES_SFIDACCESS_MAX_SDU_LEN_X(reg);
	sfi->stream_filter.maximum_sdu_size  = max_sdu;

	if (reg & ANA_TABLES_SFIDACCESS_IGR_PRIO_MATCH_ENA)
		sfi->priority_spec = ANA_TABLES_SFIDACCESS_IGR_PRIO_X(reg);
	else
		netdev_info(ndev, "priority not enable\n");

	return 0;
}

int felix_qci_sfi_set(struct net_device *ndev, u32 index, bool enable,
		      struct tsn_qci_psfp_sfi_conf *sfi)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	int igr_prio = sfi->priority_spec;
	u16 sgid  = sfi->stream_gate_instance_id;
	u16 pol_idx;
	int fmid = sfi->stream_filter.flow_meter_instance_id;
	u16 max_sdu_len = sfi->stream_filter.maximum_sdu_size;
	int sfid = index;
	u32 val;

	if (fmid == -1)
		pol_idx = capa.psfp_fmi_max;
	else
		pol_idx = (u16)fmid;

	if (sfid >= capa.num_psfp_sfid) {
		netdev_info(ndev, "Invalid index %u, maximum:%u\n",
			    sfid, capa.num_psfp_sfid);
		return -EINVAL;
	}

	if (!enable) {
		val = ANA_TABLES_SFIDACCESS_SFID_TBL_CMD(SFIDACCESS_CMD_WRITE);
		ocelot_write(ocelot,
			     ANA_TABLES_SFIDTIDX_SFID_INDEX(sfid),
			     ANA_TABLES_SFIDTIDX);
		ocelot_write(ocelot, val, ANA_TABLES_SFIDACCESS);
		return 0;
	}

	if (sgid >= capa.num_psfp_sgid) {
		netdev_info(ndev, "Invalid sgid %u, maximum:%u\n",
			    sgid, capa.num_psfp_sgid);
		return -EINVAL;
	}
	if (pol_idx > capa.psfp_fmi_max || pol_idx < capa.psfp_fmi_min) {
		netdev_info(ndev, "Invalid pol_idx %u, range:%d~%d\n",
			    pol_idx, capa.psfp_fmi_min, capa.psfp_fmi_max);
		return -EINVAL;
	}

	ocelot_write(ocelot, ANA_TABLES_SFIDTIDX_SGID_VALID |
		     ANA_TABLES_SFIDTIDX_SGID(sgid) |
		     ((fmid != -1) ? ANA_TABLES_SFIDTIDX_POL_ENA : 0) |
		     ANA_TABLES_SFIDTIDX_POL_IDX(pol_idx) |
		     ANA_TABLES_SFIDTIDX_SFID_INDEX(sfid),
		     ANA_TABLES_SFIDTIDX);

	ocelot_write(ocelot,
		     ((igr_prio >= 0) ?
		      ANA_TABLES_SFIDACCESS_IGR_PRIO_MATCH_ENA : 0) |
		     ANA_TABLES_SFIDACCESS_IGR_PRIO(igr_prio) |
		     ANA_TABLES_SFIDACCESS_MAX_SDU_LEN(max_sdu_len) |
		     ANA_TABLES_SFIDACCESS_SFID_TBL_CMD(SFIDACCESS_CMD_WRITE),
		     ANA_TABLES_SFIDACCESS);

	return 0;
}

int felix_qci_sfi_counters_get(struct net_device *ndev, u32 index,
			       struct tsn_qci_psfp_sfi_counters *sfi_counters)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 sfid = index;
	u32 match, not_pass, not_pass_sdu, red;

	if (sfid >= capa.num_psfp_sfid) {
		netdev_info(ndev, "Invalid index %u, maximum:%u\n",
			    sfid, capa.num_psfp_sfid);
		return -EINVAL;
	}

	ocelot_field_write(ocelot, SYS_STAT_CFG_STAT_VIEW_0, sfid);
	match = ocelot_read_gix(ocelot, SYS_CNT, 0x200);
	not_pass = ocelot_read_gix(ocelot, SYS_CNT, 0x201);
	not_pass_sdu = ocelot_read_gix(ocelot, SYS_CNT, 0x202);
	red = ocelot_read_gix(ocelot, SYS_CNT, 0x203);

	sfi_counters->matching_frames_count = match;
	sfi_counters->not_passing_frames_count = not_pass;
	sfi_counters->not_passing_sdu_count = not_pass_sdu;
	sfi_counters->red_frames_count  =  red;

	sfi_counters->passing_frames_count = match - not_pass;
	sfi_counters->passing_sdu_count = match - not_pass - not_pass_sdu;

	return 0;
}

static int sgi_set_glist(struct ocelot *ocelot,
			 struct tsn_qci_psfp_gcl *gcl, uint32_t num)
{
	u32 time_sum = 0;
	int i;

	if (num > capa.num_sgi_gcl)
		return -EINVAL;

	for (i = 0; i < num; i++) {
		u32 val = ANA_SG_GCL_GS_CONFIG_IPS((gcl->ipv < 0) ?
						   0 : gcl->ipv + 8);
		val |= (gcl->gate_state ? ANA_SG_GCL_GS_CONFIG_GATE_STATE : 0);
		ocelot_write_rix(ocelot, val, ANA_SG_GCL_GS_CONFIG, i);

		time_sum += gcl->time_interval;
		ocelot_write_rix(ocelot, time_sum, ANA_SG_GCL_TI_CONFIG, i);

		gcl++;
	}

	return 0;
}

static u32 sgi_read_status(struct ocelot *ocelot)
{
	u32 val;

	val = ocelot_read(ocelot, ANA_SG_ACCESS_CTRL);

	return val;
}

int felix_qci_sgi_set(struct net_device *ndev, u32 index,
		      struct tsn_qci_psfp_sgi_conf *sgi_conf)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	struct tsn_qci_sg_control *admin_list = &sgi_conf->admin;
	u32 sgid = index;
	u32 list_length = sgi_conf->admin.control_list_length;
	u32 cycle_time = sgi_conf->admin.cycle_time;
	u32 cycle_time_ex = sgi_conf->admin.cycle_time_extension;
	u32 l_basetime = sgi_conf->admin.base_time % 1000000000;
	u64 h_basetime = sgi_conf->admin.base_time / 1000000000;
	u64 cur_time;
	u32 val;
	int ret;

	if (sgid >= capa.num_psfp_sgid) {
		netdev_info(ndev, "Invalid sgid %u, maximum:%u\n",
			    sgid, capa.num_psfp_sgid);
		return -EINVAL;
	}
	if ((cycle_time < capa.sgi_ct_min ||
	     cycle_time > capa.sgi_ct_max) &&
	     sgi_conf->gate_enabled) {
		netdev_info(ndev, "Invalid cycle_time %u ns\n",
			    cycle_time);
		return -EINVAL;
	}
	if (cycle_time_ex > capa.sgi_cte_max) {
		netdev_info(ndev,
			    "Invalid cycle_time_extension %u\n",
			    cycle_time_ex);
		return -EINVAL;
	}
	if (list_length > capa.num_sgi_gcl) {
		netdev_info(ndev, "Invalid sgi_gcl len %u, maximum:%u\n",
			    list_length, capa.num_sgi_gcl);
		return -EINVAL;
	}

	/*configure SGID*/
	ocelot_field_write(ocelot, ANA_SG_ACCESS_CTRL_SGID_0, sgid);

	/*Disable SG*/
	if (!sgi_conf->gate_enabled) {
		ocelot_rmw(ocelot,
			   ANA_SG_CONFIG_REG_3_INIT_GATE_STATE,
			   ANA_SG_CONFIG_REG_3_INIT_GATE_STATE |
			   ANA_SG_CONFIG_REG_3_GATE_ENABLE,
			   ANA_SG_CONFIG_REG_3);
		return 0;
	}

	/*admin parameters*/
	cur_time = ocelot_read(ocelot, PTP_CUR_SEC_MSB);
	cur_time = cur_time << 32;
	cur_time += ocelot_read(ocelot, PTP_CUR_SEC_LSB);
	if (h_basetime < cur_time) {
		h_basetime = cur_time;
		l_basetime = ocelot_read(ocelot, PTP_CUR_NSEC);
	}

	ocelot_write(ocelot, l_basetime, ANA_SG_CONFIG_REG_1);
	ocelot_write(ocelot, h_basetime, ANA_SG_CONFIG_REG_2);

	ocelot_write(ocelot,
		     (sgi_conf->admin.init_ipv < 0 ?
		      0 : ANA_SG_CONFIG_REG_3_IPV_VALID) |
		     ANA_SG_CONFIG_REG_3_INIT_IPV(sgi_conf->admin.init_ipv) |
		     ANA_SG_CONFIG_REG_3_GATE_ENABLE |
		     ANA_SG_CONFIG_REG_3_LIST_LENGTH(list_length) |
		     (sgi_conf->admin.gate_states > 0 ?
		      ANA_SG_CONFIG_REG_3_INIT_GATE_STATE : 0) |
		     ANA_SG_CONFIG_REG_3_BASE_TIME_SEC_MSB(h_basetime >> 32),
		     ANA_SG_CONFIG_REG_3);

	ocelot_write(ocelot, cycle_time, ANA_SG_CONFIG_REG_4);
	ocelot_write(ocelot, cycle_time_ex, ANA_SG_CONFIG_REG_5);

	ret = sgi_set_glist(ocelot, admin_list->gcl, list_length);
	if (ret < 0)
		return ret;

	/*CONG_CHANGE TO 1*/
	ocelot_field_write(ocelot, ANA_SG_ACCESS_CTRL_CONFIG_CHANGE_0, 1);

	ret = readx_poll_timeout(sgi_read_status, ocelot, val,
				 (!(ANA_SG_ACCESS_CTRL_CONFIG_CHANGE & val)),
				 10, 100000);

	return ret;
}

static int sgi_get_glist(struct ocelot *ocelot,
			 struct tsn_qci_psfp_gcl *gcl,
			 uint32_t num)
{
	int i;
	u16 val;
	u32 time = 0;
	u32 reg;

	if (num > capa.num_sgi_gcl)
		return -EINVAL;

	for (i = 0; i < num; i++) {
		val = ocelot_read_rix(ocelot, ANA_SG_GCL_GS_CONFIG, i);
		gcl->gate_state = (val & ANA_SG_GCL_GS_CONFIG_GATE_STATE);

		if (val & ANA_SG_GCL_GS_CONFIG_IPV_VALID)
			gcl->ipv = ANA_SG_GCL_GS_CONFIG_IPV(val);
		else
			gcl->ipv = -1;

		reg = ocelot_read_rix(ocelot, ANA_SG_GCL_TI_CONFIG, i);
		gcl->time_interval = (reg - time);
		time = reg;

		gcl++;
	}

	return 0;
}

int felix_qci_sgi_get(struct net_device *ndev, u32 index,
		      struct tsn_qci_psfp_sgi_conf *sgi_conf)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	struct tsn_qci_sg_control *admin  = &sgi_conf->admin;
	struct tsn_qci_psfp_gcl *glist;
	u32 val, reg;
	u32 list_num;
	int ret;

	if (index >= capa.num_psfp_sgid) {
		netdev_info(ndev, "Invalid sgid %u, maximum:%u\n",
			    index, capa.num_psfp_sgid);
		return -EINVAL;
	}

	ocelot_field_write(ocelot, ANA_SG_ACCESS_CTRL_SGID_0, index);

	admin->cycle_time = ocelot_read(ocelot, ANA_SG_CONFIG_REG_4);
	admin->cycle_time_extension =
		ocelot_read(ocelot, ANA_SG_CONFIG_REG_5);

	val = ocelot_read(ocelot, ANA_SG_CONFIG_REG_2);
	admin->base_time = val;

	reg = ocelot_read(ocelot, ANA_SG_CONFIG_REG_1);
	val = ocelot_read(ocelot, ANA_SG_CONFIG_REG_3);

	admin->base_time +=
		ANA_SG_CONFIG_REG_3_BASE_TIME_SEC_MSB(val) << 32;

	admin->base_time = admin->base_time * 1000000000 + reg;

	if (val & ANA_SG_CONFIG_REG_3_IPV_VALID)
		admin->init_ipv = ANA_SG_CONFIG_REG_3_INIT_IPV_X(val);
	else
		admin->init_ipv = -1;

	if (val & ANA_SG_CONFIG_REG_3_GATE_ENABLE)
		sgi_conf->gate_enabled = 1;

	admin->control_list_length = ANA_SG_CONFIG_REG_3_LIST_LENGTH_X(val);

	list_num = admin->control_list_length;

	glist = kmalloc_array(list_num, sizeof(struct tsn_qci_psfp_gcl),
			      GFP_KERNEL);
	admin->gcl = glist;

	ret = sgi_get_glist(ocelot, glist, list_num);

	return ret;
}

int felix_qci_sgi_status_get(struct net_device *ndev, u16 index,
			     struct tsn_psfp_sgi_status *sgi_status)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 val, reg;

	if (index >= capa.num_psfp_sgid) {
		netdev_info(ndev, "Invalid sgid %u, maximum:%u\n",
			    index, capa.num_psfp_sgid);
		return -EINVAL;
	}

	/*SET SGID*/
	ocelot_field_write(ocelot, ANA_SG_ACCESS_CTRL_SGID_0, index);

	val = ocelot_read(ocelot, ANA_SG_STATUS_REG_2);
	sgi_status->config_change_time = val;

	reg = ocelot_read(ocelot, ANA_SG_STATUS_REG_1);
	val = ocelot_read(ocelot, ANA_SG_STATUS_REG_3);
	sgi_status->config_change_time +=
		ANA_SG_STATUS_REG_3_CFG_CHG_TIME_SEC_MSB(val) << 32;
	sgi_status->config_change_time =
		sgi_status->config_change_time * 1000000000 + reg;

	if (val & ANA_SG_STATUS_REG_3_CONFIG_PENDING)
		sgi_status->config_pending  = TRUE;
	else
		sgi_status->config_pending = FALSE;

	if (val & ANA_SG_STATUS_REG_3_GATE_STATE)
		sgi_status->oper.gate_states  =  TRUE;
	else
		sgi_status->oper.gate_states  =  FALSE;
	/*bit 3 encoding 0:IPV [0:2]is invalid . 1:IPV[0:2] is valid*/
	if (val & ANA_SG_STATUS_REG_3_IPV_VALID)
		sgi_status->oper.init_ipv = ANA_SG_STATUS_REG_3_IPV_X(val);
	else
		sgi_status->oper.init_ipv = -1;

	return 0;
}

int felix_qci_fmi_set(struct net_device *ndev, u32 index,
		      bool enable, struct tsn_qci_psfp_fmi *fmi)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 cir = 0, cbs = 0, pir = 0, pbs = 0;
	u32 cir_ena = 0;
	u32 pbs_max = 0, cbs_max = 0;
	bool cir_discard = 0, pir_discard = 0;

	if (index > capa.qos_pol_max) {
		netdev_info(ndev, "Invalid pol_idx %u, maximum: %u\n",
			    index, capa.qos_pol_max);
		return -EINVAL;
	}

	if (fmi->mark_red_enable && fmi->mark_red) {
		fmi->eir = 0;
		fmi->ebs = 0;
		fmi->cir = 0;
		fmi->cbs = 0;
	}

	pir = fmi->eir;
	pbs = fmi->ebs;

	if (!fmi->drop_on_yellow)
		cir_ena = 1;

	if (cir_ena) {
		cir = fmi->cir;
		cbs = fmi->cbs;
		if (cir == 0 && cbs == 0) {
			cir_discard = 1;
		} else {
			cir = DIV_ROUND_UP(cir, 100);
			cir *= 3;  /* Rate unit is 33 1/3 kbps */
			cbs = DIV_ROUND_UP(cbs, 4096);
			cbs = (cbs ? cbs : 1);
			cbs_max = capa.pol_cbs_max;
			if (fmi->cf)
				pir += fmi->cir;
		}
	}

	if (pir == 0 && pbs == 0) {
		pir_discard = 1;
	} else {
		pir = DIV_ROUND_UP(pir, 100);
		pir *= 3;  /* Rate unit is 33 1/3 kbps */
		pbs = DIV_ROUND_UP(pbs, 4096);
		pbs = (pbs ? pbs : 1);
		pbs_max = capa.pol_pbs_max;
	}
	pir = min_t(u32, GENMASK(15, 0), pir);
	cir = min_t(u32, GENMASK(15, 0), cir);
	pbs = min(pbs_max, pbs);
	cbs = min(cbs_max, cbs);

	ocelot_write_gix(ocelot, (ANA_POL_MODE_CFG_IPG_SIZE(20) |
			 ANA_POL_MODE_CFG_FRM_MODE(1) |
			 (fmi->cf ? ANA_POL_MODE_CFG_DLB_COUPLED : 0) |
			 (cir_ena ? ANA_POL_MODE_CFG_CIR_ENA : 0) |
			 ANA_POL_MODE_CFG_OVERSHOOT_ENA),
			 ANA_POL_MODE_CFG, index);

	ocelot_write_gix(ocelot, ANA_POL_PIR_CFG_PIR_RATE(pir) |
			 ANA_POL_PIR_CFG_PIR_BURST(pbs),
			 ANA_POL_PIR_CFG, index);

	ocelot_write_gix(ocelot,
			 (pir_discard ? GENMASK(22, 0) : 0),
			 ANA_POL_PIR_STATE, index);

	ocelot_write_gix(ocelot, ANA_POL_CIR_CFG_CIR_RATE(cir) |
			 ANA_POL_CIR_CFG_CIR_BURST(cbs),
			 ANA_POL_CIR_CFG, index);

	ocelot_write_gix(ocelot,
			 (cir_discard ? GENMASK(22, 0) : 0),
			 ANA_POL_CIR_STATE, index);

	return 0;
}

int felix_qci_fmi_get(struct net_device *ndev, u32 index,
		      struct tsn_qci_psfp_fmi *fmi,
		      struct tsn_qci_psfp_fmi_counters *counters)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 val, reg;

	if (index > capa.qos_pol_max) {
		netdev_info(ndev, "Invalid pol_idx %u, maximum: %u\n",
			    index, capa.qos_pol_max);
		return -EINVAL;
	}

	val = ocelot_read_gix(ocelot, ANA_POL_PIR_CFG, index);
	reg = ocelot_read_gix(ocelot, ANA_POL_CIR_CFG, index);

	fmi->eir = ANA_POL_PIR_CFG_PIR_RATE_X(val);
	fmi->eir = fmi->eir * 100 / 3;
	fmi->ebs = ANA_POL_PIR_CFG_PIR_BURST(val);
	fmi->ebs *= 4096;
	fmi->cir = ANA_POL_CIR_CFG_CIR_RATE_X(reg);
	fmi->cir = fmi->cir * 100 / 3;
	fmi->cbs = ANA_POL_CIR_CFG_CIR_BURST(reg);
	fmi->cbs *= 4096;
	if (!(fmi->eir | fmi->ebs | fmi->cir | fmi->cbs))
		fmi->mark_red = TRUE;
	else
		fmi->mark_red = FALSE;

	val = ocelot_read_gix(ocelot, ANA_POL_MODE_CFG, index);
	if (val & ANA_POL_MODE_CFG_DLB_COUPLED)
		fmi->cf = TRUE;
	else
		fmi->cf = FALSE;
	if (val & ANA_POL_MODE_CFG_CIR_ENA)
		fmi->drop_on_yellow = FALSE;
	else
		fmi->drop_on_yellow = TRUE;

	return 0;
}

int felix_seq_gen_set(struct net_device *ndev, u32 index,
		      struct tsn_seq_gen_conf *sg_conf)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u8 iport_mask = sg_conf->iport_mask;
	u8 split_mask = sg_conf->split_mask;
	u8 seq_len = sg_conf->seq_len;
	u32 seq_num = sg_conf->seq_num;

	if (index >= capa.num_frer_ssid) {
		netdev_info(ndev, "Invalid SSID %u, maximum:%u\n",
			    index, capa.num_frer_ssid - 1);
		return -EINVAL;
	}
	if (seq_len < capa.frer_seq_len_min ||
	    seq_len > capa.frer_seq_len_max) {
		netdev_info(ndev,
			    "Invalid seq_space_bits num %u,range:%d~%d\n",
			    seq_len,
			    capa.frer_seq_len_min,
			    capa.frer_seq_len_max);
		return -EINVAL;
	}

	streamid_multi_forward_set(ocelot,
				   streamhandle_map[index],
				   split_mask);

	ocelot_write(ocelot,
		     ANA_TABLES_SEQ_MASK_SPLIT_MASK(split_mask) |
		     ANA_TABLES_SEQ_MASK_INPUT_PORT_MASK(iport_mask),
		     ANA_TABLES_SEQ_MASK);

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMTIDX_S_INDEX(index) |
		     ANA_TABLES_STREAMTIDX_STREAM_SPLIT |
		     ANA_TABLES_STREAMTIDX_SEQ_SPACE_LOG2(seq_len),
		     ANA_TABLES_STREAMTIDX);

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMACCESS_GEN_REC_SEQ_NUM(seq_num) |
		     ANA_TABLES_STREAMACCESS_SEQ_GEN_REC_ENA |
		     ANA_TABLES_STREAMACCESS_STREAM_TBL_CMD(SFIDACCESS_CMD_WRITE),
		     ANA_TABLES_STREAMACCESS);

	return 0;
}

int felix_seq_rec_set(struct net_device *ndev, u32 index,
		      struct tsn_seq_rec_conf *sr_conf)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u8 seq_len = sr_conf->seq_len;
	u8 hislen = sr_conf->his_len;

	if (index >= capa.num_frer_ssid) {
		netdev_info(ndev, "Invalid SSID %u, maximum:%u\n",
			    index, capa.num_frer_ssid - 1);
		return -EINVAL;
	}
	if (seq_len < capa.frer_seq_len_min ||
	    seq_len > capa.frer_seq_len_max) {
		netdev_info(ndev,
			    "Invalid seq_space_bits num %u,range:%d~%d\n",
			    seq_len,
			    capa.frer_seq_len_min,
			    capa.frer_seq_len_max);
		return -EINVAL;
	}
	if (hislen < capa.frer_his_len_min ||
	    hislen > capa.frer_his_len_max) {
		netdev_info(ndev,
			    "Invalid history_bits num %u,range:%d~%d\n",
			    hislen,
			    capa.frer_his_len_min,
			    capa.frer_his_len_max);
		return -EINVAL;
	}

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMTIDX_S_INDEX(index) |
		     ANA_TABLES_STREAMTIDX_FORCE_SF_BEHAVIOUR |
		     ANA_TABLES_STREAMTIDX_SEQ_HISTORY_LEN(hislen) |
		     ANA_TABLES_STREAMTIDX_RESET_ON_ROGUE |
		     (sr_conf->rtag_pop_en ?
		      ANA_TABLES_STREAMTIDX_REDTAG_POP : 0) |
		     ANA_TABLES_STREAMTIDX_SEQ_SPACE_LOG2(seq_len),
		     ANA_TABLES_STREAMTIDX);

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMACCESS_SEQ_GEN_REC_ENA |
		     ANA_TABLES_STREAMACCESS_GEN_REC_TYPE |
		     ANA_TABLES_STREAMACCESS_STREAM_TBL_CMD(SFIDACCESS_CMD_WRITE),
		     ANA_TABLES_STREAMACCESS);

	return 0;
}

int felix_cb_get(struct net_device *ndev, u32 index,
		 struct tsn_cb_status *c)
{
	u32 val;
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;

	if (index >= capa.num_frer_ssid) {
		netdev_info(ndev, "Invalid SSID %u, maximum:%u\n",
			    index, capa.num_frer_ssid - 1);
		return -EINVAL;
	}

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMTIDX_S_INDEX(index),
		     ANA_TABLES_STREAMTIDX);

	ocelot_write(ocelot,
		     ANA_TABLES_STREAMACCESS_STREAM_TBL_CMD(SFIDACCESS_CMD_READ),
		     ANA_TABLES_STREAMACCESS);

	val = ocelot_read(ocelot, ANA_TABLES_STREAMACCESS);
	c->gen_rec = (ANA_TABLES_STREAMACCESS_GEN_REC_TYPE & val) >> 2;
	c->seq_num = ANA_TABLES_STREAMACCESS_GEN_REC_SEQ_NUM_X(val);

	val = ocelot_read(ocelot, ANA_TABLES_STREAMTIDX);
	c->err = ANA_TABLES_STREAMTIDX_SEQ_GEN_ERR_STATUS_X(val);
	c->his_len = ANA_TABLES_STREAMTIDX_SEQ_HISTORY_LEN_X(val);
	c->seq_len = ANA_TABLES_STREAMTIDX_SEQ_SPACE_LOG2(val);

	val = ocelot_read(ocelot, ANA_TABLES_SEQ_MASK);
	c->split_mask = ANA_TABLES_SEQ_MASK_SPLIT_MASK_X(val);
	c->iport_mask = ANA_TABLES_SEQ_MASK_INPUT_PORT_MASK(val);

	c->seq_his = ocelot_read(ocelot, ANA_TABLES_SEQ_HISTORY);

	return 0;
}

static int pcp_map_enable(struct ocelot *ocelot, u8 port)
{
	int i;

	ocelot_rmw_gix(ocelot,
		       ANA_PORT_QOS_CFG_QOS_PCP_ENA,
		       ANA_PORT_QOS_CFG_QOS_PCP_ENA,
		       ANA_PORT_QOS_CFG,
		       port);

	for (i = 0; i < NUM_MSCC_QOS_PRIO * 2; i++) {
		ocelot_rmw_ix(ocelot,
			      (ANA_PORT_PCP_DEI_MAP_DP_PCP_DEI_VAL & i) |
			      ANA_PORT_PCP_DEI_MAP_QOS_PCP_DEI_VAL(i),
			      ANA_PORT_PCP_DEI_MAP_DP_PCP_DEI_VAL |
			      ANA_PORT_PCP_DEI_MAP_QOS_PCP_DEI_VAL_M,
			      ANA_PORT_PCP_DEI_MAP,
			      port, i);
	}

	return 0;
}

static int rtag_parse_enable(struct ocelot *ocelot, u8 port)
{
	ocelot_rmw_rix(ocelot,
		       ANA_PORT_MODE_REDTAG_PARSE_CFG,
		       ANA_PORT_MODE_REDTAG_PARSE_CFG,
		       ANA_PORT_MODE,
		       port);

	return 0;
}

int felix_dscp_set(struct net_device *ndev,
		   bool enable,
		   const u8 dscp_ix,
		   struct tsn_qos_switch_dscp_conf *c)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;
	u32 val, ri = dscp_ix;

	c->dscp = 0;
	c->trust = 1;
	c->remark = 0;

	if (dscp_ix > capa.qos_dscp_max) {
		netdev_info(ndev, "Invalid dscp_ix %u\n", dscp_ix);
		return -EINVAL;
	}
	if (c->cos > capa.qos_cos_max) {
		netdev_info(ndev, "Invalid cos %d\n", c->cos);
		return -EINVAL;
	}
	if (c->dpl > capa.qos_dp_max) {
		netdev_info(ndev, "Invalid dpl %d\n", c->dpl);
		return -EINVAL;
	}

	ocelot_rmw_gix(ocelot,
		       (enable ? ANA_PORT_QOS_CFG_QOS_DSCP_ENA : 0) |
		       (c->dscp ? ANA_PORT_QOS_CFG_DSCP_TRANSLATE_ENA : 0),
		       ANA_PORT_QOS_CFG_QOS_DSCP_ENA |
		       ANA_PORT_QOS_CFG_DSCP_TRANSLATE_ENA,
		       ANA_PORT_QOS_CFG,
		       port->chip_port);

	val = (c->dpl ? ANA_DSCP_CFG_DP_DSCP_VAL : 0) |
	       ANA_DSCP_CFG_QOS_DSCP_VAL(c->cos) |
	       ANA_DSCP_CFG_DSCP_TRANSLATE_VAL(c->dscp) |
	       (c->trust ? ANA_DSCP_CFG_DSCP_TRUST_ENA : 0) |
	       (c->remark ? ANA_DSCP_CFG_DSCP_REWR_ENA : 0);

	ocelot_write_rix(ocelot, val, ANA_DSCP_CFG, ri);

	return 0;
}

void felix_tsn_init(struct net_device *ndev)
{
	struct ocelot_port *port = netdev_priv(ndev);
	struct ocelot *ocelot = port->ocelot;

	pcp_map_enable(ocelot, port->chip_port);
	rtag_parse_enable(ocelot, port->chip_port);
}
