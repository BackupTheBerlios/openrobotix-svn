/*
    bebot_ir.c - Linux kernel modules for BeBot robot IR-Sensors

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
#include <linux/fs.h>
#include <linux/jiffies.h>
#include <linux/i2c.h>
#include <linux/miscdevice.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>

/* Register */
#define IRSENSOR_REG_TYPE		0x00 /* byte, R */
#define IRSENSOR_REG_TYPE_VALUE		0x26

#define IRSENSOR_REG_SENSOR(x)		(0x20 + (x * 2)) /* word, R */

#define IRSENSOR_REG_SENSOR_ENABLE	0x2F /* byte, RW */

/* Brightness in ?? with ?? resolution */
#define IRSENSOR_BRIGHTNESS_FROM_REG(x)		((int)((s16)x / 1) * 1 / 1)
/* Enable sensor */
#define IRSENSOR_ENABLE_FROM_REG(x, i)		((x >> i) & 0x1)
#define IRSENSOR_ENABLE_TO_REG(x, i, y)		((y) ? (x | (1 << i)) : (x & ~(1 << i)))

static struct i2c_driver irsensor_driver;

struct irsensor_device {
	struct i2c_client *client;
	struct mutex update_lock;
	char valid;			/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	u16 sensors[6];			/* Register values, word */
	u8 enable;			/* Register values, byte */
};

static int inline irsensor_read_byte(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_byte_data(client, reg);
}

static int inline irsensor_write_byte(struct i2c_client *client, u8 reg, u8 value)
{
	return i2c_smbus_write_byte_data(client, reg, value);
}

static int inline irsensor_read_word(struct i2c_client *client, u8 reg)
{
	return i2c_smbus_read_word_data(client, reg);
}

static struct irsensor_device *irsensor_update_device(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct irsensor_device *sdev = i2c_get_clientdata(client);
	int i;

	mutex_lock(&sdev->update_lock);

	if (time_after(jiffies, sdev->last_updated + msecs_to_jiffies(250))
	    || !sdev->valid) {

		dev_dbg(&client->dev, "Starting irsensor update\n");

		if (!sdev->valid)
			sdev->enable = irsensor_read_byte(client,
				IRSENSOR_REG_SENSOR_ENABLE);

		for (i = 0; i < 6; i++)
			if (sdev->enable & (1 << i))
				sdev->sensors[i] = irsensor_read_word(
					client, IRSENSOR_REG_SENSOR(i));
			else
				sdev->sensors[i] = 0;

		sdev->last_updated = jiffies;
		sdev->valid = 1;
	}

	mutex_unlock(&sdev->update_lock);

	return sdev;
}

/* sysfs callback functions */
#define show_array(name, value, index, format)					\
static ssize_t show_##name##index(struct device *dev,				\
				  struct device_attribute *attr, char *buf)	\
{										\
	struct irsensor_device *sdev = irsensor_update_device(dev);		\
	return sprintf(buf, "%d\n", format(sdev->value[index]));		\
}

show_array(brightness, sensors, 0, IRSENSOR_BRIGHTNESS_FROM_REG);
show_array(brightness, sensors, 1, IRSENSOR_BRIGHTNESS_FROM_REG);
show_array(brightness, sensors, 2, IRSENSOR_BRIGHTNESS_FROM_REG);
show_array(brightness, sensors, 3, IRSENSOR_BRIGHTNESS_FROM_REG);
show_array(brightness, sensors, 4, IRSENSOR_BRIGHTNESS_FROM_REG);
show_array(brightness, sensors, 5, IRSENSOR_BRIGHTNESS_FROM_REG);

#define show_bit(value, index, format)						\
static ssize_t show_##value##index(struct device *dev,				\
				   struct device_attribute *attr, char *buf)	\
{										\
	struct irsensor_device *sdev = irsensor_update_device(dev);		\
	return sprintf(buf, "%d\n", format(sdev->value, index));		\
}

show_bit(enable, 0, IRSENSOR_ENABLE_FROM_REG);
show_bit(enable, 1, IRSENSOR_ENABLE_FROM_REG);
show_bit(enable, 2, IRSENSOR_ENABLE_FROM_REG);
show_bit(enable, 3, IRSENSOR_ENABLE_FROM_REG);
show_bit(enable, 4, IRSENSOR_ENABLE_FROM_REG);
show_bit(enable, 5, IRSENSOR_ENABLE_FROM_REG);

#define store_bit(value, index, format, reg)					\
static ssize_t store_##value##index(struct device *dev,				\
				    struct device_attribute *attr,		\
				    const char *buf, size_t count)		\
{										\
	struct i2c_client *client = to_i2c_client(dev);				\
	struct irsensor_device *sdev = irsensor_update_device(dev);		\
	char val = simple_strtoul(buf, NULL, 0);				\
	val = format(sdev->value, index, val);					\
										\
	mutex_lock(&sdev->update_lock);						\
	irsensor_write_byte(client, reg, val);					\
	sdev->valid = 0;							\
	mutex_unlock(&sdev->update_lock);					\
	return count;								\
}

