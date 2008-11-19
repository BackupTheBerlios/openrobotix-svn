/*
    bebot_motor.c - Linux kernel modules for BeBot robot motor controller

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
// #include <linux/miscdevice.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/sysfs.h>
#include <asm/uaccess.h>

/* Register */
#define MOTOR_REG_TYPE			0x00			/* byte, R */
#define MOTOR_REG_TYPE_VALUE		0x12

#define MOTOR_REG_CONFIG		0x02			/* byte, R */
	#define MOTOR_REG_CONFIG_CLEAR_POS	1

#define MOTOR_REG_SPEED			0x10			/* 2 x byte, RW */
#define MOTOR_SPEED_TYPE		1
#define MOTOR_SPEED_SIZE		(2 * MOTOR_SPEED_TYPE)

#define MOTOR_REG_INCREMENT		0x40			/* 2 x double word, R */
#define MOTOR_INCREMENT_TYPE		4
#define MOTOR_INCREMENT_SIZE		(2 * MOTOR_INCREMENT_TYPE)

#define MOTOR_REG_POSITION		0x50			/* 3 x word, R */
#define MOTOR_POSITION_TYPE		2
#define MOTOR_POSITION_SIZE		(3 * MOTOR_POSITION_TYPE)

/* Speed in mm/s with 3.33 mm/s resolution */
#define MOTOR_SPEED_FROM_REG(x)		(((s8)x * 10) / 3)
#define MOTOR_SPEED_TO_REG(x)		((u8)(((s16)x > 423) ? 127 : \
					      ((s16)x < -426) ? -128 : \
					      ((s16)x * 3) / 10))

/* Increment in mm */
#define MOTOR_INCREMENT_FROM_REG(x)	((((s32)x * 314) / 100) * 30 / (127 * 14))

/* Position in mm */
#define MOTOR_POSITION_FROM_REG(x)	((s16)x)
#define MOTOR_YAW_FROM_REG(x)		((s16)x)

#define MOTOR_SPEED_LEFT(x, yaw)	(x + ((yaw * 75) / (2 * 1000)))
#define MOTOR_SPEED_RIGHT(x, yaw)	(x - ((yaw * 75) / (2 * 1000)))

static struct i2c_driver motor_driver;

struct motor_data {
	struct i2c_client *client;
	struct mutex update_lock;
	char valid;			/* !=0 if following fields are valid */
	unsigned long last_updated;	/* In jiffies */

	/* Register values, byte */
	u8 speed[2];
	/* Register values, double word */
	u32 increment[2];
	u16 position[3];
};

static struct motor_data *motor_update_data(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct motor_data *data = i2c_get_clientdata(client);

	mutex_lock(&data->update_lock);

	if (time_after(jiffies, data->last_updated + msecs_to_jiffies(100))
	    || !data->valid) {

		dev_dbg(&client->dev, "Starting motor update\n");

		/* speed */
		i2c_smbus_read_i2c_block_data(client, MOTOR_REG_SPEED,
					      MOTOR_SPEED_SIZE, (u8 *) data->speed);

		/* position */
		i2c_smbus_read_i2c_block_data(client, MOTOR_REG_INCREMENT,
					      MOTOR_INCREMENT_SIZE, (u8 *) data->increment);

		/* position */
		i2c_smbus_read_i2c_block_data(client, MOTOR_REG_POSITION,
					      MOTOR_POSITION_SIZE, (u8 *) data->position);

		data->last_updated = jiffies;
		data->valid = 1;
	}

	mutex_unlock(&data->update_lock);

	return data;
}

/* sysfs callback functions */
#define show(value, index, format)						\
static ssize_t show_##value##index(struct device *dev,				\
				   struct device_attribute *attr, char *buf)	\
{										\
	struct motor_data *data = motor_update_data(dev);			\
	return sprintf(buf, "%d\n", format(data->value[index]));		\
}

