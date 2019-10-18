/*
 *Copyright 2016 NXP Semiconductors
 *
 *Licensed under the Apache License, Version 2.0 (the "License");
 *you may not use this file except in compliance with the License.
 *You may obtain a copy of the License at
 *
 *http://www.apache.org/licenses/LICENSE-2.0
 *
 *Unless required by applicable law or agreed to in writing, software
 *distributed under the License is distributed on an "AS IS" BASIS,
 *WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *See the License for the specific language governing permissions and
 *limitations under the License.
 */

/*
 * tfa_ext_nl.c
 *
 *	external DSP/RPC interface using netlink
 *
 *  Created on: May 23, 2016
 *      Author: wim
 */

#include "config.h"
#include "tfa_internal.h"
#include "tfa_ext.h"

#include <linux/module.h>

/* for netlink */
//TODO use generic netlink instead
#include <net/sock.h>
#include <linux/netlink.h>
#include <linux/skbuff.h>
#define NETLINK_USER 31

#define RCV_BUFFER_SIZE 2048

/*
 * module globals
 */
static tfa_event_handler_t tfa_ext_handle_event;
static int remote_connect = 0;

static struct sock *nl_sk = NULL;
static int pid = 0;

static struct work_struct event_work;	/* event handle loop */
static struct workqueue_struct *tfa_ext_wq;

static DECLARE_WAIT_QUEUE_HEAD(rcv_wait_queue);
static int rcv_data_available = 1;
static u8 rcv_buffer[RCV_BUFFER_SIZE];
static int rcv_size;

static int trace_level = 1;
module_param(trace_level, int, S_IRUGO);
MODULE_PARM_DESC(trace_level, "tfa_ext_nl trace level (0=off).");

/*
 * netlink interfaces
 */
static int tfa_netlink_send_msg(int pid, const u8 * buf, int length)
{
	struct nlmsghdr *nlh;
	struct sk_buff *skb_out;
	int res;

	if (pid == 0) {
		pr_debug("No pid\n");
		return 0;
	}

	skb_out = nlmsg_new(length, 0);
	if (!skb_out) {
		pr_err("Failed to allocate new skb\n");
		return -ENOMEM;
	}

	nlh = nlmsg_put(skb_out, 0, 0, NLMSG_DONE, length, 0);
	if (!nlh) {
		kfree_skb(skb_out);
		return -EMSGSIZE;
	}

	NETLINK_CB(skb_out).dst_group = 0;	/* not in mcast group */
	memcpy(nlmsg_data(nlh), buf, length);

	if (trace_level > 0)
		print_hex_dump_bytes("tfa_netlink_send_msg ", DUMP_PREFIX_NONE,
				     nlmsg_data(nlh), length);

	res = nlmsg_unicast(nl_sk, skb_out, pid);
	if (res < 0) {
		pr_debug("Error %d while sending back to user\n", res);
		length = 0;
	}

	return length;
}

static void tfa_netlink_recv_msg(struct sk_buff *skb)
{

	struct nlmsghdr *nlh;
	int msg_size;

	pr_debug("Entering: %s\n", __FUNCTION__);

	nlh = (struct nlmsghdr *)skb->data;
	msg_size = nlmsg_len(nlh);
	pr_debug("Netlink received msg payload: %d bytes\n", msg_size);

	if (trace_level > 0)
		print_hex_dump_bytes("tfa_ext ", DUMP_PREFIX_NONE,
				     nlmsg_data(nlh), msg_size);

	if (msg_size >= RCV_BUFFER_SIZE) {
		pr_err
		    ("%s: rcv_buffer too small: received %d bytes, rcv_buffer %d bytes\n",
		     __FUNCTION__, msg_size, RCV_BUFFER_SIZE);
		rcv_size = RCV_BUFFER_SIZE;
	} else {
		rcv_size = msg_size;
	}

	memcpy(rcv_buffer, nlmsg_data(nlh), rcv_size);
	pid = nlh->nlmsg_pid;	/* pid of sending process */

	rcv_data_available = 1;
	wake_up_interruptible(&rcv_wait_queue);

	queue_work(tfa_ext_wq, &event_work);
}

static int tfa_netlink_init(void)
{
	struct netlink_kernel_cfg cfg = {
		.input = tfa_netlink_recv_msg,
	};

	nl_sk = netlink_kernel_create(&init_net, NETLINK_USER, &cfg);
	if (!nl_sk) {
		pr_err("Error creating socket.\n");
		return -1;
	}

	return 0;
}

