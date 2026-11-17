// SPDX-License-Identifier: GPL-2.0
/*
 * drivers/samsung/sec_class.c
 *
 * COPYRIGHT(C) 2014-2024 Samsung Electronics Co., Ltd. All Rights Reserved.
 */

#define pr_fmt(fmt)     KBUILD_MODNAME ":%s() " fmt, __func__

#include <linux/device.h>
#include <linux/err.h>
#include <linux/sec_class.h>
#include <linux/module.h>
#include <linux/version.h>

/* CAUTION : Do not be declared as external sec_class */
static struct class *sec_class;
static atomic_t sec_dev = ATOMIC_INIT(0);

static int sec_class_match_device_by_name(struct device *dev, const void *data)
{
	const char *name = data;

	return sysfs_streq(name, dev_name(dev));
}

struct device *sec_dev_get_by_name(const char *name)
{
	if (!sec_class || IS_ERR(sec_class))
		return NULL;

	return class_find_device(sec_class, NULL, name,
			sec_class_match_device_by_name);
}
EXPORT_SYMBOL(sec_dev_get_by_name);

struct device *___sec_device_create(void *drvdata, const char *fmt)
{
	struct device *dev;

	if (unlikely(!sec_class)) {
		pr_err("Not yet created class(sec)!\n");
		return ERR_PTR(-ENODEV);
	}

	if (IS_ERR(sec_class)) {
		pr_err("Failed to create class(sec) %ld\n", PTR_ERR(sec_class));
		return ERR_CAST(sec_class);
	}

	dev = device_create(sec_class, NULL, MKDEV(0, atomic_inc_return(&sec_dev)),
			drvdata, "%s", fmt);
	if (IS_ERR(dev))
		pr_err("Failed to create device %s %ld\n", fmt, PTR_ERR(dev));
	else
		pr_debug("%s : %d\n", fmt, dev->devt);

	return dev;
}
EXPORT_SYMBOL(___sec_device_create);

void sec_device_destroy(dev_t devt)
{
	if (unlikely(!devt)) {
		pr_err("Not allowed to destroy dev\n");
	} else if (sec_class && !IS_ERR(sec_class)) {
		pr_info("%d\n", devt);
		device_destroy(sec_class, devt);
	}
}
EXPORT_SYMBOL_GPL(sec_device_destroy);

static int __init sec_class_create(void)
{
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6, 4, 0)
	sec_class = class_create("sec");
#else
	sec_class = class_create(THIS_MODULE, "sec");
#endif
	if (unlikely(IS_ERR(sec_class))) {
		pr_err("Failed to create class(sec) %ld\n", PTR_ERR(sec_class));
		return PTR_ERR(sec_class);
	}
	return 0;
}

arch_initcall(sec_class_create);

MODULE_DESCRIPTION("Samsung sec-class");
MODULE_LICENSE("GPL v2");
