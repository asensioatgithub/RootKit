#include <linux/module.h>
#include <linux/kallsyms.h>
#include <linux/string.h>

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("Access non-exported symbols");
MODULE_AUTHOR("Stephen Zhang");

static int __init lkm_init(void)
{
    //char *sym_name = "resume_file";
    char *sym_name = "sys_call_table";
    unsigned long sym_addr = kallsyms_lookup_name(sym_name);
    char filename[256];

    strncpy(filename, (char *)sym_addr, 255);

    printk(KERN_INFO "[%s] %s (0x%lx): %s\n", __this_module.name, sym_name, sym_addr, filename);

    return 0;
}

static void __exit lkm_exit(void)
{
}

module_init(lkm_init);
module_exit(lkm_exit);
