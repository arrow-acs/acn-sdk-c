/* Copyright (c) 2017 Arrow Electronics, Inc.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the Apache License 2.0
 * which accompanies this distribution, and is available at
 * http://apache.org/licenses/LICENSE-2.0
 * Contributors: Arrow Electronics, Inc.
 */

#include "konexios/api/gateway/gateway.h"
#include <http/routine.h>
#include <konexios/sign.h>
#include <konexios/storage.h>
#include <debug.h>
#include <data/chunk.h>
#include <json/decode.h>

#define URI_LEN sizeof(KONEXIOS_API_GATEWAY_ENDPOINT) + 100
#define GATEWAY_MSG "Gateway %d"

static void _gateway_config_init(http_request_t *request, void *arg) {
	konexios_gateway_t *gateway = (konexios_gateway_t *)arg;
	CREATE_CHUNK(uri, URI_LEN);
    int ret = snprintf(uri, URI_LEN,
                       "%s/%s/config",
                       KONEXIOS_API_GATEWAY_ENDPOINT,
                       P_VALUE(gateway->hid) );
    if ( ret > 0 ) uri[ret] = 0x0;
    http_request_init(request, GET, &p_stack(uri));
	FREE_CHUNK(uri);
}

static int _gateway_config_proc(http_response_t *response, void *arg) {
    int ret = -1;
	konexios_gateway_config_t *config = (konexios_gateway_config_t *)arg;
    konexios_gateway_config_init(config);
    DBG("payload: {%s}", P_VALUE(response->payload));
    if ( response->m_httpResponseCode != 200 ) {
        DBG("ERROR: HTTP response code: %d", response->m_httpResponseCode);
		return -1;
    }

    JsonNode *_main = json_decode_property(response->payload);
	if ( !_main ) {
		DBG("Parse error");
        goto gateway_config_error;
	}
    JsonNode *_cloud = json_find_member(_main, p_const("cloudPlatform"));
    if ( !_cloud || _cloud->tag != JSON_STRING ) {
        DBG("no Cloud Platform");
        goto gateway_config_error;
    }
    if ( property_cmp(&_cloud->string_, p_const("IotConnect")) == 0 ) {
        config->type = IoT;
    } // FIXME iot connect ibm, azure
    JsonNode *_main_key = json_find_member(_main, p_const("key"));
	if ( _main_key ) {
        JsonNode *api = json_find_member(_main_key, p_const("apiKey"));
        if (api) {
            set_api_key(P_VALUE(api->string_));
		}
        JsonNode *sec  = json_find_member(_main_key, p_const("secretKey"));
        if (sec) {
            set_secret_key(P_VALUE(sec->string_));
		}
        if ( api && sec ) {
            save_key_setting(json_string(api), json_string(sec));
		}
	} else {
        DBG("There are no keys!");
        goto gateway_config_error;
	}
#if defined(__IBM__)
    JsonNode *_main_ibm = json_find_member(_main, p_const("ibm"));
	if ( _main_ibm ) {
        JsonNode *tmp = NULL;
        tmp = json_find_member(_main_ibm, p_const("organizationId"));
        if ( tmp ) property_copy(&config->organizationId, p_stack(tmp->string_));
        tmp = json_find_member(_main_ibm, p_const("gatewayType"));
        if ( tmp ) property_copy(&config->gatewayType, p_stack(tmp->string_));
        tmp = json_find_member(_main_ibm, p_const("gatewayId"));
        if ( tmp ) property_copy(&config->gatewayId, p_stack(tmp->string_));
        tmp = json_find_member(_main_ibm, p_const("authToken"));
        if ( tmp ) property_copy(&config->authToken, p_stack(tmp->string_));
        tmp = json_find_member(_main_ibm, p_const("authMethod"));
        if ( tmp ) property_copy(&config->authMethod, p_stack(tmp->string_));
	}
#elif defined(__AZURE__)
    JsonNode *_main_azure = json_find_member(_main, p_const("azure"));
	if ( _main_azure ) {
        tmp = json_find_member(_main_azure, p_const("host"));
		if ( tmp ) konexios_gateway_config_add_host(config, tmp->string_);
        tmp = json_find_member(_main_azure, p_const("accessKey"));
		if ( tmp ) konexios_gateway_config_add_accessKey(config, tmp->string_);
	}
#endif
    ret = 0;
gateway_config_error:
	json_delete(_main);
    return ret;
}

int konexios_gateway_config(konexios_gateway_t *gateway, konexios_gateway_config_t *config) {
    STD_ROUTINE(_gateway_config_init, gateway,
                _gateway_config_proc, config,
                GATEWAY_MSG, GATEWAY_CONFIG_ERROR);
}


