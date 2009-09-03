/*
 * Generic implementation of a polled sensor and actor device
 *
 * based on input-polldev
 * Copyright (c) 2007 Dmitry Torokhov
 *
 * Copyright (c) 2009 Stefan Herbrechtsmeier
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/jiffies.h>
#include <linux/mutex.h>
#include <linux/senseact-poll.h>

static DEFINE_MUTEX(senseact_poll_mutex);
static int senseact_poll_users;
static struct workqueue_struct *senseact_poll_wq;

static int senseact_poll_start_workqueue(void)
{
	int rc;

	rc = mutex_lock_interruptible(&senseact_poll_mutex);
	if (rc)
		return rc;

	if (!senseact_poll_users) {
		senseact_poll_wq = create_singlethread_workqueue("senseactpolld");
		if (!senseact_poll_wq) {
			printk(KERN_ERR "senseact-poll: failed to create "
				"senseactpolld workqueue\n");
			rc = -ENOMEM;
			goto out;
		}
	}

	senseact_poll_users++;

 out:
	mutex_unlock(&senseact_poll_mutex);
	return rc;
}

static void senseact_poll_stop_workqueue(void)
{
	mutex_lock(&senseact_poll_mutex);

	if (!--senseact_poll_users)
		destroy_workqueue(senseact_poll_wq);

	mutex_unlock(&senseact_poll_mutex);
}

static void senseact_poll_work(struct work_struct *work)
{
	struct senseact_poll_device *senseact_poll =
		container_of(work, struct senseact_poll_device, work.work);
	unsigned long delay;

	senseact_poll->poll(senseact_poll);

	delay = msecs_to_jiffies(senseact_poll->poll_interval);
	if (delay >= HZ)
		delay = round_jiffies_relative(delay);

	queue_delayed_work(senseact_poll_wq, &senseact_poll->work, delay);
}

static int senseact_poll_open(struct senseact_device *senseact)
{
	struct senseact_poll_device *senseact_poll = senseact->private;
	int rc;

	rc = senseact_poll_start_workqueue();
	if (rc)
		return rc;

	queue_delayed_work(senseact_poll_wq, &senseact_poll->work,
			   msecs_to_jiffies(senseact_poll->poll_interval));

	return 0;
}

static void senseact_poll_close(struct senseact_device *senseact)
{
	struct senseact_poll_device *senseact_poll = senseact->private;

	cancel_delayed_work_sync(&senseact_poll->work);
	senseact_poll_stop_workqueue();
}

/**
 * senseact_allocate_poll_device - allocate memory for poll device
 *
 * The function allocates memory for a poll device and also
 * for an senseact device associated with this poll device.
 */
struct senseact_poll_device *senseact_allocate_poll_device(void)
{
	struct senseact_poll_device *senseact_poll;

	senseact_poll = kzalloc(sizeof(struct senseact_poll_device), GFP_KERNEL);
	if (!senseact_poll)
		return NULL;

	senseact_poll->senseact = senseact_allocate_device();
	if (!senseact_poll->senseact) {
		kfree(senseact_poll);
		return NULL;
	}

	return senseact_poll;
}
EXPORT_SYMBOL(senseact_allocate_poll_device);

/**
 * senseact_free_poll_device - free memory allocated for poll device
 * @senseact_poll: device to free
 *
 * The function frees memory allocated for poll device and drops
 * reference to the associated senseact device (if present).
 */
void senseact_free_poll_device(struct senseact_poll_device *senseact_poll)
{
	if (senseact_poll) {
		senseact_free_device(senseact_poll->senseact);
		kfree(senseact_poll);
	}
}
EXPORT_SYMBOL(senseact_free_poll_device);

/**
 * senseact_register_poll_device - register poll device
 * @senseact_poll: device to register
 *
 * The function registers previously initialized poll  device with
 * senseact layer. The device should be allocated with call to
 * senseact_allocate_poll_device(). Callers should also set up poll()
 * method and set up capabilities (id, name, phys, ??) of the
 * corresponing senseact_device structure.
 */
int senseact_register_poll_device(struct senseact_poll_device *senseact_poll)
{
	struct senseact_device *senseact = senseact_poll->senseact;

	senseact->private = senseact_poll;
	INIT_DELAYED_WORK(&senseact_poll->work, senseact_poll_work);
	if (!senseact_poll->poll_interval)
		senseact_poll->poll_interval = 500;
	senseact->open = senseact_poll_open;
	senseact->close = senseact_poll_close;

	return senseact_register_device(senseact);
}
EXPORT_SYMBOL(senseact_register_poll_device);

/**
 * senseact_unregister_poll_device - unregister poll device
 * @senseact_poll: device to unregister
 *
 * The function unregisters previously registered poll senseact
 * device from senseact layer. Polling is stopped and device is
 * ready to be freed with call to senseact_free_poll_device().
 * Callers should not attempt to access dev->senseact pointer
 * after calling this function.
 */
void senseact_unregister_poll_device(struct senseact_poll_device *senseact_poll)
{
	senseact_unregister_device(senseact_poll->senseact);
	senseact_poll->senseact = NULL;
}
EXPORT_SYMBOL(senseact_unregister_poll_device);

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("Generic implementation of a polled sensor and actor device");
MODULE_LICENSE("GPL");
