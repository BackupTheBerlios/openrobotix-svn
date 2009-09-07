/*
 * The senseact core
 *
 * based on input
 * Copyright (c) 2007 Dmitry Torokhov
 *
 * Copyright (c) 2009 Stefan Herbrechtsmeier
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/input.h>
#include <linux/module.h>
#include <linux/random.h>
#include <linux/major.h>
#include <linux/proc_fs.h>
#include <linux/seq_file.h>
#include <linux/poll.h>
#include <linux/device.h>
#include <linux/mutex.h>
#include <linux/rcupdate.h>
#include <linux/smp_lock.h>

#include <linux/senseact.h>

#define SENSEACT_MAJOR			240

#define SENSEACT_DEVICES		32

#define SENSEACT_BUFFER_SIZE	64


struct senseact_queue {
	struct senseact_action buffer[SENSEACT_BUFFER_SIZE];
	int head;
	int tail;
	spinlock_t buffer_lock;
	struct fasync_struct *fasync;
	struct senseact_device *senseact;
	struct list_head node;
};

static struct senseact_device *senseact_table[SENSEACT_DEVICES];
static DEFINE_MUTEX(senseact_table_mutex);

static inline int is_type_supported(struct senseact_device *senseact, unsigned int code)
{
	return code <= SENSEACT_TYPE_MAX && test_bit(code, senseact->types);
}

static int senseact_open_device(struct senseact_device *senseact)
{
	int retval;

	retval = mutex_lock_interruptible(&senseact->mutex);
	if (retval)
		return retval;

	if (senseact->going_away) {
		retval = -ENODEV;
	} else if (!senseact->users++ && senseact->open) {
		retval = senseact->open(senseact);
		if (retval)
			senseact->users--;
	}

	mutex_unlock(&senseact->mutex);
	return retval;
}

static void senseact_close_device(struct senseact_device *senseact)
{
	mutex_lock(&senseact->mutex);

	if (!--senseact->users && senseact->close)
		senseact->close(senseact);

	mutex_unlock(&senseact->mutex);
}

static void senseact_attach_queue(struct senseact_device *senseact,
				struct senseact_queue *queue)
{
	spin_lock(&senseact->queue_lock);
	list_add_tail_rcu(&queue->node, &senseact->queue_list);
	spin_unlock(&senseact->queue_lock);
	synchronize_rcu();
}

static void senseact_detach_queue(struct senseact_device *senseact,
				struct senseact_queue *queue)
{
	spin_lock(&senseact->queue_lock);
	list_del_rcu(&queue->node);
	spin_unlock(&senseact->queue_lock);
	synchronize_rcu();
}

static void senseact_queue_insert_action(struct senseact_queue *queue,
			     struct senseact_action *action)
{
	struct senseact_device *senseact = queue->senseact;


	/*
	 * Interrupts are disabled, just acquire the lock
	 */
	spin_lock(&queue->buffer_lock);
	queue->buffer[queue->head++] = *action;
	queue->head %= SENSEACT_BUFFER_SIZE;
	spin_unlock(&queue->buffer_lock);

	if (queue->head == queue->tail)
		printk(KERN_ERR
			"senseact_user: buffer overrun on device %s\n", senseact->name);

	kill_fasync(&queue->fasync, SIGIO, POLL_IN);
}

static void senseact_handle_actions(struct senseact_device *senseact,
			unsigned int type, unsigned int prefix, unsigned int index, unsigned int count, int *values)
{
	struct senseact_queue *queue;
	struct senseact_action action;
	int i;

	for (i = 0; i < count; i++) {
		action.type = type;
		action.prefix = prefix;
		action.index = index + i;
		if (action.type == SENSEACT_TYPE_SYNC)
			action.value = jiffies_to_msecs(jiffies);
		else
			action.value = values[i];

		rcu_read_lock();

		list_for_each_entry_rcu(queue, &senseact->queue_list, node)
			senseact_queue_insert_action(queue, &action);

		rcu_read_unlock();
	}

	wake_up_interruptible(&senseact->wait);
}

