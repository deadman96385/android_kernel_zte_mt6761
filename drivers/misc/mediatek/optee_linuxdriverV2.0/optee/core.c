/*
 * Copyright (c) 2015, Linaro Limited
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#define DEBUG	1
#include <linux/errno.h>
#include <linux/io.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_platform.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/tee_drv.h>
#include <linux/types.h>
#include <linux/uaccess.h>
#include "optee_private.h"
#include "optee_smc.h"
#include <linux/interrupt.h>
#include <linux/timer.h>
#include <linux/jiffies.h>


#define DRIVER_NAME "optee"

#define OPTEE_SHM_NUM_PRIV_PAGES	10

/* liuliang */
/* struct timer_list mytimer;*/
/*
static void myfunc(unsigned long data)
{
	pr_info("OP-TEE Driver Test Timer Running!\n");

	mod_timer(&mytimer, jiffies + 1*HZ);
}
*/

/**
 * optee_from_msg_param() - convert from OPTEE_MSG parameters to
 *			    struct tee_param
 * @params:	subsystem internal parameter representation
 * @num_params:	number of elements in the parameter arrays
 * @msg_params:	OPTEE_MSG parameters
 * Returns 0 on success or <0 on failure
 */
int optee_from_msg_param(struct tee_param *params, size_t num_params,
			 const struct optee_msg_param *msg_params)
{
	int rc;
	size_t n;
	struct tee_shm *shm;
	phys_addr_t pa;
	/*pr_info("optee_from_msg_param ---start\n");*/

	for (n = 0; n < num_params; n++) {
		struct tee_param *p = params + n;
		const struct optee_msg_param *mp = msg_params + n;
		u32 attr = mp->attr & OPTEE_MSG_ATTR_TYPE_MASK;
		/*pr_info("attr:%x mp:%p",attr,(char *)mp);*/
		switch (attr) {
		case OPTEE_MSG_ATTR_TYPE_NONE:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&p->u, 0, sizeof(p->u));
			break;
		case OPTEE_MSG_ATTR_TYPE_VALUE_INPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_VALUE_INOUT:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT +
				  attr - OPTEE_MSG_ATTR_TYPE_VALUE_INPUT;
			p->u.value.a = mp->u.value.a;
			p->u.value.b = mp->u.value.b;
			p->u.value.c = mp->u.value.c;
			break;
		case OPTEE_MSG_ATTR_TYPE_TMEM_INPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT:
		case OPTEE_MSG_ATTR_TYPE_TMEM_INOUT:
			p->attr = TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT +
				  attr - OPTEE_MSG_ATTR_TYPE_TMEM_INPUT;
			p->u.memref.size = mp->u.tmem.size;
			shm = (struct tee_shm *)(unsigned long)
				mp->u.tmem.shm_ref;
			if (!shm) {
				p->u.memref.shm_offs = 0;
				p->u.memref.shm = NULL;
				break;
			}
			rc = tee_shm_get_pa(shm, 0, &pa);
			if (rc)
				return rc;
			p->u.memref.shm_offs = pa - mp->u.tmem.buf_ptr;
			p->u.memref.shm = shm;

			/* Check that the memref is covered by the shm object */
			if (p->u.memref.size) {
				size_t o = p->u.memref.shm_offs +
					   p->u.memref.size - 1;

				rc = tee_shm_get_pa(shm, o, NULL);
				if (rc)
					return rc;
			}
			break;
		default:
			pr_info("chenlu [gf_ca_entry] attr=%x", attr);
			return -EINVAL;
		}
	}
	/*pr_info("\n optee_from_msg_param ---end\n");	*/
	return 0;
}

/**
 * optee_to_msg_param() - convert from struct tee_params to OPTEE_MSG parameters
 * @msg_params:	OPTEE_MSG parameters
 * @num_params:	number of elements in the parameter arrays
 * @params:	subsystem itnernal parameter representation
 * Returns 0 on success or <0 on failure
 */