static void tfa_netlink_exit(void)
{
	netlink_kernel_release(nl_sk);
}

/*
 * remote interface wrappers
 */
static int remote_send(u8 * buf, int length)
{
	return tfa_netlink_send_msg(pid, (const u8 *)buf, length);
}

static int remote_rcv(u8 * buf, int length)
{
	memcpy(buf, rcv_buffer, rcv_size);
	return rcv_size;
}

static int remote_init(void)
{
	rcv_size = 0;
	return tfa_netlink_init();	/* 0 if ok */
}

static void remote_exit(void)
{
	tfa_netlink_exit();
}

/*
 * remote tfa interfacing
 *
 *  The user space uses the scribo protocol pin command to carry events.
 *
 *  The interrupt pin id is used for compatibity with simulations.
 *    event header format = 'ps'(16) + id(8) + val(16) + 0x02
 *
 *    example event message: 0x73 0x70 0x08 0x34 0x12 0x02
 *                           --pin=GPIO_INT_L 0x1234
 *
 * event sequence >>>TBD update when enums are stable
 *  TFADSP_EXT_PWRUP 	: DSP starting
 *  	set cold
 *  TFADSP_CMD_READY	: Ready to receive commands
 *  	call tfa_start to send msgs
 *  TFADSP_WARM			: config complete
 *  	enable amp
 *  	send Re0
 *	await pilot tone [audio power detect]
 *	audio active
 *
 *	async events when audio active
 *	TFADSP_SPARSESIG_DETECTED :Sparse signal detected
 *		call sparse protection
 *
 *	TFADSP_EXT_PWRDOWN 	: DSP stopping
 *
 *	TFADSP_SOFT_MUTE_READY	[audio low power detect]: Muting completed
 *		amp off
 *	TFADSP_VOLUME_READY : Volume change completed
 *
 *  TFADSP_DAMAGED_SPEAKER : Damaged speaker was detected
 *  	amp off
 *
 *
 */
enum tfadsp_event_en tfa_ext_wait_event(void);
int tfa_ext_event_ack(u16 err);

/*
 * static communication buffer
 */
static u8 event_msg_buf[8];

/*
 * return the event code from the raw mesaage
 */
static int tfa_ext_event_code(uint8_t buf[6], int pin)
{
	int code;

	// interrupt pin
	if (buf[0] == 's' && buf[1] == 'p' && buf[2] == pin)
		code = buf[4] << 8 | buf[3];
	else
		code = -1;

	return code;
}

/*
 * return the response to an event
 */
int tfa_ext_event_ack(u16 response)
{
	event_msg_buf[0] = 's';
	event_msg_buf[1] = 'p';
	event_msg_buf[2] = response & 0xFF;
	event_msg_buf[3] = response >> 8;
	event_msg_buf[4] = 0;
	event_msg_buf[5] = 0;
	event_msg_buf[6] = 0x02;

	return remote_send(event_msg_buf, 7) == 7 ? 0 : 1;
}

/*
 * wait for an event from the remote controller
 */
enum tfadsp_event_en tfa_ext_wait_event(void)
{
	u8 buf[6];
	int length;
	enum tfadsp_event_en this_dsp_event = 0;

	wait_event_interruptible(rcv_wait_queue, rcv_data_available != 0);
	rcv_data_available = 0;

	length = remote_rcv(buf, sizeof(buf));
	if ((length) && (trace_level > 0)) {
		print_hex_dump_bytes("tfa_ext_wait_event ", DUMP_PREFIX_NONE,
				     buf, length);
	}

	if (length >= 6)	/* why do we get 2 more bytes? because it is aligned to multiples of 4 bytes */
		this_dsp_event = tfa_ext_event_code((uint8_t *) buf, 8);	//GPIO_INT_L

	pr_debug("event:%d/0x%04x\n", this_dsp_event, this_dsp_event);

	return this_dsp_event;

}

