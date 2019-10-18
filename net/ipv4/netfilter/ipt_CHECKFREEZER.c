#define pr_fmt(fmt) KBUILD_MODNAME ": " fmt
#include <linux/types.h>
#include <linux/module.h>
#include <linux/netfilter.h>
#include <linux/netfilter/x_tables.h>
#include <linux/freezer.h>
#include <linux/skbuff.h>

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Netfilter Core Team <coreteam@netfilter.org>");
MODULE_DESCRIPTION("Xtables: check freezer");

struct nf_check_freezer {
	unsigned int uid_start;
	unsigned int uid_end;
};

static unsigned int
check_freezer_by_uid(struct sk_buff *skb, const struct xt_action_param *par)
{
	struct nf_check_freezer *temp = (struct nf_check_freezer *)par->targinfo;

	if (cgroup_needunfreeze_uid(temp->uid_start))
		cgroup_network_unfreeze(temp->uid_start);
	return NF_ACCEPT;
}

static struct xt_target check_freezer_reg __read_mostly = {
	.name		= "CHECKFREEZER",
	.family		= NFPROTO_UNSPEC,
	.target		= check_freezer_by_uid,
	.targetsize	= sizeof(struct nf_check_freezer),
	.table		= "mangle",
	.hooks		= 1 << NF_INET_PRE_ROUTING
					| 1 << NF_INET_LOCAL_IN
					| 1 << NF_INET_POST_ROUTING,
	.me		= THIS_MODULE,
};

static int __init check_freezer_init(void)
{
	return xt_register_target(&check_freezer_reg);
}

static void __exit check_freezer_exit(void)
{
	xt_unregister_target(&check_freezer_reg);
}

module_init(check_freezer_init);
module_exit(check_freezer_exit);
