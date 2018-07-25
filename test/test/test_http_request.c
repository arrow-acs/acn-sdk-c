#include "unity.h"
#include <config.h>
#include <debug.h>
#include <arrow/credentials.h>
#include <data/static_buf.h>
#include <data/static_alloc.h>
#include <arrow/utf8.h>
#include <data/linkedlist.h>
#include <data/property.h>
#include <data/ringbuffer.h>
#include <data/propmap.h>
#include <data/property_base.h>
#include <data/property_const.h>
#include <data/property_dynamic.h>
#include <data/property_stack.h>
#include <json/property_json.h>
#include <json/json.h>
#include <sb.h>
#include <encode.h>
#include <decode.h>
#include <bsd/socket.h>
#include <http/request.h>
#include <http/response.h>
#include <data/find_by.h>

void setUp(void) {
    property_types_init();
}

void tearDown(void) {
    property_types_deinit();
}

static http_request_t _test_request;

void test_http_request_init(void) {
    http_request_init(&_test_request, GET, "http://api.arrowconnect.io:80/api/v1/kronos/gateways");
    TEST_ASSERT(_test_request._response_payload_meth._p_add_handler);
    TEST_ASSERT(_test_request._response_payload_meth._p_set_handler);
    TEST_ASSERT_EQUAL_INT(0, _test_request.is_corrupt);
    TEST_ASSERT_EQUAL_STRING("api.arrowconnect.io", P_VALUE(_test_request.host));
    TEST_ASSERT_EQUAL_INT(80, _test_request.port);
    TEST_ASSERT_EQUAL_STRING("GET", P_VALUE(_test_request.meth));
    TEST_ASSERT_EQUAL_STRING("http", P_VALUE(_test_request.scheme));
    TEST_ASSERT( IS_EMPTY(_test_request.payload) );
    TEST_ASSERT( ! _test_request.header );
    TEST_ASSERT( IS_EMPTY(_test_request.content_type.value) );
    TEST_ASSERT( IS_EMPTY(_test_request.content_type.key) );
    TEST_ASSERT( ! _test_request.content_type.node.next );
}

void test_http_request_add_header(void) {
    http_request_add_header(&_test_request, p_heap("hello"), p_heap("kokoko"));
    TEST_ASSERT( _test_request.header );
    property_map_t *tmp = NULL;
    arrow_linked_list_for_each(tmp, _test_request.header, property_map_t) {
        TEST_ASSERT_EQUAL_STRING("hello", P_VALUE(tmp->key));
        TEST_ASSERT_EQUAL_STRING("kokoko", P_VALUE(tmp->value));
    }
}

