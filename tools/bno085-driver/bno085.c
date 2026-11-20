// SPDX-License-Identifier: GPL-2.0
/*
 * Minimal I2C driver for the CEVA BNO085 IMU, written in place of the Nunchuk
 * driver in Bootlin's Embedded Linux lab 6 ("Accessing hardware devices").
 *
 * It only binds to the device and reads one packet header to prove the chip
 * answers. Reading sensor data would mean implementing CEVA's SH-2 protocol.
 *
 * The BNO085 talks SHTP: every packet starts with a 4-byte header:
 *   bytes 0-1  length, LSB first; bit 15 is a continuation flag
 *   byte 2     channel
 *   byte 3     sequence number
 * After power-up it has an advertisement packet waiting on channel 0, so the
 * first read returns a non-zero length.
 *
 * Device tree node (on the I2C bus the BNO085 is wired to):
 *
 *	imu@4b {
 *		compatible = "ceva,bno085";
 *		reg = <0x4b>;
 *	};
 */
#include <linux/i2c.h>
#include <linux/module.h>
#include <linux/of.h>

/* Called when the kernel finds a device matching bno085_of_match.
 * Since Linux 6.3, probe() takes only the i2c_client. */
static int bno085_probe(struct i2c_client *client)
{
	u8 hdr[4];
	int ret;

	ret = i2c_master_recv(client, hdr, sizeof(hdr));
	if (ret < 0)
		return dev_err_probe(&client->dev, ret, "no answer from the BNO085\n");

	dev_info(&client->dev, "BNO085 found: %u-byte packet on channel %u\n",
		 (hdr[0] | hdr[1] << 8) & 0x7fff, hdr[2]);
	return 0;
}

static void bno085_remove(struct i2c_client *client)
{
	dev_info(&client->dev, "BNO085 removed\n");
}

/* MODULE_DEVICE_TABLE puts these IDs in the module's aliases, so modprobe
 * (and udev/mdev) can find the module from the device's modalias. */
static const struct of_device_id bno085_of_match[] = {
	{ .compatible = "ceva,bno085" },
	{ }
};
MODULE_DEVICE_TABLE(of, bno085_of_match);

static const struct i2c_device_id bno085_id[] = {
	{ "bno085", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bno085_id);

static struct i2c_driver bno085_driver = {
	.driver = {
		.name = "bno085",
		.of_match_table = bno085_of_match,
	},
	.probe = bno085_probe,
	.remove = bno085_remove,
	.id_table = bno085_id,
};
/* Replaces the module_init()/module_exit() boilerplate. */
module_i2c_driver(bno085_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Minimal BNO085 I2C driver for Bootlin's lab 6");
