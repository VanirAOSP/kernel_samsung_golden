/*
 * drivers/cpufreq/cpufreq_limits_on_screenoff.c
 * 
 * Copyright (c) 2014, Shilin Victor <chrono.monochrome@gmail.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/cpu.h>
#include <linux/notifier.h>
#include <linux/init.h>
#include <linux/cpufreq.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>

static bool screenoff_cpu_freq_limits = true;
static unsigned int screenoff_min_cpufreq = 100000;
static unsigned int screenoff_max_cpufreq = 500000;
static unsigned int screenoff_prev_min_cpufreq = 0;
static unsigned int screenoff_prev_max_cpufreq = 0;
bool (*touchscreen_is_suspend)(void);

#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT404
extern bool bt404_is_suspend(void);
#endif

#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT224S
extern bool mxt244s_is_suspend(void);
#endif

static int cpufreq_callback(struct notifier_block *nfb,
		unsigned long event, void *data)
{
	if (screenoff_cpu_freq_limits) {
		struct cpufreq_policy *policy = data;
		int new_min = 0, new_max = 0;
		
		bool is_suspend = touchscreen_is_suspend();

		if (event != CPUFREQ_ADJUST)
			return 0;
		
		screenoff_prev_min_cpufreq = policy->min;
		screenoff_prev_max_cpufreq = policy->max;
		new_min = is_suspend ? screenoff_min_cpufreq : screenoff_prev_min_cpufreq;
		new_max = is_suspend ? screenoff_max_cpufreq : screenoff_prev_max_cpufreq;
		
		if (new_min > new_max) 
			new_max = new_min;
		
		policy->min = new_min;
		policy->max = new_max;
	}
	return 0;
}

static struct notifier_block cpufreq_notifier_block = 
{
	.notifier_call = cpufreq_callback,
};

static ssize_t screenoff_cpufreq_limits_show(struct kobject *kobj, struct kobj_attribute *attr, char *buf) 
{
	sprintf(buf, "status: %s\nmin = %d KHz\nmax = %d KHz\n", screenoff_cpu_freq_limits ? "on" : "off",
		screenoff_min_cpufreq,
		screenoff_max_cpufreq);

	return strlen(buf);
}

static ssize_t screenoff_cpufreq_limits_store(struct kobject *kobj, struct kobj_attribute *attr, const char *buf, size_t count)
{
	if (!strncmp(buf, "on", 2)) {
		screenoff_cpu_freq_limits = true;
		return count;
	}

	if (!strncmp(buf, "off", 3)) {
		screenoff_cpu_freq_limits = false;
		return count;
	}
	
	if (!strncmp(&buf[0], "min=", 4)) {
		if (!sscanf(&buf[4], "%d", &screenoff_min_cpufreq))
			goto invalid_input;
	}

	if (!strncmp(&buf[0], "max=", 4)) {
		if (!sscanf(&buf[4], "%d", &screenoff_max_cpufreq))
			goto invalid_input;
	}

	return count;

invalid_input:
	pr_err("[screenoff_cpufreq_limits] invalid input");
	return -EINVAL;
}

static struct kobj_attribute screenoff_cpufreq_limits_interface = __ATTR(screenoff_cpufreq_limits, 0644,
									   screenoff_cpufreq_limits_show,
									   screenoff_cpufreq_limits_store);

static struct attribute *cpufreq_attrs[] = {
	&screenoff_cpufreq_limits_interface.attr,
	NULL,
};

static struct attribute_group cpufreq_interface_group = {
	.attrs = cpufreq_attrs,
};

static struct kobject *cpufreq_kobject;

static int cpufreqlimits_driver_init(void)
{
	struct cpufreq_policy *data = cpufreq_cpu_get(0);
        int ret;
	
#ifdef CONFIG_TOUCHSCREEN_ZINITIX_BT404
	touchscreen_is_suspend = bt404_is_suspend;
#endif
#ifdef CONFIG_TOUCHSCREEN_ATMEL_MXT224S
	touchscreen_is_suspend = mxt244s_is_suspend;
#endif
	
        if (!touchscreen_is_suspend)
		/*
		 * FixMe! Replace it with -EOPNOTSUPP
		 */
		return -1;
	
	if (!screenoff_min_cpufreq)
		screenoff_min_cpufreq = data->min;
	if (!screenoff_max_cpufreq)
		screenoff_max_cpufreq = data->max;
	screenoff_prev_min_cpufreq = screenoff_min_cpufreq;
	screenoff_prev_max_cpufreq = screenoff_max_cpufreq;
	pr_err("[cpufreq] initialized screenoff cpufreq limits module with min %d and max %d MHz limits",
					 screenoff_min_cpufreq / 1000,  screenoff_max_cpufreq / 1000
	);
	
	cpufreq_kobject = kobject_create_and_add("cpufreq", kernel_kobj);
	if (!cpufreq_kobject) {
		pr_err("[cpufreq] Failed to create kobject interface\n");
	}

	ret = sysfs_create_group(cpufreq_kobject, &cpufreq_interface_group);
	if (ret) {
		kobject_put(cpufreq_kobject);
	}
	
	cpufreq_register_notifier(&cpufreq_notifier_block, CPUFREQ_POLICY_NOTIFIER);
	
	return ret;
}
late_initcall(cpufreqlimits_driver_init);

static void cpufreqlimits_driver_exit(void)
{
}

module_exit(cpufreqlimits_driver_exit);

MODULE_AUTHOR("Shilin Victor <chrono.monochrome@gmail.com>");
MODULE_DESCRIPTION("CPUfreq limits on screen off");
MODULE_LICENSE("GPL");
