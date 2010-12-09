/*
 * Driver for BeBot infrared sensors
 *
 * Copyright (C) 2009
 * Heinz Nixdorf Institute - University of Paderborn
 * Department of System and Circuit Technology
 * Stefan Herbrechtsmeier <hbmeier@hni.uni-paderborn.de
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/senseact-poll.h>

#define SENSOR_REG		0x20	/* word, R */
#define SENSOR_TYPE		2	/* word */
#define SENSOR_COUNT		12
#define SENSOR_SIZE		(SENSOR_COUNT * SENSOR_TYPE)

#define ENABLE_REG		0x2F	/* byte, RW */
#define ENABLE_REG2		0x30	/* word, RW */

struct bebot_ir_device {
	struct senseact_poll_device *senseact_poll;
	struct i2c_client *client;
	char addr[32];
	u16 count;
	u16 enable;
};

static int bebot_ir_write_enable(struct bebot_ir_device *ir)
{
	int rc;

	if (ir->count == 6)
		rc = i2c_smbus_write_byte_data(ir->client, ENABLE_REG, ir->enable);
	else
		rc = i2c_smbus_write_word_data(ir->client, ENABLE_REG2, ir->enable);
	return rc;
}

/*
 * Timer function which is run every x ms when the device is opened.
 */
static int bebot_ir_poll(struct senseact_poll_device *senseact_poll)
{
	struct senseact_device *senseact = senseact_poll->senseact;
	struct bebot_ir_device *ir = senseact_get_drvdata(senseact);
	struct i2c_client *client = ir->client;
	u8 buffer[SENSOR_SIZE];
	u16 temp;
	int values[SENSOR_COUNT];
	int n, i;

	n = i2c_smbus_read_i2c_block_data(client, SENSOR_REG,
					  SENSOR_TYPE * ir->count, buffer);
	if (n <= 0)
		return n;

	for (i = 0; i < (n / SENSOR_TYPE); i++) {
		temp = (buffer[(i * 2) + 1] << 8) | buffer[i * 2];
		values[i] = le16_to_cpu(temp);
	}

	senseact_pass_actions(senseact, SENSEACT_TYPE_BRIGHTNESS, SENSEACT_PREFIX_NONE, 0, n / SENSOR_TYPE, values);
	senseact_sync(senseact, SENSEACT_SYNC_SENSOR);

	return 0;
}

static int bebot_ir_pass(struct senseact_device *senseact, unsigned int type, unsigned int index, unsigned int count, int *values)
{
	struct bebot_ir_device *ir = senseact_get_drvdata(senseact);
	struct i2c_client *client = ir->client;
	int buffer[SENSOR_COUNT];
	int i, n, rc;

	for (i = 0; i < count; i++) {
		switch (type) {
		case SENSEACT_TYPE_ENABLE:
			if (index + i < ir->count) {
				if (values[i])
					ir->enable |= (1 << (index + i));
				else
					ir->enable &= ~(1 << (index + i));

			}
			break;

		case SENSEACT_TYPE_SYNC:
			rc = bebot_ir_write_enable(ir);
			if (rc < 0)
				return rc;

			for (n = 0; n < ir->count; n++)
				buffer[n] = (ir->enable & (1 << n)) ? 1 : 0;

			senseact_pass_actions(senseact, SENSEACT_TYPE_ENABLE, SENSEACT_PREFIX_NONE, 0, ir->count, buffer);
			senseact_sync(senseact, SENSEACT_SYNC_ACTOR);

			break;
		}
	}

	return 0;
}

static int bebot_ir_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bebot_ir_device *ir;
	struct senseact_poll_device *senseact_poll;
	struct senseact_device *senseact;
	int rc;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE_DATA
				     | I2C_FUNC_SMBUS_WORD_DATA
				     | I2C_FUNC_SMBUS_I2C_BLOCK))
		return -ENODEV;

	ir = kmalloc(sizeof(struct bebot_ir_device), GFP_KERNEL);
	if (!ir) {
		dev_err(&client->dev, "not enough memory for bebot_ir device\n");
		rc = -ENOMEM;
		goto exit;
	}

	ir->client = client;

	/* enable all LEDs */
	ir->count = id->driver_data;
	ir->enable = 0xff;
	rc = bebot_ir_write_enable(ir);
	if (rc < 0)
		goto exit_kfree;


	senseact_poll = senseact_allocate_poll_device();
	if (!senseact_poll) {
		dev_err(&client->dev, "not enough memory for senseact poll device\n");
		return -ENOMEM;
		goto exit_kfree;		
	}

	ir->senseact_poll = senseact_poll;

	/* set senseact poll device handler */
	senseact_poll->poll = bebot_ir_poll;
	senseact_poll->poll_interval = 250;

	/* set senseact device handler */	
	senseact = senseact_poll->senseact;
	senseact->name = client->name;
	snprintf(ir->addr, sizeof(ir->addr), "%01d-%04x", client->adapter->nr, client->addr);
	senseact->addr = ir->addr;
	senseact->dev.parent = &client->dev;
	senseact->pass = bebot_ir_pass;

	senseact_set_drvdata(senseact, ir);

	senseact_set_capabilities(senseact, SENSEACT_TYPE_BRIGHTNESS, ir->count);
	senseact_set_capabilities(senseact, SENSEACT_TYPE_ENABLE, ir->count);

	rc = senseact_register_poll_device(senseact_poll);
	if (rc) {
		dev_err(&client->dev, "could not register senseact poll device\n");
		goto exit_free_poll;
	}

	i2c_set_clientdata(client, ir);

	return 0;

exit_free_poll:
	senseact_free_poll_device(senseact_poll);
exit_kfree:
	kfree(ir);
exit:
	return rc;
}

static int bebot_ir_remove(struct i2c_client *client)
{
	struct bebot_ir_device *ir = i2c_get_clientdata(client);
	struct senseact_poll_device *senseact_poll = ir->senseact_poll;

	senseact_unregister_poll_device(senseact_poll);
	senseact_free_poll_device(senseact_poll);

	kfree(ir);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id bebot_ir_id[] = {
	{ "bebot-ir", 6 },
	{ "bebot-ir2", 12 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bebot_ir_id);

static struct i2c_driver bebot_ir_driver = {
	.driver = {
		.name	= "bebot-ir",
	},
	.probe		= bebot_ir_probe,
	.remove		= bebot_ir_remove,
	.id_table 	= bebot_ir_id,
};

static int __init bebot_ir_init(void)
{
	return i2c_add_driver(&bebot_ir_driver);
}
module_init(bebot_ir_init);

static void __exit bebot_ir_exit(void)
{
	i2c_del_driver(&bebot_ir_driver);
}
module_exit(bebot_ir_exit);

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("BeBot ir driver");
MODULE_LICENSE("GPL");