show(speed, 0, MOTOR_SPEED_FROM_REG);
show(speed, 1, MOTOR_SPEED_FROM_REG);
show(increment, 0, MOTOR_INCREMENT_FROM_REG);
show(increment, 1, MOTOR_INCREMENT_FROM_REG);
show(position, 0, MOTOR_POSITION_FROM_REG);
show(position, 1, MOTOR_POSITION_FROM_REG);
show(position, 2, MOTOR_YAW_FROM_REG);

#define store(value, index, format, reg)					\
static ssize_t store_##value##index(struct device *dev,				\
				    struct device_attribute *attr, 		\
				    const char *buf, size_t count)		\
{										\
	struct i2c_client *client = to_i2c_client(dev);				\
	struct motor_data *data = i2c_get_clientdata(client);			\
	short val = format(simple_strtol(buf, NULL, 0));			\
										\
	mutex_lock(&data->update_lock);						\
	i2c_smbus_write_byte_data(client, reg + index, val);			\
	data->valid = 0;							\
	mutex_unlock(&data->update_lock);					\
	return count;								\
}

store(speed, 0, MOTOR_SPEED_TO_REG, MOTOR_REG_SPEED);
store(speed, 1, MOTOR_SPEED_TO_REG, MOTOR_REG_SPEED);

static DEVICE_ATTR(speed_left, S_IWUGO | S_IRUGO,
		   show_speed0, store_speed0);
static DEVICE_ATTR(speed_right, S_IWUGO | S_IRUGO,
		   show_speed1, store_speed1);
static DEVICE_ATTR(increment_left, S_IRUGO,
		   show_increment0, NULL);
static DEVICE_ATTR(increment_right, S_IRUGO,
		   show_increment1, NULL);
static DEVICE_ATTR(position_x, S_IRUGO,
		   show_position0, NULL);
static DEVICE_ATTR(position_y, S_IRUGO,
		   show_position1, NULL);
static DEVICE_ATTR(position_yaw, S_IRUGO,
		   show_position2, NULL);

static struct attribute *motor_attributes[] = {
	&dev_attr_speed_left.attr,
	&dev_attr_speed_right.attr,
	&dev_attr_increment_left.attr,
	&dev_attr_increment_right.attr,
	&dev_attr_position_x.attr,
	&dev_attr_position_y.attr,
	&dev_attr_position_yaw.attr,
	NULL
};

static struct attribute_group motor_attr_group = {
	.attrs = motor_attributes,
};

static ssize_t motor_speed_read(struct kobject *kobj, struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	u8 speed[2];
	s16 *value = (s16 *) buf;
	int i;

	dev_dbg(&client->dev, "Starting motor speed read (p=%p, off=%lli, c=%zi)\n",
		buf, off, count);

	if (count == 0)
		return 0;

	if (count < MOTOR_SPEED_SIZE * 2)
		return -EINVAL;
	
	if (i2c_smbus_read_i2c_block_data(client, MOTOR_REG_SPEED,
					   2, speed) < 0)
		return -EIO;

	for(i = 0; i < 2; i++)
		value[i] = MOTOR_SPEED_FROM_REG(speed[i]);

	return MOTOR_SPEED_SIZE * 2;
}

static ssize_t motor_speed_write(struct kobject *kobj, struct bin_attribute *attr,
				   char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	struct motor_data *data = i2c_get_clientdata(client);
	u8 speed[2];
	s16 *value = (s16 *) buf;
	int i;
	int rc;

	dev_dbg(&client->dev, "Starting motor speed write (p=%p, off=%lli, c=%zi)\n",
		buf, off, count);

	if (count == 0)
		return 0;

	if (count < MOTOR_SPEED_SIZE * 2)
		return -EINVAL;

	for(i = 0; i < 2; i++)
		speed[i] = (u8) MOTOR_SPEED_TO_REG(value[i]);

	mutex_lock(&data->update_lock);

	/* Write out to the device */
	if (i2c_smbus_write_i2c_block_data(client, MOTOR_REG_SPEED,
					   2, speed) < 0)
		rc = -EIO;
	else
		rc = MOTOR_SPEED_SIZE * 2;

	data->valid = 0;

	mutex_unlock(&data->update_lock);

	return rc;
}

