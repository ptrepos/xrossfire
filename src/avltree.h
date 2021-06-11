static int XF_AVLTREE(get_height_)(XF_AVLTREE_NODE_T *node)
{
	return node == NULL ? 0 : node->height;
}

static int XF_AVLTREE(max_)(int a, int b)
{
	return a < b ? b : a;
}

static int XF_AVLTREE(refresh_)(XF_AVLTREE_NODE_T *node)
{
	node->height = XF_AVLTREE(max_)(XF_AVLTREE(get_height_)(node->left), XF_AVLTREE(get_height_)(node->right)) + 1;
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(rotate_left_)(XF_AVLTREE_NODE_T *a)
{
	XF_AVLTREE_NODE_T *c = a->right;
	XF_AVLTREE_NODE_T *d = c->left;
	c->left = a;
	a->right = d;
	
	XF_AVLTREE(refresh_)(a);
	//XF_AVLTREE(refresh_)(c);
	c->height = a->height + 1;
	return c;
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(rotate_right_)(XF_AVLTREE_NODE_T *a)
{
	XF_AVLTREE_NODE_T *b = a->left;
	XF_AVLTREE_NODE_T *d = b->right;
	
	b->right = a;
	a->left = d;
	
	XF_AVLTREE(refresh_)(a);
	//XF_AVLTREE(refresh_)(b);
	b->height = a->height + 1;
	
	return b;
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(rotate_left_right_)(XF_AVLTREE_NODE_T *node)
{
	node->left = XF_AVLTREE(rotate_left_)(node->left);
	return XF_AVLTREE(rotate_right_)(node);
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(rotate_right_left_)(XF_AVLTREE_NODE_T *node)
{
	node->right = XF_AVLTREE(rotate_right_)(node->right);
	return XF_AVLTREE(rotate_left_)(node);
}

static int XF_AVLTREE(bias_)(XF_AVLTREE_NODE_T *node)
{
	if (node == NULL)
		return 0;
	return XF_AVLTREE(get_height_)(node->left) - XF_AVLTREE(get_height_)(node->right);
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(balance_left_)(XF_AVLTREE_NODE_T *node, /*out*/bool *changed)
{
	int height = node->height;
	
	XF_AVLTREE(refresh_)(node);
	
	if (XF_AVLTREE(bias_)(node) >= 2) {
		if (XF_AVLTREE(bias_)(node->left) >= 0)
			node = XF_AVLTREE(rotate_right_)(node);
		else
			node = XF_AVLTREE(rotate_left_right_)(node);
	} else {
		*changed = node->height != height;
	}
	
	return node;
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(balance_right_)(XF_AVLTREE_NODE_T *node, /*out*/bool *changed)
{
	int height = node->height;

	XF_AVLTREE(refresh_)(node);
	
	if (XF_AVLTREE(bias_)(node) <= -2) {
		if (XF_AVLTREE(bias_)(node->right) <= 0)
			node = XF_AVLTREE(rotate_left_)(node);
		else
			node = XF_AVLTREE(rotate_right_left_)(node);
	} else {
		*changed = node->height != height;
	}
	
	return node;
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(insert_node_)(XF_AVLTREE_T *self, XF_AVLTREE_NODE_T *node, XF_AVLTREE_NODE_T *new_node, /*out*/bool *inserted, /*out*/bool *changed)
{
	if (node == NULL) {
		*inserted = true;
		*changed = true;
		new_node->height = 1;
		return new_node;
	}
	
	int ret = self->compare(new_node, node);
	if(ret < 0) {
		XF_AVLTREE_NODE_T *left = XF_AVLTREE(insert_node_)(self, node->left, new_node, /*out*/inserted, /*out*/changed);
		if (*changed) {
			node->left = left;
			return XF_AVLTREE(balance_left_)(node, /*out*/changed);
		} else {
			return node;
		}
	} else if (ret > 0){
		XF_AVLTREE_NODE_T *right = XF_AVLTREE(insert_node_)(self, node->right, new_node, /*out*/inserted, /*out*/changed);
		if (*inserted) {
			node->right = right;
			return XF_AVLTREE(balance_right_)(node, /*out*/changed);
		} else {
			return node;
		}
	} else {
		*inserted = false;
		*changed = false;
		return node;
	}
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(pop_min_)(XF_AVLTREE_NODE_T *node, /*out*/XF_AVLTREE_NODE_T **min, /*out*/bool *changed)
{
	if (node->left == NULL) {
		*min = node;
		*changed = true;
		return node->right;
	} else {
		XF_AVLTREE_NODE_T *left = XF_AVLTREE(pop_min_)(node->left, /*out*/min, /*out*/changed);
		if (*changed) {
			node->left = left;
			return XF_AVLTREE(balance_right_)(node, /*out*/changed);
		}
		
		return node;
	}
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(pop_max_)(XF_AVLTREE_NODE_T *node, /*out*/XF_AVLTREE_NODE_T **max, /*out*/bool *changed)
{
	if (node->right == NULL) {
		*max = node;
		*changed = true;
		return node->left;
	} else {
		XF_AVLTREE_NODE_T *right = XF_AVLTREE(pop_max_)(node->right, /*out*/max, /*out*/changed);
		if (*changed) {
			node->right = right;
			return XF_AVLTREE(balance_left_)(node, /*out*/changed);
		}
		
		return node;
	}
}


static XF_AVLTREE_NODE_T *XF_AVLTREE(remove_node_)(XF_AVLTREE_T *self, XF_AVLTREE_NODE_T *node, XF_AVLTREE_NODE_T *del_node, /*out*/bool *removed)
{
	bool changed;
	if (node == NULL) {
		*removed = false;
		return NULL;
	}
	
	int ret = self->compare(del_node, node);
	if(ret < 0) {
		node->left = XF_AVLTREE(remove_node_)(self, node->left, del_node, /*out*/removed);
		
		if (*removed) {
			return XF_AVLTREE(balance_right_)(node, &changed);
		} else {
			return node;
		}
	} else if (ret > 0) {
		node->right = XF_AVLTREE(remove_node_)(self, node->right, del_node, /*out*/removed);
		
		if (*removed) {
			return XF_AVLTREE(balance_left_)(node, /*out*/&changed);
		} else {
			return node;
		}
	} else {
		if (node == del_node) {
			*removed = true;
			
			if (node->left == NULL)
				return node->right;
			
			XF_AVLTREE_NODE_T *promotion;
			XF_AVLTREE_NODE_T *left = XF_AVLTREE(pop_max_)(node->left, /*out*/&promotion, &changed);
			
			promotion->left = left;
			promotion->right = node->right;
			
			return XF_AVLTREE(balance_right_)(promotion, /*out*/&changed);
		} else {
			*removed = false;
			return node;
		}
	}
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(remove_key_)(XF_AVLTREE_T *self, XF_AVLTREE_NODE_T *node, XF_AVLTREE_KEY_T key, /*out*/XF_AVLTREE_NODE_T **removed)
{
	bool changed;
	if (node == NULL) {
		*removed = NULL;
		return NULL;
	}
	
	int ret = XF_AVLTREE_COMPARE(key, node->key);
	if(ret < 0) {
		node->left = XF_AVLTREE(remove_key_)(self, node->left, key, /*out*/removed);
		
		if (*removed) {
			return XF_AVLTREE(balance_right_)(node, &changed);
		} else {
			return node;
		}
	} else if (ret > 0) {
		node->right = XF_AVLTREE(remove_key_)(self, node->right, key, /*out*/removed);
		
		if (*removed) {
			return XF_AVLTREE(balance_left_)(node, /*out*/&changed);
		} else {
			return node;
		}
	} else {
		*removed = node;
		
		if (node->left == NULL)
			return node->right;
		
		XF_AVLTREE_NODE_T *promotion;
		XF_AVLTREE_NODE_T *left = XF_AVLTREE(pop_max_)(node->left, /*out*/&promotion, &changed);
		
		promotion->left = left;
		promotion->right = node->right;
		
		return XF_AVLTREE(balance_right_)(promotion, /*out*/&changed);
	}
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(get_min_)(XF_AVLTREE_NODE_T *node)
{
	if (node == NULL)
		return NULL;
		
	for (;;) {
		if (node->left == NULL)
			return node;
		node = node->left;
	}
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(get_max_)(XF_AVLTREE_NODE_T *node)
{
	if (node == NULL)
		return NULL;
		
	for (;;) {
		if (node->right == NULL)
			return node;
		node = node->right;
	}
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(remove_min_)(XF_AVLTREE_NODE_T *node, /*out*/bool *changed)
{
	if (node->left == NULL) {
		*changed = true;
		return node->right;
	}
	
	XF_AVLTREE_NODE_T *left = XF_AVLTREE(remove_min_)(node->left, /*out*/changed);
	if (*changed) {
		node->left = left;
		return XF_AVLTREE(balance_right_)(node, /*out*/changed);
	}
	
	return node;
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(remove_max_)(XF_AVLTREE_NODE_T *node, /*out*/bool *changed)
{
	if (node->right == NULL) {
		*changed = true;
		return node->left;
	}
	
	XF_AVLTREE_NODE_T *right = XF_AVLTREE(remove_max_)(node->right, /*out*/changed);
	if (*changed) {
		node->right = right;
		return XF_AVLTREE(balance_left_)(node, /*out*/changed);
	}
	
	return node;
}

static int XF_AVLTREE(count_)(XF_AVLTREE_NODE_T *node)
{
	if (node == NULL)
		return 0;
	
	return XF_AVLTREE(count_)(node->left) + XF_AVLTREE(count_)(node->right) + 1;
}

static int XF_AVLTREE(count_height_)(XF_AVLTREE_NODE_T *node)
{
	if (node == NULL)
		return 0;
	
	return XF_AVLTREE(max_)(XF_AVLTREE(count_height_)(node->left), XF_AVLTREE(count_height_)(node->right)) + 1;
}

static bool XF_AVLTREE(balanced_)(XF_AVLTREE_NODE_T *node)
{
	if (node == NULL)
		return true;
	
	int bias = XF_AVLTREE(bias_)(node);
	if (!(-1 <= bias && bias <= 1))
		return false;
	
	return XF_AVLTREE(balanced_)(node->left) && XF_AVLTREE(balanced_)(node->right);
}

static bool XF_AVLTREE(check_)(XF_AVLTREE_NODE_T *node, avltree_compare_node_t compare)
{
	if (node == NULL)
		return true;
	
	if (node->left != NULL) {
		if (!(compare(node->left, node) < 0)) {
			return false;
		}
	}
	
	if (node->right != NULL) {
		if (!(compare(node, node->right) <= 0)) {
			return false;
		}
	}
	
	if (XF_AVLTREE(count_height_)(node) != node->height) {
		return false;
	}
	
	return XF_AVLTREE(check_)(node->left, compare) && XF_AVLTREE(check_)(node->right, compare);
}

static XF_AVLTREE_NODE_T *XF_AVLTREE(conditional_pop_min_sub_)(
		XF_AVLTREE_NODE_T *node,
		bool (*condition)(XF_AVLTREE_NODE_T *node, void *context),
		void *context,
		/*out*/XF_AVLTREE_NODE_T **min, 
		/*out*/bool *changed)
{
	if (node->left == NULL) {
		if (condition(node, context)) {
			*min = node;
			*changed = true;
			return node->right;
		} else {
			*min = NULL;
			*changed = false;
			return node;
		}
	} else {
		XF_AVLTREE_NODE_T *left = XF_AVLTREE(conditional_pop_min_sub_)(node->left, condition, context, /*out*/min, /*out*/changed);
		if (*changed) {
			node->left = left;
			return XF_AVLTREE(balance_right_)(node, /*out*/changed);
		}
		
		return node;
	}
}

void XF_AVLTREE(init)(XF_AVLTREE_T *self, avltree_compare_node_t compare)
{
	self->root = NULL;
}

void XF_AVLTREE(insert)(XF_AVLTREE_T *self, XF_AVLTREE_NODE_T *node, /*out*/bool *inserted)
{
	bool changed;
	
	self->root = XF_AVLTREE(insert_node_)(self, self->root, node, /*out*/inserted, /*out*/&changed);
}

void XF_AVLTREE(remove)(XF_AVLTREE_T *self, XF_AVLTREE_KEY_T key, /*out*/XF_AVLTREE_NODE_T **removed)
{
	bool changed;
	
	self->root = XF_AVLTREE(remove_key_)(self, self->root, key, /*out*/removed);
}

XF_AVLTREE_NODE_T *XF_AVLTREE(get_min)(XF_AVLTREE_T *self)
{
	return XF_AVLTREE(get_min_)(self->root);
}

XF_AVLTREE_NODE_T *XF_AVLTREE(get_max)(XF_AVLTREE_T *self)
{
	return XF_AVLTREE(get_max_)(self->root);
}

void XF_AVLTREE(remove_min)(XF_AVLTREE_T *self)
{
	bool changed;
	self->root = XF_AVLTREE(remove_min_)(self->root, /*out*/&changed);
}

void XF_AVLTREE(remove_max)(XF_AVLTREE_T *self)
{
	bool changed;
	self->root = XF_AVLTREE(remove_max_)(self->root, /*out*/&changed);
}

void XF_AVLTREE(pop_min)((XF_AVLTREE_T *self, /*out*/XF_AVLTREE_NODE_T **min)
{
	bool changed;
	if (self->root == NULL) {
		*min = NULL;
		return;
	}
	self->root = XF_AVLTREE(pop_min_)(self->root, /*out*/min, &changed);
}

void XF_AVLTREE(pop_max)(XF_AVLTREE_T *self, /*out*/XF_AVLTREE_NODE_T **max)
{
	bool changed;
	if (self->root == NULL) {
		*max = NULL;
		return;
	}
	self->root = XF_AVLTREE(pop_max_)(self->root, /*out*/max, &changed);
}

void XF_AVLTREE(conditional_pop_min)(
		XF_AVLTREE_T *self,
		bool (*condition)(XF_AVLTREE_NODE_T *node, void *context),
		void *context,
		/*out*/XF_AVLTREE_NODE_T **popped)
{
	bool changed;
	self->root = XF_AVLTREE(conditional_pop_min_)(self->root, condition, context, /*out*/popped, &changed);
}

int XF_AVLTREE(count)(XF_AVLTREE_T *self)
{
	return XF_AVLTREE(count_)(self->root);
}

int XF_AVLTREE(count_height)(XF_AVLTREE_T *self)
{
	return XF_AVLTREE(count_height_)(self->root);
}

bool XF_AVLTREE(balanced)(XF_AVLTREE_T *self)
{
	return XF_AVLTREE(balanced_)(self->root);
}

bool XF_AVLTREE(check)(XF_AVLTREE_T *self)
{
	return XF_AVLTREE(check_)(self->root, self->compare);
}
