// SPDX-License-Identifier: GPL-2.0+
/*
 * Copyright 2021 NXP
 */

#include <linux/bitfield.h>
#include <linux/clk.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/iopoll.h>
#include <linux/mfd/syscon.h>
#include <linux/mfd/syscon/imx7-iomuxc-gpr.h>
#include <linux/module.h>
#include <linux/of_device.h>
#include <linux/phy/phy.h>
#include <linux/platform_device.h>
#include <linux/regmap.h>
#include <linux/reset.h>

#include <dt-bindings/phy/phy-imx8-pcie.h>

#define IMX8MM_PCIE_PHY_CMN_REG061	0x184
#define  ANA_PLL_CLK_OUT_TO_EXT_IO_EN	BIT(0)
#define IMX8MM_PCIE_PHY_CMN_REG062	0x188
#define  ANA_PLL_CLK_OUT_TO_EXT_IO_SEL	BIT(3)
#define IMX8MM_PCIE_PHY_CMN_REG063	0x18C
#define  AUX_PLL_REFCLK_SEL_SYS_PLL	GENMASK(7, 6)
#define IMX8MM_PCIE_PHY_CMN_REG064	0x190
#define  ANA_AUX_RX_TX_SEL_TX		BIT(7)
#define  ANA_AUX_RX_TERM_GND_EN		BIT(3)
#define  ANA_AUX_TX_TERM		BIT(2)
#define IMX8MM_PCIE_PHY_CMN_REG065	0x194
#define  ANA_AUX_RX_TERM		(BIT(7) | BIT(4))
#define  ANA_AUX_TX_LVL			GENMASK(3, 0)
#define IMX8MM_PCIE_PHY_CMN_REG075	0x1D4
#define  ANA_PLL_DONE			0x3
#define PCIE_PHY_TRSV_REG5		0x414
#define PCIE_PHY_TRSV_REG6		0x418

#define IMX8MM_GPR_PCIE_REF_CLK_SEL	GENMASK(25, 24)
#define IMX8MM_GPR_PCIE_REF_CLK_PLL	FIELD_PREP(IMX8MM_GPR_PCIE_REF_CLK_SEL, 0x3)
#define IMX8MM_GPR_PCIE_REF_CLK_EXT	FIELD_PREP(IMX8MM_GPR_PCIE_REF_CLK_SEL, 0x2)
#define IMX8MM_GPR_PCIE_AUX_EN		BIT(19)
#define IMX8MM_GPR_PCIE_CMN_RST		BIT(18)
#define IMX8MM_GPR_PCIE_POWER_OFF	BIT(17)
#define IMX8MM_GPR_PCIE_SSC_EN		BIT(16)
#define IMX8MM_GPR_PCIE_AUX_EN_OVERRIDE	BIT(9)

#define IMX8MP_PCIE_PHY_TRSV_REG001	0x404
#define  LN0_OVRD_TX_DRV_LVL_G1		0x3F
#define IMX8MP_PCIE_PHY_TRSV_REG002	0x408
#define  LN0_OVRD_TX_DRV_LVL_G2		0x1F
#define IMX8MP_PCIE_PHY_TRSV_REG003	0x40C
#define  LN0_OVRD_TX_DRV_LVL_G3		0x1F
#define IMX8MP_PCIE_PHY_TRSV_REG005	0x414
#define  LN0_OVRD_TX_DRV_PST_LVL_G1	0x2B
#define IMX8MP_PCIE_PHY_TRSV_REG006	0x418
#define  LN0_OVRD_TX_DRV_PST_LVL_G2	0xB
#define IMX8MP_PCIE_PHY_TRSV_REG007	0x41C
#define  LN0_OVRD_TX_DRV_PST_LVL_G3	0xB
#define IMX8MP_PCIE_PHY_TRSV_REG009	0x424
#define  LN0_OVRD_TX_DRV_PRE_LVL_G1	0x15
#define IMX8MP_PCIE_PHY_TRSV_REG00A	0x428
#define  LN0_OVRD_TX_DRV_PRE_LVL_G23	0x55
#define IMX8MP_PCIE_PHY_TRSV_REG059	0x4EC
#define  LN0_OVRD_RX_CTLE_RS1_G1	0x13
#define IMX8MP_PCIE_PHY_TRSV_REG060	0x4F0
#define  LN0_OVRD_RX_CTLE_RS1_G2_G3	0x25
#define IMX8MP_PCIE_PHY_TRSV_REG069	0x514
#define  LN0_ANA_RX_CTLE_IBLEED		0x7
#define IMX8MP_PCIE_PHY_TRSV_REG107	0x5AC
#define  LN0_OVRD_RX_RTERM_VCM_EN	0xB8
#define IMX8MP_PCIE_PHY_TRSV_REG109	0x5B4
#define  LN0_ANA_OVRD_RX_SQHS_DIFN_OC	0xD4
#define IMX8MP_PCIE_PHY_TRSV_REG110	0x5B8
#define  LN0_ANA_OVRD_RX_SQHS_DIFP_OC	0x6A
#define IMX8MP_PCIE_PHY_TRSV_REG158	0x678
#define  LN0_RX_CDR_FBB_FINE_G1_G2	0x55
#define IMX8MP_PCIE_PHY_TRSV_REG159	0x67C
#define  LN0_RX_CDR_FBB_FINE_G3_G4	0x53
#define IMX8MP_PCIE_PHY_TRSV_REG206	0x738
#define  LN0_TG_RX_SIGVAL_LBF_DELAY	0x4

