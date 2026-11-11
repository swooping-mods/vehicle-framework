/* One owner for native vehicle integration; consumers supply definitions. */
#include "../../runtime/native/gameplay.h"
#include "../../runtime/native/services.h"
#include "../../runtime/native/vehicle_adapter.h"
#include "sdk/unity.h"
int _fltused = 0;
/* Large POD snapshots can lower to this intrinsic in the freestanding DLL. */
void *memcpy(void *to,const void *from,U64 n){su_copy(to,from,n);return to;}
static PTR mv_log, scene_get_layer, scene_set_layer, mv_world_field,
    mv_restoring;
static int mv_started;
static DWORD mv_thread;
static unsigned int mv_ticks;
static PTR rv_object(PTR c) {
  int ok = 1;
  return call(method("UnityEngine", "Component", "get_gameObject", 0), c, 0,
              &ok);
}
static PTR rv_transform(PTR c) {
  int ok = 1;
  return call(method("UnityEngine", "Component", "get_transform", 0), c, 0,
              &ok);
}
#include "sdk/scene_adapter.h"
static void event(const char *name) {
  if (!mv_log)
    return;
  char b[256], *p = cat(b, "{\"event\":\"");
  p = cat(p, name);
  p = cat(p, "\",\"ticks\":");
  p = num(p, mv_ticks);
  p = cat(p, "}\n");
  DWORD n = 0;
  WriteFile(mv_log, b, (DWORD)(p - b), &n, 0);
  FlushFileBuffers(mv_log);
}
#include "catalog_registry.h"
#include "hover_runtime.h"
#include "dispatch.h"
#include "berth.h"
#include "garage_ui.h"
#include "instances.h"
static int mv_index(PTR vehicle) {
  int ok = 1;
  int id = integer(mv_local_index, vehicle, &ok);
  return ok ? id : -1;
}
#include "../map-control-framework/sdk/map_controls.h"
/* Optional service: standalone native cars retain their own UI admission when
 * no shared map provider is loaded. The table is immutable after publication. */
static int mv_map_context(void *owner){
    static const MoonMapControls *maps;
    if(!moon_runtime_loader_present())return 0;
    if(!maps){const MoonRuntimeApiV1 *runtime=moon_runtime_connect();maps=runtime?(const MoonMapControls *)runtime->find("map-control-framework","map-controls",MMC_ABI,sizeof(MoonMapControls)):0;}
    return maps&&maps->abi==MMC_ABI&&maps->size>=sizeof(MoonMapControls)&&maps->context_active(owner);
}
#include "autonomous_runtime.h"
static int mv_physics(int id, Context *c, MoonVehicleInput *input) {
  (void)id;
  if (mv_thread != GetCurrentThreadId())
    return 0;
  if(mva_stepping((PTR)c->rcx))return 0;
  int index=mv_index((PTR)c->rcx);
  MVEntry *entry=mv_entry(index);
  if(mva_physics((PTR)c->rcx,entry))return 1;
  if(entry&&entry->def.hover&&flight_fixed((PTR)c->rcx,input))return 1;
  return mv_physics_callback(index, (PTR)c->rcx, input);
}
typedef struct {
  DWORD thread;
  int id;
  PTR storage;
} MVDelivery;
/* BringOut synchronously reaches Spot on the same thread. Clear at every
 * BringOut and consume once; Bay is independently keyed by its vehicle ID. */
static MVDelivery mv_delivery;
static int before(int id, Context *c) {
  if (id == 6) {
    int index = (int)c->rdx;
    MVEntry *e = mv_entry(index);
    if (e)
      c->rdx = (U64)e->def.template_id;
    else if (index >= MV_VANILLA_COUNT && index <= MV_LAST_ID)
      c->rdx = 4; /* Reserved missing-mod presentation. */
    return 0;
  }
  if (id == 3) {
    mv_delivery = (MVDelivery){GetCurrentThreadId(), (int)c->r8,(PTR)c->r9};
    return 0;
  }
  if (id == 7) {
    if (mv_delivery.thread == GetCurrentThreadId()) {
      MVEntry *e = mv_entry(mv_delivery.id);
      mv_delivery.thread = 0;
      if (e && mv_bay((PTR)c->rdx, e,mv_delivery.storage,1)) {
        int ok = 1, index = e->def.id;
        void *args[] = {&index};
        PTR pose = call(method("MoonCleaners", "MoonGarage", "Bay", 1),
                        (PTR)c->rdx, args, &ok);
        if (ok && pose) {
          su_copy((PTR)c->rcx, unbox(pose), 28);
          c->rax = c->rcx;
          return 1;
        }
        event("vehicle-delivery-failed");
      }
    }
    return 0;
  }
  if (id == 5) {
    MVEntry *e = mv_entry((int)c->r8);
    if (e)
      mv_bay((PTR)c->rdx, e,0,0);
    return 0;
  }
  if (id == 4) {
    if (mv_entry((int)c->rcx)) {
      c->rax = 0;
      return 1;
    }
    return 0;
  }
  if (!mv_thread)
    mv_thread = GetCurrentThreadId();
  if (mv_thread != GetCurrentThreadId())
    return 0;
  if (id == 1)
    return mv_physics(id, c, 0);
  if (id == 2) {
    if (mh_exit((PTR)c->rcx,(int)c->rdx&255)||mv_exit_callback(mv_index((PTR)c->rcx), (PTR)c->rcx,
                         (int)c->rdx & 255)) {
      c->rax = 0;
      return 1;
    }
    return 0;
  }
  if (id != 0)
    return 0;
  mv_sealed = 1;
  mv_ticks++;
  PTR world = 0;
  int ok = 1;
  field_value((PTR)c->rcx, mv_world_field, &world);
  mh_update((PTR)c->rcx,world);
  mv_update_callbacks((PTR)c->rcx);
  if (!su_alive(world) || boolean(mv_restoring, world, &ok) || !ok)
    return 0;
  if (mv_ticks % 10 == 0)
    mv_menu((PTR)c->rcx);
  if (mv_ticks % 60 == 0) {
    mv_instance_cleanup();
    mh_cleanup();
    mv_scan_type("MoonPrototypeVehicle", 0);
    mv_scan_type("MoonRemoteVehicle", 1);
  }
  return 0;
}
static const MoonVehicles mv_api = {MV_ABI, sizeof(MoonVehicles), mv_register,
                                    mv_configure, mv_offer_index,mh_state,mh_request};
