// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright (C) 2016 Freescale Semiconductor, Inc.
 * Copyright 2017~2018 NXP
 *  Author: Dong Aisheng <aisheng.dong@nxp.com>
 *
 * File containing client-side RPC functions for the MISC service. These
 * function are ported to clients that communicate to the SC.
 *
 */

#include <linux/firmware/imx/svc/misc.h>

struct imx_sc_msg_req_misc_set_ctrl {
	struct imx_sc_rpc_msg hdr;
	u32 ctrl;
	u32 val;
	u16 resource;
} __packed __aligned(4);


struct imx_sc_msg_req_misc_set_dma_group {
	struct imx_sc_rpc_msg hdr;
	u16 resource;
	u8 val;
} __packed __aligned(4);

struct imx_sc_msg_req_cpu_start {
	struct imx_sc_rpc_msg hdr;
	u32 address_hi;
	u32 address_lo;
	u16 resource;
	u8 enable;
} __packed __aligned(4);

struct imx_sc_msg_req_misc_get_ctrl {
	struct imx_sc_rpc_msg hdr;
	u32 ctrl;
	u16 resource;
} __packed __aligned(4);

struct imx_sc_msg_resp_misc_get_ctrl {
	struct imx_sc_rpc_msg hdr;
	u32 val;
} __packed __aligned(4);

/* TO DO:  Add PMIC I2C address (now hardcoded to MEKs PMIC1 on SCU)*/
struct imx_sc_msg_req_misc_get_mode {
	struct imx_sc_rpc_msg hdr;
	u32 pmic_reg; /* PMIC register */
	u32 data; /* Data to store on register */
	u32 dataLength /* Data length in bytes */;
} __packed __aligned(4);

struct imx_sc_msg_resp_misc_get_mode {
	struct imx_sc_rpc_msg hdr;
	/* IDNEO: to do */
} __packed __aligned(4);

/*
 * This function sets a miscellaneous control value.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource the control is associated with
 * @param[in]     ctrl        control to change
 * @param[in]     val         value to apply to the control
 *
 * @return Returns 0 for success and < 0 for errors.
 */

int imx_sc_misc_set_control(struct imx_sc_ipc *ipc, u32 resource,
			    u8 ctrl, u32 val)
{
	struct imx_sc_msg_req_misc_set_ctrl msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = (uint8_t)IMX_SC_RPC_SVC_MISC;
	hdr->func = (uint8_t)IMX_SC_MISC_FUNC_SET_CONTROL;
	hdr->size = 4;

	msg.ctrl = ctrl;
	msg.val = val;
	msg.resource = resource;

	return imx_scu_call_rpc(ipc, &msg, true);
}
EXPORT_SYMBOL(imx_sc_misc_set_control);

int sc_misc_board_ioctl(struct imx_sc_ipc *ipc, uint32_t *parm1,
				     uint32_t *parm2, uint32_t *parm3)
{
	/* TO DO: Add PMIC I2C address as parameter (now hardcoded to MEKs PMIC1 on SCU)*/
	/*        For this, "board_ioctl" function on SCU board.c must be modified. */
	struct imx_sc_msg_req_misc_get_mode msg;
	struct imx_sc_msg_resp_misc_get_mode *resp; /* IDNEO: Not implemented */
	struct imx_sc_rpc_msg *hdr = &msg.hdr;
	int ret;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = (uint8_t)IMX_SC_RPC_SVC_MISC;
	hdr->func = (uint8_t)IMX_SC_MISC_FUNC_BOARD_IOCTL;
	hdr->size = 4;

	msg.pmic_reg = (u32)(*parm1);
	msg.data = (u32)(*parm2);
	msg.dataLength = (u32)(*parm3);

	ret = imx_scu_get_handle(&ipc); // IDNEO: Supported if CONFIG_IMX_SCU is set
	if (ret) {
		pr_err("failed to get scu ipc handle %d\n", ret);
		return ret;
	}

	pr_info("Reg = 0x%x, datos = %d\n", msg.pmic_reg, msg.data);

	ret = imx_scu_call_rpc(ipc, &msg, true); // IDNEO: Supported if CONFIG_IMX_SCU is set
	
	if (ret)
		return ret;

	pr_info("Received Response\r\n");

	return 0;

}
EXPORT_SYMBOL(sc_misc_board_ioctl);

