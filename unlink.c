#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/types.h>
#include <asm/unistd.h>
#include <linux/syscalls.h>
static unsigned long **sct;
int (* old_unlink)(void);

int orig_cr0;
/*
    关闭写保护的源代码：将CR0 寄存器从 0开始数的第 16 个比特置为 0。
*/
void disable_write_protection(void)
{
        unsigned long cr0 = read_cr0();
        clear_bit(16, &cr0);
        write_cr0(cr0);
}
/*
    开启写保护的源代码：将CR0 寄存器从 0开始数的第 16 个比特置为 1。
*/
    void enable_write_protection(void)
{
        unsigned long cr0 = read_cr0();
        set_bit(16, &cr0);
        write_cr0(cr0);
}

int my_unlink(void)
{
        printk(KERN_INFO "unlink unavailable\n");
        return 0;
}

unsigned long **get_sys_call_table(void)
{
  unsigned long **entry = (unsigned long **)PAGE_OFFSET;//PAGE_OFFSET是内核内存空间的起始地址。

  for (;(unsigned long)entry < ULONG_MAX; entry += 1) {
    if (entry[__NR_close] == (unsigned long *)sys_close) {
        return entry;
      }
  }
  return NULL;
}

static int enter (void)
{
        sct=(void *)get_sys_call_table();
        disable_write_protection();
        old_unlink=(void *)sct[__NR_unlinkat];
        sct[__NR_unlinkat]=(unsigned long *)&my_unlink;
        enable_write_protection();
        return 0;
}
static void go_out(void)
{
        printk(KERN_INFO "Bye, restoring syscalls\n");
        disable_write_protection();
        sct[__NR_unlinkat]=(unsigned long *)old_unlink;
        enable_write_protection();
}
module_init(enter);
module_exit(go_out);