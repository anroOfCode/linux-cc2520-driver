#include <linux/types.h>
#include <linux/slab.h>
#include <linux/spinlock.h>
#include <linux/hrtimer.h>
#include <linux/list.h>

#include "unique.h"
#include "packet.h"
#include "cc2520.h"
#include "debug.h"

struct node_list{
	struct list_head list;
	u64 src;
	u8 dsn;
};

struct list_head nodes;

struct cc2520_interface *unique_top;
struct cc2520_interface *unique_bottom;

static int cc2520_unique_tx(u8 * buf, u8 len);
static void cc2520_unique_tx_done(u8 status);
static void cc2520_unique_rx_done(u8 *buf, u8 len);

int cc2520_unique_init()
{
	unique_top->tx = cc2520_unique_tx;
	unique_bottom->tx_done = cc2520_unique_tx_done;
	unique_bottom->rx_done = cc2520_unique_rx_done;

	INIT_LIST_HEAD(&nodes);
	return 0;
}

void cc2520_unique_free()
{
	struct node_list *tmp;
	struct list_head *pos, *q;

	list_for_each_safe(pos, q, &nodes){
		tmp = list_entry(pos, struct node_list, list);
		list_del(pos);
		kfree(tmp);
	}
}

static int cc2520_unique_tx(u8 * buf, u8 len)
{
	return unique_bottom->tx(buf, len);
}

static void cc2520_unique_tx_done(u8 status)
{
	unique_top->tx_done(status);
}

static void cc2520_unique_rx_done(u8 *buf, u8 len)
{
	struct node_list *tmp;
	u8 dsn;
	u64 src;
	bool found;
	bool drop;


	dsn = cc2520_packet_get_header(buf)->dsn;
	src = cc2520_packet_get_src(buf);

	found = false;
	drop = false;

	list_for_each_entry(tmp, &nodes, list) {
		if (tmp->src == src) {
			found = true;
			if (tmp->dsn != dsn) {
				tmp->dsn = dsn;
			}
			else {
				drop = true;
			}
			break;
		}
	}

	if (!found) {
		tmp = kmalloc(sizeof(struct node_list), GFP_ATOMIC);
		if (tmp) {
			tmp->dsn = dsn;
			tmp->src = src;
			list_add(&(tmp->list), &nodes);
			INFO((KERN_INFO "[cc2520] - unique found new mote: %lld\n", src));
		}
		else {
			INFO((KERN_INFO "[cc2520] - alloc failed.\n"));
		}
	}

	if (!drop)
		unique_top->rx_done(buf, len);
}