static int imx8_pcie_phy_tuned;
enum imx8_pcie_phy_type {
	IMX8MM,
	IMX8MP,
};

struct imx8_pcie_phy_drvdata {
	const	char			*gpr;
	enum	imx8_pcie_phy_type	variant;
};

struct imx8_pcie_phy {
	void __iomem		*base;
	struct clk		*clk;
	struct phy		*phy;
	struct regmap		*iomuxc_gpr;
	struct reset_control	*perst;
	struct reset_control	*reset;
	u32			refclk_pad_mode;
	u32			tx_deemph_gen1;
	u32			tx_deemph_gen2;
	bool			clkreq_unused;
	const struct imx8_pcie_phy_drvdata	*drvdata;
};

static int imx8_pcie_phy_power_on(struct phy *phy)
{
	int ret;
	u32 val, pad_mode;
	struct imx8_pcie_phy *imx8_phy = phy_get_drvdata(phy);

	pad_mode = imx8_phy->refclk_pad_mode;
	switch (imx8_phy->drvdata->variant) {
	case IMX8MM:
		reset_control_assert(imx8_phy->reset);

		/* Tune PHY de-emphasis setting to pass PCIe compliance. */
		if (imx8_phy->tx_deemph_gen1)
			writel(imx8_phy->tx_deemph_gen1,
			       imx8_phy->base + PCIE_PHY_TRSV_REG5);
		if (imx8_phy->tx_deemph_gen2)
			writel(imx8_phy->tx_deemph_gen2,
			       imx8_phy->base + PCIE_PHY_TRSV_REG6);
		break;
	case IMX8MP: /* Do nothing. */
		break;
	}

	if (pad_mode == IMX8_PCIE_REFCLK_PAD_INPUT ||
	    pad_mode == IMX8_PCIE_REFCLK_PAD_UNUSED) {
		/* Configure the pad as input */
		val = readl(imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG061);
		writel(val & ~ANA_PLL_CLK_OUT_TO_EXT_IO_EN,
		       imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG061);
	} else {
		/* Configure the PHY to output the refclock via pad */
		writel(ANA_PLL_CLK_OUT_TO_EXT_IO_EN,
		       imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG061);
	}

	if (pad_mode == IMX8_PCIE_REFCLK_PAD_OUTPUT ||
	    pad_mode == IMX8_PCIE_REFCLK_PAD_UNUSED) {
		/* Source clock from SoC internal PLL */
		writel(ANA_PLL_CLK_OUT_TO_EXT_IO_SEL,
		       imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG062);
		writel(AUX_PLL_REFCLK_SEL_SYS_PLL,
		       imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG063);
		val = ANA_AUX_RX_TX_SEL_TX | ANA_AUX_TX_TERM;
		writel(val | ANA_AUX_RX_TERM_GND_EN,
		       imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG064);
		writel(ANA_AUX_RX_TERM | ANA_AUX_TX_LVL,
		       imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG065);
	}

	/* Set AUX_EN_OVERRIDE 1'b0, when the CLKREQ# isn't hooked */
	regmap_update_bits(imx8_phy->iomuxc_gpr, IOMUXC_GPR14,
			   IMX8MM_GPR_PCIE_AUX_EN_OVERRIDE,
			   imx8_phy->clkreq_unused ?
			   0 : IMX8MM_GPR_PCIE_AUX_EN_OVERRIDE);
	regmap_update_bits(imx8_phy->iomuxc_gpr, IOMUXC_GPR14,
			   IMX8MM_GPR_PCIE_AUX_EN,
			   IMX8MM_GPR_PCIE_AUX_EN);
	regmap_update_bits(imx8_phy->iomuxc_gpr, IOMUXC_GPR14,
			   IMX8MM_GPR_PCIE_POWER_OFF, 0);
	regmap_update_bits(imx8_phy->iomuxc_gpr, IOMUXC_GPR14,
			   IMX8MM_GPR_PCIE_SSC_EN, 0);

	regmap_update_bits(imx8_phy->iomuxc_gpr, IOMUXC_GPR14,
			   IMX8MM_GPR_PCIE_REF_CLK_SEL,
			   pad_mode == IMX8_PCIE_REFCLK_PAD_INPUT ?
			   IMX8MM_GPR_PCIE_REF_CLK_EXT :
			   IMX8MM_GPR_PCIE_REF_CLK_PLL);
	usleep_range(100, 200);

	/*
	 * Fine tune the parameters of the PHY, let PCIe link up to Gen3
	 * between two i.MX8MP EVK boards in the EP/RC validation system.
	 */
	if (imx8_pcie_phy_tuned && (imx8_phy->drvdata->variant == IMX8MP)) {
		writel(LN0_OVRD_TX_DRV_LVL_G1,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG001);
		writel(LN0_OVRD_TX_DRV_LVL_G2,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG002);
		writel(LN0_OVRD_TX_DRV_LVL_G3,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG003);
		writel(LN0_OVRD_TX_DRV_PST_LVL_G1,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG005);
		writel(LN0_OVRD_TX_DRV_PST_LVL_G2,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG006);
		writel(LN0_OVRD_TX_DRV_PST_LVL_G3,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG007);
		writel(LN0_OVRD_TX_DRV_PRE_LVL_G1,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG009);
		writel(LN0_OVRD_TX_DRV_PRE_LVL_G23,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG00A);
		writel(LN0_OVRD_RX_CTLE_RS1_G1,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG059);
		writel(LN0_OVRD_RX_CTLE_RS1_G2_G3,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG060);
		writel(LN0_ANA_RX_CTLE_IBLEED,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG069);
		writel(LN0_OVRD_RX_RTERM_VCM_EN,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG107);
		writel(LN0_ANA_OVRD_RX_SQHS_DIFN_OC,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG109);
		writel(LN0_ANA_OVRD_RX_SQHS_DIFP_OC,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG110);
		writel(LN0_RX_CDR_FBB_FINE_G1_G2,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG158);
		writel(LN0_RX_CDR_FBB_FINE_G3_G4,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG159);
		writel(LN0_TG_RX_SIGVAL_LBF_DELAY,
		       imx8_phy->base + IMX8MP_PCIE_PHY_TRSV_REG206);
	}

	/* Do the PHY common block reset */
	regmap_update_bits(imx8_phy->iomuxc_gpr, IOMUXC_GPR14,
			   IMX8MM_GPR_PCIE_CMN_RST,
			   IMX8MM_GPR_PCIE_CMN_RST);

	switch (imx8_phy->drvdata->variant) {
	case IMX8MP:
		reset_control_deassert(imx8_phy->perst);
		fallthrough;
	case IMX8MM:
		reset_control_deassert(imx8_phy->reset);
		usleep_range(200, 500);
		break;
	}

	/* Polling to check the phy is ready or not. */
	ret = readl_poll_timeout(imx8_phy->base + IMX8MM_PCIE_PHY_CMN_REG075,
				 val, val == ANA_PLL_DONE, 10, 20000);
	return ret;
}

