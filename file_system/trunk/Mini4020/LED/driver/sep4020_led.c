#include <linux/module.h>
#include <linux/types.h>
#include <linux/fs.h>
#include <linux/errno.h>
#include <linux/mm.h>
#include <linux/sched.h>
#include <linux/init.h>
#include <linux/cdev.h>
//#include <linux/interrupt.h>
#include <linux/time.h>
#include <linux/delay.h>
#include <asm/system.h>
#include <asm/uaccess.h>
#include <asm/io.h>
#include <asm/hardware.h>

#define KEY_MAJOR 253 /* 主设备号*/
#define LED_ON 1
#define LED_OFF 0
static void sep4020_led_setup(void);
struct led_dev
{
	struct cdev cdev;
	unsigned char value;
};

struct led_dev *leddev;

int sep4020_led_open(struct inode *inode, struct file *filp)
{
	sep4020_led_setup();
	filp->private_data = &leddev;  //将设备结构体指针赋值给文件私有数据指针
	return 0;
}

int sep4020_led_release(struct inode *inode, struct file *filp)
{
	return 0;
}

static ssize_t sep4020_led_read(struct file *filp, char __user *buf, size_t size, loff_t *ppos)
{
	//struct led_dev *pdev = flip->private_data;
	
	if (copy_to_user(buf, &(leddev->value),1))
	{
		return -EFAULT;
	}
	return 0;
}

static ssize_t sep4020_led_write(struct file *filp, const char __user *buf, size_t size, loff_t *ppos)
{
	return 0;
}

int sep4020_led_ioctl(struct inode *inode, struct file *filp, unsigned int cmd, unsigned long arg)
{
	struct led_dev *dev = filp->private_data;
	
	//printk("the arg is :%d cmd  =%d\n",arg,cmd);
	switch (cmd)
	{
		case LED_ON:
		{
			switch (arg)
			{
				case 1: *(volatile unsigned long*)GPIO_PORTD_DATA_V &= ~(1 << 0);break;
				case 2: *(volatile unsigned long*)GPIO_PORTD_DATA_V &= ~(1 << 2);break;
				case 3: *(volatile unsigned long*)GPIO_PORTG_DATA_V &= ~(1 << 5);break;
				case 4: *(volatile unsigned long*)GPIO_PORTG_DATA_V &= ~(1 << 6);break;	
				default:break;
			}	
		}	
		break;		
		case LED_OFF:
		{
			switch (arg)
			{
				case 1: *(volatile unsigned long*)GPIO_PORTD_DATA_V |= (1 << 0);break;
				case 2: *(volatile unsigned long*)GPIO_PORTD_DATA_V |= (1 << 2);break;
				case 3: *(volatile unsigned long*)GPIO_PORTG_DATA_V |= (1 << 5);break;
				case 4: *(volatile unsigned long*)GPIO_PORTG_DATA_V |= (1 << 6);break;		
				default:break;
			}
		}
		break;
		default:
			return -ENOTTY;
	}
	
	return 0;
}



static struct file_operations sep4020_led_fops = 
{
	.owner = THIS_MODULE,
	.read  = sep4020_led_read,
	.write = sep4020_led_write,
	.ioctl = sep4020_led_ioctl,
	.open  = sep4020_led_open,
	.release = sep4020_led_release,
};

static void sep4020_led_setup(void)
{
		*(volatile unsigned long*)GPIO_PORTD_SEL_V |= 0x05; //作为通用用途
		*(volatile unsigned long*)GPIO_PORTD_DIR_V &= ~0x05;//输出
	  	*(volatile unsigned long*)GPIO_PORTD_DATA_V |= 0x05;//初始输出拉高

		*(volatile unsigned long*)GPIO_PORTG_SEL_V |= 0x060; //作为通用用途
		*(volatile unsigned long*)GPIO_PORTG_DIR_V &= ~0x060;//输出
	  	*(volatile unsigned long*)GPIO_PORTG_DATA_V |= 0x060;//初始输出拉高
}

static int __init sep4020_led_init(void)
{
	int err,result;
	dev_t devno = MKDEV(KEY_MAJOR, 0);
	if(KEY_MAJOR)
		result = register_chrdev_region(devno, 1, "sep4020_led");
	else
	{
		result = alloc_chrdev_region(&devno, 0, 1, "sep4020_led");		
	}
	
	if(result < 0)
		return result;
	
	/*动态申请设备结构体的内存*/
	leddev = kmalloc(sizeof(struct led_dev),GFP_KERNEL);
	if (!leddev)
	{
		result = -ENOMEM;
		goto fail_malloc;
	}
	memset(leddev,0,sizeof(struct led_dev));
	/*注册中断函数*/
	/*对键盘进行初始化*/

	sep4020_led_setup();
	cdev_init(&(leddev->cdev), &sep4020_led_fops);
	leddev->cdev.owner = THIS_MODULE;
	err = cdev_add(&leddev->cdev, devno, 1);
	if(err)
		printk("adding err\r\n");	
	return 0;
	
	fail_malloc: unregister_chrdev_region(devno,1);
	return result;
}

static void __init sep4020_led_exit(void)
{
	cdev_del(&leddev->cdev);
	kfree(leddev);
	unregister_chrdev_region(MKDEV(KEY_MAJOR, 0),1);
}

module_init(sep4020_led_init);
module_exit(sep4020_led_exit);

MODULE_AUTHOR("Leeming Zhang");
MODULE_LICENSE("GPL");
