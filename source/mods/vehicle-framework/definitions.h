#ifndef MOON_VEHICLE_DEFINITIONS_H
#define MOON_VEHICLE_DEFINITIONS_H
#include "sdk/vehicle.h"
typedef struct {
  MoonVehicleDefinition def;
  MoonHoverProfile hover;
  MoonHoverBox hover_boxes[MV_HOVER_BOX_LIMIT];
  int used, offer;
  SURoot roots[6], texture, sprite;
} MVEntry;
static MVEntry mv_entries[MV_LIMIT];
static int mv_sealed;
static int mv_equal(const char *a, const char *b) {
  if (!a || !b)
    return a == b;
  while (*a && *a == *b) {
    a++;
    b++;
  }
  return *a == *b;
}
static int mv_text(const char *s, int limit) {
  if (!s || !*s)
    return 0;
  for (int i = 0; i < limit; i++)
    if (!s[i])
      return 1;
  return 0;
}
static int mv_number(float x, float low, float high) {
  return x >= low && x <= high;
}
static MVEntry *mv_entry(int id) {
  return id >= MV_VANILLA_COUNT && id <= MV_LAST_ID &&
                 mv_entries[id - MV_VANILLA_COUNT].used
             ? mv_entries + id - MV_VANILLA_COUNT
             : 0;
}
static int mv_validate(const MoonVehicleDefinition *d) {
  if (!d || d->abi != MV_ABI ||
      (d->size != sizeof(*d) && d->size != MV_DEFINITION_V1_SIZE) || mv_sealed)
    return 0;
  if (d->id < MV_VANILLA_COUNT || d->id > MV_LAST_ID || d->template_id < 0 ||
      d->template_id >= MV_VANILLA_COUNT)
    return 0;
  if (!mv_text(d->key, 96) || !mv_text(d->name, 256) ||
      !mv_text(d->hint, 1024) || !mv_text(d->summary, 1024))
    return 0;
  for (const char *p = d->key; *p; p++)
    if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '-' ||
          *p == '.'))
      return 0;
  if (d->price < 0 || d->capacity < 0 || d->capacity > 10000 ||
      (d->garage != 0 && d->garage != 1) || d->cockpit < -1 || d->cockpit > 1 ||
      (d->fit_seat != 0 && d->fit_seat != 1))
    return 0;
  if (!mv_number(d->width, .1f, 100) || !mv_number(d->depth, .1f, 100) ||
      !mv_number(d->height, .1f, 100) || !mv_number(d->speed, 0, 1000))
    return 0;
  for (int i = 0; i < 3; i++)
    if (!mv_number(d->driver[i], -100, 100) ||
        !mv_number(d->seat[i], -100, 100))
      return 0;
  if (d->icon_png) {
    if (!d->icon_size || d->icon_size > 16 * 1024 * 1024 || !d->icon_width ||
        !d->icon_height || d->icon_width > 4096 || d->icon_height > 4096)
      return 0;
  } else if (d->icon_size || d->icon_width || d->icon_height)
    return 0;
  if(d->size==sizeof(*d)&&d->hover&&
     (!mv_hover_profile_valid(d->hover)||d->physics||d->exit))return 0;
  for (int i = 0; i < MV_LIMIT; i++)
    if (mv_entries[i].used && (mv_entries[i].def.id == d->id ||
                               mv_equal(mv_entries[i].def.key, d->key)))
      return 0;
  return 1;
}
#endif
