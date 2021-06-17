#include <process.h>
#include <xrossfire/base.h>
#include <xrossfire/timeout.h>
#include <xrossfire/poll.h>

typedef struct xf_timeout_tree
{
	xf_timeout_t *root;
} xf_timeout_tree_t;

static int xf_timeout_compare(xf_timeout_handle_t a, xf_timeout_handle_t b);

#define XF_AVLTREE(name)	xf_timeout_tree_ ## name
#define XF_AVLTREE_T		xf_timeout_tree_t
#define XF_AVLTREE_NODE_T	xf_timeout_t
#define XF_AVLTREE_KEY_T	xf_timeout_handle_t
#define XF_AVLTREE_COMPARE(a, b) xf_timeout_compare(a, b)

#include "avltree.h"

static xf_timeout_tree_t tree;
static long long seq;

static void xf_timeout_poll_procedure(int method_id, void *args);

XROSSFIRE_PRIVATE xf_error_t xf_timeout_init()
{
	seq = 0;
	xf_timeout_tree_init(&tree);
	xf_poll_add(xf_timeout_poll_procedure);

	return 0;
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
	xf_timeout_procedure_t procedure, 
	void *context)
{
	self->left = NULL;
	self->right = NULL;
	self->height = 0;
	self->procedure = procedure;
	self->context = context;
	self->key.timestamp = xf_ticks() + timeout;

	xf_poll_enter();
	
	self->key.id = seq++;
	
	bool inserted;
	xf_timeout_tree_insert(&tree, self, /*out*/&inserted);
	
	xf_timeout_t *node = xf_timeout_tree_get_min(&tree);
	if (node == self) {
		xf_poll_wakeup();
	}
	
	xf_poll_leave();
	
	return 0;
}

XROSSFIRE_API void xf_timeout_cancel(xf_timeout_t *self)
{
	xf_poll_enter();
	
	xf_timeout_t *node;
	xf_timeout_tree_remove(&tree, self->key, /*out*/&node);
	
	xf_poll_leave();
}

static void xf_timeout_poll_procedure(int method_id, void *args)
{
	switch (method_id) {
	case XF_POLLING_GET_WAIT_TIMEOUT: 
		{
			Xf_poll_get_wait_timeout_args_t *a = (Xf_poll_get_wait_timeout_args_t *)args;
			
			long long timeout;
			xf_timeout_t *node;
			
			node = xf_timeout_tree_get_min(&tree);
			if (node == NULL) {
				timeout = 300 * 1000;
			} else {
				timeout = node->key.timestamp - xf_ticks();
			}
			
			a->timeout = timeout;
			
			return;
		}
	case XF_POLLING_PROCESS:
		{
			xf_poll_process_args_t *a = (xf_poll_process_args_t *)args;
			
			long long ticks = xf_ticks();
				
			xf_timeout_t *node = xf_timeout_tree_get_min(&tree);
			
			while (node->key.timestamp <= ticks) {
				xf_timeout_tree_remove_min(&tree);
				
				xf_poll_leave();
				
				node->procedure(node->context);
				
				xf_poll_enter();
				
				node = xf_timeout_tree_get_min(&tree);
			}
			
			return;
		}
	default:
		xf_abort();
	}
}