static int imx8_pcie_phy_init(struct phy *phy)
{
	struct imx8_pcie_phy *imx8_phy = phy_get_drvdata(phy);

	return clk_prepare_enable(imx8_phy->clk);
}

static int imx8_pcie_phy_exit(struct phy *phy)
{
	struct imx8_pcie_phy *imx8_phy = phy_get_drvdata(phy);

	clk_disable_unprepare(imx8_phy->clk);

	return 0;
}

static const struct phy_ops imx8_pcie_phy_ops = {
	.init		= imx8_pcie_phy_init,
	.exit		= imx8_pcie_phy_exit,
	.power_on	= imx8_pcie_phy_power_on,
	.owner		= THIS_MODULE,
};

static const struct imx8_pcie_phy_drvdata imx8mm_drvdata = {
	.gpr = "fsl,imx8mm-iomuxc-gpr",
	.variant = IMX8MM,
};

static const struct imx8_pcie_phy_drvdata imx8mp_drvdata = {
	.gpr = "fsl,imx8mp-iomuxc-gpr",
	.variant = IMX8MP,
};

static const struct of_device_id imx8_pcie_phy_of_match[] = {
	{.compatible = "fsl,imx8mm-pcie-phy", .data = &imx8mm_drvdata, },
	{.compatible = "fsl,imx8mp-pcie-phy", .data = &imx8mp_drvdata, },
	{ },
};
MODULE_DEVICE_TABLE(of, imx8_pcie_phy_of_match);