int optee_to_msg_param(struct optee_msg_param *msg_params, size_t num_params,
		       const struct tee_param *params)
{
	int rc;
	size_t n;
	phys_addr_t pa;

	/*pr_info("optee_to_msg_param ---start\n");*/
	for (n = 0; n < num_params; n++) {
		const struct tee_param *p = params + n;
		struct optee_msg_param *mp = msg_params + n;

		switch (p->attr) {
		case TEE_IOCTL_PARAM_ATTR_TYPE_NONE:
			mp->attr = TEE_IOCTL_PARAM_ATTR_TYPE_NONE;
			memset(&mp->u, 0, sizeof(mp->u));
			/*pr_info("mp->attr:%llx ,mp=%p",mp->attr,(char *)mp);*/
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT:
			mp->attr = OPTEE_MSG_ATTR_TYPE_VALUE_INPUT + p->attr -
				   TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INPUT;
			mp->u.value.a = p->u.value.a;
			mp->u.value.b = p->u.value.b;
			mp->u.value.c = p->u.value.c;
			/*pr_info("mp->attr:%llx ,mp=%p",mp->attr,(char *)mp);*/
			break;
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_OUTPUT:
		case TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INOUT:
			mp->attr = OPTEE_MSG_ATTR_TYPE_TMEM_INPUT +
				   p->attr -
				   TEE_IOCTL_PARAM_ATTR_TYPE_MEMREF_INPUT;
			mp->u.tmem.shm_ref = (unsigned long)p->u.memref.shm;
			mp->u.tmem.size = p->u.memref.size;
			if (!p->u.memref.shm) {
				mp->u.tmem.buf_ptr = 0;
				break;
			}
			rc = tee_shm_get_pa(p->u.memref.shm,
					    p->u.memref.shm_offs, &pa);
			if (rc)
				return rc;
			mp->u.tmem.buf_ptr = pa;
			mp->attr |= OPTEE_MSG_ATTR_CACHE_PREDEFINED <<
					OPTEE_MSG_ATTR_CACHE_SHIFT;
			/*pr_info("mp->attr:%llx ,mp=%p",mp->attr,(char *)mp);*/
			break;
		default:
			pr_info("mp->attr:default ");
			return -EINVAL;
		}
	}
	/*pr_info("\n optee_to_msg_param ---end\n");*/
	return 0;
}

static void optee_get_version(struct tee_device *teedev,
			      struct tee_ioctl_version_data *vers)
{
	struct tee_ioctl_version_data v = {
		.impl_id = TEE_IMPL_ID_OPTEE,
		.impl_caps = TEE_OPTEE_CAP_TZ,
		.gen_caps = TEE_GEN_CAP_GP,
	};
	*vers = v;
}

static int optee_open(struct tee_context *ctx)
{
	struct optee_context_data *ctxdata;
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);

	ctxdata = kzalloc(sizeof(*ctxdata), GFP_KERNEL);
	if (!ctxdata)
		return -ENOMEM;

	if (teedev == optee->supp_teedev) {
		if (!atomic_dec_and_test(&optee->supp.available)) {
			/* Supplicant device is already open */
			atomic_inc(&optee->supp.available);
			kfree(ctxdata);
			return -EBUSY;
		}
	}

	mutex_init(&ctxdata->mutex);
	INIT_LIST_HEAD(&ctxdata->sess_list);

	ctx->data = ctxdata;
	return 0;
}

static void optee_release(struct tee_context *ctx)
{
	struct optee_context_data *ctxdata = ctx->data;
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct tee_shm *shm;
	struct optee_msg_arg *arg = NULL;
	phys_addr_t parg;

	if (!ctxdata)
		return;

	shm = tee_shm_alloc(ctx->teedev, sizeof(struct optee_msg_arg),
			    TEE_SHM_MAPPED);
	if (!IS_ERR(shm)) {
		arg = tee_shm_get_va(shm, 0);
		/*
		 * If va2pa fails for some reason, we can't call
		 * optee_close_session(), only free the memory. Secure OS
		 * will leak sessions and finally refuse more session, but
		 * we will at least let normal world reclaim its memory.
		 */
		if (!IS_ERR(arg))
			tee_shm_va2pa(shm, arg, &parg);
	}

	while (true) {
		struct optee_session *sess;

		sess = list_first_entry_or_null(&ctxdata->sess_list,
						struct optee_session,
						list_node);
		if (!sess)
			break;
		list_del(&sess->list_node);
		if (!IS_ERR_OR_NULL(arg)) {
			memset(arg, 0, sizeof(*arg));
			arg->cmd = OPTEE_MSG_CMD_CLOSE_SESSION;
			/* pr_info("chenlu msg_arg->cmd = OPTEE_MSG_CMD_CLOSE_SESSION"); */
			arg->session = sess->session_id;
			optee_do_call_with_arg(ctx, parg);
		}
		kfree(sess);
	}
	kfree(ctxdata);

	if (!IS_ERR(shm))
		tee_shm_free(shm);

	ctx->data = NULL;

	if (teedev == optee->supp_teedev)
		atomic_inc(&optee->supp.available);
}

