#include "arrow/routine.h"
#include <config.h>
#include <debug.h>
#include <time/time.h>
#include <sys/watchdog.h>
#include <http/routine.h>
#include <arrow/mqtt.h>
#include <arrow/events.h>
#include <arrow/state.h>
#include <arrow/device_command.h>
#include <arrow/mqtt.h>
#include <arrow/api/gateway/gateway.h>
#include <arrow/api/device/device.h>
#include <arrow/telemetry_api.h>
#include <arrow/storage.h>
#include <json/property_json.h>
#include <json/aob.h>

#define GATEWAY_CONNECT "Gateway connection [%s]"
#define GATEWAY_CONFIG "Gateway config [%s]"
#define DEVICE_CONNECT "Device connection [%s]"
#define DEVICE_TELEMETRY "Device telemetry [%s]"
#define DEVICE_MQTT_CONNECT "Device mqtt connection [%s]"
#define DEVICE_MQTT_TELEMETRY "Device mqtt telemetry [%s]"

static arrow_gateway_t _gateway;
static arrow_gateway_config_t _gateway_config;
static arrow_device_t _device;
static int _init_done = 0;

enum MQTT_INIT_FLAGS {
  MQTT_INIT_TELEMETRY_ROUTINE = 0x01,
  MQTT_INIT_COMMAND_ROUTINE   = 0x02
};
static int _init_mqtt = 0;

arrow_device_t *current_device(void) {
  return &_device;
}

arrow_gateway_t *current_gateway(void) {
  return &_gateway;
}

arrow_gateway_config_t *current_gateway_config(void) {
  return &_gateway_config;
}

