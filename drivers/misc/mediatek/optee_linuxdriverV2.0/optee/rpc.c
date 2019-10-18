/*
 * Copyright (c) 2015-2016, Linaro Limited
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
#include <linux/device.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/tee_drv.h>
#include "optee_private.h"
#include "optee_smc.h"
#include <linux/err.h>

/*by chenlu begin */
#if 0
typedef __s64 time64_t;

struct timespec64 {
	time64_t	tv_sec;			/* seconds */
	long		tv_nsec;		/* nanoseconds */
};

static inline struct timespec64 timespec_to_timespec64(const struct timespec ts)
{
	struct timespec64 ret;

	ret.tv_sec = ts.tv_sec;
	ret.tv_nsec = ts.tv_nsec;
	return ret;
}

static inline void getnstimeofday64(struct timespec64 *ts64)
{
	struct timespec ts;

	getnstimeofday(&ts);
	*ts64 = timespec_to_timespec64(ts);
}
#endif
/*by chenlu end */

struct wq_entry {
	struct list_head link;
	struct completion c;
	u32 key;
};

void optee_wait_queue_init(struct optee_wait_queue *priv)
{
	mutex_init(&priv->mu);
	INIT_LIST_HEAD(&priv->db);
}

void optee_wait_queue_exit(struct optee_wait_queue *priv)
{
	mutex_destroy(&priv->mu);
}