static struct tee_driver_ops optee_ops = {
	.get_version = optee_get_version,
	.open = optee_open,
	.release = optee_release,
	.open_session = optee_open_session,
	.close_session = optee_close_session,
	.invoke_func = optee_invoke_func,
	.cancel_req = optee_cancel_req,
};

static struct tee_desc optee_desc = {
	.name = DRIVER_NAME "-clnt",
	.ops = &optee_ops,
	.owner = THIS_MODULE,
};

static struct tee_driver_ops optee_supp_ops = {
	.get_version = optee_get_version,
	.open = optee_open,
	.release = optee_release,
	.supp_recv = optee_supp_recv,
	.supp_send = optee_supp_send,
};

static struct tee_desc optee_supp_desc = {
	.name = DRIVER_NAME "-supp",
	.ops = &optee_supp_ops,
	.owner = THIS_MODULE,
	.flags = TEE_DESC_PRIVILEGED,
};

static bool optee_msg_api_uid_is_optee_api(optee_invoke_fn *invoke_fn)
{
	struct arm_smccc_res res;

	invoke_fn(OPTEE_SMC_CALLS_UID, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 == OPTEE_MSG_UID_0 && res.a1 == OPTEE_MSG_UID_1 &&
	    res.a2 == OPTEE_MSG_UID_2 && res.a3 == OPTEE_MSG_UID_3)
		return true;
	return false;
}

static bool optee_msg_api_revision_is_compatible(optee_invoke_fn *invoke_fn)
{
	struct arm_smccc_res res;

	invoke_fn(OPTEE_SMC_CALLS_REVISION, 0, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 == OPTEE_MSG_REVISION_MAJOR &&
	    (int)res.a1 >= OPTEE_MSG_REVISION_MINOR)
		return true;
	return false;
}

static bool optee_msg_exchange_capabilities(optee_invoke_fn *invoke_fn,
					    u32 *sec_caps)
{
	struct arm_smccc_res res;
	u32 a1 = 0;

	/*
	 * TODO This isn't enough to tell if it's UP system (from kernel
	 * point of view) or not, is_smp() returns the the information
	 * needed, but can't be called directly from here.
	 */
#ifndef CONFIG_SMP
	a1 |= OPTEE_SMC_NSEC_CAP_UNIPROCESSOR;
#endif

	invoke_fn(OPTEE_SMC_EXCHANGE_CAPABILITIES, a1, 0, 0, 0, 0, 0, 0, &res);

	if (res.a0 != OPTEE_SMC_RETURN_OK)
		return false;

	*sec_caps = res.a1;
	return true;
}

static struct tee_shm_pool *optee_config_shm_ioremap(struct device *dev,
			optee_invoke_fn *invoke_fn,
			void __iomem **ioremaped_shm)
{
	struct arm_smccc_res res;
	struct tee_shm_pool *pool;
	unsigned long vaddr;
	phys_addr_t paddr;
	size_t size;
	phys_addr_t begin;
	phys_addr_t end;
	void __iomem *va;
	struct tee_shm_pool_mem_info priv_info;
	struct tee_shm_pool_mem_info dmabuf_info;

	invoke_fn(OPTEE_SMC_GET_SHM_CONFIG, 0, 0, 0, 0, 0, 0, 0, &res);
	if (res.a0 != OPTEE_SMC_RETURN_OK) {
		dev_info(dev, "shm service not available\n");
		return ERR_PTR(-ENOENT);
	}

	if (res.a3 != OPTEE_SMC_SHM_CACHED) {
		dev_err(dev, "only normal cached shared memory supported\n");
		return ERR_PTR(-EINVAL);
	}

	begin = roundup(res.a1, PAGE_SIZE);
	end = rounddown(res.a1 + res.a2, PAGE_SIZE);
	paddr = begin;
	size = end - begin;

	if (size < 2 * OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE) {
		dev_err(dev, "too small shared memory area\n");
		return ERR_PTR(-EINVAL);
	}

	va = ioremap_cache(paddr, size);
	/*va = ioremap_cache_zte(paddr, size);//by chenlu */
	if (!va) {
		dev_err(dev, "shared memory ioremap failed\n");
		return ERR_PTR(-EINVAL);
	}
	vaddr = (unsigned long)va;

	priv_info.vaddr = vaddr;
	priv_info.paddr = paddr;
	priv_info.size = OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.vaddr = vaddr + OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.paddr = paddr + OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE;
	dmabuf_info.size = size - OPTEE_SHM_NUM_PRIV_PAGES * PAGE_SIZE;

	pool = tee_shm_pool_alloc_res_mem(dev, &priv_info, &dmabuf_info);
	if (IS_ERR(pool))
		iounmap(va);
	else
		*ioremaped_shm = va;
	return pool;
}

