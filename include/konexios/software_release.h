#if !defined(KONEXIOS_SOFTWARE_RELEASE_H_)
#define KONEXIOS_SOFTWARE_RELEASE_H_

#if defined(__cplusplus)
extern "C" {
#endif

#include <konexios/gateway.h>
#include <konexios/device.h>
#include <data/linkedlist.h>

enum konexios_ota_result {
    FW_SUCCESS = 0x00,
    FW_MD5SUM,
    FW_DOWNLOAD_FAIL
};

enum konexios_ota_init {
    FW_FIRST = 0x00,
    FW_NEXT
};

typedef int (*__release_cb)(const char *url,
                           const char *chsum,
                           const char *from,
                           const char *to);

typedef int (*__download_init_cb)(void *arg);
typedef int (*__download_payload_cb)(const char *,int,int);
typedef int (*__download_complete_cb)(int);
typedef void (*__download_finish_cb)(void);

typedef struct _release_sched_ {
  property_t trans_hid;
  const char *schedule_hid;
  const char *release_hid;
} release_sched_t;

void create_release_schedule(release_sched_t *rs, const char *shed_hid, const char *rel_hid);
void free_release_schedule(release_sched_t *rs);

int konexios_gateway_software_releases_trans(konexios_gateway_t *gate, release_sched_t *rs);
int konexios_device_software_releases_trans(konexios_device_t *gate, release_sched_t *rs);

// mark software release transaction as failed
int konexios_software_releases_trans_fail(const char *hid, const char *error);
// mark software release transaction as received
int konexios_software_releases_trans_received(const char *hid);
// mark software release transaction as succeeded
int konexios_software_releases_trans_success(const char *hid);

// start software release transaction
int konexios_software_releases_trans_start(const char *hid);

// DeviceSoftwareRelease event handler
int ev_DeviceSoftwareRelease(void *_ev, JsonNode *_parameters);

int konexios_software_release_download(const char *token, const char *tr_hid, const char *checksum);

// set software release download callback
// will be executed when download complete
int konexios_software_release_dowload_set_cb(
        __download_init_cb icb,
        __download_payload_cb pcb,
        __download_complete_cb ccb,
        __download_finish_cb finish_callback);

int konexios_software_release(const char *token,
                           const char *chsum,
                           const char *from,
                           const char *to);

int konexios_software_release_set_cb(__release_cb cb);

// schedule
/* model
 * {
 *   "deviceCategory": "GATEWAY",
 *   "objectHids": [
 *     "string"
 *   ],
 *   "softwareReleaseHid": "string",
 *   "userHid": "string"
 * }
 */
enum schedule_device_category {
    schedule_GATEWAY,
    schedule_DEVICE
};

struct object_hid_list {
    property_t hid;
    konexios_linked_list_head_node;
};

typedef struct _schedule_ {
  int device_category;
  property_t software_release_hid;
  property_t user_hid;
  struct object_hid_list *_hids;
} konexios_schedule_t;

int konexios_schedule_model_init(konexios_schedule_t *sch, int category, property_t sw_hid, property_t user_hid);
int konexios_schedule_model_add_object(konexios_schedule_t *sch, property_t hid);
int konexios_schedule_model_free(konexios_schedule_t *sch);

//POST /api/v1/kronos/software/releases/schedules/start
int konexios_software_releases_schedules_start(konexios_schedule_t *sch);

#if defined(__cplusplus)
}
#endif

#endif  // KONEXIOS_SOFTWARE_RELEASE_H_