/* To demonstrate how we need to call sc_misc_board_ioctl from application */
int scu_pmic_ioctl(u32 pmic_reg, u32 data, u32 dataLength)
{
	int ret;
	u32 parm1 = pmic_reg;  
	u32 parm2 = data;  
	u32 parm3 = dataLength;

	/* TO DO:  Add PMIC I2C address as parameter (now hardcoded to MEKs PMIC1 on SCU)*/
	ret = sc_misc_board_ioctl(NULL, &parm1, &parm2, &parm3);
	if (ret)
		return ret;

	//printk("Mode value: %d\r\n", parm3);
	
	return 0;
}
EXPORT_SYMBOL(scu_pmic_ioctl);

int imx_sc_misc_set_dma_group(struct imx_sc_ipc *ipc, u32 resource,
			    u32 val)
{
	struct imx_sc_msg_req_misc_set_dma_group msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = (uint8_t)IMX_SC_RPC_SVC_MISC;
	hdr->func = (uint8_t)IMX_SC_MISC_FUNC_SET_DMA_GROUP;
	hdr->size = 2;

	msg.val = val;
	msg.resource = resource;

	return imx_scu_call_rpc(ipc, &msg, true);
}
EXPORT_SYMBOL(imx_sc_misc_set_dma_group);

/*
 * This function gets a miscellaneous control value.
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource the control is associated with
 * @param[in]     ctrl        control to get
 * @param[out]    val         pointer to return the control value
 *
 * @return Returns 0 for success and < 0 for errors.
 */

int imx_sc_misc_get_control(struct imx_sc_ipc *ipc, u32 resource,
			    u8 ctrl, u32 *val)
{
	struct imx_sc_msg_req_misc_get_ctrl msg;
	struct imx_sc_msg_resp_misc_get_ctrl *resp;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;
	int ret;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = (uint8_t)IMX_SC_RPC_SVC_MISC;
	hdr->func = (uint8_t)IMX_SC_MISC_FUNC_GET_CONTROL;
	hdr->size = 3;

	msg.ctrl = ctrl;
	msg.resource = resource;

	ret = imx_scu_call_rpc(ipc, &msg, true);
	if (ret)
		return ret;

	resp = (struct imx_sc_msg_resp_misc_get_ctrl *)&msg;
	if (val != NULL)
		*val = resp->val;

	return 0;
}
EXPORT_SYMBOL(imx_sc_misc_get_control);

/*
 * This function starts/stops a CPU identified by @resource
 *
 * @param[in]     ipc         IPC handle
 * @param[in]     resource    resource the control is associated with
 * @param[in]     enable      true for start, false for stop
 * @param[in]     phys_addr   initial instruction address to be executed
 *
 * @return Returns 0 for success and < 0 for errors.
 */
int imx_sc_pm_cpu_start(struct imx_sc_ipc *ipc, u32 resource,
			bool enable, u64 phys_addr)
{
	struct imx_sc_msg_req_cpu_start msg;
	struct imx_sc_rpc_msg *hdr = &msg.hdr;

	hdr->ver = IMX_SC_RPC_VERSION;
	hdr->svc = IMX_SC_RPC_SVC_PM;
	hdr->func = IMX_SC_PM_FUNC_CPU_START;
	hdr->size = 4;

	msg.address_hi = phys_addr >> 32;
	msg.address_lo = phys_addr;
	msg.resource = resource;
	msg.enable = enable;

	return imx_scu_call_rpc(ipc, &msg, true);
}
EXPORT_SYMBOL(imx_sc_pm_cpu_start);
