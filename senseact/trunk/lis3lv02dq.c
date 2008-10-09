/*
    lis3lv02dq.c - Linux kernel modules for accelerometer LIS3LV02DQ

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
#include <linux/workqueue.h>
#include <asm/uaccess.h>

/* Register */
#define LIS3LV02DQ_REG_WHO_AM_I		0x0F /* byte, R */
	#define LIS3LV02DQ_ADDRESS		0x1D
#define LIS3LV02DQ_REG_OFFSET_X		0x16 /* byte, RW */
#define LIS3LV02DQ_REG_OFFSET_Y		0x17 /* byte, RW */
#define LIS3LV02DQ_REG_OFFSET_Z		0x18 /* byte, RW */
#define LIS3LV02DQ_REG_OFFSET(x)	(0x16 + x)
#define LIS3LV02DQ_REG_GAIN_X		0x19 /* byte, RW */
#define LIS3LV02DQ_REG_GAIN_Y		0x1A /* byte, RW */
#define LIS3LV02DQ_REG_GAIN_Z		0x1B /* byte, RW */
#define LIS3LV02DQ_REG_GAIN(x)		(0x19 + x)
#define LIS3LV02DQ_REG_CTRL1		0x20 /* byte, RW */
	#define LIS3LV02DQ_CTRL1_XEN		(1)
	#define LIS3LV02DQ_CTRL1_YEN		(1 << 1)
	#define LIS3LV02DQ_CTRL1_ZEN		(1 << 2)
	#define LIS3LV02DQ_CTRL1_ST		(1 << 3)
	#define LIS3LV02DQ_CTRL1_DF_40HZ	(0 << 4)
	#define LIS3LV02DQ_CTRL1_DF_160HZ	(1 << 4)
	#define LIS3LV02DQ_CTRL1_DF_640HZ	(2 << 4)
	#define LIS3LV02DQ_CTRL1_DF_2560HZ	(3 << 4)
	#define LIS3LV02DQ_CTRL1_PD(x)		(0 << 6)
	#define LIS3LV02DQ_CTRL1_PEN		(3 << 6)
#define LIS3LV02DQ_REG_CTRL2		0x21 /* byte, RW */
	#define LIS3LV02DQ_CTRL2_DAS		(1)
	#define LIS3LV02DQ_CTRL2_SIM		(1 << 1)
	#define LIS3LV02DQ_CTRL2_DRDY		(1 << 2)
	#define LIS3LV02DQ_CTRL2_IEN		(1 << 3)
	#define LIS3LV02DQ_CTRL2_BOOT		(1 << 4)
	#define LIS3LV02DQ_CTRL2_LE		(0 << 5)
	#define LIS3LV02DQ_CTRL2_BE		(1 << 5)
	#define LIS3LV02DQ_CTRL2_BDU		(1 << 6)
	#define LIS3LV02DQ_CTRL2_FS_2G		(0 << 7)
	#define LIS3LV02DQ_CTRL2_FS_6G		(1 << 7)
#define LIS3LV02DQ_REG_CTRL3		0x22 /* byte, RW */
	#define LIS3LV02DQ_CTRL3_CFS_512	(0)
	#define LIS3LV02DQ_CTRL3_CFS_1024	(1)
	#define LIS3LV02DQ_CTRL3_CFS_2048	(2)
	#define LIS3LV02DQ_CTRL3_CFS_4096	(3)
	#define LIS3LV02DQ_CTRL3_FDS		(1 << 4)
	#define LIS3LV02DQ_CTRL3_HPFF		(1 << 5)
	#define LIS3LV02DQ_CTRL3_HPDD		(1 << 6)
	#define LIS3LV02DQ_CTRL3_ECK		(1 << 7)
#define LIS3LV02DQ_REG_CTRL(x)		(0x20 + x)
#define LIS3LV02DQ_REG_HP_FILTER_RESET	0x23 /* byte, R */
#define LIS3LV02DQ_REG_STATUS		0x27 /* byte, RW */
	#define LIS3LV02DQ_STATUS_XDA(x)	(x & 1)
	#define LIS3LV02DQ_STATUS_YDA(x)	((x >> 1) & 1)
	#define LIS3LV02DQ_STATUS_ZDA(x)	((x >> 2) & 1)
	#define LIS3LV02DQ_STATUS_ZYXDA(x)	((x >> 3) & 1)
	#define LIS3LV02DQ_STATUS_XOR(x)	((x >> 4) & 1)
	#define LIS3LV02DQ_STATUS_YOR(x)	((x >> 5) & 1)
	#define LIS3LV02DQ_STATUS_ZOR(x)	((x >> 6) & 1)
	#define LIS3LV02DQ_STATUS_ZYXOR(x)	((x >> 7) & 1)
