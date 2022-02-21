#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/fs.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/cred.h>
#include <asm/io.h>

#include <linux/slab.h>
#include <linux/of_platform.h>
#include <linux/delay.h>

#include "include/gr.h"
#include "include/gr_mempool.h"
#include "include/gr_irq.h"
#include "include/gr_hw.h"

#define GR_DRV_NAME	"gr"

struct gr_device *grdev;

int in_use = 0;
struct gr_mempool *pool;
void __iomem *reg;

MODULE_LICENSE("GPL");
MODULE_AUTHOR("HPark");
MODULE_VERSION("1.0");
MODULE_DESCRIPTION("gr");

static int gr_open(struct inode *inode, struct file *file)
{
	if (in_use) {
		return -EBUSY;
	}
	in_use++;

	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_CLEAR), GPU_IRQ_REG_ALL);
	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_MASK), GPU_IRQ_REG_ALL);

	gr_reg_write(JOB_CONTROL_REG(JOB_IRQ_CLEAR), 0xFFFFFFFF);
	gr_reg_write(JOB_CONTROL_REG(JOB_IRQ_MASK), 0xFFFFFFFF);

	gr_reg_write(MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF);
	gr_reg_write(MMU_REG(MMU_IRQ_MASK), 0xFFFFFFFF);

	if (gr_install_interrupts(grdev)) {
		printk(KERN_INFO "install interrupts fail");
	}

	init_waitqueue_head(&grdev->job_irq_queue);

#ifdef DEBUG
	printk(KERN_INFO "gr Open\n");
#endif
	return 0;
}

static int gr_release(struct inode *inode, struct file *file)
{
	in_use--;
#ifdef DEBUG
	printk(KERN_INFO "gr Closed\n");
#endif

	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_MASK), 0);
	gr_reg_write(GPU_CONTROL_REG(GPU_IRQ_CLEAR), GPU_IRQ_REG_ALL);
	gr_reg_write(JOB_CONTROL_REG(JOB_IRQ_MASK), 0);
	gr_reg_write(JOB_CONTROL_REG(JOB_IRQ_CLEAR), 0xFFFFFFFF);

	gr_reg_write(MMU_REG(MMU_IRQ_CLEAR), 0xFFFFFFFF);

	gr_release_interrupts(grdev);
	return 0;
}

static int gr_ioctl(struct file *file, unsigned int cmd, unsigned long arg)
{
	int retval = 0;
	int err;
	void __user *uarg = (void __user *)arg;

	struct page *p;
	int size;

	size = _IOC_SIZE(cmd);
	/** printk(KERN_INFO "gr ioctl | cmd : %u, rw size : %d\n", cmd, size); */

	if(size) {
		if(_IOC_DIR(cmd) & _IOC_READ)
			if(access_ok(VERIFY_WRITE, (void*)arg, size) < 0)  
				return -EINVAL;
		if(_IOC_DIR(cmd) & _IOC_WRITE)
			if(access_ok(VERIFY_READ, (void*)arg, size) < 0)   
				return -EINVAL;
	}

	switch (cmd) {
	case GR_MEMPOOL_INIT:
		/** printk(KERN_INFO "gr ioctl | MEMPOOL_INIT\n"); */
		pool = gr_mempool_init(0/*order*/);
		pool->grdev = grdev;
		break;
	case GR_MEMPOOL_GET_PAGE:
		{
			struct gr_ioctl_get_page param;
			/** printk(KERN_INFO "gr ioctl | MEMPOOL_PAGE_GET\n"); */
			gr_mempool_grow(pool, 1);
			p = gr_mempool_alloc(pool);
			param.pa = (__u64) page_to_phys(p);
			err = copy_to_user(uarg, &param, sizeof(param));
		}
		break;
	case GR_MEMPOOL_GROW:
		{
			struct gr_ioctl_mem_alloc param;
			err = copy_from_user(&param, uarg, sizeof(param));

			/** printk(KERN_INFO "gr ioctl | MEMPOOL_MEMPOL_GROW (%d pages)\n", param.nr_pages); */
			gr_mempool_grow(pool, param.nr_pages);
		}
		break;
	case GR_MEMPOOL_GET_PAGES:
		{
			/** printk(KERN_INFO "gr ioctl | MEMPOOL_GET_PAGES"); */
			struct gr_ioctl_get_pages param;
			int i;
			err = copy_from_user(&param, uarg, sizeof(param));

			/** printk(KERN_INFO "gr ioctl | MEMPOOL_GET_PAGES (%lu pages)\n", param.nr_pages); */
			for (i = 0; i < param.nr_pages; i++) {
				p = gr_mempool_alloc(pool);
				param.pa[i] = (__u64) page_to_phys(p);
				/** printk(KERN_INFO "%lx", param.pa[i]); */
			}
			err = copy_to_user(uarg, &param, sizeof(param));
		}
		break;
	case GR_MEMPOOL_TERM:
		/** printk(KERN_INFO "gr ioctl | MEMPOOL_TERM\n"); */
		gr_mempool_term(pool);
		break;
	case GR_IOCTL_MEM_SYNC:
		{
			struct gr_ioctl_mem_sync param;
			err = copy_from_user(&param, uarg, sizeof(param));
			gr_mem_sync(grdev, &param);
		}
		break;
	case GR_IOCTL_IRQ_SETUP:
		{
			struct gr_ioctl_irqfds param;
			/** printk(KERN_INFO "gr ioctl | IRQ_SETUP\n"); */
			param.job_irq = job_irq_init(grdev);
			param.gpu_irq = gpu_irq_init(grdev);
			/** printk(KERN_INFO "job_irq : %d\n", param.job_irq); */
				
			err = copy_to_user(uarg, &param, sizeof(param));
		}
		break;
	}
	return retval;
}

