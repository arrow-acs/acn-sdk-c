/* Copyright (c) 2017 Arrow Electronics, Inc.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Apache License 2.0
 * which accompanies this distribution, and is available at
 * http://apache.org/licenses/LICENSE-2.0
 * Contributors: Arrow Electronics, Inc.
 */

#include <config.h>
#include <bsd/inet.h>
#if !defined(USER_BYTE_CONVERTER)
uint16_t htons(uint16_t n) {
  return (uint16_t)((n & 0xff) << 8) | ((n & 0xff00) >> 8);
}

uint16_t ntohs(uint16_t n) {
  return htons(n);
}

uint32_t htonl(uint32_t n) {
  return ((n & 0xff) << 24) |
    ((n & 0xff00) << 8) |
    ((n & 0xff0000UL) >> 8) |
    ((n & 0xff000000UL) >> 24);
}

uint32_t ntohl(uint32_t n) {
  return htonl(n);
}
#else
typedef int __dummy;
#endif