static struct bin_attribute motor_speed_attr = {
	.attr = {
		.name = "speed",
		.mode = S_IRUGO | S_IWUGO,
		.owner = THIS_MODULE,
	},
	.size = MOTOR_SPEED_SIZE * 2,
	.read = motor_speed_read,
	.write = motor_speed_write,
};

static ssize_t motor_position_read(struct kobject *kobj, struct bin_attribute *attr,
				  char *buf, loff_t off, size_t count)
{
	struct i2c_client *client = kobj_to_i2c_client(kobj);
	s32 *value = (s32 *) buf ;
	int i;

	dev_dbg(&client->dev, "Starting motor position read (p=%p, off=%lli, c=%zi)\n",
		buf, off, count);

	if (count == 0)
		return 0;

	if (count > MOTOR_POSITION_SIZE)
		return -EINVAL;

	if (i2c_smbus_read_i2c_block_data(client, MOTOR_REG_POSITION,
					   MOTOR_POSITION_SIZE, buf) < 0)
		return -EIO;

	for(i = 0; i < 2; i++)
		value[i] = MOTOR_POSITION_FROM_REG(value[i]);
	value[2] = MOTOR_YAW_FROM_REG(value[2]);

	return count;
}

static struct bin_attribute motor_position_attr = {
	.attr = {
		.name = "position",
		.mode = S_IRUGO,
		.owner = THIS_MODULE,
	},
	.size = MOTOR_POSITION_SIZE,
	.read = motor_position_read,
};





/*
 * Called when a motor device is matched with this driver
 */
static int motor_probe(struct i2c_client *client, 
		       const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct motor_data *data;
	int rc = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA
				     | I2C_FUNC_SMBUS_I2C_BLOCK)) {
		rc = -EIO;
		goto exit;
	}

	data = kzalloc(sizeof(struct motor_data), GFP_KERNEL);
	if (!data) {
		rc = -ENOMEM;
		goto exit;
	}

	data->client = client;
	i2c_set_clientdata(client, data);
	data->valid = 0;

	mutex_init(&data->update_lock);

	/* set speed to 0 */
	rc = i2c_smbus_write_word_data(client, MOTOR_REG_SPEED, 0x0);
	if (rc < 0)
		goto exit_kfree;

	/* Register sysfs hooks */
	rc = sysfs_create_group(&client->dev.kobj, &motor_attr_group);
	if (rc)
		goto exit_kfree;

	/* Register sysfs bin file speed */
	rc = sysfs_create_bin_file(&client->dev.kobj, &motor_speed_attr);
	if (rc)
		goto exit_remove_sys;

	/* Register sysfs bin file position */
	rc = sysfs_create_bin_file(&client->dev.kobj, &motor_position_attr);
	if (rc)
		goto exit_remove_bin;

	return 0;

exit_remove_bin:
	sysfs_remove_bin_file(&client->dev.kobj, &motor_speed_attr);
exit_remove_sys:
	sysfs_remove_group(&client->dev.kobj, &motor_attr_group);	
exit_kfree:
	kfree(data);
exit:
	return rc;
}

static int motor_remove(struct i2c_client *client)
{
	sysfs_remove_bin_file(&client->dev.kobj, &motor_position_attr);
	sysfs_remove_bin_file(&client->dev.kobj, &motor_speed_attr);
	sysfs_remove_group(&client->dev.kobj, &motor_attr_group);

	/* set speed to 0 */
	i2c_smbus_write_word_data(client, MOTOR_REG_SPEED, 0x0);

	kfree(i2c_get_clientdata(client));

	return 0;
}

static const struct i2c_device_id motor_id[] = {
	{ "motor", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, motor_id);

static struct i2c_driver motor_driver = {
	.driver = {
		.name	= "motor",
	},
	.probe		= motor_probe,
	.remove		= motor_remove,
	.id_table 	= motor_id,
};

static int __init motor_init(void)
{
	return i2c_add_driver(&motor_driver);
}

static void __exit motor_exit(void)
{
	i2c_del_driver(&motor_driver);
}

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("motor driver");
MODULE_LICENSE("GPL");

module_init(motor_init);
module_exit(motor_exit);



