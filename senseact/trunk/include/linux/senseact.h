#ifndef _SENSEACT_H
#define _SENSEACT_H

/*
 * Copyright (c) 1999-2002 Vojtech Pavlik
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#ifdef __KERNEL__
#include <linux/time.h>
#include <linux/list.h>
#else
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/types.h>
#include <linux/types.h>
#endif

/*
 * Protocol version.
 */
#define SENSEACT_VERSION		0x0010

/*
 * The event structure itself
 */
struct senseact_action {
	__u8 type;
	__s8 prefix;
	__u8 unit;
	__u8 index;
	__s32 value;
};

#define senseact_action_size(void) sizeof(struct senseact_action)

/*
 * Action types
 */
#define SENSEACT_TYPE_SYNC		0x00
#define SENSEACT_TYPE_BRIGHTNESS	0x01
#define SENSEACT_TYPE_ENABLE		0x02
#define SENSEACT_TYPE_SPEED		0x03
#define SENSEACT_TYPE_POSITION		0x04
#define SENSEACT_TYPE_ANGLE		0x05
#define SENSEACT_TYPE_INCREMENT		0x06
#define SENSEACT_TYPE_MAX		0x07
#define SENSEACT_TYPE_CNT		(SENSEACT_TYPE_MAX + 1)

/*
 * Action prefix
 */
#define SENSEACT_PREFIX_ZETTA		7
#define SENSEACT_PREFIX_EXA		6
#define SENSEACT_PREFIX_PETA		5
#define SENSEACT_PREFIX_TERA		4
#define SENSEACT_PREFIX_GIGA		3
#define SENSEACT_PREFIX_MEGA		2
#define SENSEACT_PREFIX_KILO		1
#define SENSEACT_PREFIX_NONE		0
#define SENSEACT_PREFIX_MILLI		-1
#define SENSEACT_PREFIX_MICRO		-2
#define SENSEACT_PREFIX_NANO		-3
#define SENSEACT_PREFIX_PICO		-4
#define SENSEACT_PREFIX_FEMTO		-5
#define SENSEACT_PREFIX_ATTO		-6
#define SENSEACT_PREFIX_ZEPTO		-7
#define SENSEACT_PREFIX_YOCTO		-8

/*
 * Sync subtypes
 */
#define SENSEACT_SYNC_SENSOR		1
#define SENSEACT_SYNC_ACTOR		2

/*
 * In-kernel definitions.
 */
#ifdef __KERNEL__

#include <linux/device.h>
#include <linux/fs.h>
#include <linux/timer.h>
#include <linux/mod_devicetable.h>

/**
 * struct senseact_dev - represents an input device
 * @name: name of the device
 * @private: private driver data
 * @types: bit mask for supported types
 * @open: this method is called when the very first user calls
 *	senseact_open_device(). The driver must prepare the device
 *	to start generating actions (start polling thread,
 *	request an IRQ, submit URB, etc.)
 * @close: this method is called when the very last user calls
 *	senseact_close_device().
 * @flush: purges the device. Most commonly used to get rid of actor
 *	values loaded into the device when disconnecting from it
 * @pass: action handler for actions sent _to_ the device, like.
 *      The device is expected to carry out the requested
 *	action. The call is protected by @action_lock and must not sleep ????
 * @mutex: serializes calls to open(), close() and flush() methods
 * @users: stores number of users that opened this device.
 *      It is used by senseact_open_device() and senseact_close_device()
 *	to make sure that dev->open() is only called when the first
 *	user opens device and dev->close() is called when the very
 *	last user closes the device
 * @going_away: marks devices that are in a middle of unregistering and
 *	causes senseact_open_device*() fail with -ENODEV.
 * @action_lock: this spinlock is is taken when senseact core receives
 *	and processes a new actions for the device.
 * @queue_list: list of senseact handles associated with the device. When
 *	accessing the list dev->queue_lock must be held
 * @queue_lock: this spinlock is is taken when senseact core receives
 *	and processes a new action for the user.
 * @dev: driver model's view of this device
 */
struct senseact_device {
	const char *name;

	void *private;

	unsigned long types[BITS_TO_LONGS(SENSEACT_TYPE_CNT)];

	int (*open)(struct senseact_device *senseact);
	void (*close)(struct senseact_device *senseact);
	int (*flush)(struct senseact_device *senseact, struct file *file);
	int (*pass)(struct senseact_device *senseact, unsigned int type, unsigned int index, unsigned int count, int *values);

	struct mutex mutex;

	unsigned int minor;
	unsigned int users;
	int going_away;

	spinlock_t action_lock;

	struct list_head queue_list;
	spinlock_t queue_lock;

	wait_queue_head_t wait;	

	struct device dev;
};

#define to_senseact_device(d) container_of(d, struct senseact_device, dev)

struct senseact_device *senseact_allocate_device(void);
void senseact_free_device(struct senseact_device *senseact);

void senseact_set_capabilities(struct senseact_device *senseact, unsigned int type, unsigned int count);

static inline void senseact_set_capability(struct senseact_device *senseact, unsigned int type)
{
	senseact_set_capabilities(senseact, type, 1);
}

int __must_check senseact_register_device(struct senseact_device *senseact);
void senseact_unregister_device(struct senseact_device *senseact);

static inline void *senseact_get_drvdata(struct senseact_device *senseact)
{
	return dev_get_drvdata(&senseact->dev);
}

static inline void senseact_set_drvdata(struct senseact_device *senseact, void *data)
{
	dev_set_drvdata(&senseact->dev, data);
}

void senseact_pass_actions(struct senseact_device *senseact, unsigned int type, unsigned int prefix, unsigned int index, unsigned int count, int *values);

static inline void senseact_pass_action(struct senseact_device *senseact,  unsigned int type, unsigned int prefix, unsigned int index, int value)
{
	senseact_pass_actions(senseact, type, prefix, index, 1, &value);
}

static inline void senseact_sync(struct senseact_device *senseact, unsigned int index)
{
	senseact_pass_action(senseact, SENSEACT_TYPE_SYNC, 0, index, 0);
}

#endif
#endif
