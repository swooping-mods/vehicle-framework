#ifndef MOON_VEHICLE_FRAMEWORK_V1_H
#define MOON_VEHICLE_FRAMEWORK_V1_H
#include "../../../runtime/native/vehicle_input.h"
#include "hover.h"
#define MV_ABI 1u
#define MV_VANILLA_COUNT 22
#define MV_LIMIT 64
#define MV_LAST_ID (MV_VANILLA_COUNT + MV_LIMIT - 1)
/* IDs are a save/network contract. Reserve an explicit ID; never derive it
 * from load order. Strings, PNG bytes and callbacks must outlive the process.
 */
typedef struct {
  unsigned int abi, size;
  const char *key, *name, *hint, *summary;
  int id, template_id, price, capacity, garage;
  float width, depth, height, speed;
  int cockpit; /* -1: native template; 0: open; 1: enclosed */
  int fit_seat;
  float driver[3], seat[3];
  const unsigned char *icon_png;
  unsigned int icon_size, icon_width, icon_height;
  void (*update)(void *walker);
  int (*physics)(void *vehicle, const MoonVehicleInput *input);
  int (*exit)(void *vehicle, int jump);
  int (*spawn)(void *vehicle, int remote);
  void (*destroy)(void *vehicle, int remote);
  /* Optional tail: omitted by legacy v1 consumers. A profile gives the
   * framework exclusive physics/exit ownership for this vehicle. */
  const MoonHoverProfile *hover;
} MoonVehicleDefinition;
#define MV_DEFINITION_V1_SIZE ((unsigned int)__builtin_offsetof(MoonVehicleDefinition,hover))
typedef struct {
  unsigned int abi, size;
  int (*register_vehicle)(const MoonVehicleDefinition *definition);
  int (*configure_instance)(void *vehicle, void *authored, int remote);
  int (*offer_index)(int id);
  int (*hover_state)(void *vehicle, MoonHoverState *state, unsigned int size);
  int (*hover_request)(void *vehicle, int action);
} MoonVehicles;
#define MV_API_V1_SIZE ((unsigned int)__builtin_offsetof(MoonVehicles,hover_state))
typedef const MoonVehicles *(*MoonVehiclesGet)(unsigned int abi,
                                               unsigned int size);
/* Call during MoonLab_ModStart. The loader starts the required framework
 * first; standalone clients start the same owned dependency explicitly. */
#endif
