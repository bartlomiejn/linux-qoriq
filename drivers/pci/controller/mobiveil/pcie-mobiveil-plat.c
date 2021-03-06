// SPDX-License-Identifier: GPL-2.0
/*
 * PCIe host controller driver for Mobiveil PCIe Host controller
 *
 * Copyright (c) 2018 Mobiveil Inc.
 * Author: Subrahmanya Lingappa <l.subrahmanya@mobiveil.co.in>
 * Refactor: Zhiqiang Hou <Zhiqiang.Hou@nxp.com>
 */

#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/of_pci.h>
#include <linux/pci.h>
#include <linux/platform_device.h>
#include <linux/slab.h>

#include "pcie-mobiveil.h"

static int mobiveil_pcie_probe(struct platform_device *pdev)
{
	struct mobiveil_pcie *pcie;
	struct device *dev = &pdev->dev;

	pcie = devm_kzalloc(dev, sizeof(*pcie), GFP_KERNEL);
	if (!pcie)
		return -ENOMEM;

	pcie->pdev = pdev;

	return mobiveil_pcie_host_probe(pcie);
}

static const struct of_device_id mobiveil_pcie_of_match[] = {
	{.compatible = "mbvl,gpex40-pcie",},
	{},
};

MODULE_DEVICE_TABLE(of, mobiveil_pcie_of_match);

static struct platform_driver mobiveil_pcie_driver = {
	.probe = mobiveil_pcie_probe,
	.driver = {
		.name = "mobiveil-pcie",
		.of_match_table = mobiveil_pcie_of_match,
		.suppress_bind_attrs = true,
	},
};

builtin_platform_driver(mobiveil_pcie_driver);

MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Mobiveil PCIe host controller driver");
MODULE_AUTHOR("Subrahmanya Lingappa <l.subrahmanya@mobiveil.co.in>");