static void handle_rpc_func_cmd_get_time(struct optee_msg_arg *arg)
{
	struct optee_msg_param *params;
	struct timespec64 ts;

	if (arg->num_params != 1)
		goto bad;
	params = OPTEE_MSG_GET_PARAMS(arg);
	if ((params->attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_OUTPUT)
		goto bad;

	getnstimeofday64(&ts);
	params->u.value.a = ts.tv_sec;
	params->u.value.b = ts.tv_nsec;

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static struct wq_entry *wq_entry_get(struct optee_wait_queue *wq, u32 key)
{
	struct wq_entry *w;

	mutex_lock(&wq->mu);

	list_for_each_entry(w, &wq->db, link)
		if (w->key == key)
			goto out;

	w = kmalloc(sizeof(*w), GFP_KERNEL);
	if (w) {
		init_completion(&w->c);
		w->key = key;
		list_add_tail(&w->link, &wq->db);
	}
out:
	mutex_unlock(&wq->mu);
	return w;
}

static void wq_sleep(struct optee_wait_queue *wq, u32 key)
{
	struct wq_entry *w = wq_entry_get(wq, key);

	if (w) {
		wait_for_completion(&w->c);
		mutex_lock(&wq->mu);
		list_del(&w->link);
		mutex_unlock(&wq->mu);
		kfree(w);
	}
}

static void wq_wakeup(struct optee_wait_queue *wq, u32 key)
{
	struct wq_entry *w = wq_entry_get(wq, key);

	if (w)
		complete(&w->c);
}

extern struct semaphore	irq_sem;
static void handle_rpc_func_cmd_wq(struct optee *optee,
				   struct optee_msg_arg *arg)
{
	struct optee_msg_param *params;
	int ret = 0;

	if (arg->num_params != 1)
		goto bad;

	params = OPTEE_MSG_GET_PARAMS(arg);
	if ((params->attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto bad;

	switch (params->u.value.a) {
	case OPTEE_MSG_RPC_WAIT_QUEUE_SLEEP:
		wq_sleep(&optee->wait_queue, params->u.value.b);
	break;
	case OPTEE_MSG_RPC_WAIT_QUEUE_WAKEUP:
		wq_wakeup(&optee->wait_queue, params->u.value.b);
	break;
	/* liuliang */
	case OPTEE_MSG_RPC_CLEAR_WAIT_QUEUE_IRQ: {
		/* clear spi irq semphone */
		/*pr_info("\nchenlu handle_rpc_func_cmd_wq OPTEE_MSG_RPC_CLEAR_WAIT_QUEUE_IRQ sleep\n");*/
		while (ret == 0) {
			ret = down_trylock(&irq_sem);
		}
		/*pr_info("\nchenlu handle_rpc_func_cmd_wq OPTEE_MSG_RPC_CLEAR_WAIT_QUEUE_IRQ wake\n");*/
	}
	break;
	case OPTEE_MSG_RPC_WAIT_QUEUE_IRQ:
		/*pr_info("\nchenlu handle_rpc_func_cmd_wq OPTEE_MSG_RPC_WAIT_QUEUE_IRQ sleep\n");*/
		down(&irq_sem);
		/*pr_info("\nchenlu handle_rpc_func_cmd_wq OPTEE_MSG_RPC_WAIT_QUEUE_IRQ wake\n");*/
	break;
	default:
		goto bad;
	}

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static void handle_rpc_func_cmd_wait(struct optee_msg_arg *arg)
{
	struct optee_msg_param *params;
	u32 msec_to_wait;

	if (arg->num_params != 1)
		goto bad;

	params = OPTEE_MSG_GET_PARAMS(arg);
	if ((params->attr & OPTEE_MSG_ATTR_TYPE_MASK) !=
			OPTEE_MSG_ATTR_TYPE_VALUE_INPUT)
		goto bad;

	msec_to_wait = params->u.value.a;

	/* set task's state to interruptible sleep */
	set_current_state(TASK_INTERRUPTIBLE);

	/* take a nap */
	schedule_timeout(msecs_to_jiffies(msec_to_wait));

	arg->ret = TEEC_SUCCESS;
	return;
bad:
	arg->ret = TEEC_ERROR_BAD_PARAMETERS;
}

static void handle_rpc_supp_cmd(struct tee_context *ctx,
				struct optee_msg_arg *arg)
{
	struct tee_param *params;
	struct optee_msg_param *msg_params = OPTEE_MSG_GET_PARAMS(arg);

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	params = kmalloc_array(arg->num_params, sizeof(struct tee_param),
			       GFP_KERNEL);
	if (!params) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	if (optee_from_msg_param(params, arg->num_params, msg_params)) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		goto out;
	}

	arg->ret = optee_supp_thrd_req(ctx, arg->cmd, arg->num_params, params);

	if (optee_to_msg_param(msg_params, arg->num_params, params))
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
out:
	kfree(params);
}

static struct tee_shm *cmd_alloc_suppl(struct tee_context *ctx, size_t sz)
{
	u32 ret;
	struct tee_param param;
	struct optee *optee = tee_get_drvdata(ctx->teedev);

	/*pr_info("\nchenlu cmd_alloc_suppl\n"); */

	param.attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param.u.value.a = OPTEE_MSG_RPC_SHM_TYPE_APPL;
	param.u.value.b = sz;
	param.u.value.c = 0;

	ret = optee_supp_thrd_req(ctx, OPTEE_MSG_RPC_CMD_SHM_ALLOC, 1, &param);
	if (ret)
		return ERR_PTR(-ENOMEM);

	/* Increases count as secure world doesn't have a reference */
	return tee_shm_get_from_id(optee->supp_teedev, param.u.value.c);
}

static void handle_rpc_func_cmd_shm_alloc(struct tee_context *ctx,
					  struct optee_msg_arg *arg)
{
	struct tee_device *teedev = ctx->teedev;
	struct optee_msg_param *params = OPTEE_MSG_GET_PARAMS(arg);
	phys_addr_t pa;
	struct tee_shm *shm;
	size_t sz;
	size_t n;

	/*pr_info("\nchenlu handle_rpc_func_cmd_shm_alloc\n"); */

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	if (!arg->num_params ||
	    params->attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	for (n = 1; n < arg->num_params; n++) {
		if (params[n].attr != OPTEE_MSG_ATTR_TYPE_NONE) {
			arg->ret = TEEC_ERROR_BAD_PARAMETERS;
			return;
		}
	}

	sz = params->u.value.b;
	/*pr_info("\nchenlu sz=%zu, params->u.value.a=%llx",sz,params->u.value.a); */
	switch (params->u.value.a) {
	case OPTEE_MSG_RPC_SHM_TYPE_APPL:
		shm = cmd_alloc_suppl(ctx, sz);
		break;
	case OPTEE_MSG_RPC_SHM_TYPE_KERNEL:
		shm = tee_shm_alloc(teedev, sz, TEE_SHM_MAPPED);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	if (IS_ERR(shm)) {
		arg->ret = TEEC_ERROR_OUT_OF_MEMORY;
		return;
	}

	if (tee_shm_get_pa(shm, 0, &pa)) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		goto bad;
	}

	params[0].attr = OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT;
	params[0].u.tmem.buf_ptr = pa;
	params[0].u.tmem.size = sz;
	params[0].u.tmem.shm_ref = (unsigned long)shm;
	arg->ret = TEEC_SUCCESS;
	return;
bad:
	tee_shm_free(shm);
}

static void cmd_free_suppl(struct tee_context *ctx, struct tee_shm *shm)
{
	struct tee_param param;

	param.attr = TEE_IOCTL_PARAM_ATTR_TYPE_VALUE_INOUT;
	param.u.value.a = OPTEE_MSG_RPC_SHM_TYPE_APPL;
	param.u.value.b = tee_shm_get_id(shm);
	param.u.value.c = 0;

	/*
	 * Match the tee_shm_get_from_id() in cmd_alloc_suppl() as secure
	 * world has released its reference.
	 *
	 * It's better to do this before sending the request to supplicant
	 * as we'd like to let the process doing the initial allocation to
	 * do release the last reference too in order to avoid stacking
	 * many pending fput() on the client process. This could otherwise
	 * happen if secure world does many allocate and free in a single
	 * invoke.
	 */
	tee_shm_put(shm);

	optee_supp_thrd_req(ctx, OPTEE_MSG_RPC_CMD_SHM_FREE, 1, &param);
}

static void handle_rpc_func_cmd_shm_free(struct tee_context *ctx,
					 struct optee_msg_arg *arg)
{
	struct optee_msg_param *params = OPTEE_MSG_GET_PARAMS(arg);
	struct tee_shm *shm;

	arg->ret_origin = TEEC_ORIGIN_COMMS;

	if (arg->num_params != 1 ||
	    params->attr != OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
		return;
	}

	shm = (struct tee_shm *)(unsigned long)params->u.value.b;
	switch (params->u.value.a) {
	case OPTEE_MSG_RPC_SHM_TYPE_APPL:
		cmd_free_suppl(ctx, shm);
		break;
	case OPTEE_MSG_RPC_SHM_TYPE_KERNEL:
		tee_shm_free(shm);
		break;
	default:
		arg->ret = TEEC_ERROR_BAD_PARAMETERS;
	}
	arg->ret = TEEC_SUCCESS;
}

static void handle_rpc_func_cmd(struct tee_context *ctx, struct optee *optee,
				struct tee_shm *shm)
{
	struct optee_msg_arg *arg;

	arg = tee_shm_get_va(shm, 0);
	if (IS_ERR(arg)) {
		dev_err(optee->dev, "%s: tee_shm_get_va %p failed\n",
			__func__, shm);
		/*pr_info(" chenlu return\n"); */
		return;
	}

	/*pr_info("\n chenlu handle_rpc_func_cmd:arg->cmd=%x, arg->func=%x, arg->session=%x,
			arg->cancel_id=%x, arg->pad=%x, arg->ret=%x, arg->ret_origin=%x, arg->num_params=%x\n",
			arg->cmd, arg->func, arg->session, arg->cancel_id, arg->pad, arg->ret, arg->ret_origin,
			arg->num_params);*/

/*
	if (arg->cmd == OPTEE_MSG_RPC_CMD_LOAD_TA) {
	    struct optee_msg_param *params;
		pr_info("\n chenlu handle_rpc_func_cmd OPTEE_MSG_RPC_CMD_LOAD_TA\n");
	    params = OPTEE_MSG_GET_PARAMS(arg);
		pr_info("\n chenlu params[0].attr = %llx\n",params[0].attr);
		if (params[0].attr == OPTEE_MSG_ATTR_TYPE_VALUE_INPUT) {
			pr_info("\n params[0].u.value.a = %llx\n",params[0].u.value.a);
			pr_info("\n params[0].u.value.b = %llx\n",params[0].u.value.b);
			pr_info("\n params[0].u.value.c = %llx\n",params[0].u.value.c);
		}
		pr_info("\n chenlu params[1].attr = %llx\n",params[1].attr);
		if (params[1].attr == OPTEE_MSG_ATTR_TYPE_TMEM_OUTPUT) {
			pr_info("\n params[1].u.tmem.buf_ptr = %llx\n",params[1].u.tmem.buf_ptr);
			pr_info("\n params[1].u.tmem.size = %llx\n",params[1].u.tmem.size);
			pr_info("\n params[1].u.tmem.shm_ref = %llx\n",params[1].u.tmem.shm_ref);
		}
    }
*/
	switch (arg->cmd) {
	case OPTEE_MSG_RPC_CMD_GET_TIME:
		handle_rpc_func_cmd_get_time(arg);
		break;
	case OPTEE_MSG_RPC_CMD_WAIT_QUEUE:
		handle_rpc_func_cmd_wq(optee, arg);
		break;
	case OPTEE_MSG_RPC_CMD_SUSPEND:
		handle_rpc_func_cmd_wait(arg);
		break;
	case OPTEE_MSG_RPC_CMD_SHM_ALLOC:
		handle_rpc_func_cmd_shm_alloc(ctx, arg);
		break;
	case OPTEE_MSG_RPC_CMD_SHM_FREE:
		handle_rpc_func_cmd_shm_free(ctx, arg);
		break;
	default:
		handle_rpc_supp_cmd(ctx, arg);
	}
}

/**
 * optee_handle_rpc() - handle RPC from secure world
 * @ctx:	context doing the RPC
 * @param:	value of registers for the RPC
 *
 * Result of RPC is written back into @param.
 */
void optee_handle_rpc(struct tee_context *ctx, struct optee_rpc_param *param)
{
	struct tee_device *teedev = ctx->teedev;
	struct optee *optee = tee_get_drvdata(teedev);
	struct tee_shm *shm;
	phys_addr_t pa;
	struct optee_msg_arg *arg;

	/*pr_info("chenlu optee_handle_rpc 1 OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0) = %d\n",
			OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0));*/

	switch (OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0)) {
	case OPTEE_SMC_RPC_FUNC_ALLOC:
		shm = tee_shm_alloc(teedev, param->a1, TEE_SHM_MAPPED);
		if (!IS_ERR(shm) && !tee_shm_get_pa(shm, 0, &pa)) {
			reg_pair_from_64(&param->a1, &param->a2, pa);
			reg_pair_from_64(&param->a4, &param->a5,
					 (unsigned long)shm);
			arg = tee_shm_get_va(shm, 0);
			/*arg->cmd=0x1;
			arg->func=0x2;
			arg->session=0x3;
			arg->cancel_id=0x4;
			arg->pad=0x5;
			arg->ret=0x6;
			arg->ret_origin=0x7;
			arg->num_params=0x8;*/
			/*pr_info("\noptee_handle_rpc OPTEE_SMC_RPC_FUNC_ALLOC:arg->cmd=%x, arg->func=%x,
					arg->session=%x, arg->cancel_id=%x, arg->pad=%x, arg->ret=%x,
					arg->ret_origin=%x, arg->num_params=%x\n", arg->cmd, arg->func, arg->session,
					arg->cancel_id, arg->pad, arg->ret, arg->ret_origin, arg->num_params);*/
		} else {
			param->a1 = 0;
			param->a2 = 0;
			param->a4 = 0;
			param->a5 = 0;
		}
		/*pr_info("OPTEE_SMC_RPC_FUNC_ALLOC shm = %lx, shm->paddr = %lx,shm->kaddr = %lx",
				(unsigned long)shm, tee_shm_get_pa_debug(shm), tee_shm_get_va_debug(shm));*/

		break;
	case OPTEE_SMC_RPC_FUNC_FREE:
		shm = reg_pair_to_ptr(param->a1, param->a2);
		tee_shm_free(shm);
		break;
	case OPTEE_SMC_RPC_FUNC_IRQ:
		/*
		 * An IRQ was raised while secure world was executing,
		 * since all IRQs a handled in Linux a dummy RPC is
		 * performed to let Linux take the IRQ through the normal
		 * vector.
		 */

		/* pr_info("chenlu OPTEE_SMC_RPC_FUNC_IRQ\n"); */
		break;

	case OPTEE_SMC_RPC_FUNC_CMD:
		shm = reg_pair_to_ptr(param->a1, param->a2);
		/*pr_info("chenlu OPTEE_SMC_RPC_FUNC_CMD shm = %lx, shm->paddr = %lx, shm->kaddr = %lx",
				(unsigned long)shm, tee_shm_get_pa_debug(shm), tee_shm_get_va_debug(shm));*/
		handle_rpc_func_cmd(ctx, optee, shm);
		break;

	/*guyoupeng*/
	case OPTEE_SMC_RPC_FUNC_FIQ:
		/*
		 * An FIQ was raised while secure world was executing,
		 * since all FIQs a handled in Linux a dummy RPC is
		 * performed to let Linux take the FIQ through the normal
		 * vector.
		 */

		/*pr_info("chenlu OPTEE_SMC_RPC_FUNC_FIQ\n");*/
		break;
	default:
		dev_warn(optee->dev, "Unknown RPC func 0x%x\n",
			 (u32)OPTEE_SMC_RETURN_GET_RPC_FUNC(param->a0));
		break;
	}

	param->a0 = OPTEE_SMC_CALL_RETURN_FROM_RPC;
}
