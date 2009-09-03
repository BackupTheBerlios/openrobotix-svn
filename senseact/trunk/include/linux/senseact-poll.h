#ifndef _SENSEACT_POLL_H
#define _SENSEACT_POLL_H

/*
 * Copyright (c) 2007 Dmitry Torokhov
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include "senseact.h"
#include <linux/workqueue.h>

/**
 * struct senseact_polled_dev - simple polled senseact device
 * @poll: driver-supplied method that polls the device and posts
 *	senseact events (mandatory).
 * @poll_interval: specifies how often the poll() method shoudl be called.
 * @senseact: senseact device structure associated with the poll device.
 *	Must be properly initialized by the driver.
 *
 * Polled senseact device provides a skeleton for supporting simple senseact
 * devices that do not raise interrupts but have to be periodically
 * scanned or polled to detect changes in their state.
 */
struct senseact_poll_device {
	int (*poll)(struct senseact_poll_device *dev);
	unsigned int poll_interval; /* msec */

	struct senseact_device *senseact;
	struct delayed_work work;
};

struct senseact_poll_device *senseact_allocate_poll_device(void);
void senseact_free_poll_device(struct senseact_poll_device *senseact_poll);
int senseact_register_poll_device(struct senseact_poll_device *senseact_poll);
void senseact_unregister_poll_device(struct senseact_poll_device *senseact_poll);

#endif