#define LIS3LV02DQ_REG_OUTX		0x28 /* word, R */
#define LIS3LV02DQ_REG_OUTY		0x2A /* word, R */
#define LIS3LV02DQ_REG_OUTZ		0x2C /* word, R */
#define LIS3LV02DQ_REG_OUT(x)		(0x28 + (2 * x)) /* word, R */
#define LIS3LV02DQ_REG_FF_WU_CFG	0x30 /* byte, RW */
#define LIS3LV02DQ_REG_FF_WU_SRC	0x31 /* byte, RW */
#define LIS3LV02DQ_REG_FF_WU_ACK	0x32 /* byte, R */
#define LIS3LV02DQ_REG_FF_WU_THS	0x34 /* word, RW */
#define LIS3LV02DQ_REG_FF_WU_DURATION	0x36 /* byte, RW */
#define LIS3LV02DQ_REG_DD_CFG		0x38 /* byte, RW */
#define LIS3LV02DQ_REG_DD_SRC		0x39 /* byte, RW */
#define LIS3LV02DQ_REG_DD_ACK		0x3A /* byte, R */
#define LIS3LV02DQ_REG_DD_THSI		0x3C /* word, RW */
#define LIS3LV02DQ_REG_DD_THSE		0x3E /* word, RW */

#define LIS3LV02DQ_REG_INC		0x80

/* Default control values */
#define LIS3LV02DQ_CTRL1		(LIS3LV02DQ_CTRL1_XEN | \
					 LIS3LV02DQ_CTRL1_YEN | \
					 LIS3LV02DQ_CTRL1_ZEN | \
					 LIS3LV02DQ_CTRL1_DF_40HZ | \
					 LIS3LV02DQ_CTRL1_PEN)
#define LIS3LV02DQ_CTRL2		(LIS3LV02DQ_CTRL2_LE | \
					 LIS3LV02DQ_CTRL2_BDU | \
					 LIS3LV02DQ_CTRL2_FS_2G)

/* Acceleration in g / 1000 with ~0,98 mg resolution */
#define LIS3LV02DQ_ACCELERATION_FROM_REG(x)	((int)((s16)x) * 1000 / 1024)

#define LIS3LV02DQ_POLL_DELAY		msecs_to_jiffies(10)

static struct i2c_driver lis3lv02dq_driver;

/* Addresses to scan */
static unsigned short normal_i2c[] = {LIS3LV02DQ_ADDRESS, I2C_CLIENT_END};

/* Insmod parameters */
I2C_CLIENT_INSMOD_1(lis3lv02dq);

struct lis3lv02dq_data {
	struct list_head list;
	struct i2c_client client;
	struct miscdevice dev;
	struct delayed_work work;
	struct mutex update_lock;
	char valid;			/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	/* Register values, word */
	s16 out[3];
	/* Register values, byte */
	u8 offset[3], gain[3], ctrl[3], status;
};

static LIST_HEAD(lis3lv02dq_data_list);
static DEFINE_SPINLOCK(lis3lv02dq_data_list_lock);
static unsigned int lis3lv02dq_data_no;

static struct workqueue_struct *lis3lv02dq_wq;

static int lis3lv02dq_update_device(struct lis3lv02dq_data *data)
{
	int result = 0;
	struct i2c_client *client = &data->client;

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + LIS3LV02DQ_POLL_DELAY)
	    || !data->valid) {

		if (!data->valid) {
			dev_err(&data->client.dev, "Starting lis3lv02dq update\n");
			result = i2c_smbus_read_i2c_block_data(client,
				LIS3LV02DQ_REG_OFFSET(0) | LIS3LV02DQ_REG_INC,
				3, data->offset);
			if (result < 0)
				goto exit;

			result = i2c_smbus_read_i2c_block_data(client,
				LIS3LV02DQ_REG_GAIN(0) | LIS3LV02DQ_REG_INC,
				3, data->gain);
			if (result < 0)
				goto exit;

			result = i2c_smbus_read_i2c_block_data(client,
				LIS3LV02DQ_REG_CTRL(0) | LIS3LV02DQ_REG_INC,
				3, data->ctrl);
			if (result < 0)
				goto exit;

			data->valid = 1;
		}

		data->status = i2c_smbus_read_byte_data(client,
			LIS3LV02DQ_REG_STATUS);
		if (LIS3LV02DQ_STATUS_ZYXOR(data->status))
			dev_err(&data->client.dev, "Data Overrun\n");

		if (LIS3LV02DQ_STATUS_ZYXDA(data->status)) {
			result = i2c_smbus_read_i2c_block_data(client,
				LIS3LV02DQ_REG_OUT(0) | LIS3LV02DQ_REG_INC,
				6, (u8 *) data->out);
			if (result < 0)
				goto exit;
		}

		data->last_updated = jiffies;
	}

	result = 0;
