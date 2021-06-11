#include <xrossfire/base.h>
#include <xrossfire/timeout.h>

typedef struct xf_timeout_tree
{
	xf_timeout_node_t *root;
} xf_timeout_tree_t;

#define XF_AVLTREE(name)	xf_timeout_tree_ ## name
#define XF_AVLTREE_T		xf_timeout_tree_t
#define XF_AVLTREE_NODE_T	xf_timeout_t
#define XF_AVLTREE_COMPARE(a, b) xf_timeout_compare(a, b)

#include "avltree.h"

static xf_monitor_t lock;
static xf_timeout_tree_t tree;
static long long seq;

static void timeout_loop(void *param);

XF_PRIVATE xf_error_t xf_timeout_init()
{
	xf_error_t err;
	
	xf_monitor_init(&lock);
		
	seq = 0;
	
	err = xf_timeout_tree_init(&tree);
	if (err != 0)
		goto _ERROR;
	
	err = xf_thread_start(timeout_loop, NULL);
	if (err != 0)
		goto _ERROR;

	return 0;
_ERROR:
	return err;
}

static int xf_timeout_compare(xf_timeout_handle_t a, xf_timeout_handle_t b)
{
	if (a.timestamp != b.timestamp)
		return a.timestamp < b.timestamp ? -1 : 1;
	if (a.id != b.id)
		return a.id < b.id ? -1 : 1;
	return 0;
}

XROSSFIRE_API xf_error_t xf_timeout_schedule(
	xf_timeout_t *self,
	int timeout, 
	xf_timeout_procedure_t *procedure, 
	void *context)
{
	self->left = NULL;
	self->right = NULL;
	self->height = 0;
	self->procedure = procedure;
	self->context = context;
	self->key.timestamp = xf_ticks() + timeout;

	xf_monitor_enter(&lock);
	
	self->key.id = seq++;
	
	xf_timeout_tree_insert(tree, self);
	
	xf_timeout_t *node = xf_timeout_tree_get_min(&tree);
	if (node == self) {
		xf_monitor_notify(&lock);
	}
	
	xf_monitor_leave(&lock);
	
	return 0;
_ERROR:
	return err;
}

XROSSFIRE_API xf_error_t xf_timeout_cancel(xf_timeout_t *self)
{
	xf_monitor_enter(&lock);
	
	xf_timeout_t *node;
	xf_timeout_tree_remove(&tree, handle->key, /*out*/&node);
	
	xf_monitor_leave(&lock);
}

static void timeout_loop(void *param)
{
	xf_monitor_enter(&lock);

	for (;;) {
		long long ticks = xf_ticks();
		
		xf_timeout_t *node = xf_timeout_tree_get_min(&tree, &node);
		if (node == NULL || ticks < node->key.timestamp) {
			xf_monitor_wait(&lock, node == NULL ? 60000 : (node->key.timestamp - ticks));
		} else {
			node = xf_timeout_queue_get_min(&tree);
			while (node->key.timestamp <= ticks) {
				xf_timeout_tree_remove_min(&tree);
				
				xf_monitor_leave(&lock);
				
				node->procedure(node->context);
				
				xf_monitor_enter(&lock);
			}
		}
	}
	
	xf_monitor_leave(&lock);
}
