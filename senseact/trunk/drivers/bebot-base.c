/*
 * Driver for BeBot baseboard
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
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/senseact-poll.h>

/* Register */
#define MOTOR_REG_TYPE			0x00			/* byte, R */
#define MOTOR_REG_TYPE_VALUE		0x12

#define MOTOR_REG_CONFIG		0x02			/* byte, R */
	#define MOTOR_REG_CONFIG_CLEAR_POS	1

#define SETSPEED_REG		0x10	/* 4 x byte, RW */
#define SETSPEED_TYPE		1
#define SETSPEED_COUNT		2
#define SETSPEED_SIZE		(SETSPEED_COUNT * SETSPEED_TYPE)

#define GETSPEED_REG		0x12	/* 4 x byte, RW */
#define GETSPEED_TYPE		1
#define GETSPEED_COUNT		2
#define GETSPEED_SIZE		(GETSPEED_COUNT * GETSPEED_TYPE)

#define INCREMENT_REG		0x40	/* 2 x word, R */
#define INCREMENT_TYPE		2
#define INCREMENT_COUNT		2
#define INCREMENT_SIZE		(INCREMENT_COUNT * INCREMENT_TYPE)

#define POSITION_REG		0x50			/* 2 x double word + 1 x word, R */
#define POSITION_TYPE		4
#define POSITION_COUNT		2
#define ANGLE_TYPE		2
#define ANGLE_COUNT		1
#define POSITION_SIZE		(POSITION_COUNT * POSITION_TYPE + \
				 ANGLE_COUNT * ANGLE_TYPE)

/* Speed in mm/s with 3.33 mm/s resolution */
#define SPEED_FROM_REG(x)	((x * 10) / 3)
#define SPEED_TO_REG(x)		((x > 400) ? 120 : \
				 (x < -400) ? -120 : \
				 ((x * 3) / 10))

/* Increment in mm */
#define INCREMENT_FROM_REG(x)	(x)

/* Position in mm */
#define POSITION_FROM_REG(x)	(x)

struct bebot_base_device {
	struct senseact_poll_device *senseact_poll;
	struct i2c_client *client;
	char addr[32];
	u8 speed[SETSPEED_COUNT];
};

/*
 * Timer function which is run every x ms when the device is opened.
 */
static int bebot_base_poll(struct senseact_poll_device *senseact_poll)
{
	struct senseact_device *senseact = senseact_poll->senseact;
	struct bebot_base_device *base = senseact_get_drvdata(senseact);
	struct i2c_client *client = base->client;
	u8 buffer[12];
	int temp;
	int values[3];
	int n, i;

	/* read position */
	n = i2c_smbus_read_i2c_block_data(client, POSITION_REG,
					  POSITION_SIZE, buffer);
	if (n <= 0)
		return n;

	for (i = 0; i < POSITION_COUNT; i++) {
		temp = buffer[(i * 4) + 3] << 8 | buffer[(i * 4) + 2] |
		       buffer[(i * 4) + 1] << 8 | buffer[i * 4];	
		values[i] = INCREMENT_FROM_REG(le32_to_cpu(temp));
	}

	temp = buffer[(POSITION_COUNT * 4) + 1] << 8 | buffer[POSITION_COUNT * 4];
	values[i] = INCREMENT_FROM_REG(le32_to_cpu(temp));

	senseact_pass_actions(senseact, SENSEACT_TYPE_POSITION, SENSEACT_PREFIX_MILLI, 0, POSITION_COUNT, values);
	senseact_pass_action(senseact, SENSEACT_TYPE_ANGLE, SENSEACT_PREFIX_MILLI, 0, values[POSITION_COUNT]);

	/* read increment */
	n = i2c_smbus_read_i2c_block_data(client, INCREMENT_REG,
					  INCREMENT_SIZE, buffer);
	if (n <= 0)
		return n;

	for (i = 0; i < INCREMENT_COUNT; i++) {
		temp = buffer[(i * 2) + 1] << 8 | buffer[i * 2];
		values[i] = INCREMENT_FROM_REG(le32_to_cpu(temp));
	}

	senseact_pass_actions(senseact, SENSEACT_TYPE_INCREMENT, SENSEACT_PREFIX_NONE, 0, INCREMENT_COUNT, values);

	/* read speed */
	n = i2c_smbus_read_i2c_block_data(client, GETSPEED_REG,
					  GETSPEED_SIZE, buffer);
	if (n <= 0)
		return n;

	for (i = 0; i < GETSPEED_COUNT; i++) {
		temp =  buffer[i];
		values[i] = SPEED_FROM_REG(le32_to_cpu(temp));
	}

	senseact_pass_actions(senseact, SENSEACT_TYPE_SPEED, SENSEACT_PREFIX_MILLI, 2, GETSPEED_COUNT, values);
	senseact_sync(senseact, SENSEACT_SYNC_SENSOR);

	return 0;
}