exit:
	mutex_unlock(&data->update_lock);
	return result;
}

static struct lis3lv02dq_data *lis3lv02dq_data_locate(unsigned int minor)
{
	struct lis3lv02dq_data *data;

	spin_lock(&lis3lv02dq_data_list_lock);
	list_for_each_entry(data, &lis3lv02dq_data_list, list) {
		if (data->dev.minor == minor)
			goto found;
	}
	data = NULL;

found:
	spin_unlock(&lis3lv02dq_data_list_lock);
	return data;
}

static void lis3lv02dq_work_func(struct work_struct *work)
{
	struct lis3lv02dq_data *data = container_of(work,
		struct lis3lv02dq_data, work.work);

	lis3lv02dq_update_device(data);

	queue_delayed_work(lis3lv02dq_wq, &data->work, LIS3LV02DQ_POLL_DELAY);
}

/* sysfs callback functions */
#define show_array(name, value, index, format)					\
static ssize_t show_##name##index(struct device *dev,				\
				  struct device_attribute *attr, char *buf)	\
{										\
	struct i2c_client *client = to_i2c_client(dev);				\
	struct lis3lv02dq_data *data = i2c_get_clientdata(client);		\
	return sprintf(buf, "%d\n", format(data->value[index]));		\
}

show_array(acceleration, out, 0, LIS3LV02DQ_ACCELERATION_FROM_REG);
show_array(acceleration, out, 1, LIS3LV02DQ_ACCELERATION_FROM_REG);
show_array(acceleration, out, 2, LIS3LV02DQ_ACCELERATION_FROM_REG);

static DEVICE_ATTR(acceleration_x, S_IRUGO, show_acceleration0, NULL);
static DEVICE_ATTR(acceleration_y, S_IRUGO, show_acceleration1, NULL);
static DEVICE_ATTR(acceleration_z, S_IRUGO, show_acceleration2, NULL);

static struct attribute *lis3lv02dq_attributes[] = {
	&dev_attr_acceleration_x.attr,
	&dev_attr_acceleration_y.attr,
	&dev_attr_acceleration_z.attr,
	NULL
};

static struct attribute_group lis3lv02dq_attr_group = {
	.attrs = lis3lv02dq_attributes,
};

/* dev callback functions */
static ssize_t lis3lv02dq_read(struct file *file, char __user *buf,
			     size_t count, loff_t *offset)
{
	struct lis3lv02dq_data *data
		 = (struct lis3lv02dq_data *) file->private_data;
	s16 tmp[3];
	int i;

	if (count < 6)
		return -EINVAL;

	if (count > 6)
		count = 6;

	for (i = 0; i < 3; i++)
		tmp[i] = LIS3LV02DQ_ACCELERATION_FROM_REG(data->out[i]);

	if(copy_to_user(buf, tmp, count))
		return -EFAULT;

	return count;
}

static int lis3lv02dq_open(struct inode *inode, struct file *file)
{
	unsigned int minor = iminor(inode);
	struct lis3lv02dq_data *data = lis3lv02dq_data_locate(minor);

	file->private_data = data;
	return 0;
}

static const struct file_operations lis3lv02dq_fops = {
	.owner	= THIS_MODULE,
	.read	= lis3lv02dq_read,
	.open	= lis3lv02dq_open,
};