store_bit(enable, 0, IRSENSOR_ENABLE_TO_REG, IRSENSOR_REG_SENSOR_ENABLE);
store_bit(enable, 1, IRSENSOR_ENABLE_TO_REG, IRSENSOR_REG_SENSOR_ENABLE);
store_bit(enable, 2, IRSENSOR_ENABLE_TO_REG, IRSENSOR_REG_SENSOR_ENABLE);
store_bit(enable, 3, IRSENSOR_ENABLE_TO_REG, IRSENSOR_REG_SENSOR_ENABLE);
store_bit(enable, 4, IRSENSOR_ENABLE_TO_REG, IRSENSOR_REG_SENSOR_ENABLE);
store_bit(enable, 5, IRSENSOR_ENABLE_TO_REG, IRSENSOR_REG_SENSOR_ENABLE);

static DEVICE_ATTR(brightness0, S_IRUGO, show_brightness0, NULL);
static DEVICE_ATTR(brightness1, S_IRUGO, show_brightness1, NULL);
static DEVICE_ATTR(brightness2, S_IRUGO, show_brightness2, NULL);
static DEVICE_ATTR(brightness3, S_IRUGO, show_brightness3, NULL);
static DEVICE_ATTR(brightness4, S_IRUGO, show_brightness4, NULL);
static DEVICE_ATTR(brightness5, S_IRUGO, show_brightness5, NULL);
static DEVICE_ATTR(enable0, S_IWUGO | S_IRUGO, show_enable0, store_enable0);
static DEVICE_ATTR(enable1, S_IWUGO | S_IRUGO, show_enable1, store_enable1);
static DEVICE_ATTR(enable2, S_IWUGO | S_IRUGO, show_enable2, store_enable2);
static DEVICE_ATTR(enable3, S_IWUGO | S_IRUGO, show_enable3, store_enable3);
static DEVICE_ATTR(enable4, S_IWUGO | S_IRUGO, show_enable4, store_enable4);
static DEVICE_ATTR(enable5, S_IWUGO | S_IRUGO, show_enable5, store_enable5);

static ssize_t show_brightness(struct device *dev, 
			       struct device_attribute *attr, char *buf)
{
	struct irsensor_device *sdev = irsensor_update_device(dev);
	memcpy(buf, (char *) sdev->sensors, sizeof(unsigned short) * 6);
	return sizeof(unsigned short) * 6;
}

static DEVICE_ATTR(brightness, S_IRUGO, show_brightness, NULL);

static ssize_t show_enable(struct device *dev, struct device_attribute *attr,
			   char *buf)
{
	struct irsensor_device *sdev = irsensor_update_device(dev);
	memcpy(buf, (char *) &sdev->enable, 1);
	return 1;
}

static ssize_t store_enable(struct device *dev, struct device_attribute *attr,
			    const char *buf, size_t count)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct irsensor_device *sdev = irsensor_update_device(dev);

	if (count != 1)
		return -EFAULT;

	mutex_lock(&sdev->update_lock);
	irsensor_write_byte(client, IRSENSOR_REG_SENSOR_ENABLE, buf[0]);
	sdev->valid = 0;
	mutex_unlock(&sdev->update_lock);
	return count;
}

static DEVICE_ATTR(enable, S_IRUGO, show_enable, store_enable);

static struct attribute *irsensor_attributes[] = {
	&dev_attr_brightness.attr,
	&dev_attr_brightness0.attr,
	&dev_attr_brightness1.attr,
	&dev_attr_brightness2.attr,
	&dev_attr_brightness3.attr,
	&dev_attr_brightness4.attr,
	&dev_attr_brightness5.attr,
	&dev_attr_enable.attr,
	&dev_attr_enable0.attr,
	&dev_attr_enable1.attr,
	&dev_attr_enable2.attr,
	&dev_attr_enable3.attr,
	&dev_attr_enable4.attr,
	&dev_attr_enable5.attr,
	NULL
};

static struct attribute_group irsensor_attr_group = {
	.attrs = irsensor_attributes,
};

/*
 * Called when a motor device is matched with this driver
 */
static int irsensor_probe(struct i2c_client *client, 
			  const struct i2c_device_id *id)

{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct irsensor_device *sdev;
	int rc = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA)) {
		rc = -EIO;
		goto exit;
	}

	sdev = kzalloc(sizeof(struct irsensor_device), GFP_KERNEL);
	if (!sdev) {
		rc = -ENOMEM;
		goto exit;
	}

	sdev->client = client;
	i2c_set_clientdata(client, sdev);
	sdev->valid = 0;

	mutex_init(&sdev->update_lock);

	/* Register sysfs hooks */
	rc = sysfs_create_group(&client->dev.kobj, &irsensor_attr_group);
	if (rc)
		goto exit_kfree;

	return 0;

exit_kfree:
	kfree(sdev);
exit:
	return rc;
}

static int irsensor_remove(struct i2c_client *client)
{
	sysfs_remove_group(&client->dev.kobj, &irsensor_attr_group);

	kfree(i2c_get_clientdata(client));

	return 0;
}

static const struct i2c_device_id irsensor_id[] = {
	{ "irsensor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, irsensor_id);

static struct i2c_driver irsensor_driver = {
	.driver = {
		.name	= "irsensor",
	},
	.probe		= irsensor_probe,
	.remove		= irsensor_remove,
	.id_table 	= irsensor_id,
};

static int __init irsensor_init(void)
{
	return i2c_add_driver(&irsensor_driver);
}

static void __exit irsensor_exit(void)
{
	i2c_del_driver(&irsensor_driver);
}

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("irsensor driver");
MODULE_LICENSE("GPL");

module_init(irsensor_init);
module_exit(irsensor_exit);