__declspec(dllexport) const MoonVehicles *
MoonLab_Vehicles_v1(unsigned int abi, unsigned int size) {
  return mv_started && abi == MV_ABI && (size == sizeof(MoonVehicles)||size==MV_API_V1_SIZE) ? &mv_api
                                                                     : 0;
}
__declspec(dllexport) int MoonLab_ModStart(PTR domain, PTR game) {
  if (mv_started)
    return 1;
  char folder[1000], path[1100];
  DWORD n = GetEnvironmentVariableA("MOONLAB_EVIDENCE", folder, 1000);
  if (!n || n >= 1000)
    return 0;
  cat(cat(path, folder), "\\vehicle-framework.jsonl");
  mv_log = CreateFileA(path, 0x40000000, 1, 0, 1, 0x80, 0);
  if (mv_log == (PTR)(U64)-1) {
    mv_log = 0;
    return 0;
  }
  if (!bind_runtime(domain, game) || !su_bind(game) || !mv_catalog_bind(game)) {
    event("framework-bind-failed");
    return 0;
  }
  mv_local_index =
      method("MoonCleaners", "MoonPrototypeVehicle", "get_Index", 0);
  mv_remote_index = method("MoonCleaners", "MoonRemoteVehicle", "get_Index", 0);
  mv_authored_field =
      get_field(find_class("MoonCleaners", "MoonPrototypeVehicle"), "authored");
  mv_world_field = get_field(find_class("MoonCleaners", "MoonWalker"), "world");
  mv_restoring =
      method("MoonCleaners", "MoonWorld", "get_IsRestoringRendering", 0);
  mv_find_objects = su_method("UnityEngine", "Object", "FindObjectsByType",
                              "System.Type;UnityEngine.FindObjectsInactive;"
                              "UnityEngine.FindObjectsSortMode");
  scene_get_layer = method("UnityEngine", "GameObject", "get_layer", 0);
  scene_set_layer = method("UnityEngine", "GameObject", "set_layer", 1);
  if (!mv_local_index || !mv_remote_index || !mv_authored_field ||
      !mv_find_objects || !scene_get_layer || !scene_set_layer ||
      !mv_world_field || !mv_restoring || !mv_berth_bind() || !mh_bind(game) || !mva_bind(game,mv_map_context))
    return 0;
  if (!prepare_hook(method("MoonCleaners", "MoonWalker", "Update", 0),
                    100671733, 0, game) ||
      !prepare_hook(
          method("MoonCleaners", "MoonPrototypeVehicle", "FixedUpdate", 0),
          100669323, 1, game) ||
      !prepare_hook(su_method("MoonCleaners", "MoonPrototypeVehicle", "Exit",
                              "System.Boolean"),
                    100669266, 2, game) ||
      !prepare_hook(method("MoonCleaners", "MoonWorld", "BringOut", 3),
                    100665193, 3, game) ||
      !prepare_hook(method("MoonCleaners", "MoonGarageMenu", "Render", 1),
                    100667223, 4, game) ||
      !prepare_hook(method("MoonCleaners", "MoonGarage", "Bay", 1), 100667153,
                    5, game) ||
      !prepare_hook(method("MoonCleaners", "MoonPrototypeModels", "Vehicle", 2),
                    100669186, 6, game) ||
      !prepare_hook(method("MoonCleaners", "MoonGarage", "Spot", 0), 100667154,
                    7, game)) {
    event("framework-hook-shape-failed");
    return 0;
  }
  before_hook = before;
  if (!install_vehicle_adapter(2, 1, mv_physics) || !install_hooks()) {
    event("framework-hook-install-failed");
    return 0;
  }
  if (moon_runtime_loader_present()) {
    const MoonRuntimeApiV1 *runtime = moon_runtime_connect();
    const MoonServiceLimitV1 limits[] = {{"max_definitions", MV_LIMIT}, {"max_vehicle_id", MV_LAST_ID}};
    MoonServicePublicationLimitsV1 publication = {
        .base = {.size = sizeof(publication), .service_id = "vehicles", .abi = MV_ABI,
                 .api_size = sizeof(mv_api), .api = &mv_api},
        .limit_count = sizeof(limits) / sizeof(limits[0]), .reserved = 0, .limits = limits};
    if (!runtime || !runtime->publish(&publication.base))
      return 0;
    const MoonServiceLimitV1 auto_limits[] = {{"max_cars", MVA_CAR_LIMIT}};
    MoonServicePublicationLimitsV1 auto_publication = {
        .base = {.size = sizeof(auto_publication), .service_id = "autonomous", .abi = MVA_ABI,
                 .api_size = sizeof(mva_api), .api = &mva_api},
        .limit_count = 1, .reserved = 0, .limits = auto_limits};
    if (!runtime->publish(&auto_publication.base))return 0;
  }
  mv_started = 1;
  event("ready");
  return 1;
}