/* Simple wrapper functions to be able to use a function pointer */
static void optee_smccc_smc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_smc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static void optee_smccc_hvc(unsigned long a0, unsigned long a1,
			    unsigned long a2, unsigned long a3,
			    unsigned long a4, unsigned long a5,
			    unsigned long a6, unsigned long a7,
			    struct arm_smccc_res *res)
{
	arm_smccc_hvc(a0, a1, a2, a3, a4, a5, a6, a7, res);
}

static int get_invoke_func(struct device *dev, optee_invoke_fn **invoke_fn)
{
	struct device_node *np = dev->of_node;
	const char *method;

	dev_info(dev, "probing for conduit method from DT.\n");

	if (of_property_read_string(np, "method", &method)) {
		dev_warn(dev, "missing \"method\" property\n");
		return -ENXIO;
	}

	if (!strcmp("hvc", method)) {
		*invoke_fn = optee_smccc_hvc;
	} else if (!strcmp("smc", method)) {
		*invoke_fn = optee_smccc_smc;
	} else {
		dev_warn(dev, "invalid \"method\" property: %s\n", method);
		return -EINVAL;
	}
	return 0;
}

struct semaphore	irq_sem;
#define SPI_STATUS0_REG                   (0x001c)
/*#include <mach/sync_write.h> */
#define spi_readl(reg, offset) __raw_readl(reg+(offset))
extern void __iomem *get_spi_regs(void);
static irqreturn_t __attribute__((__unused__)) mt_spi_interrupt(int irq, void *dev_id)
{
	struct optee *optee = platform_get_drvdata((struct platform_device *)dev_id);
	u32 reg_val;

	/*pr_info("\n llllllllllllll mt_spi_interrupt cpu:%d\n", smp_processor_id());*/

	/* liuliang */
	/*
	if (!optee) {
		pr_info("llllllllllllll mt_spi_interrupt dev data is NULL\n");
		return IRQ_NONE;
	}

	if (optee->regs == NULL) {
		optee->regs = get_spi_regs();
		if (optee->regs == NULL) {
			pr_info("llllllllllllll mt_spi_interrupt memory is NULL\n");
			return IRQ_NONE;
		}
		platform_set_drvdata((struct platform_device *)dev_id, optee);
	}
	*/

	reg_val = spi_readl(optee->regs, SPI_STATUS0_REG);
	/*pr_info("llllllllllllllll interrupt status:%x\n", reg_val & 0x3);*/

	up(&irq_sem);

	/*pr_info("\n llllllllllllllllll mt_spi_interrupt return cpu:%d end\n", smp_processor_id());*/
	return IRQ_HANDLED;
}

/*
#include "mach/emi_mpu.h"
extern int emi_mpu_set_region_protection(unsigned long long start_addr,
unsigned long long end_addr, int region, unsigned int access_permission);
*/

