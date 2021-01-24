/* Copyright (c) 2017 Arrow Electronics, Inc.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Apache License 2.0
 * which accompanies this distribution, and is available at
 * http://apache.org/licenses/LICENSE-2.0
 * Contributors: Arrow Electronics, Inc.
 */

#include "data/property_dynamic.h"
#include <debug.h>

#if defined(STATIC_DYNAMIC_PROPERTY)
#include <data/static_buf.h>
CREATE_BUFFER(dynamicbuf, KONEXIOS_DYNAMIC_STATIC_BUFFER_SIZE, 0x20)

int dyn_static_mem_size() {
    return static_buf_free_size(dynamicbuf);
}

static void *static_strndup(char *ptr, int size) {
    if ( size <= 0  ) return NULL;
    void *p = static_buf_alloc(dynamicbuf, size + 1);
    if ( !p ) {
        DBG("Out of Memory: static dynamic");
        return NULL;
    }
    memcpy(p, ptr, size);
    ((char *)p)[size] = 0x0;
    return p;
}

static void *static_strdup(char *ptr) {
    int size =  strlen(ptr);
    return static_strndup(ptr, size);
}

void static_free(void *p) {
    static_buf_free(dynamicbuf, p);
}

#define STRNDUP static_strndup
#define STRDUP  static_strdup

#define REALLOC(...) static_buf_realloc(dynamicbuf, __VA_ARGS__)
#define FREE    static_free
#else
#define STRNDUP strndup
#define STRDUP  strdup
#define REALLOC realloc
#define FREE    free
#endif

void dynmc_copy(property_t *dst, property_t *src) {
    dst->size = src->size;
    if ( ! (src->flags & is_owner) ) {
        dst->value = src->value;
        dst->flags = src->flags;
        return;
    }
    if ( src->flags & is_raw ) {
        dst->value = (char *)STRNDUP(src->value, src->size);
    } else {
        dst->value = (char *)STRDUP(src->value);
    }
    dst->flags = is_owner | PROPERTY_DYNAMIC_TAG;
}

void dynmc_weak(property_t *dst, property_t *src) {
    dst->value = src->value;
    dst->size = src->size;
    dst->flags = PROPERTY_DYNAMIC_TAG;
}

void dynmc_move(property_t *dst, property_t *src) {
    dst->value = src->value;
    dst->size = src->size;
    dst->flags = PROPERTY_DYNAMIC_TAG;
    if ( src->flags & is_owner ) {
        dst->flags |= is_owner;
        src->flags &= ~is_owner; // make weak
    }
}

void dynmc_destroy(property_t *dst) {
    if ( dst->flags & is_owner ) {
        FREE(dst->value);
    }
}

void dynmc_concat(property_t *dst, property_t *src) {
    int size_dst = 0;
    int size_src = 0;
    if ( ! (dst->flags & is_owner) ) return;
    if ( dst->flags & is_raw ) {
        size_dst += dst->size;
    } else {
        size_dst += strlen(dst->value);
    }
    if ( src->flags & is_raw ) {
        size_src += src->size;
    } else {
        size_src += strlen(src->value);
    }
    dst->value = REALLOC(dst->value, size_src + size_dst + 1);
    if ( !dst->value ) {
        DBG("Out of Memory: static realloc");
        dynmc_destroy(dst);
        return;
    }
    dst->size += size_src;
    memcpy(dst->value + size_dst, src->value, size_src);
    dst->value[size_dst + size_src] = '\0';
}

static property_dispetcher_t dynamic_property_type = {
    PROPERTY_DYNAMIC_TAG, { dynmc_copy, dynmc_weak, dynmc_move, dynmc_destroy }, {NULL}
};

property_dispetcher_t *property_type_get_dynamic() {
    return &dynamic_property_type;
}

void property_dynamic_destroy() {
#if defined(STATIC_DYNAMIC_PROPERTY)
    static_buf_clear_all(dynamicbuf);
#endif
}

property_t string_to_dynamic_property(const char *name) {
    property_t dst;
    dst.value = (char *)STRDUP((char*)name);
    dst.size = strlen(name);
    dst.flags = PROPERTY_DYNAMIC_TAG | is_owner;
    return dst;
}

property_t raw_to_dynamic_property(const char *name, int len) {
    property_t dst;
    dst.value = (char *)STRNDUP((char*)name, len);
    dst.size = len;
    dst.flags = PROPERTY_DYNAMIC_TAG | is_owner | is_raw;
    return dst;
}
