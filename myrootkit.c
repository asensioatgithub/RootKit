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
#include <linux/syscalls.h>


#define CALLOFF 100

/*
    http://www.freebuf.com/articles/system/108392.html
    http://www.xzbu.com/8/view-7178072.htm
*/
static unsigned long **sct; //指针数组
int (* old_unlink)(void);
int orig_cr0;

/*
    IDT表可以驻留在线性地址空间的任何地方，处理器使用IDTR寄存器来定位IDT表的位置。
    这个寄存器中含有IDT表32位的基地址和16位的长度（限长）值。
    IDT表基地址应该对齐在8字节边界上以提高处理器的访问效率。
    限长值是以字节为单位的IDT表的长度。
*/
struct {
    unsigned short limit;
    unsigned int base;
} __attribute__ ((packed)) idtr;

struct {
    unsigned short highoff;
    unsigned short sel;
    unsigned char none,flags;
    unsigned short lowoff;
} __attribute__ ((packed)) * idt;


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
/*
    通过对反汇编分析可知，所有SCT中的系统调用都是通过call指令进行调用的。
    因此，可以通过搜索call指令来寻找SCT的入口。
    在x86架构下，call指令的16进制码为/xff/x14/x85，所以首先通过字符匹配搜说call指令。
    从system_call的起始地址开始搜索，一旦搜索到连续的三个字节分别为ff,14,85,可得到call指令的地址。
*/
char * findoffset(char *start)
{
    char *p;
    for (p = start; p < start + CALLOFF; p++)
    if (*(p + 0) == '\xff' && *(p + 1) == '\x14' && *(p + 2) == '\x85')//获取call指令地址。
        return p;
    return NULL;
}


void **get_sct_addr(void)
{
    unsigned sys_call_off;
    unsigned sct = 0;
    char *p;
    /*
        使用内联汇编的办法调用sidt这一汇编指令加载IDTR寄存器的内容，然后储存到我们自己的这个结构体中
    */
    asm("sidt %0":"=m"(idtr));      
    printk("Arciryas:idt table-0x%x\n", idtr.base);
    /*
        这条语句的目的是获取0×80中断所对应的IDT中的表项。
        中断描述符表共256项，每项8字节，每项代表一种中断类型，所以最多需要256*8=2048个字节来存放IDT。
        我们要从IDT起始地址后的8*0×80位置拷贝一个IDT表项大小的数据，也就是0×80中断所对应的IDT中的表项
    */
    idt = (void *) (idtr.base + 8 * 0x80);
    /*
        获取128号中断的中断服务程序system_call的地址。
        此时highoffset即为ox80程序地址的高16位，lowoffset为低16位。
        将highoffset左移16位加上lowoffset即可得到0x80中断处理程序的system_call的起始地址
    */
    sys_call_off = (idt->highoff << 16) | idt->lowoff;
    /*
        获取SCT地址
    */
    if ((p = findoffset((char *) sys_call_off)))
        sct = *(unsigned *) (p + 3);    //由于SCT地址紧跟在call指令后面，所以p+3即为SCT的地址
    return ((void **)sct);
}

int my_unlink(void)
{
        printk(KERN_INFO "unlink unavailable\n");
        return 0;
}


static int enter (void)
{
    sct = get_sct_addr();
    if (!sct)
    {
        printk("get_act_addr(): NULL...\n");
        return 0;
    }
    else{
        printk("sct: 0x%x\n", (unsigned int)sct);
        return 0;
    }
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



MODULE_LICENSE("GPL");
module_init(enter);
module_exit(go_out);
