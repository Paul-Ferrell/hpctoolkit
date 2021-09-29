#include "cupti-ip-norm-map.h"

#include <lib/prof-lean/splay-macros.h>
#include <hpcrun/memory/hpcrun-malloc.h>

//******************************************************************************
// type declarations
//******************************************************************************

typedef struct splay_ip_norm_node_t {
  struct splay_ip_norm_node_t *left;
  struct splay_ip_norm_node_t *right;
  ip_normalized_t key;
} splay_ip_norm_node_t;


typedef enum splay_order_t {
  splay_inorder = 1,
  splay_allorder = 2
} splay_order_t;


typedef enum splay_visit_t {
  splay_preorder_visit = 1,
  splay_inorder_visit = 2,
  splay_postorder_visit = 3
} splay_visit_t;


typedef void (*splay_fn_t)
(
 splay_ip_norm_node_t *node,
 splay_visit_t order,
 void *arg
);


static bool ip_norm_cmp_lt(ip_normalized_t left, ip_normalized_t right);
static bool ip_norm_cmp_gt(ip_normalized_t left, ip_normalized_t right);

#define ip_norm_builtin_lt(a, b) (ip_norm_cmp_lt(a, b))
#define ip_norm_builtin_gt(a, b) (ip_norm_cmp_gt(a, b))

#define IP_NORM_SPLAY_TREE(type, root, key, value, left, right)	\
  GENERAL_SPLAY_TREE(type, root, key, value, value, left, right, ip_norm_builtin_lt, ip_norm_builtin_gt)

//*****************************************************************************
// macros 
//*****************************************************************************

#define splay_macro_body_ignore(x) ;
#define splay_macro_body_show(x) x

// declare typed interface functions 
#define typed_splay_declare(type)		\
  typed_splay(type, splay_macro_body_ignore)

// implementation of typed interface functions 
#define typed_splay_impl(type)			\
  typed_splay(type, splay_macro_body_show)

// routine name for a splay operation
#define splay_op(op) \
  splay_ip_norm ## _ ## op

#define typed_splay_node(type) \
  type ## _ ## splay_ip_norm_node_t

// routine name for a typed splay operation
#define typed_splay_op(type, op) \
  type ## _  ## op

// insert routine name for a typed splay 
#define typed_splay_splay(type) \
  typed_splay_op(type, splay)

// insert routine name for a typed splay 
#define typed_splay_insert(type) \
  typed_splay_op(type, insert)

// lookup routine name for a typed splay 
#define typed_splay_lookup(type) \
  typed_splay_op(type, lookup)

// delete routine name for a typed splay 
#define typed_splay_delete(type) \
  typed_splay_op(type, delete)

// forall routine name for a typed splay 
#define typed_splay_forall(type) \
  typed_splay_op(type, forall)

// count routine name for a typed splay 
#define typed_splay_count(type) \
  typed_splay_op(type, count)

// define typed wrappers for a splay type 
#define typed_splay(type, macro) \
  static bool \
  typed_splay_insert(type) \
  (typed_splay_node(type) **root, typed_splay_node(type) *node)	\
  macro({ \
    return splay_op(insert)((splay_ip_norm_node_t **) root, (splay_ip_norm_node_t *) node); \
  }) \
\
  static typed_splay_node(type) * \
  typed_splay_lookup(type) \
  (typed_splay_node(type) **root, ip_normalized_t key) \
  macro({ \
    typed_splay_node(type) *result = (typed_splay_node(type) *) \
      splay_op(lookup)((splay_ip_norm_node_t **) root, key); \
    return result; \
  }) \
\
  static void \
  typed_splay_forall(type) \
  (typed_splay_node(type) *root, splay_order_t order, void (*fn)(typed_splay_node(type) *, splay_visit_t visit, void *), void *arg) \
  macro({ \
    splay_op(forall)((splay_ip_norm_node_t *) root, order, (splay_fn_t) fn, arg); \
  }) 

#define typed_splay_alloc(free_list, splay_node_type)		\
  (splay_node_type *) splay_ip_norm_alloc_helper		\
  ((splay_ip_norm_node_t **) free_list, sizeof(splay_node_type))	

#define typed_splay_free(free_list, node)			\
  splay_ip_norm_free_helper					\
  ((splay_ip_norm_node_t **) free_list,				\
   (splay_ip_norm_node_t *) node)

//******************************************************************************
// interface operations
//******************************************************************************

static bool
ip_norm_cmp_gt
(
 ip_normalized_t left,
 ip_normalized_t right
)
{
  if (left.lm_id > right.lm_id) {
    return true;
  } else if (left.lm_id == right.lm_id) {
    if (left.lm_ip > right.lm_ip) {
      return true;
    } 
  }
  return false;
}


static bool
ip_norm_cmp_lt
(
 ip_normalized_t left,
 ip_normalized_t right
)
{
  if (left.lm_id < right.lm_id) {
    return true;
  } else if (left.lm_id == right.lm_id) {
    if (left.lm_ip < right.lm_ip) {
      return true;
    }
  }
  return false;
}


static bool
ip_norm_cmp_eq
(
 ip_normalized_t left,
 ip_normalized_t right
)
{
  return left.lm_id == right.lm_id && left.lm_ip && right.lm_ip;
}


