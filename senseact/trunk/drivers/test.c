/*
 * Driver for senseact test
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
#include <linux/platform_device.h>
#include <linux/senseact-poll.h>

struct test_device {
	struct senseact_poll_device *senseact_poll;
	int values[2];
};

/*
 * Timer function which is run every x ms when the device is opened.
 */
static int test_poll(struct senseact_poll_device *senseact_poll)
{
	struct senseact_device *senseact = senseact_poll->senseact;
	struct test_device *test = senseact_get_drvdata(senseact);

	test->values[0]++;
	test->values[1]--;

	printk("senseact_poll POSITION %i - %i\n", test->values[0], test->values[1]);


	senseact_pass_actions(senseact, SENSEACT_TYPE_POSITION, SENSEACT_PREFIX_NONE, 0, 2, test->values);
	senseact_sync(senseact, SENSEACT_SYNC_SENSOR);

	return 0;
}

static int test_pass(struct senseact_device *senseact, unsigned int type, unsigned int index, unsigned int count, int *values)
{
	struct test_device *test = senseact_get_drvdata(senseact);
	int i, n;

	for (i = 0; i < count; i++) {
		switch (type) {
		case SENSEACT_TYPE_POSITION:
			n = index + i;
			if (n < 2) {
				printk("senseact_pass POSITION %i <- %i\n", n, values[i]);
				test->values[n] = values[i];
			} else {
				printk("senseact_pass POSITION overrun %i\n", n);
			}
			break;

		case SENSEACT_TYPE_SYNC:
			printk("senseact_pass SYNC\n");

			senseact_pass_actions(senseact, SENSEACT_TYPE_POSITION, SENSEACT_PREFIX_NONE, 0, 2, test->values);
			senseact_sync(senseact, SENSEACT_SYNC_ACTOR);

			break;
		}
	}

	return 0;
}

static int __devinit test_probe(struct platform_device *dev)
{
	struct test_device *test;
	struct senseact_poll_device *senseact_poll;
	struct senseact_device *senseact;
	int rc;

	test = kzalloc(sizeof(struct test_device), GFP_KERNEL);
	if (!test) {
		dev_err(&dev->dev, "not enough memory for test device\n");
		rc = -ENOMEM;
		goto exit;
	}

	senseact_poll = senseact_allocate_poll_device();
	if (!senseact_poll) {
		dev_err(&dev->dev, "not enough memory for senseact poll device\n");
		return -ENOMEM;
		goto exit_kfree;		
	}

	test->senseact_poll = senseact_poll;

	/* set senseact poll device handler */
	senseact_poll->poll = test_poll;
	senseact_poll->poll_interval = 2000;

	/* set senseact device handler */	
	senseact = senseact_poll->senseact;
	senseact->name = dev->name;
	senseact->dev.parent = &dev->dev;
	senseact->pass = test_pass;

	senseact_set_drvdata(senseact, test);

	senseact_set_capabilities(senseact, SENSEACT_TYPE_POSITION, 2);

	rc = senseact_register_poll_device(senseact_poll);
	if (rc) {
		dev_err(&dev->dev, "could not register senseact poll device\n");
		goto exit_free_poll;
	}

	platform_set_drvdata(dev, test);

	return 0;

exit_free_poll:
	senseact_free_poll_device(senseact_poll);
exit_kfree:
	kfree(test);
exit:
	return rc;
}

static int __devexit test_remove(struct platform_device *dev)
{
	struct test_device *test = platform_get_drvdata(dev);
	struct senseact_poll_device *senseact_poll = test->senseact_poll;

	senseact_unregister_poll_device(senseact_poll);
	platform_set_drvdata(dev, NULL);
	senseact_free_poll_device(senseact_poll);

	kfree(test);

	return 0;
}

static struct platform_device *test_device;

static struct platform_driver test_driver = {
	.driver		= {
		.name	= "senseact-test",
		.owner	= THIS_MODULE,
	},
	.probe		= test_probe,
	.remove		= test_remove,
};

static int __init test_init(void)
{
	int rc;

	rc = platform_driver_register(&test_driver);
	if (rc)
		return rc;

	test_device = platform_device_alloc("senseact-test", -1);
	if (!test_device) {
		rc = -ENOMEM;
		goto exit_unregister_driver;
	}

	rc = platform_device_add(test_device);
	if (rc)
		goto exit_free_device;

	return 0;

 exit_free_device:
	platform_device_put(test_device);
 exit_unregister_driver:
	platform_driver_unregister(&test_driver);

	return rc;
}
module_init(test_init);

static void __exit test_exit(void)
{
	platform_device_unregister(test_device);
	platform_driver_unregister(&test_driver);
}
module_exit(test_exit);

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("senseact test driver");
MODULE_LICENSE("GPL");