#ifndef MODULE
static int __init imx8_pcie_phy_fine_tune(char *str)
{
	if (!strcmp(str, "yes")) {
		pr_info("i.MX PCIe PHY is fine tuned in EP/RC SYS.\n");
		imx8_pcie_phy_tuned = 1;
	}
	return 1;
}

__setup("pcie_phy_tuned=", imx8_pcie_phy_fine_tune);
#endif

static int imx8_pcie_phy_probe(struct platform_device *pdev)
{
	struct phy_provider *phy_provider;
	struct device *dev = &pdev->dev;
	struct device_node *np = dev->of_node;
	struct imx8_pcie_phy *imx8_phy;
	struct resource *res;

	imx8_phy = devm_kzalloc(dev, sizeof(*imx8_phy), GFP_KERNEL);
	if (!imx8_phy)
		return -ENOMEM;

	imx8_phy->drvdata = of_device_get_match_data(dev);

	/* get PHY refclk pad mode */
	of_property_read_u32(np, "fsl,refclk-pad-mode",
			     &imx8_phy->refclk_pad_mode);

	if (of_property_read_u32(np, "fsl,tx-deemph-gen1",
				 &imx8_phy->tx_deemph_gen1))
		imx8_phy->tx_deemph_gen1 = 0;

	if (of_property_read_u32(np, "fsl,tx-deemph-gen2",
				 &imx8_phy->tx_deemph_gen2))
		imx8_phy->tx_deemph_gen2 = 0;

	if (of_property_read_bool(np, "fsl,clkreq-unsupported"))
		imx8_phy->clkreq_unused = true;
	else
		imx8_phy->clkreq_unused = false;

	imx8_phy->clk = devm_clk_get(dev, "ref");
	if (IS_ERR(imx8_phy->clk)) {
		dev_err(dev, "failed to get imx pcie phy clock\n");
		return PTR_ERR(imx8_phy->clk);
	}

	/* Grab GPR config register range */
	imx8_phy->iomuxc_gpr =
		 syscon_regmap_lookup_by_compatible(imx8_phy->drvdata->gpr);
	if (IS_ERR(imx8_phy->iomuxc_gpr)) {
		dev_err(dev, "unable to find iomuxc registers\n");
		return PTR_ERR(imx8_phy->iomuxc_gpr);
	}

	imx8_phy->reset = devm_reset_control_get_exclusive(dev, "pciephy");
	if (IS_ERR(imx8_phy->reset)) {
		dev_err(dev, "Failed to get PCIEPHY reset control\n");
		return PTR_ERR(imx8_phy->reset);
	}

	if (imx8_phy->drvdata->variant == IMX8MP) {
		imx8_phy->perst =
			devm_reset_control_get_exclusive(dev, "perst");
		if (IS_ERR(imx8_phy->perst))
			dev_err_probe(dev, PTR_ERR(imx8_phy->perst),
				      "Failed to get PCIE PHY PERST control\n");
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	imx8_phy->base = devm_ioremap_resource(dev, res);
	if (IS_ERR(imx8_phy->base))
		return PTR_ERR(imx8_phy->base);

	imx8_phy->phy = devm_phy_create(dev, NULL, &imx8_pcie_phy_ops);
	if (IS_ERR(imx8_phy->phy))
		return PTR_ERR(imx8_phy->phy);

	phy_set_drvdata(imx8_phy->phy, imx8_phy);

	phy_provider = devm_of_phy_provider_register(dev, of_phy_simple_xlate);

	return PTR_ERR_OR_ZERO(phy_provider);
}

static struct platform_driver imx8_pcie_phy_driver = {
	.probe	= imx8_pcie_phy_probe,
	.driver = {
		.name	= "imx8-pcie-phy",
		.of_match_table	= imx8_pcie_phy_of_match,
	}
};
module_platform_driver(imx8_pcie_phy_driver);

MODULE_DESCRIPTION("FSL IMX8 PCIE PHY driver");
MODULE_LICENSE("GPL v2");
