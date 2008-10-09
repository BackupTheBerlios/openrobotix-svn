/*
    ds2782.c - Linux kernel modules for DS2782

    Copyright (C) 2007
    Heinz Nixdorf Institute - University of Paderborn
    Department of System and Circuit Technology
    Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de>

    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License along
    with this program; if not, write to the Free Software Foundation, Inc.,
    51 Franklin Street, Fifth Floor, Boston, MA 02110-1301 USA.
*/
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>

/* Register */
#define DS2782_REG_STATUS		0x01 /* byte, RW */
#define DS2782_REG_RAAC			0x02 /* word, RO */
#define DS2782_REG_RSAC			0x04 /* word, RO */
#define DS2782_REG_RARC			0x06 /* byte, RO */
#define DS2782_REG_RSRC			0x07 /* byte, RO */
#define DS2782_REG_IAVG			0x08 /* word, RO */
#define DS2782_REG_TEMP			0x0A /* word, RO */
#define DS2782_REG_VOLT			0x0C /* word, RO */
#define DS2782_REG_CURRENT		0x0E /* word, RO */
#define DS2782_REG_ACR			0x10 /* word, RW */
#define DS2782_REG_ACRL			0x12 /* word, RO */
#define DS2782_REG_AS			0x14 /* byte, RW */
#define DS2782_REG_SFR			0x15 /* byte, RW */
#define DS2782_REG_FULL			0x16 /* word, RO */
#define DS2782_REG_AE			0x18 /* word, RO */
#define DS2782_REG_SE			0x1A /* word, RO */
#define DS2782_REG_EEPROM		0x1F /* byte, RW */
#define DS2782_EEPROM_USER		0x20 /* 24 byte, RW */
#define DS2782_EEPROM_PARA		0x60 /* 32 byte, RW */
#define DS2782_REG_FAMILY		0xF0 /* byte, RO */
#define DS2782_REG_FAMILY_VALUE		0xB2
#define DS2782_REG_FC			0xFE /* byte, WO */

/* Voltage in mV with 4.88mV resolution */
#define DS2782_VOLT_FROM_REG(x)		((int)((s16)x / 32) * 488 / 100)
/* Temperatur in °C with 0.125°C resolution */
#define DS2782_TEMP_FROM_REG(x)		((int)((s16)x / 32) * 125 / 1000)
/* Current in mA with 156.3uA resolution */
#define DS2782_CURRENT_FROM_REG(x)	((int)((s16)x) * 1563 / 10000)

static struct i2c_driver ds2782_driver;

/* Addresses to scan */
static unsigned short normal_i2c[] = {0x34, I2C_CLIENT_END};

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(ds2782);

struct ds2782_data {
	struct i2c_client client;
	struct mutex update_lock;
	char valid;			/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 temp, volt, curr, iavg;	/* Register values, word */
};

static int inline ds2782_read_byte(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int inline ds2782_write_byte(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

/* DS2782 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int inline ds2782_read_word(struct i2c_client *client, u8 reg)
{
	return swab16(i2c_smbus_read_word_data(client, reg));
}

/* DS2782 uses a high-byte first convention, which is exactly opposite to
   the usual practice. */
static int inline ds2782_write_word(struct i2c_client *client, u8 reg, u16 value)
{
	return i2c_smbus_write_word_data(client, reg, swab16(value));
}

static struct ds2782_data *ds2782_update_client(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct ds2782_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + HZ + HZ / 2)
	    || !data->valid) {

		data->temp = ds2782_read_word(client, DS2782_REG_TEMP);

		data->volt = ds2782_read_word(client, DS2782_REG_VOLT);

		data->curr = ds2782_read_word(client, DS2782_REG_CURRENT);

		data->iavg = ds2782_read_word(client, DS2782_REG_IAVG);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

/* sysfs callback functions */
#define show(value, format)								\
static ssize_t show_##value(struct device *dev, struct device_attribute *attr, char *buf)	\
{												\
	struct ds2782_data *data = ds2782_update_client(dev);					\
	return sprintf(buf, "%d\n", format(data->value));					\
}

show(temp, DS2782_TEMP_FROM_REG);
show(volt, DS2782_VOLT_FROM_REG);
show(curr, DS2782_CURRENT_FROM_REG);
show(iavg, DS2782_CURRENT_FROM_REG);

static DEVICE_ATTR(temperature, S_IRUGO, show_temp, NULL);
static DEVICE_ATTR(voltage, S_IRUGO , show_volt, NULL);
static DEVICE_ATTR(active_current, S_IRUGO , show_curr, NULL);
static DEVICE_ATTR(average_current, S_IRUGO , show_iavg, NULL);

static struct attribute *ds2782_attributes[] = {
	&dev_attr_temperature.attr,
	&dev_attr_voltage.attr,
	&dev_attr_active_current.attr,
	&dev_attr_average_current.attr,
	NULL
};

static struct attribute_group ds2782_attr_group = {
	.attrs = ds2782_attributes,
};

/* This function is called by i2c_probe */
static int ds2782_probe(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *client;
	struct ds2782_data *data;
	int family, result = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA))
		goto exit;

	data = kzalloc(sizeof(struct ds2782_data), GFP_KERNEL);
	if (!data) {
		result = -ENOMEM;
		goto exit;
	}

	client = &data->client;
	i2c_set_clientdata(client, data);
	client->addr = address;
	client->adapter = adapter;
	client->driver = &ds2782_driver;
	client->flags = 0;

	/* Now, we do the remaining detection. It is lousy. */
	if (kind < 0) {
		family = ds2782_read_byte(client, DS2782_REG_FAMILY);
		printk("ds2782 Famuly:%x", family);
//		if (family != DS2782_REG_FAMILY_VALUE)
//			goto exit_kfree;
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = ds2782;

	/* Fill in remaining client fields and put it into the global list */
	strlcpy(client->name, "ds2782", I2C_NAME_SIZE);
	data->valid = 0;
	mutex_init(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	result = i2c_attach_client(client);
	if (result)
		goto exit_kfree;

	/* Register sysfs hooks */
	result = sysfs_create_group(&client->dev.kobj, &ds2782_attr_group);
	if (result)
		goto exit_detach;

	return 0;

exit_detach:
	i2c_detach_client(client);
exit_kfree:
	kfree(data);
exit:
	return result;
}

static int ds2782_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, ds2782_probe);
}

static int ds2782_detach_client(struct i2c_client *client)
{
	int result;
	struct ds2782_data *data = i2c_get_clientdata(client);

	sysfs_remove_group(&client->dev.kobj, &ds2782_attr_group);

	result = i2c_detach_client(client);
	if (!result)
		kfree(data);

	return result;
}

static struct i2c_driver ds2782_driver = {
	.driver = {
		.name	= "ds2782",
	},
	.attach_adapter	= ds2782_attach_adapter,
	.detach_client	= ds2782_detach_client,
};

static int __init ds2782_init(void)
{
	return i2c_add_driver(&ds2782_driver);
}

static void __exit ds2782_exit(void)
{
	i2c_del_driver(&ds2782_driver);
}

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("ds2782 driver");
MODULE_LICENSE("GPL");

module_init(ds2782_init);
module_exit(ds2782_exit);



