// SPDX-License-Identifier: GPL-2.0+
/*
 */

#include <log.h>
#include <asm/io.h>
#include <dm.h>
#include <mailbox-uclass.h>
#include <dm/device_compat.h>
#include <dm/lists.h>
#include <dm/of_access.h>
#include <linux/ioport.h>
#include <linux/io.h>
#include <wait_bit.h>

static int everdrive_fifo_probe(struct udevice *dev)
{
	return 0;
};

static const struct udevice_id everdrive_fifo_ids[] = {
	{ .compatible = "krikzz,everdrive-fifo" },
	{ }
};

U_BOOT_DRIVER(everdrive_fifo) = {
	.name = "everdrive_fifo",
	.id = UCLASS_MISC,
	.of_match = everdrive_fifo_ids,
	.probe = everdrive_fifo_probe,
};
