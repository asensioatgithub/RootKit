#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/syscalls.h>

void ** sys_call_table;

void **get_sct_addr(void)
{
	unsigned int sct = 0;
	unsigned long **entry = (unsigned long **)PAGE_OFFSET;

	for (;(unsigned long)entry < ULONG_MAX; entry += 1) {
		if (entry[__NR_close] == (unsigned long *)sys_close) {
			return entry;
		}
	}

	return NULL;

	return ((void **)sct);
}


static int filter_init(void)
{
	sys_call_table = get_sct_addr();
	if (!sys_call_table)
	{
		printk("get_act_addr(): NULL...\n");
		return 0;
	}
	else{
		printk("sct: 0x%x\n", (unsigned int)sys_call_table);
		return 0;
	}	
}


static void filter_exit(void)
{
	printk(KERN_INFO "printsct3: module removed\n");
}


MODULE_LICENSE("GPL");
module_init(filter_init);
module_exit(filter_exit);