static splay_ip_norm_node_t *
splay_ip_norm_splay
(
 splay_ip_norm_node_t *root,
 ip_normalized_t splay_key
)
{
  IP_NORM_SPLAY_TREE(splay_ip_norm_node_t, root, splay_key, key, left, right);

  return root;
}


static bool
splay_ip_norm_insert
(
 splay_ip_norm_node_t **root,
 splay_ip_norm_node_t *node
)
{
  node->left = node->right = NULL;

  if (*root != NULL) {
    *root = splay_ip_norm_splay(*root, node->key);
    
    if (ip_norm_cmp_lt(node->key, (*root)->key)) {
      node->left = (*root)->left;
      node->right = *root;
      (*root)->left = NULL;
    } else if (ip_norm_cmp_gt(node->key, (*root)->key)) {
      node->left = *root;
      node->right = (*root)->right;
      (*root)->right = NULL;
    } else {
      // key already present in the tree
      return true; // insert failed 
    }
  } 

  *root = node;

  return true; // insert succeeded 
}


static splay_ip_norm_node_t *
splay_ip_norm_lookup
(
 splay_ip_norm_node_t **root,
 ip_normalized_t key
)
{
  splay_ip_norm_node_t *result = 0;

  *root = splay_ip_norm_splay(*root, key);

  if (*root && ip_norm_cmp_eq((*root)->key, key)) {
    result = *root;
  }

  return result;
}


static void
splay_forall_allorder
(
 splay_ip_norm_node_t *node,
 splay_fn_t fn,
 void *arg
)
{
  if (node) {
    fn(node, splay_preorder_visit, arg);
    splay_forall_allorder(node->left, fn, arg);
    fn(node, splay_inorder_visit, arg);
    splay_forall_allorder(node->right, fn, arg);
    fn(node, splay_postorder_visit, arg);
  }
}


static void
splay_ip_norm_forall
(
 splay_ip_norm_node_t *root,
 splay_order_t order,
 splay_fn_t fn,
 void *arg
)
{
  switch (order) {
    case splay_allorder: 
      splay_forall_allorder(root, fn, arg);
      break;
    default:
      break;
  }
}


static splay_ip_norm_node_t *
splay_ip_norm_alloc_helper
(
 splay_ip_norm_node_t **free_list, 
 size_t size
)
{
  splay_ip_norm_node_t *first = *free_list; 

  if (first) { 
    *free_list = first->left;
  } else {
    first = (splay_ip_norm_node_t *) hpcrun_malloc_safe(size);
  }

  memset(first, 0, size); 

  return first;
}


static void
splay_ip_norm_free_helper
(
 splay_ip_norm_node_t **free_list, 
 splay_ip_norm_node_t *e 
)
{
  e->left = *free_list;
  *free_list = e;
}


//******************************************************************************
// macros
//******************************************************************************

#define st_insert				\
  typed_splay_insert(ip_norm)

#define st_lookup				\
  typed_splay_lookup(ip_norm)

#define st_delete				\
  typed_splay_delete(ip_norm)

#define st_forall				\
  typed_splay_forall(ip_norm)

#define st_count				\
  typed_splay_count(ip_norm)

#define st_alloc(free_list)			\
  typed_splay_alloc(free_list, typed_splay_node(ip_norm))

#define st_free(free_list, node)		\
  typed_splay_free(free_list, node)

#undef typed_splay_node
#define typed_splay_node(ip_norm) cupti_ip_norm_map_entry_t 

struct cupti_ip_norm_map_entry_s {
  struct cupti_ip_norm_map_entry_s *left;
  struct cupti_ip_norm_map_entry_s *right;
  ip_normalized_t ip_norm;
  cct_node_t *cct;
};

static cupti_ip_norm_map_entry_t *map_root = NULL;
static cupti_ip_norm_map_entry_t *free_list = NULL;


typed_splay_impl(ip_norm)


static void
flush_fn_helper
(
 cupti_ip_norm_map_entry_t *entry,
 splay_visit_t visit_type,
 void *args
)
{
  if (visit_type == splay_postorder_visit) {
    st_free(&free_list, entry);
  }
}


cupti_ip_norm_map_ret_t
cupti_ip_norm_map_lookup
(
 cct_node_t *cct
)
{
  cct_addr_t *addr = hpcrun_cct_addr(cct);
  ip_normalized_t ip_norm = addr->ip_norm;
  cupti_ip_norm_map_entry_t *result = st_lookup(&map_root, ip_norm);

  if (result == NULL) {
    return CUPTI_IP_NORM_MAP_NOT_EXIST;
  } else if (result->cct != cct) {
    return CUPTI_IP_NORM_MAP_DUPLICATE;
  } else {
    return CUPTI_IP_NORM_MAP_EXIST;
  }
}


void
cupti_ip_norm_map_insert
(
 cct_node_t *cct
)
{
  cct_addr_t *addr = hpcrun_cct_addr(cct);
  ip_normalized_t ip_norm = addr->ip_norm;
  cupti_ip_norm_map_entry_t *entry = st_lookup(&map_root, ip_norm);

  if (entry == NULL) {
    entry = st_alloc(&free_list);
    entry->ip_norm = ip_norm;
    entry->cct = cct;
    st_insert(&map_root, entry);
  }
}


void
cupti_ip_norm_map_clear
(
)
{
  st_forall(map_root, splay_allorder, flush_fn_helper, NULL);
  map_root = NULL;
}