static ssize_t gr_read(struct file *filp, char __user *buf, size_t length, loff_t *offset)
{
	printk(KERN_INFO "gr read\n");

	return 0;
}

static ssize_t gr_write(struct file *flip, const char *buf, size_t len, loff_t *off)
{
	printk(KERN_INFO "gr write\n");

	return 0;
}

static unsigned int gr_poll(struct file *filp, poll_table *wait)
{
	printk(KERN_INFO "gr poll\n");

	return 0;
}

static void gr_common_reg_unmap(void) {
	if (reg) {
		iounmap(reg);
		release_mem_region(grdev->reg_start, grdev->reg_size);
		reg = NULL;
		grdev->reg_start = 0;
		grdev->reg_size = 0;
	}
}

static int gr_common_reg_map(struct platform_device *pdev)
{
	int err = 0;

	if (!request_mem_region(grdev->reg_start, grdev->reg_size, dev_name(grdev->dev))) {
		dev_err(grdev->dev, "Register window unavailable\n");
		err = -EIO;
		goto out_region;
	}

	reg = ioremap(grdev->reg_start, grdev->reg_size);
	if (!reg) {
		printk(KERN_INFO "Can't remap register window\n");
		dev_err(grdev->dev, "Can't remap register window\n");
		err = -EINVAL;
		goto out_ioremap;
	}
	return err;
	
out_ioremap:
	release_mem_region(grdev->reg_start, grdev->reg_size);
out_region:
	return err;
}

static const struct file_operations gr_fops = {
	.owner = THIS_MODULE,
	.open = &gr_open,
	.read = &gr_read,
	.write = &gr_write,
	.poll = &gr_poll,
	.release = &gr_release,
	.unlocked_ioctl = (void *) &gr_ioctl,
	.compat_ioctl = (void *) &gr_ioctl
};

static int gr_platform_device_probe(struct platform_device *pdev)
{
	struct resource *reg_res;
	int err = 0;

	grdev = kzalloc(sizeof(struct gr_device), GFP_KERNEL);

	if (!grdev) {
		dev_err(&pdev->dev, "Allocate device failed\n");
		return -ENOMEM;
	}

	grdev->dev = &pdev->dev;

	grdev->mdev.minor = MISC_DYNAMIC_MINOR;
	grdev->mdev.name =  GR_DRV_NAME;
	grdev->mdev.fops = &gr_fops;
	grdev->mdev.parent = get_device(grdev->dev);
	grdev->mdev.mode = 0666;

	err = misc_register(&grdev->mdev);

	if (err) {
		dev_err(grdev->dev, "Device initialization failed\n");
		kfree(grdev);
	}

	// map GPU register map here
	reg_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!reg_res) {
		dev_err(grdev->dev, "Invalid register resource\n");
		return -ENOENT;
	}

	grdev->reg_start = reg_res->start;
	grdev->reg_size = resource_size(reg_res);
	printk(KERN_INFO "%llx size: %lu", grdev->reg_start, grdev->reg_size);
	gr_common_reg_map(pdev);

	printk(KERN_INFO "gr init\n");
	return err;
}

static int gr_platform_device_remove(struct platform_device *pdev)
{
	printk(KERN_INFO "gr exit\n");
	if (!grdev)
		return -ENODEV;

	gr_common_reg_unmap();

	put_device(grdev->dev);
	misc_deregister(&grdev->mdev);
	kfree(grdev);
	return 0;
}

/** #ifdef CONFIG_OF */
#if 1
static const struct of_device_id gr_dt_ids[] = {
	{ .compatible = "arm,malit6xx" },
	{ .compatible = "arm,mali-midgard" },
	{ .compatible = "arm,mali-bifrost" },
	{ /* sentinel */ }
};
MODULE_DEVICE_TABLE(of, gr_dt_ids);


static struct platform_driver gr_platform_driver = {
	.probe 	= gr_platform_device_probe,
	.remove = gr_platform_device_remove,
	.driver = {
			.name = GR_DRV_NAME,
			.owner = THIS_MODULE,
			.of_match_table = of_match_ptr(gr_dt_ids),
	},
};
#endif

/** #ifdef CONFIG_OF */
#if 1
module_platform_driver(gr_platform_driver);
#else
static int __init gr_init(void)
{
	int err;

	grdev = kzalloc(sizeof(struct gr_device), GFP_KERNEL);

	if (!grdev) {
		/** dev_err(&pdev->dev, "Allocate device failed\n"); */
		return -ENOMEM;
	}

	/** grdev->dev = &pdev->dev; */

	grdev->mdev.minor = MISC_DYNAMIC_MINOR;
	grdev->mdev.name =  GR_DRV_NAME;
	grdev->mdev.fops = &gr_fops;
	grdev->mdev.parent = get_device(grdev->dev);
	grdev->mdev.mode = 0666;

	err = misc_register(&grdev->mdev);

	if (err) {
		dev_err(grdev->dev, "Device initialization failed\n");
		kfree(grdev);
	}

	// map GPU register map here
	gr_common_reg_map();
	reg = ioremap(GPU_REG_PA_START, GPU_REG_PA_SIZE);

	printk(KERN_INFO "gr init\n");
	return err;
}

static void __exit gr_exit(void)
{
	printk(KERN_INFO "gr exit\n");
	if (!grdev)
		return -ENODEV;

	/** put_device(grdev->dev); */
	misc_deregister(&grdev->mdev);
	kfree(grdev);
}

module_init(gr_init);
module_exit(gr_exit);

#endif