/**
 * class functions
 */
#define SENSEACT_DEV_STRING_ATTR_SHOW(name)				\
static ssize_t senseact_dev_show_##name(struct device *dev,		\
					struct device_attribute *attr,	\
					char *buf)			\
{									\
	struct senseact_device *senseact = to_senseact_device(dev);	\
									\
	return scnprintf(buf, PAGE_SIZE, "%s\n",			\
			 senseact->name ? senseact->name : "");		\
}									\
static DEVICE_ATTR(name, S_IRUGO, senseact_dev_show_##name, NULL)

SENSEACT_DEV_STRING_ATTR_SHOW(name);
SENSEACT_DEV_STRING_ATTR_SHOW(addr);

static struct attribute *senseact_dev_attrs[] = {
	&dev_attr_name.attr,
	&dev_attr_addr.attr,
	NULL
};

static struct attribute_group senseact_dev_attr_group = {
	.attrs	= senseact_dev_attrs,
};

static struct attribute_group *senseact_dev_attr_groups[] = {
	&senseact_dev_attr_group,
	NULL
};

static void senseact_dev_release(struct device *dev)
{
	struct senseact_device *senseact = to_senseact_device(dev);

	kfree(senseact);

	module_put(THIS_MODULE);
}

static int senseact_dev_uevent(struct device *dev, struct kobj_uevent_env *env)
{
	struct senseact_device *senseact = to_senseact_device(dev);
	int retval;

	if (senseact->name) {
		retval = add_uevent_var(env, "NAME=\"%s\"", senseact->name);
		if (retval)
			return retval;
	}

	return 0;
}

static struct device_type senseact_type = {
	.groups		= senseact_dev_attr_groups,
	.release	= senseact_dev_release,
	.uevent		= senseact_dev_uevent,
};

struct class senseact_class = {
	.name		= "senseact",
};
EXPORT_SYMBOL_GPL(senseact_class);

/**
 * senseact_allocate_device - allocate memory for new senseact device
 *
 * Returns prepared struct senseact_device or NULL.
 *
 * NOTE: Use senseact_free_device() to free devices that have not been
 * registered; senseact_unregister_device() should be used for already
 * registered devices.
 */
struct senseact_device *senseact_allocate_device(void)
{
	struct senseact_device *senseact;
	unsigned int minor;

	senseact = kzalloc(sizeof(struct senseact_device), GFP_KERNEL);
	if (!senseact)
		return NULL;

	mutex_lock(&senseact_table_mutex);
	for (minor = 0; minor < SENSEACT_DEVICES; minor++)
		if (!senseact_table[minor]) {
			senseact_table[minor] = senseact;
			break;
		}
	mutex_unlock(&senseact_table_mutex);

	if (minor == SENSEACT_DEVICES) {
		printk(KERN_ERR "senseact: no more free MINOR numbers\n");
		goto err_kfree;
	}

	senseact->minor = minor;

	dev_set_name(&senseact->dev, "senseact%d", minor);
	senseact->dev.devt = MKDEV(SENSEACT_MAJOR, minor);
	senseact->dev.type = &senseact_type;
	senseact->dev.class = &senseact_class;
	device_initialize(&senseact->dev);
	mutex_init(&senseact->mutex);
	spin_lock_init(&senseact->queue_lock);
	INIT_LIST_HEAD(&senseact->queue_list);
	init_waitqueue_head(&senseact->wait);

	__module_get(THIS_MODULE);

	senseact_set_capabilities(senseact, SENSEACT_TYPE_SYNC, 1);

	return senseact;

 err_kfree:
	kfree(senseact);
	return NULL;
}
EXPORT_SYMBOL(senseact_allocate_device);

/**
 * senseact_free_device - free memory occupied by senseact_device structure
 * @senseact: senseact device to free
 *
 * This function should only be used if senseact_register_device()
 * was not called yet or if it failed. Once device was registered
 * use isenseact_unregister_device() and memory will be freed once last
 * reference to the device is dropped.
 *
 * Device should be allocated by senseact_allocate_device().
 *
 * NOTE: If there are references to the senseact device then memory
 * will not be freed until last reference is dropped.
 */
void senseact_free_device(struct senseact_device *senseact)
{
	if (senseact) 
		put_device(&senseact->dev);	
}
EXPORT_SYMBOL(senseact_free_device);

/**
 * senseact_set_capability_array - mark device as capable of certain values
 * @senseact: device that is capable of emitting or accepting event
 * @type: type of the value
 * @count: number of values
 */
void senseact_set_capabilities(struct senseact_device *senseact, unsigned int type, unsigned int count)
{
	if (type >= SENSEACT_TYPE_MAX) {
		dev_err(&senseact->dev,
			"senseact_set_capability: unknown type %u\n", type);
	} else {
		__set_bit(type, senseact->types);
	}
}
EXPORT_SYMBOL(senseact_set_capabilities);

/**
 * senseact_register_device - register device with senseact core
 * @senseact: device to be registered
 *
 * This function registers device with senseact core. The device must be
 * allocated with senseact_allocate_device() and all it's capabilities
 * set up before registering.
 * If function fails the device must be freed with senseact_free_device().
 * Once device has been successfully registered it can be unregistered
 * with senseact_unregister_device(); senseact_free_device() should not be
 * called in this case.
 */
int senseact_register_device(struct senseact_device *senseact)
{
	const char *path;
	int retval;

	if (!senseact->name)
		senseact->name = dev_name(&senseact->dev);

	retval = device_add(&senseact->dev);
	if (retval)
		return retval;

	path = kobject_get_path(&senseact->dev.kobj, GFP_KERNEL);
	printk(KERN_INFO "senseact: %s as %s\n",
		senseact->name, path ? path : "N/A");
	kfree(path);

	return 0;
}
EXPORT_SYMBOL(senseact_register_device);

/**
 * senseact_unregister_device - unregister previously registered device
 * @senseact: device to be unregistered
 *
 * This function unregisters an senseact device. Once device is unregistered
 * the caller should not try to access it as it may get freed at any moment.
 */
void senseact_unregister_device(struct senseact_device *senseact)
{
	struct senseact_queue *queue;

	mutex_lock(&senseact->mutex);
	senseact->going_away = 1;
	mutex_unlock(&senseact->mutex);

	spin_lock(&senseact->queue_lock);
	list_for_each_entry(queue, &senseact->queue_list, node)
		kill_fasync(&queue->fasync, SIGIO, POLL_HUP);
	spin_unlock(&senseact->queue_lock);

	wake_up_interruptible(&senseact->wait);

	mutex_lock(&senseact_table_mutex);
	senseact_table[senseact->minor] = NULL;
	mutex_unlock(&senseact_table_mutex);

	if (senseact->users) {
		if (senseact->flush)
			senseact->flush(senseact, NULL);
	}

	device_unregister(&senseact->dev);
}
EXPORT_SYMBOL(senseact_unregister_device);


/**
 * senseact_pass_actions() - report new senseact actions values
 * @senseact: deivce that generated the values
 * @type: type of values
 * @index: first index of values
 * @count: number of values
 * @values: value of the event
 *
 * This function should be used by drivers implementing various senseact
 * devices.
 */
void senseact_pass_actions(struct senseact_device *senseact,
			unsigned int type, unsigned int prefix, unsigned int index, unsigned int count, int *values)
{
	unsigned long flags;

	if (is_type_supported(senseact, type)) {

		spin_lock_irqsave(&senseact->action_lock, flags);
		senseact_handle_actions(senseact, type, prefix, index, count, values);
		spin_unlock_irqrestore(&senseact->action_lock, flags);
	}
}
EXPORT_SYMBOL(senseact_pass_actions);

/**
 * file functions
 */
static int senseact_fasync_file(int fd, struct file *file, int on)
{
	struct senseact_queue *queue = file->private_data;

	return fasync_helper(fd, file, on, &queue->fasync);
}

static int senseact_flush_file(struct file *file, fl_owner_t id)
{
	struct senseact_queue *queue = file->private_data;
	struct senseact_device *senseact = queue->senseact;
	int retval;

	retval = mutex_lock_interruptible(&senseact->mutex);
	if (retval)
		return retval;

	if (senseact->going_away)
		retval = -ENODEV;
	else
		if (senseact->flush)
			retval = senseact->flush(senseact, file);
		else
			retval = 0;

	mutex_unlock(&senseact->mutex);
	return retval;
}

static int senseact_release_file(struct inode *inode, struct file *file)
{
	struct senseact_queue *queue = file->private_data;
	struct senseact_device *senseact = queue->senseact;

	senseact_detach_queue(senseact, queue);
	kfree(queue);

	senseact_close_device(senseact);
	put_device(&senseact->dev);

	return 0;
}

static int senseact_open_file(struct inode *inode, struct file *file)
{
	struct senseact_device *senseact;
	struct senseact_queue *queue;
	int i = iminor(inode);
	int retval;

	if (i >= SENSEACT_DEVICES)
		return -ENODEV;

	retval = mutex_lock_interruptible(&senseact_table_mutex);
	if (retval)
		return retval;

	senseact = senseact_table[i];
	if (senseact)
		get_device(&senseact->dev);
	mutex_unlock(&senseact_table_mutex);

	if (!senseact)
		return -ENODEV;

	queue = kzalloc(sizeof(struct senseact_queue), GFP_KERNEL);
	if (!queue) {
		retval = -ENOMEM;
		goto err_put_dev;
	}

	spin_lock_init(&queue->buffer_lock);
	queue->senseact = senseact;
	senseact_attach_queue(senseact, queue);

	retval = senseact_open_device(senseact);
	if (retval)
		goto err_free_queue;

	file->private_data = queue;
	return 0;

 err_free_queue:
	senseact_detach_queue(senseact, queue);
	kfree(queue);
 err_put_dev:
	put_device(&senseact->dev);
	return retval;
}

static ssize_t senseact_write_file(struct file *file, const char __user *buffer,
			   size_t count, loff_t *ppos)
{
	struct senseact_queue *queue = file->private_data;
	struct senseact_device *senseact = queue->senseact;
	struct senseact_action action;
	int retval, offset = 0, value = 0;

	if (!senseact->pass)
		return -EFAULT;

	retval = mutex_lock_interruptible(&senseact->mutex);
	if (retval)
		return retval;

	if (senseact->going_away) {
		retval = -ENODEV;
		goto err_unlock;
	}

	while (offset < count) {
		if (copy_from_user(&action, buffer + offset, sizeof(struct senseact_action))) {
			retval = -EFAULT;
			goto err_unlock;
		}

		retval = senseact->pass(senseact, action.type, action.index, 1, &action.value);
		if (retval)
			goto err_unlock;

		offset +=  sizeof(struct senseact_action);
	}

	senseact->pass(senseact, SENSEACT_TYPE_SYNC, SENSEACT_SYNC_ACTOR, 1, &value);

	retval = offset;

 err_unlock:
	mutex_unlock(&senseact->mutex);
	return retval;
}

static int senseact_queue_fetch_next_action(struct senseact_queue *queue,
				  struct senseact_action *action)
{
	int empty;

	spin_lock_irq(&queue->buffer_lock);

	empty = queue->head == queue->tail;
	if (!empty) {
		*action = queue->buffer[queue->tail++];
		queue->tail &= SENSEACT_BUFFER_SIZE - 1;
	}

	spin_unlock_irq(&queue->buffer_lock);

	return !empty;
}

static ssize_t senseact_read_file(struct file *file, char __user *buffer,
			  size_t count, loff_t *ppos)
{
	struct senseact_queue *queue = file->private_data;
	struct senseact_device *senseact = queue->senseact;
	struct senseact_action action;
	int retval;

	if (count < sizeof(struct senseact_action))
		return -EINVAL;

	if (senseact->going_away)
		return -ENODEV;

	if (queue->head == queue->tail && (file->f_flags & O_NONBLOCK))
		return -EAGAIN;

	retval = wait_event_interruptible(senseact->wait,
		queue->head != queue->tail || senseact->going_away);
	if (retval)
		return retval;

	if (senseact->going_away)
		return -ENODEV;

	while (retval + sizeof(struct senseact_action) <= count &&
	       senseact_queue_fetch_next_action(queue, &action)) {

		if (copy_to_user(buffer + retval, &action, sizeof(struct senseact_action)))
			return -EFAULT;

		retval += sizeof(struct senseact_action);
	}

	return retval;
}

static unsigned int senseact_poll_file(struct file *file, poll_table *wait)
{
	struct senseact_queue *queue = file->private_data;
	struct senseact_device *senseact = queue->senseact;

	poll_wait(file, &senseact->wait, wait);
	return ((queue->head == queue->tail) ? 0 : (POLLIN | POLLRDNORM)) |
		(senseact->going_away ? (POLLHUP | POLLERR) : 0);
}

static long senseact_do_ioctl(struct file *file, unsigned int cmd,
			   void __user *p)
{
	int __user *ip = (int __user *)p;

	switch (cmd) {
	case EVIOCGVERSION:
		return put_user(EV_VERSION, ip);
	}

	return -EINVAL;
}

static long senseact_ioctl_file(struct file *file, unsigned int cmd, unsigned long arg)
{
	struct senseact_queue *queue = file->private_data;
	struct senseact_device *senseact = queue->senseact;
	void __user *argp = (void __user *)arg;
	int retval;

	retval = mutex_lock_interruptible(&senseact->mutex);
	if (retval)
		return retval;

	if (senseact->going_away) {
		retval = -ENODEV;
		goto out;
	}

	retval = senseact_do_ioctl(file, cmd, argp);

 out:
	mutex_unlock(&senseact->mutex);
	return retval;
}

static const struct file_operations senseact_fops = {
	.owner		= THIS_MODULE,
	.read		= senseact_read_file,
	.write		= senseact_write_file,
	.poll		= senseact_poll_file,
	.open		= senseact_open_file,
	.release	= senseact_release_file,
	.unlocked_ioctl	= senseact_ioctl_file,
	.fasync		= senseact_fasync_file,
	.flush		= senseact_flush_file
};

static int __init senseact_init(void)
{
	int retval;

	retval = class_register(&senseact_class);
	if (retval) {
		printk(KERN_ERR "senseact: unable to register senseact class\n");
		return retval;
	}

	retval = register_chrdev(SENSEACT_MAJOR, "senseact", &senseact_fops);
	if (retval) {
		printk(KERN_ERR "senseact: unable to register char major %d", SENSEACT_MAJOR);
		goto fail1;
	}

	return 0;

 fail1:
	class_unregister(&input_class);
	return retval;
}
subsys_initcall(senseact_init);

static void __exit senseact_exit(void)
{
	unregister_chrdev(SENSEACT_MAJOR, "senseact");
	class_unregister(&senseact_class);
}
module_exit(senseact_exit);

MODULE_AUTHOR("Stefan Herbrechtsmeier <hbmeier@hni.upb.de>");
MODULE_DESCRIPTION("SenseAct core");
MODULE_LICENSE("GPL");
