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
#define CALLOFF 100



void** sys_call_table;

void *
get_lstar_sct_addr(void)
{
    u64 lstar;
    u64 index;

    rdmsrl(MSR_LSTAR, lstar);
    for (index = 0; index <= PAGE_SIZE; index += 1) {
        u8 *arr = (u8 *)lstar + index;

        if (arr[0] == 0xff && arr[1] == 0x14 && arr[2] == 0xc5) {
            return arr + 3;
        }
    }

    return NULL;
}


unsigned long **
get_lstar_sct(void)
{
    unsigned long *lstar_sct_addr = get_lstar_sct_addr();
    if (lstar_sct_addr != NULL) {
        u64 base = 0xffffffff00000000;
        u32 code = *(u32 *)lstar_sct_addr;
        return (void *)(base | code);
    } else {
        return NULL;
}                                         
}



static int filter_init(void)
{	
    int i = 0;
    sys_call_table = get_lstar_sct();
    if (!sys_call_table)
    {
        printk("get_act_addr(): NULL...\n");
        return 0;
    }
    else{
        printk("sct: 0x%p\n", (unsigned long)sys_call_table);
	for(i = 0; i < 5; i++)
		printk("SYSCALLNO %d,ADDRESS 0x%x\n",i,(unsigned int)sys_call_table[i]);
		printk("SYSCALLNO getdents,ADDRESS 0x%x\n",(unsigned int)sys_call_table[__NR_getdents]);
		printk("SYSCALLNO 217,ADDRESS 0x%x\n",(unsigned int)sys_call_table[217]);
	return 0;
	}	
}


static void filter_exit(void)
{
    printk(KERN_INFO "hideps: module removed\n");
}


MODULE_LICENSE("GPL");
module_init(filter_init);
module_exit(filter_exit);
