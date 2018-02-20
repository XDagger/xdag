/* red-black tree, T11.684-T13.285; $DVS:time$ */

#ifndef	LDUS_RBTREE_H_INCLUDED
#define LDUS_RBTREE_H_INCLUDED

#include <stdint.h>

struct ldus_rbtree {
	struct ldus_rbtree *left;
	struct ldus_rbtree *right;
};

static inline struct ldus_rbtree *_rbtree_ptr(struct ldus_rbtree *ptr) { return (struct ldus_rbtree *)((uintptr_t)ptr & ~(uintptr_t)1); }

static inline int _rbtree_color(struct ldus_rbtree *ptr) { return (uintptr_t)ptr & 1; }

static inline struct ldus_rbtree *_rbtree_set_color(struct ldus_rbtree *ptr, int color) { return (struct ldus_rbtree *)(((uintptr_t)ptr & ~(uintptr_t)1) | color); }

static inline void ldus_rbtree_init(struct ldus_rbtree **proot) { *proot = 0; }

static inline void ldus_rbtree_walk_up(struct ldus_rbtree *root, void (*callback)(struct ldus_rbtree *node)) {
    if (root) {
		root = _rbtree_ptr(root);
		if (root->left) ldus_rbtree_walk_up(root->left, callback);
		if (root->right) ldus_rbtree_walk_up(root->right, callback);
		callback(root);
    }
}

static inline void ldus_rbtree_walk_right(struct ldus_rbtree *root, void (*callback)(struct ldus_rbtree *node)) {
    while (root) {
		root = _rbtree_ptr(root);
		if (root->left) ldus_rbtree_walk_right(root->left, callback);
		callback(root);
		root = root->right;
    }
}

static inline void _rbtree_rotate_left(struct ldus_rbtree **proot) {
	struct ldus_rbtree *root = _rbtree_ptr(*proot), *node = _rbtree_ptr(root->right);
	int color = _rbtree_color(root->right);
	*proot = _rbtree_set_color(node, _rbtree_color(*proot));
	root->right = node->left;
	node->left = _rbtree_set_color(root, color);
}

static inline void _rbtree_rotate_right(struct ldus_rbtree **proot) {
	struct ldus_rbtree *root = _rbtree_ptr(*proot), *node = _rbtree_ptr(root->left);
	int color = _rbtree_color(root->left);
	*proot = _rbtree_set_color(node, _rbtree_color(*proot));
	root->left = node->right;
	node->right = _rbtree_set_color(root, color);
}

static inline int _rbtree_insert_balance_left(struct ldus_rbtree **proot, int res) {
	struct ldus_rbtree *root;
	if (res <= 0) return res;
	root = _rbtree_ptr(*proot);
	if (res == 1) {
		root->left = _rbtree_set_color(root->left, 1);
		if (!_rbtree_color(*proot)) return 0;
		return 2;
	} else {
		if (_rbtree_color(root->right)) {
			root->left = _rbtree_set_color(root->left, 0);
			root->right = _rbtree_set_color(root->right, 0);
			return 1;
		}
		if (res == 3) _rbtree_rotate_left(&root->left);
		_rbtree_rotate_right(proot);
		return 0;
	}
}

static inline int _rbtree_insert_balance_right(struct ldus_rbtree **proot, int res) {
	struct ldus_rbtree *root;
	if (res <= 0) return res;
	root = _rbtree_ptr(*proot);
	if (res == 1) {
		root->right = _rbtree_set_color(root->right, 1);
		if (!_rbtree_color(*proot)) return 0;
		return 3;
	} else {
		if (_rbtree_color(root->left)) {
			root->left = _rbtree_set_color(root->left, 0);
			root->right = _rbtree_set_color(root->right, 0);
			return 1;
		}
		if (res == 2) _rbtree_rotate_right(&root->right);
		_rbtree_rotate_left(proot);
		return 0;
	}
}

static inline int _rbtree_remove_balance_left(struct ldus_rbtree **proot, int res) {
	struct ldus_rbtree *root, *s;
	if (res <= 0) return res;
	root = _rbtree_ptr(*proot);
	if (_rbtree_color(root->right)) {
		_rbtree_rotate_left(proot);
		proot = &_rbtree_ptr(*proot)->left;
	}
	s = _rbtree_ptr(root->right);
	if (!_rbtree_color(s->right)) {
		if (!_rbtree_color(s->left)) {
			root->right = _rbtree_set_color(s, 1);
			if (!_rbtree_color(*proot)) return 1;
			*proot = _rbtree_set_color(root, 0);
			return 0;
		}
		_rbtree_rotate_right(&root->right);
		s = _rbtree_ptr(root->right);
	}
	_rbtree_rotate_left(proot);
	s->right = _rbtree_set_color(s->right, 0);
	return 0;
}