/* This function is called by i2c_probe */
static int lis3lv02dq_probe(struct i2c_adapter *adapter, int address, int kind)
{
	struct i2c_client *client;
	struct lis3lv02dq_data *data;
	struct miscdevice *misc;
	int value, result = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA))
		goto exit;

	data = kzalloc(sizeof(struct lis3lv02dq_data), GFP_KERNEL);
	if (!data) {
		result = -ENOMEM;
		goto exit;
	}

	client = &data->client;
	i2c_set_clientdata(client, data);
	client->addr = address;
	client->adapter = adapter;
	client->driver = &lis3lv02dq_driver;
	client->flags = 0;

	/* Now, we do the remaining detection. It is lousy. */
	if (kind < 0) {
		value = i2c_smbus_read_byte_data(client, LIS3LV02DQ_REG_WHO_AM_I);
		if ((value >> 1) != LIS3LV02DQ_ADDRESS)
			goto exit_kfree;
		i2c_smbus_write_byte_data(client, LIS3LV02DQ_REG_CTRL2, LIS3LV02DQ_CTRL2);
		i2c_smbus_write_byte_data(client, LIS3LV02DQ_REG_CTRL1, LIS3LV02DQ_CTRL1);
	}

	/* Determine the chip type - only one kind supported! */
	if (kind <= 0)
		kind = lis3lv02dq;

	/* Fill in remaining device fields */
	snprintf(client->name, I2C_NAME_SIZE, "accelerometer%d", lis3lv02dq_data_no++);
	data->valid = 0;
	mutex_init(&data->update_lock);

	/* Tell the I2C layer a new client has arrived */
	result = i2c_attach_client(client);
	if (result)
		goto exit_power_down;

	/* Register sysfs hooks */
	result = sysfs_create_group(&client->dev.kobj, &lis3lv02dq_attr_group);
	if (result)
		goto exit_detach;

	/* Register misc device */
	misc = &data->dev;
	misc->minor = MISC_DYNAMIC_MINOR;
	misc->name = client->name;
	misc->parent = &client->dev;
	misc->fops = &lis3lv02dq_fops;
	result = misc_register(misc);
	if (result)
		goto exit_remove;

	/* Add device into global lidatat */
	spin_lock(&lis3lv02dq_data_list_lock);	
	list_add_tail(&data->list, &lis3lv02dq_data_list);
	spin_unlock(&lis3lv02dq_data_list_lock);

	INIT_DELAYED_WORK(&data->work, lis3lv02dq_work_func);
	queue_delayed_work(lis3lv02dq_wq, &data->work, LIS3LV02DQ_POLL_DELAY);

	return 0;

exit_remove:
	sysfs_remove_group(&client->dev.kobj, &lis3lv02dq_attr_group);	
exit_detach:
	i2c_detach_client(client);
exit_power_down:
	i2c_smbus_write_byte_data(client, LIS3LV02DQ_REG_CTRL1, LIS3LV02DQ_CTRL1_PD(0));
exit_kfree:
	kfree(data);
exit:
	return result;
}

static int lis3lv02dq_attach_adapter(struct i2c_adapter *adapter)
{
	return i2c_probe(adapter, &addr_data, lis3lv02dq_probe);
}

static int lis3lv02dq_detach_client(struct i2c_client *client)
{
	int result;
	struct lis3lv02dq_data *data = i2c_get_clientdata(client);
	struct miscdevice *misc = &data->dev;

	cancel_rearming_delayed_workqueue(lis3lv02dq_wq, &data->work);

	misc_deregister(misc);

	sysfs_remove_group(&client->dev.kobj, &lis3lv02dq_attr_group);

	spin_lock(&lis3lv02dq_data_list_lock);	
	list_del(&data->list);
	spin_unlock(&lis3lv02dq_data_list_lock);	

	i2c_smbus_write_byte_data(client, LIS3LV02DQ_REG_CTRL1, LIS3LV02DQ_CTRL1_PD(0));

	result = i2c_detach_client(client);
	if (!result)
		kfree(data);

	return result;
}

static struct i2c_driver lis3lv02dq_driver = {
	.driver = {
		.name	= "lis3lv02dq",
	},
	.attach_adapter	= lis3lv02dq_attach_adapter,
	.detach_client	= lis3lv02dq_detach_client,
};

static int __init lis3lv02dq_init(void)
{
	int result;

	/* Init workqueue */
	lis3lv02dq_wq = create_singlethread_workqueue("lis3lv02dq_wq");
	if (!lis3lv02dq_wq)
		return -ENOMEM;

	result = i2c_add_driver(&lis3lv02dq_driver);
	if (result)
		goto exit;

	return 0;
	
exit:
	destroy_workqueue(lis3lv02dq_wq);
	return result;
}

static void __exit lis3lv02dq_exit(void)
{
	i2c_del_driver(&lis3lv02dq_driver);

	if (lis3lv02dq_wq) {
		destroy_workqueue(lis3lv02dq_wq);
		lis3lv02dq_wq = NULL;
	}
}

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("lis3lv02dq driver");
MODULE_LICENSE("GPL");

module_init(lis3lv02dq_init);
module_exit(lis3lv02dq_exit);