static int optee_probe(struct platform_device *pdev)
{
	optee_invoke_fn *invoke_fn;
	struct tee_shm_pool *pool;
	struct optee *optee = NULL;
	void __iomem *ioremaped_shm = NULL;
	struct tee_device *teedev;
	u32 sec_caps;
	int rc;
	/*struct resource *regs; */

	pr_info("\noptee_probe\n");

	rc = get_invoke_func(&pdev->dev, &invoke_fn);
	if (rc)
		return rc;

	if (!optee_msg_api_uid_is_optee_api(invoke_fn) ||
	    !optee_msg_api_revision_is_compatible(invoke_fn) ||
	    !optee_msg_exchange_capabilities(invoke_fn, &sec_caps))
		return -EINVAL;

	/*
	 * We have no other option for shared memory, if secure world
	 * doesn't have any reserved memory we can use we can't continue.
	 */
	if (!(sec_caps & OPTEE_SMC_SEC_CAP_HAVE_RESERVERED_SHM))
		return -EINVAL;

	pool = optee_config_shm_ioremap(&pdev->dev, invoke_fn, &ioremaped_shm);
	if (IS_ERR(pool))
		return PTR_ERR(pool);

	optee = devm_kzalloc(&pdev->dev, sizeof(*optee), GFP_KERNEL);
	if (!optee) {
		rc = -ENOMEM;
		goto err;
	}

	optee->dev = &pdev->dev;
	optee->invoke_fn = invoke_fn;

	teedev = tee_device_alloc(&optee_desc, &pdev->dev, pool, optee);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err;
	}
	optee->teedev = teedev;

	teedev = tee_device_alloc(&optee_supp_desc, &pdev->dev, pool, optee);
	if (IS_ERR(teedev)) {
		rc = PTR_ERR(teedev);
		goto err;
	}
	optee->supp_teedev = teedev;

	rc = tee_device_register(optee->teedev);
	if (rc)
		goto err;

	rc = tee_device_register(optee->supp_teedev);
	if (rc)
		goto err;

	mutex_init(&optee->call_queue.mutex);
	INIT_LIST_HEAD(&optee->call_queue.waiters);
	optee_wait_queue_init(&optee->wait_queue);
	optee_supp_init(&optee->supp);
	optee->ioremaped_shm = ioremaped_shm;
	optee->pool = pool;

	#if 0
	regs = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!regs) {
		pr_info("get resource regs NULL.\n");
		goto err;
	}

	optee->irq = platform_get_irq(pdev, 0);

	if (optee->irq  < 0) {
		pr_info("platform_get_irq error. get invalid irq\n");
		goto err;
	}

	if (!request_mem_region(regs->start, resource_size(regs), pdev->name)) {
		pr_info("SPI register memory region failed");
		goto err;
	}

	optee->regs = ioremap(regs->start, resource_size(regs));

	#endif

	/* liuliang, spi driver move to system spi driver spi.c */
	/*
	optee->irq = 118 + 32;
	optee->regs = get_spi_regs();

	optee->regs = ioremap(0x1100A000, 4 * 1024);

	if (optee->regs == NULL) {
		pr_info("lllllllllllllllllll ioremap error. get invalid optee->regs,
				this will remap later in spi driver\n");
		goto err;
	}

	pr_info("Controller at 0x%p (irq %d)\n", optee->regs, optee->irq);
	*/
	platform_set_drvdata(pdev, optee);
	/*
	rc = request_irq(optee->irq, mt_spi_interrupt, IRQF_TRIGGER_LOW, dev_name(&pdev->dev), pdev);

	if (rc) {
		pr_info("request_irq fail rc=%d\n",rc);
		goto err;
	}
	*/
	optee_enable_shm_cache(optee);

	/*sema_init(&irq_sem, 0);*/

	pr_info("llllllllllllll optee probe end\n");

	return 0;

err:
	tee_device_unregister(optee->teedev);
	tee_device_unregister(optee->supp_teedev);
	if (pool)
		tee_shm_pool_free(pool);
	if (ioremaped_shm)
		iounmap(optee->ioremaped_shm);
	if (optee->regs)
		iounmap(optee->regs);

	return rc;
}

static int optee_remove(struct platform_device *pdev)
{
	struct optee *optee = platform_get_drvdata(pdev);

	optee_disable_shm_cache(optee);

	tee_device_unregister(optee->teedev);
	tee_device_unregister(optee->supp_teedev);
	tee_shm_pool_free(optee->pool);
	if (optee->ioremaped_shm)
		iounmap(optee->ioremaped_shm);
	#if 0
	if (optee->regs)
		iounmap(optee->regs);
	#endif
	optee_wait_queue_exit(&optee->wait_queue);
	optee_supp_uninit(&optee->supp);
	mutex_destroy(&optee->call_queue.mutex);

	free_irq(optee->irq, pdev);

	return 0;
}

static const struct of_device_id optee_match[] = {
	{ .compatible = "linaro,optee-tz" },
	{},
};

static struct platform_driver optee_driver = {
	.driver = {
		.name = DRIVER_NAME,
		.of_match_table = optee_match,
	},
	.probe = optee_probe,
	.remove = optee_remove,
};

/* liuliang
static int mytimer_init(void)
{
	setup_timer(&mytimer, myfunc, (unsigned long)"Hello, world!");

	mytimer.expires = jiffies + HZ;

	add_timer(&mytimer);
	pr_info("optee test timer init!\n");
	return 0;
}
*/

static int __init optee_driver_init(void)
{
	struct device_node *node;

	/*
	 * Preferred path is /firmware/optee, but it's the matching that
	 * matters.
	 */
	pr_info("\noptee_driver_init\n");
	/* mytimer_init(); */

	for_each_matching_node(node, optee_match)
		of_platform_device_create(node, NULL, NULL);

	return platform_driver_register(&optee_driver);
}
module_init(optee_driver_init);

static void __exit optee_driver_exit(void)
{
	platform_driver_unregister(&optee_driver);
}
module_exit(optee_driver_exit);

MODULE_AUTHOR("Linaro");
MODULE_DESCRIPTION("OP-TEE driver");
MODULE_SUPPORTED_DEVICE("");
MODULE_VERSION("1.0");
MODULE_LICENSE("GPL v2");