static void _gateway_register_init(http_request_t *request, void *arg) {
  konexios_gateway_t *gateway = (konexios_gateway_t *)arg;
  http_request_init(request, POST, &p_const(KONEXIOS_API_GATEWAY_ENDPOINT));
  if ( IS_EMPTY(gateway->uid) )
      konexios_prepare_gateway(gateway);
  property_t pay = konexios_gateway_serialize(gateway);
  if ( IS_EMPTY(pay) ) return;
  http_request_set_payload(request, pay);
}

static int _gateway_register_proc(http_response_t *response, void *arg) {
  konexios_gateway_t *gateway = (konexios_gateway_t *)arg;
  DBG("payload: {%s}", IS_EMPTY(response->payload)?"NULL PAYLOAD":P_VALUE(response->payload));
  if ( response->m_httpResponseCode != 200 ) {
      DBG("ERROR: HTTP response code: %d", response->m_httpResponseCode);
      return -1;
  }
  if ( konexios_gateway_parse(gateway, P_VALUE(response->payload)) < 0 ) {
      DBG("parse error");
      return -1;
  } else {
      DBG("gateway hid: %s", P_VALUE(gateway->hid) );
  }
  return 0;
}

int konexios_register_gateway(konexios_gateway_t *gateway) {
  STD_ROUTINE(_gateway_register_init, gateway,
              _gateway_register_proc, gateway,
              GATEWAY_MSG, GATEWAY_REGISTER_ERROR);
}

static void _gateway_heartbeat_init(http_request_t *request, void *arg) {
  konexios_gateway_t *gateway = (konexios_gateway_t *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
                     "%s/%s/heartbeat",
                     KONEXIOS_API_GATEWAY_ENDPOINT,
                     P_VALUE(gateway->hid) );
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, PUT, &p_stack(uri));
  FREE_CHUNK(uri);
}

int konexios_gateway_heartbeat(konexios_gateway_t *gateway) {
  STD_ROUTINE(_gateway_heartbeat_init, gateway,
              NULL, NULL,
              GATEWAY_MSG, GATEWAY_HEARTBEAT_ERROR);
}

static void _gateway_checkin_init(http_request_t *request, void *arg) {
  konexios_gateway_t *gateway = (konexios_gateway_t *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
                     "%s/%s/checkin",
                     KONEXIOS_API_GATEWAY_ENDPOINT,
                     P_VALUE(gateway->hid));
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, PUT, &p_stack(uri));
  FREE_CHUNK(uri);
}

int konexios_gateway_checkin(konexios_gateway_t *gateway) {
  STD_ROUTINE(_gateway_checkin_init, gateway,
              NULL, NULL,
              GATEWAY_MSG, GATEWAY_CHECKIN_ERROR);
}

static void _gateway_find_init(http_request_t *request, void *arg) {
  char *hid = (char *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
                     "%s/%s",
                     KONEXIOS_API_GATEWAY_ENDPOINT,
                     hid);
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, GET, &p_stack(uri));
  FREE_CHUNK(uri);
}

static int _gateway_find_proc(http_response_t *response, void *arg) {
    gateway_info_t *info = (gateway_info_t *)arg;
    gateway_info_t *list;
    int ret = gateway_info_parse(&list, P_VALUE(response->payload));
    if ( ret < 0 ) return -1;
    if ( list ) {
        gateway_info_move(info, list);
        gateway_info_t *tmp = NULL;
        konexios_linked_list_for_each_safe(tmp, list, gateway_info_t) {
            gateway_info_free(tmp);
            free(tmp);
        }
    }
    return 0;
}

int konexios_gateway_find(gateway_info_t *info, const char *hid) {
  STD_ROUTINE(_gateway_find_init, (void*)hid,
              _gateway_find_proc, (void*)info,
              GATEWAY_MSG, GATEWAY_FIND_ERROR);
}

static void _gateway_find_by_init(http_request_t *request, void *arg) {
  find_by_t *params = (find_by_t *)arg;
  http_request_init(request, GET, &p_const(KONEXIOS_API_GATEWAY_ENDPOINT));
  http_request_set_findby(request, params);
}

static int _gateway_find_by_proc(http_response_t *response, void *arg) {
  gateway_info_t **info = (gateway_info_t **)arg;
  *info = NULL;
  return gateway_info_parse(info, P_VALUE(response->payload));
}


int konexios_gateway_find_by(gateway_info_t **info, int n, ...) {
  find_by_t *params = NULL;
  find_by_collect(params, n);
  STD_ROUTINE(_gateway_find_by_init, params,
              _gateway_find_by_proc, (void*)info,
              GATEWAY_MSG, GATEWAY_FINDBY_ERROR);
}

typedef struct _gate_param_ {
  konexios_gateway_t *gate;
  find_by_t *params;
} gate_param_t;

static void _gateway_list_logs_init(http_request_t *request, void *arg) {
  gate_param_t *dp = (gate_param_t *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
                     "%s/%s/logs",
                     KONEXIOS_API_GATEWAY_ENDPOINT,
                     P_VALUE(dp->gate->hid) );
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, GET, &p_stack(uri));
  FREE_CHUNK(uri);
  http_request_set_findby(request, dp->params);
}