static int bebot_base_pass(struct senseact_device *senseact, unsigned int type, unsigned int index, unsigned int count, int *values)
{
	struct bebot_base_device *base = senseact_get_drvdata(senseact);
	struct i2c_client *client = base->client;
	int buffer[SETSPEED_COUNT];
	int i, n, rc;

	for (i = 0; i < count; i++) {
		switch (type) {
		case SENSEACT_TYPE_SPEED:
			if ((index + i) < SETSPEED_COUNT) {
				base->speed[index + i] = SPEED_TO_REG(values[i]);
			}
			break;

		case SENSEACT_TYPE_SYNC:
			rc = i2c_smbus_write_i2c_block_data(client, SETSPEED_REG, SETSPEED_SIZE, base->speed);
			if (rc < 0)
				return rc;

			for (n = 0; n < SETSPEED_COUNT; n++)
				values[n] = SPEED_FROM_REG(base->speed[n]);

			senseact_pass_actions(senseact, SENSEACT_TYPE_SPEED, SENSEACT_PREFIX_MILLI, 0, SETSPEED_COUNT, buffer);
			senseact_sync(senseact, SENSEACT_SYNC_ACTOR);

			break;
		}
	}

	return 0;
}

static int bebot_base_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct bebot_base_device *base;
	struct senseact_poll_device *senseact_poll;
	struct senseact_device *senseact;
	int rc;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_WORD_DATA
				     | I2C_FUNC_SMBUS_I2C_BLOCK))
		return -ENODEV;

	base = kzalloc(sizeof(struct bebot_base_device), GFP_KERNEL);
	if (!base) {
		dev_err(&client->dev, "not enough memory for bebot_base device\n");
		rc = -ENOMEM;
		goto exit;
	}

	/* set speed to 0 */
	rc = i2c_smbus_write_i2c_block_data(client, SETSPEED_REG, SETSPEED_COUNT, base->speed);
	if (rc < 0)
		goto exit_kfree;

	base->client = client;

	senseact_poll = senseact_allocate_poll_device();
	if (!senseact_poll) {
		dev_err(&client->dev, "not enough memory for senseact poll device\n");
		rc = -ENOMEM;
		goto exit_kfree;
	}

	base->senseact_poll = senseact_poll;

	/* set senseact poll device handler */
	senseact_poll->poll = bebot_base_poll;
	senseact_poll->poll_interval = 250;

	/* set senseact device handler */	
	senseact = senseact_poll->senseact;
	senseact->name = client->name;
	snprintf(base->addr, sizeof(base->addr), "%01d-%04x", client->adapter->nr, client->addr);
	senseact->addr = base->addr;
	senseact->dev.parent = &client->dev;
	senseact->pass = bebot_base_pass;

	senseact_set_drvdata(senseact, base);

	senseact_set_capabilities(senseact, SENSEACT_TYPE_SPEED, 4);
	senseact_set_capabilities(senseact, SENSEACT_TYPE_INCREMENT, 2);
	senseact_set_capabilities(senseact, SENSEACT_TYPE_POSITION, 2);
	senseact_set_capability(senseact, SENSEACT_TYPE_ANGLE);

	rc = senseact_register_poll_device(senseact_poll);
	if (rc) {
		dev_err(&client->dev, "could not register senseact poll device\n");
		goto exit_free_poll;
	}

	i2c_set_clientdata(client, base);

	return 0;

exit_free_poll:
	senseact_free_poll_device(senseact_poll);
exit_kfree:
	kfree(base);
exit:
	return rc;
}

static int bebot_base_remove(struct i2c_client *client)
{
	struct bebot_base_device *base = i2c_get_clientdata(client);
	struct senseact_poll_device *senseact_poll = base->senseact_poll;

	senseact_unregister_poll_device(senseact_poll);

	/* set speed to 0 */
	i2c_smbus_write_word_data(client, SETSPEED_REG, 0x0);

	senseact_free_poll_device(senseact_poll);

	kfree(base);

	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id bebot_base_id[] = {
	{ "bebot-base", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, bebot_base_id);

static struct i2c_driver bebot_base_driver = {
	.driver = {
		.name	= "bebot-base",
	},
	.probe		= bebot_base_probe,
	.remove		= bebot_base_remove,
	.id_table 	= bebot_base_id,
};

static int __init bebot_base_init(void)
{
	return i2c_add_driver(&bebot_base_driver);
}
module_init(bebot_base_init);

static void __exit bebot_base_exit(void)
{
	i2c_del_driver(&bebot_base_driver);
}
module_exit(bebot_base_exit);

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("BeBot base driver");
MODULE_LICENSE("GPL");






