#include "unity.h"
#include <stdlib.h>
#include <string.h>
#include <konexios_config.h>
#include <debug.h>
#include <konexios/credentials.h>
#include <konexios/gateway.h>
#include <sys/mem.h>
#include <data/static_buf.h>
#include <data/static_alloc.h>
#include <data/linkedlist.h>
#include <data/property.h>
#include <data/property_base.h>
#include <data/property_const.h>
#include <data/property_dynamic.h>
#include <data/property_stack.h>
#include <json/property_json.h>
#include <json/json.h>
#include <json/sb.h>
#include <json/aob.h>
#include <encode.h>
#include <json/decode.h>
#include "mock_mac.h"
#include "storage_weak.h"

void setUp(void) {
    property_types_init();
}

void tearDown(void) {
    property_types_deinit();
}

static konexios_gateway_t _test_gateway;

void test_gateway_init(void) {
    konexios_gateway_init(&_test_gateway);
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.hid));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.name));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.os));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.sdkVersion));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.software_name));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.software_version));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.type));
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.uid));
}

#define GATEWAY_UID GATEWAY_UID_PREFIX "-111213141516"
void test_gateway_prepare( void ) {
    char mac[6] = {0x11, 0x12, 0x13, 0x14, 0x15, 0x16};
//    get_mac_address_ExpectAndReturn(mac, 0);
    get_mac_address_ExpectAnyArgsAndReturn(0);
    get_mac_address_ReturnArrayThruPtr_mac(mac, 6);
    konexios_prepare_gateway(&_test_gateway);
    TEST_ASSERT_EQUAL_STRING(NULL, P_VALUE(_test_gateway.hid));
    TEST_ASSERT_EQUAL_STRING(GATEWAY_NAME, P_VALUE(_test_gateway.name));
    TEST_ASSERT_EQUAL_STRING(GATEWAY_OS, P_VALUE(_test_gateway.os));
    TEST_ASSERT_EQUAL_STRING(xstr(SDK_VERSION), P_VALUE(_test_gateway.sdkVersion));
    TEST_ASSERT_EQUAL_STRING(GATEWAY_SOFTWARE_NAME, P_VALUE(_test_gateway.software_name));
    TEST_ASSERT_EQUAL_STRING(GATEWAY_SOFTWARE_VERSION, P_VALUE(_test_gateway.software_version));
    TEST_ASSERT_EQUAL_STRING(GATEWAY_TYPE, P_VALUE(_test_gateway.type));
    TEST_ASSERT_EQUAL_STRING(GATEWAY_UID, P_VALUE(_test_gateway.uid));
}