/**
@brief DSP message interface that sends the RPC to the remote TFADSP

This is the the function that get called by the tfa driver for each
RPC message that is to be send to the TFADSP.
The tfa_ext_registry() function will connect this.

@param [in] devidx : tfa device index that owns the TFADSP
@param [in] length : length in bytes of the message in the buffer
@param [in] buffer : buffer pointer to the RPC message payload

@return 0  success

*/
static int tfa_ext_dsp_msg(int devidx, int length, const char *buffer)
{
	int error = 0, real_length;
	u8 *buf = (u8 *) buffer;

	if (trace_level > 0)
		pr_debug("%s, id:0x%02x%02x%02x, length:%d \n", __FUNCTION__,
			 buf[0], buf[1], buf[2], length);

	real_length = remote_send((u8 *) buffer, length);
	if (real_length != length) {
		pr_err("%s: length mismatch: exp:%d, act:%d\n", __FUNCTION__,
		       length, real_length);
		error = 6;	/* communication with the DSP failed */
	}
	if (tfa_ext_wait_event() == TFADSP_CMD_ACK) {
		tfa_ext_event_ack(TFADSP_CMD_ACK);
	} else {
		pr_err("%s: no TFADSP_CMD_ACK event received\n", __FUNCTION__);
		tfa_ext_event_ack(0);
		error = 6;	/* communication with the DSP failed */
	}
	return error;
}

static int tfa_ext_dsp_msg_read(int devidx, int length, char *buffer)
{
	int error = 0;
	int size = min(length, rcv_size);

	pr_debug("%s(%d,%d,%p)\n", __FUNCTION__, devidx, length, buffer);

	memcpy(buffer, rcv_buffer, size);

	return error;
}


/**
@brief Register at the tfa driver and instantiate the remote interface functions.

This function must be called once at startup after the tfa driver module is
loaded.
The tfa_ext_register() will be called to get the  event handler and dsp message
interface functions and the remote TFADSP will be connected after successful
return.

@param void

@return 0 on success

*/
static int tfa_ext_registry(void)
{
	int ret = -1;

	pr_debug("%s\n", __FUNCTION__);

	if (tfa_ext_register(NULL, tfa_ext_dsp_msg, tfa_ext_dsp_msg_read, &tfa_ext_handle_event)) {
		pr_err("Cannot register to tfa driver!\n");
		return 1;
	}
	if (tfa_ext_handle_event == NULL) {
		pr_err("even callback not registered by tfa driver!\n");
		return 1;
	}
	if (remote_connect == 0) {
		remote_connect = (remote_init() == 0);
		if (remote_connect == 0)
			pr_err("remote_init failed\n");
		else
			ret = 0;
	}

	return remote_connect == 0;
}

/**
@brief Un-register and close the remote interface.

This function must be called once at shutdown of the remote device.

@param void

@return 0 on success

*/

static void tfa_ext_unregister(void)
{
	pr_debug("%s\n", __FUNCTION__);

	if (remote_connect)
		remote_exit();

	remote_connect = 0;

}

/****************************** event loop *****************************/

static void tfa_ext_event_loop(struct work_struct *work)
{
	enum tfadsp_event_en event;
	int event_return = -1;

	pr_info("%s\n", __FUNCTION__);

	if (tfa_ext_handle_event == NULL)
		return;		//-EFAULT;

	event = tfa_ext_wait_event();
	if (trace_level > 0)
		pr_debug("%s, event:0x%04x\n", __FUNCTION__, event);

	if (event == TFADSP_CMD_READY) {
		tfa_ext_event_ack(1);
	}

	/* call the tfa driver here to handle this event */
	event_return = tfa_ext_handle_event(0, event);
	if (event_return <= 0)
		return;		//break;

	tfa_ext_event_ack(event_return);

	return;			//-EPIPE; /* let's say: Broken pipe */
}

static int __init tfa_ext_init(void)
{
#ifdef TFA98XX_API_REV_STR
	pr_info("tfa_ext_nl driver version %s\n", TFA98XX_API_REV_STR);
#endif

	tfa_ext_registry();	//TODO add error check

	/* setup work queue  */
	tfa_ext_wq = create_singlethread_workqueue("tfa_ext");
	if (!tfa_ext_wq)
		return -ENOMEM;
	INIT_WORK(&event_work, tfa_ext_event_loop);

	pr_info("%s\n", __FUNCTION__);
	return 0;

}

static void __exit tfa_ext_exit(void)
{
	/* make sure the work queue is not blocked on the wait queue */
	rcv_data_available = 1;
	wake_up_interruptible(&rcv_wait_queue);

	cancel_work_sync(&event_work);
	if (tfa_ext_wq)
		destroy_workqueue(tfa_ext_wq);
	tfa_netlink_exit();
	tfa_ext_unregister();
}

module_init(tfa_ext_init);
module_exit(tfa_ext_exit);

MODULE_DESCRIPTION("TFA98xx remote DSP driver");
MODULE_LICENSE("GPL");