arrow_routine_error_t arrow_init(void) {
  property_types_init();
  property_type_add(property_type_get_json());
  property_type_add(property_type_get_aob());
  arrow_hosts_init();
  arrow_gateway_init(&_gateway);
  arrow_device_init(&_device);
  if ( __http_init() < 0 ) {
    return ROUTINE_ERROR;
  }
#if !defined(NO_EVENTS)
  arrow_mqtt_events_init();
#endif
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_deinit(void) {
#if !defined(NO_EVENTS)
  arrow_mqtt_events_done();
#endif
  arrow_state_deinit();
  __http_done();
  property_types_deinit();
  return ROUTINE_SUCCESS;
}

int arrow_connect_gateway(arrow_gateway_t *gateway) {
  arrow_prepare_gateway(gateway);
  if ( IS_EMPTY(gateway->hid) ) {
      int ret = restore_gateway_info(gateway);
      if ( ret < 0 ) {
          // new registration
          if ( (ret = arrow_register_gateway(gateway)) < 0 ) {
              return ret;
          }
          save_gateway_info(gateway);
          return 0;
      }
  }
  // hid already set
  if (!P_VALUE(gateway->hid)) {
  	  DBG("gateway hid NULL");
  	  return 0;
  }
  DBG("gateway checkin hid %s", P_VALUE(gateway->hid));
  int ret = arrow_gateway_checkin(gateway);
  if ( !ret ) {
      ret = arrow_gateway_update(gateway);
  }
  return ret;
}

int arrow_connect_device(arrow_gateway_t *gateway, arrow_device_t *device) {
  int ret = 0;
  arrow_prepare_device(gateway, device);
  if ( !IS_EMPTY(device->hid) )
      return ROUTINE_SUCCESS;
  if ( restore_device_info(device) < 0 ) {
    if ( (ret = arrow_register_device(gateway, device)) < 0 ) {
      goto dev_reg_error;
    }
    save_device_info(device);
  }
#if !defined(NO_SOFTWARE_UPDATE) && !defined(NO_RELEASE_UPDATE)
  else {
      ret = arrow_device_update(gateway, device);
      if ( ret < 0 )
          goto dev_reg_error;
  }
#else
# if defined(CHECK_DEVICE_REG)
  else {
    device_info_t list;
    if ( (ret = arrow_device_find_by_hid(&list, P_VALUE(device->hid))) < 0 ) {
      return ret;
    } else {
      if ( list.enabled ) {
        DBG("device: %s", P_VALUE(list.name));
      }
      device_info_free(&list);
    }
  }
# else
  DBG("No device request: close socket");
  http_client_close(current_client());
# endif
#endif
  arrow_state_mqtt_run(device);
  return ret;
dev_reg_error:
  arrow_device_free(device);
  return ret;
}

arrow_routine_error_t arrow_gateway_initialize_routine(void) {
  wdt_feed();
  http_session_close_set(current_client(), false);
  int retry = 0;
  while ( arrow_connect_gateway(&_gateway) < 0 ) {
    RETRY_UP(retry, {return ROUTINE_ERROR;});
    DBG(GATEWAY_CONNECT, "fail");
    msleep(ARROW_RETRY_DELAY);
  }
  DBG(GATEWAY_CONNECT, "ok");
  http_session_close_set(current_client(), true);
  wdt_feed();
  RETRY_CR(retry);
  while ( arrow_gateway_config(&_gateway, &_gateway_config) < 0 ) {
    RETRY_UP(retry, {return ROUTINE_ERROR;});
    DBG(GATEWAY_CONFIG, "fail");
    msleep(ARROW_RETRY_DELAY);
  }
  DBG(GATEWAY_CONFIG, "ok");
  _init_done = 1;
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_initialize_routine(void) {
  wdt_feed();
  http_session_set_protocol(current_client(), api_via_http);
  http_session_close_set(current_client(), false);
  int retry = 0;
  int ret = 0;
  DBG("register gateway via API");
  while ( (ret = arrow_connect_gateway(&_gateway)) < 0 ) {
    RETRY_UP(retry, {goto gateway_reg_error;});
    DBG(GATEWAY_CONNECT, "fail");
    msleep(ARROW_RETRY_DELAY);
  }
  DBG(GATEWAY_CONNECT, "ok");

  wdt_feed();
  RETRY_CR(retry);
  while ( (ret = arrow_gateway_config(&_gateway, &_gateway_config)) < 0 ) {
    RETRY_UP(retry, {goto gateway_config_error;});
    DBG(GATEWAY_CONFIG, "fail");
    msleep(ARROW_RETRY_DELAY);
  }
  DBG(GATEWAY_CONFIG, "ok");

  // device registaration
  wdt_feed();
  RETRY_CR(retry);
  DBG("register device via API");
  // close session after next request
  http_session_close_set(current_client(), true);
  while ( (ret = arrow_connect_device(&_gateway, &_device)) < 0 ) {
    RETRY_UP(retry, {goto device_reg_error;});
    DBG(DEVICE_CONNECT, "fail");
    msleep(ARROW_RETRY_DELAY);
  }
  DBG(DEVICE_CONNECT, "ok");
  _init_done = 1;
  return ROUTINE_SUCCESS;
device_reg_error:
  arrow_device_free(&_device);
gateway_config_error:
  arrow_gateway_config_free(&_gateway_config);
gateway_reg_error:
  arrow_gateway_free(&_gateway);
  return ret;
}

arrow_routine_error_t arrow_device_states_sync() {
    arrow_state_receive(current_device());
    if ( !_init_done ) return ROUTINE_ERROR;
    arrow_post_state_request(current_device());
    return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_device_states_update() {
    if ( !_init_done ) return ROUTINE_ERROR;
    arrow_post_state_update(&_device);
    return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_send_telemetry_routine(void *data) {
  if ( !_init_done ) {
    return ROUTINE_NOT_INITIALIZE;
  }
  wdt_feed();
  int retry = 0;
  while ( arrow_send_telemetry(&_device, data) < 0) {
    RETRY_UP(retry, {return ROUTINE_ERROR;});
    DBG(DEVICE_TELEMETRY, "fail");
    msleep(ARROW_RETRY_DELAY);
  }
  DBG(DEVICE_TELEMETRY, "ok");
  return ROUTINE_SUCCESS;
}

// MQTT

arrow_routine_error_t arrow_mqtt_connect_telemetry_routine(void) {
  if ( _init_mqtt & MQTT_INIT_TELEMETRY_ROUTINE ) {
    return ROUTINE_ERROR;
  }
  int retry = 0;
  while ( mqtt_telemetry_connect(&_gateway, &_device, &_gateway_config) < 0 ) {
    RETRY_UP(retry, {return ROUTINE_MQTT_CONNECT_FAILED;});
    DBG(DEVICE_MQTT_CONNECT, "fail");
    msleep(MQTT_RETRY_DELAY);
  }
  _init_mqtt |= MQTT_INIT_TELEMETRY_ROUTINE;
  DBG(DEVICE_MQTT_CONNECT, "ok");
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_disconnect_telemetry_routine(void) {
  if ( ! ( _init_mqtt & MQTT_INIT_TELEMETRY_ROUTINE ) ) {
    return ROUTINE_ERROR;
  }
  if ( mqtt_telemetry_terminate() < 0 ) {
    return ROUTINE_ERROR;
  }
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_terminate_telemetry_routine(void) {
  if ( mqtt_telemetry_terminate() < 0 ) {
    return ROUTINE_ERROR;
  }
  return ROUTINE_SUCCESS;
}


#if !defined(NO_EVENTS)
arrow_routine_error_t arrow_mqtt_connect_event_routine(void) {
  if ( _init_mqtt & MQTT_INIT_COMMAND_ROUTINE ) {
    return ROUTINE_ERROR;
  }
  int retry = 0;
  while(mqtt_subscribe_connect(&_gateway, &_device, &_gateway_config) < 0 ) {
    RETRY_UP(retry, {return ROUTINE_MQTT_SUBSCRIBE_FAILED;});
    DBG(DEVICE_MQTT_CONNECT, "fail");
    msleep(MQTT_RETRY_DELAY);
  }
  _init_mqtt |= MQTT_INIT_COMMAND_ROUTINE;
  return ROUTINE_SUCCESS;
}
arrow_routine_error_t arrow_mqtt_subscribe_event_routine(void) {
  int retry = 0;
  while( mqtt_subscribe() < 0 ) {
    RETRY_UP(retry, {return ROUTINE_MQTT_SUBSCRIBE_FAILED;});
    DBG(DEVICE_MQTT_CONNECT, "fail");
    msleep(MQTT_RETRY_DELAY);
    _init_mqtt &= ~MQTT_INIT_COMMAND_ROUTINE;
  }
  _init_mqtt |= MQTT_INIT_COMMAND_ROUTINE;
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_disconnect_event_routine(void) {
  if ( ! ( _init_mqtt & MQTT_INIT_COMMAND_ROUTINE ) ) {
    return ROUTINE_ERROR;
  }
  if ( mqtt_subscribe_disconnect() < 0 ) {
    return ROUTINE_ERROR;
  }
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_terminate_event_routine(void) {
  if ( mqtt_subscribe_terminate() < 0 ) {
    return ROUTINE_ERROR;
  }
  return ROUTINE_SUCCESS;
}

#endif

arrow_routine_error_t arrow_mqtt_connect_routine(void) {
  if ( !_init_done ) {
    return ROUTINE_NOT_INITIALIZE;
  }
  // init MQTT
  DBG("mqtt connect...");
  arrow_routine_error_t ret = ROUTINE_ERROR;
  ret = arrow_mqtt_connect_telemetry_routine();
  if ( ret != ROUTINE_SUCCESS ) {
    return ret;
  }

#if !defined(NO_EVENTS)
  ret = arrow_mqtt_connect_event_routine();
  if ( ret != ROUTINE_SUCCESS ) {
    return ret;
  }

  // process postponed messages
  if ( arrow_mqtt_event_receive_routine() != ROUTINE_RECEIVE_EVENT ) {
    arrow_mqtt_subscribe_event_routine();
  }
#endif
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_disconnect_routine() {
  if ( _init_mqtt ) {
    mqtt_disconnect();
    _init_mqtt = 0;
    return ROUTINE_SUCCESS;
  }
  return ROUTINE_ERROR;
}

arrow_routine_error_t arrow_mqtt_terminate_routine() {
  mqtt_terminate();
  _init_mqtt = 0;
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_pause_routine(int pause) {
    mqtt_pause(pause);
    return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_send_telemetry_routine(get_data_cb data_cb, void *data) {
  if ( !_init_done ||
       !(_init_mqtt & MQTT_INIT_TELEMETRY_ROUTINE)
#if !defined(NO_EVENTS)
       || !(_init_mqtt & MQTT_INIT_COMMAND_ROUTINE)
#endif
     ) {
    DBG(DEVICE_MQTT_TELEMETRY, "Cloud not initialize");
    return ROUTINE_NOT_INITIALIZE;
  }
  wdt_feed();
  while (1) {
    mqtt_receive(TELEMETRY_DELAY);
#if !defined(NO_EVENTS)
    if ( arrow_mqtt_has_events() ) {
      DBG("There is an event");
      return ROUTINE_RECEIVE_EVENT;
    }
#endif
    int get_data_result = data_cb(data);
    if ( get_data_result < 0 ) {
      DBG(DEVICE_MQTT_TELEMETRY, "Fail to get telemetry data");
      return ROUTINE_GET_TELEMETRY_FAILED;
    } else if (get_data_result > 0 ) {
      // skip this
      continue;
    }
    wdt_feed();
    if ( mqtt_publish(&_device, data) < 0 ) {
      DBG(DEVICE_MQTT_TELEMETRY, "fail");
      return ROUTINE_MQTT_PUBLISH_FAILED;
    }
#if defined(VALGRIND_TEST)
    static int count = 0;
    if ( count++ > VALGRIND_TEST ) {
      return ROUTINE_TEST_DONE;
    }
    DBG("test count [%d]", count);
#endif
    DBG(DEVICE_MQTT_TELEMETRY, "ok");
  }
  return ROUTINE_SUCCESS;
}

void arrow_close(void) {
  arrow_mqtt_terminate_routine();
  if ( arrow_state_mqtt_is_running() )
      arrow_state_mqtt_stop();
  if ( _init_done ) {
    arrow_device_free(&_device);
    arrow_gateway_free(&_gateway);
    arrow_gateway_config_free(&_gateway_config);
    _init_done = 0;
  }
}

arrow_routine_error_t arrow_mqtt_telemetry_routine(get_data_cb data_cb, void *data) {
  if ( !_init_done ||
       !(_init_mqtt & MQTT_INIT_TELEMETRY_ROUTINE) ) {
    DBG(DEVICE_MQTT_TELEMETRY, "Cloud not initialize");
    return ROUTINE_NOT_INITIALIZE;
  }
  wdt_feed();
  while (1) {
    msleep(TELEMETRY_DELAY);
    int get_data_result = data_cb(data);
    if ( get_data_result < 0 ) {
      DBG(DEVICE_MQTT_TELEMETRY, "Fail to get telemetry data");
      return ROUTINE_GET_TELEMETRY_FAILED;
    } else if (get_data_result > 0 ) {
      // skip this
      continue;
    }
    wdt_feed();
    if ( mqtt_publish(&_device, data) < 0 ) {
      DBG(DEVICE_MQTT_TELEMETRY, "fail");
      return ROUTINE_MQTT_PUBLISH_FAILED;
    }
#if defined(VALGRIND_TEST)
    static int count = 0;
    if ( count++ > VALGRIND_TEST ) {
      return ROUTINE_TEST_DONE;
    }
    DBG("test count [%d]", count);
#endif
    DBG(DEVICE_MQTT_TELEMETRY, "ok");
  }
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_telemetry_once_routine(get_data_cb data_cb, void *data) {
  if ( !_init_done ||
       !(_init_mqtt & MQTT_INIT_TELEMETRY_ROUTINE) ) {
    DBG(DEVICE_MQTT_TELEMETRY, "Cloud not initialize");
    return ROUTINE_NOT_INITIALIZE;
  }
  wdt_feed();
  int get_data_result = data_cb(data);
  if ( get_data_result != 0 ) {
    DBG(DEVICE_MQTT_TELEMETRY, "Fail to get telemetry data");
    return ROUTINE_GET_TELEMETRY_FAILED;
  }
  if ( mqtt_publish(&_device, data) < 0 ) {
    DBG(DEVICE_MQTT_TELEMETRY, "fail");
    return ROUTINE_MQTT_PUBLISH_FAILED;
  }
  DBG(DEVICE_MQTT_TELEMETRY, "ok");
  return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_event_receive_routine(void) {
  if ( !_init_done ||
       !(_init_mqtt & MQTT_INIT_COMMAND_ROUTINE) ) {
    DBG(DEVICE_MQTT_TELEMETRY, "Cloud not initialize");
    return ROUTINE_NOT_INITIALIZE;
  }
#if defined(NO_EVENTS)
  int ret = MQTT_SUCCESS;
  msleep(TELEMETRY_DELAY);
#else
  int ret = mqtt_yield(TELEMETRY_DELAY);
#endif
  wdt_feed();
  if ( ret > 0 ) {
    return ROUTINE_RECEIVE_EVENT;
  } else if (ret == FAILURE_REQUIRES_RESTART) {
      return ROUTINE_ERROR;
  } else return ROUTINE_SUCCESS;
}

arrow_routine_error_t arrow_mqtt_check_init(void) {
  if ( !_init_done ||
       !(_init_mqtt & MQTT_INIT_COMMAND_ROUTINE) ) {
    DBG(DEVICE_MQTT_TELEMETRY, "Cloud not initialize");
    return ROUTINE_NOT_INITIALIZE;
  }
  return ROUTINE_SUCCESS;
}
