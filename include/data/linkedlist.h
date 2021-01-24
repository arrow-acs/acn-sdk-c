/* Copyright (c) 2017 Arrow Electronics, Inc.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Apache License 2.0
 * which accompanies this distribution, and is available at
 * http://apache.org/licenses/LICENSE-2.0
 * Contributors: Arrow Electronics, Inc.
 */

#if !defined(KONEXIOS_LINKED_LIST_H_)
#define KONEXIOS_LINKED_LIST_H_

#if defined(__cplusplus)
extern "C" {
#endif

typedef struct _konexios_linked_list_ {
  struct _konexios_linked_list_ *next;
} konexios_linked_list_t;

#include <stddef.h>
#if !defined(offsetof)
#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
#endif
#if defined(_GCC_EXT_)
#define container_of(ptr, type, member) ({                   \
        const typeof( ((type *)0)->member ) *__mptr = (ptr); \
        (type *)( (char *)__mptr - offsetof(type,member) );})
#else
#define container_of(ptr, type, member) ((type *)((char *)(1 ? (ptr) : &((type *)0)->member) - offsetof(type, member)))
#endif

#define CONCAT_(x,y) x##y
#define CONCAT(x,y) CONCAT_(x,y)
#define UN(base) CONCAT(base, __LINE__)

#define konexios_linked_list_for_each(p, root, type) \
  konexios_linked_list_t *UN(base_p) = NULL; \
  for ( UN(base_p) = (root)?&(root)->node:NULL, \
    (p) = (root) ; \
    UN(base_p) != NULL ; \
    UN(base_p) = UN(base_p)->next, \
    (p) = UN(base_p)?container_of(UN(base_p), type, node):(p) )
      
#define konexios_linked_list_for_each_safe(p, root, type) \
  konexios_linked_list_t *UN(base_p) = NULL; \
  konexios_linked_list_t UN(base_p_obj) = { NULL }; \
  for ( UN(base_p) = (root)?&(root)->node:NULL, \
    (p) = (root), \
    UN(base_p_obj) = UN(base_p)?*UN(base_p):UN(base_p_obj) ; \
    UN(base_p) != NULL ; \
    UN(base_p) = UN(base_p_obj).next, \
    (p) = UN(base_p)?container_of(UN(base_p), type, node):NULL, UN(base_p_obj) = (UN(base_p)?(*UN(base_p)):UN(base_p_obj)) )

#define linked_list_find_node(p, root, type, func, data) { \
      konexios_linked_list_for_each(p, root, type) { \
          if ( (func)((p), (data)) == 0 ) break; \
          (p) = NULL; \
      } \
}

konexios_linked_list_t *konexios_linked_list_add(konexios_linked_list_t *root, konexios_linked_list_t *el);
konexios_linked_list_t *konexios_linked_list_add_first(konexios_linked_list_t *root, konexios_linked_list_t *el);
konexios_linked_list_t *konexios_linked_list_del(konexios_linked_list_t *root, konexios_linked_list_t *el);
konexios_linked_list_t *konexios_linked_list_del_last(konexios_linked_list_t *root);

#define konexios_linked_list_head_node konexios_linked_list_t node

#define konexios_linked_list_init(ptr) memset((void *)&(ptr)->node, 0x0, sizeof(konexios_linked_list_t))

#define konexios_linked_list_add_node_last(root, type, el) { \
  konexios_linked_list_t *base_p = konexios_linked_list_add((root)?&(root)->node:NULL, &(el)->node); \
  root = container_of(base_p, type, node); \
}

#define konexios_linked_list_add_node_first(root, type, el) { \
  konexios_linked_list_t *base_p = konexios_linked_list_add_first((root)?&(root)->node:NULL, &(el)->node); \
  root = container_of(base_p, type, node); \
}

#define konexios_linked_list_del_node(root, type, el) { \
  konexios_linked_list_t *base_p = konexios_linked_list_del(&(root)->node, &(el)->node); \
  if ( base_p ) root = container_of(base_p, type, node); \
  else root = NULL; \
}

#define konexios_linked_list_del_node_first(root, type) { \
  konexios_linked_list_t *base_p = konexios_linked_list_del(&(root)->node, &(root)->node); \
  if ( base_p ) root = container_of(base_p, type, node); \
  else root = NULL; \
}

#define konexios_linked_list_del_node_last(root, type) { \
  konexios_linked_list_t *base_p = konexios_linked_list_del_last(&(root)->node); \
  if ( base_p ) root = container_of(base_p, type, node); \
  else root = NULL; \
}

#define konexios_linked_list_next_node(tmp, root, type) { \
    konexios_linked_list_t *next_p = (root)?(root)->node.next:NULL; \
    if ( next_p ) tmp = container_of(next_p, type, node); \
    else tmp = NULL; \
  }

#if defined(__cplusplus)
}
#endif

#endif  // KONEXIOS_LINKED_LIST_H_