static int _gateway_list_logs_proc(http_response_t *response, void *arg) {
    log_t **logs = (log_t **)arg;
    *logs = NULL;
    return log_parse(logs, P_VALUE(response->payload));
}

int konexios_gateway_logs_list(log_t **logs, konexios_gateway_t *gateway, int n, ...) {
  find_by_t *params = NULL;
  find_by_collect(params, n);
  gate_param_t dp = { gateway, params };
  STD_ROUTINE(_gateway_list_logs_init, &dp,
              _gateway_list_logs_proc, (void*)logs,
              GATEWAY_MSG, GATEWAY_LOGS_ERROR);
}

static void _gateway_devices_list_init(http_request_t *request, void *arg) {
  char *hid = (char *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN, "%s/%s/devices", KONEXIOS_API_GATEWAY_ENDPOINT, hid);
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, GET, &p_stack(uri));
  FREE_CHUNK(uri);
}

static int _gateway_devices_list_proc(http_response_t *response, void *arg) {
    device_info_t **devs = (device_info_t **)arg;
    *devs = NULL;
    return device_info_parse(devs, P_VALUE(response->payload));
}

int konexios_gateway_devices_list(device_info_t **list, const char *hid) {
  STD_ROUTINE(_gateway_devices_list_init, (void*)hid,
              _gateway_devices_list_proc, list,
              GATEWAY_MSG, GATEWAY_DEVLIST_ERROR);
}

typedef struct _gate_dev_cmd_ {
  const char *g_hid;
  const char *d_hid;
  const char *cmd;
  const char *payload;
} gate_dev_cmd_t;

static void _gateway_device_cmd_init(http_request_t *request, void *arg) {
  gate_dev_cmd_t *gdc = (gate_dev_cmd_t *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
           "%s/%s/devices/%s/actions/command",
           KONEXIOS_API_GATEWAY_ENDPOINT,
           gdc->g_hid,
           gdc->d_hid);
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, GET, &p_stack(uri));
  FREE_CHUNK(uri);
  JsonNode *_main = json_mkobject();
  json_append_member(_main, p_const("command"), json_mkstring(gdc->cmd));
  json_append_member(_main, p_const("deviceHid"), json_mkstring(gdc->d_hid));
  json_append_member(_main, p_const("payload"), json_mkstring(gdc->payload));
  http_request_set_payload(request, json_encode_property(_main));
  json_delete(_main);
}

int konexios_gateway_device_send_command(const char *gHid, const char *dHid, const char *cmd, const char *payload) {
  gate_dev_cmd_t gdc = {gHid, dHid, cmd, payload};
  STD_ROUTINE(_gateway_device_cmd_init, &gdc,
              NULL, NULL,
              GATEWAY_MSG, GATEWAY_DEVCOMS_ERROR);
}

typedef struct _gateway_error {
  konexios_gateway_t *gateway;
  const char *error;
} gateway_error_t;

static void _gateway_errors_init(http_request_t *request, void *arg) {
  gateway_error_t *de = (gateway_error_t *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
                     "%s/%s/errors",
                     KONEXIOS_API_GATEWAY_ENDPOINT,
                     P_VALUE(de->gateway->hid) );
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, POST, &p_stack(uri));
  FREE_CHUNK(uri);
  JsonNode *error = json_mkobject();
  json_append_member(error, p_const("error"), json_mkstring(de->error));
  http_request_set_payload(request, json_encode_property(error));
  json_delete(error);
}

int konexios_gateway_error(konexios_gateway_t *gateway, const char *error) {
  gateway_error_t de = { gateway, error };
  STD_ROUTINE(_gateway_errors_init, &de,
              NULL, NULL,
              GATEWAY_MSG, GATEWAY_ERROR);
}

static void _gateway_update_init(http_request_t *request, void *arg) {
  konexios_gateway_t *gate = (konexios_gateway_t *)arg;
  CREATE_CHUNK(uri, URI_LEN);
  int ret = snprintf(uri, URI_LEN,
                     "%s/%s",
                     KONEXIOS_API_GATEWAY_ENDPOINT,
                     P_VALUE(gate->hid) );
  if ( ret > 0 ) uri[ret] = 0x0;
  http_request_init(request, PUT, &p_stack(uri));
  FREE_CHUNK(uri);
  http_request_set_payload(request, konexios_gateway_serialize(gate));
}

static int _gateway_update_proc(http_response_t *response, void *arg) {
  SSP_PARAMETER_NOT_USED(arg);
  if ( response->m_httpResponseCode != 200 ) return -1;
  return 0;
}

int konexios_gateway_update(konexios_gateway_t *gateway) {
  STD_ROUTINE(_gateway_update_init, gateway,
              _gateway_update_proc, NULL,
              GATEWAY_MSG, GATEWAY_UPDATE_ERROR);
}