static inline int _rbtree_remove_balance_right(struct ldus_rbtree **proot, int res) {
	struct ldus_rbtree *root, *s;
	if (res <= 0) return res;
	root = _rbtree_ptr(*proot);
	if (_rbtree_color(root->left)) {
		_rbtree_rotate_right(proot);
		proot = &_rbtree_ptr(*proot)->right;
	}
	s = _rbtree_ptr(root->left);
	if (!_rbtree_color(s->left)) {
		if (!_rbtree_color(s->right)) {
			root->left = _rbtree_set_color(s, 1);
			if (!_rbtree_color(*proot)) return 1;
			*proot = _rbtree_set_color(root, 0);
			return 0;
		}
		_rbtree_rotate_left(&root->left);
		s = _rbtree_ptr(root->left);
	}
	_rbtree_rotate_right(proot);
	s->left = _rbtree_set_color(s->left, 0);
	return 0;
}

static inline int _rbtree_remove_left(struct ldus_rbtree **proot, struct ldus_rbtree **pnode) {
	struct ldus_rbtree *root = _rbtree_ptr(*proot), *node = _rbtree_ptr(*pnode);
	if (root->right) {
		int res = _rbtree_remove_left(&root->right, pnode);
		if (proot == &node->left) proot = &_rbtree_ptr(*pnode)->left;
		return _rbtree_remove_balance_right(proot, res);
	} else {
		int color = _rbtree_color(*proot);
		*proot = root->left;
		root->left = node->left;
		root->right = node->right;
		*pnode = _rbtree_set_color(root, _rbtree_color(*pnode));
		if (color) return 0;
		if (_rbtree_color(*proot)) {
			if (*proot == root->left) proot = &root->left;
			*proot = _rbtree_set_color(*proot, 0);
			return 0;
		}
		return 1;
	}
}

static inline int _rbtree_remove_right(struct ldus_rbtree **proot, struct ldus_rbtree **pnode) {
	struct ldus_rbtree *root = _rbtree_ptr(*proot), *node = _rbtree_ptr(*pnode);
	if (root->left) {
		int res = _rbtree_remove_right(&root->left, pnode);
		if (proot == &node->right) proot = &_rbtree_ptr(*pnode)->right;
		return _rbtree_remove_balance_left(proot, res);
	} else {
		int color = _rbtree_color(*proot);
		*proot = root->right;
		root->left = node->left;
		root->right = node->right;
		*pnode = _rbtree_set_color(root, _rbtree_color(*pnode));
		if (color) return 0;
		if (_rbtree_color(*proot)) {
			if (*proot == root->right) proot = &root->right;
			*proot = _rbtree_set_color(*proot, 0);
			return 0;
		}
		return 1;
	}
}

#define ldus_rbtree_define_prefix(lessthan, prefix, postfix) \
   \
prefix int ldus_rbtree_insert(struct ldus_rbtree **proot, struct ldus_rbtree *node) { \
	struct ldus_rbtree *root = _rbtree_ptr(*proot); \
	if (!root) { *proot = node, node->left = node->right = 0; return 1; } \
	if (lessthan(node, root)) return _rbtree_insert_balance_left (proot, ldus_rbtree_insert(&root->left,  node)); \
	if (lessthan(root, node)) return _rbtree_insert_balance_right(proot, ldus_rbtree_insert(&root->right, node)); \
	return -1; \
} \
    \
prefix struct ldus_rbtree *ldus_rbtree_find(struct ldus_rbtree *root, struct ldus_rbtree *node) postfix { \
	root = _rbtree_ptr(root); \
	if (!root) return 0; \
	if (lessthan(node, root)) return ldus_rbtree_find(root->left,  node); \
	if (lessthan(root, node)) return ldus_rbtree_find(root->right, node); \
	return root; \
} \
    \
prefix int ldus_rbtree_remove(struct ldus_rbtree **proot, struct ldus_rbtree *node) { \
	struct ldus_rbtree *root = _rbtree_ptr(*proot); \
	if (!root) return -1; \
	if (lessthan(node, root)) return _rbtree_remove_balance_left (proot,  ldus_rbtree_remove (&root->left,  node )); \
	if (lessthan(root, node)) return _rbtree_remove_balance_right(proot,  ldus_rbtree_remove (&root->right, node )); \
	if (root->left )          return _rbtree_remove_balance_left (proot, _rbtree_remove_left (&root->left,  proot)); \
	if (root->right)          return _rbtree_remove_balance_right(proot, _rbtree_remove_right(&root->right, proot)); \
	if (_rbtree_color(*proot)) { *proot = 0; return 0; } \
	*proot = 0; return 1; \
}

#endif
