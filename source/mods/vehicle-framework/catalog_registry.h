#ifndef MOON_VEHICLE_CATALOG_REGISTRY_H
#define MOON_VEHICLE_CATALOG_REGISTRY_H
#include "definitions.h"
static void (*rvr_init_class)(PTR), (*rvr_static_set)(PTR, void *),
    (*rvr_set)(PTR, PTR, void *);
static void (*rvr_barrier)(PTR, PTR *, PTR);
static SURoot mv_catalog_root, mv_offers_root;
static PTR mv_catalog_field, mv_offers_field;
static unsigned mv_native_bays;
static int rvr_scalar(PTR object, const char *name, void *value) {
  PTR f = get_field(object_class(object), name);
  if (!f)
    return 0;
  rvr_set(object, f, value);
  return 1;
}
static int rvr_reference(PTR object, const char *name, PTR value) {
  PTR f = get_field(object_class(object), name);
  if (!f)
    return 0;
  int offset = su.field_offset(f);
  if (offset < 16)
    return 0;
  rvr_barrier(object, (PTR *)((char *)object + offset), value);
  return 1;
}
static int mv_catalog_bind(PTR game) {
  rvr_init_class =
      (void (*)(PTR))GetProcAddress(game, "il2cpp_runtime_class_init");
  rvr_static_set = (void (*)(PTR, void *))GetProcAddress(
      game, "il2cpp_field_static_set_value");
  rvr_set = (void (*)(PTR, PTR, void *))GetProcAddress(
      game, "il2cpp_field_set_value");
  rvr_barrier = (void (*)(PTR, PTR *, PTR))GetProcAddress(
      game, "il2cpp_gc_wbarrier_set_field");
  if (!rvr_init_class || !rvr_static_set || !rvr_set || !rvr_barrier)
    return 0;
  PTR catalog = find_class("MoonCleaners", "MoonPrototypeCatalog"),
      world = find_class("MoonCleaners", "MoonWorld");
  if (!catalog || !world)
    return 0;
  mv_catalog_field = get_field(catalog, "Vehicles");
  mv_offers_field = get_field(world, "GarageOffers");
  if (!mv_catalog_field || !mv_offers_field)
    return 0;
  rvr_init_class(catalog);
  rvr_init_class(world);
  PTR items = 0, offers = 0;
  su.static_field(mv_catalog_field, &items);
  su.static_field(mv_offers_field, &offers);
  if (!items || su.array_length(items) != MV_VANILLA_COUNT || !offers ||
      su.array_length(offers) < 1 || su.array_length(offers) > 32)
    return 0;
  for (U64 i = 0; i < su.array_length(offers); i++) {
    PTR offer = *(PTR *)su.array_address(offers, 8, i);
    PTR field = offer ? get_field(object_class(offer), "vehicle") : 0;
    int id = -1;
    if (!field)
      return 0;
    field_value(offer, field, &id);
    if (id < 0 || id >= MV_VANILLA_COUNT)
      return 0;
  }
  mv_native_bays=(unsigned)su.array_length(offers);
  mv_catalog_root = su_hold(items);
  mv_offers_root = su_hold(offers);
  return mv_catalog_root && mv_offers_root;
}
static void mv_release_registration(MVEntry *e) {
  for (int i = 0; i < 6; i++)
    su_drop(e->roots + i);
}
static int mv_register(const MoonVehicleDefinition *definition) {
  if (!mv_validate(definition))
    return 0;
  MVEntry *e = mv_entries + definition->id - MV_VANILLA_COUNT;
  su_zero(e, sizeof(*e));
  su_copy(&e->def, definition, definition->size);
  if(e->def.hover){
    e->hover=*e->def.hover;
    su_copy(e->hover_boxes,e->hover.boxes,e->hover.box_count*sizeof(MoonHoverBox));
    e->hover.boxes=e->hover_boxes;e->def.hover=&e->hover;
  }
  e->offer = -1;
  PTR original = 0, offers = 0;
  su.static_field(mv_catalog_field, &original);
  su.static_field(mv_offers_field, &offers);
  /* Reject foreign catalog writers and lost roots before touching either
   * array. Each publication replaces only arrays owned by this framework. */
  if (original != su_get(mv_catalog_root) || offers != su_get(mv_offers_root))
    return 0;
  U64 n = su.array_length(original), on = su.array_length(offers);
  if (n > MV_LAST_ID + 1 || on > MV_LIMIT + 32)
    return 0;
  PTR donor =
      *(PTR *)su.array_address(original, 8, (U64)definition->template_id);
  PTR clone = su_method("System", "Object", "MemberwiseClone", ""),
      decimal = su_method("System", "Decimal", "op_Implicit", "System.Int32");
  int ok = 1;
  PTR spec = donor ? call(clone, donor, 0, &ok) : 0;
  if (!ok || !spec)
    goto fail;
  e->roots[0] = su_hold(spec);
  if (!e->roots[0])
    goto fail;
  if (!rvr_reference(spec, "name", su.string(definition->name)) ||
      !rvr_reference(spec, "hint", su.string(definition->hint)) ||
      !rvr_scalar(spec, "width", &e->def.width) ||
      !rvr_scalar(spec, "depth", &e->def.depth) ||
      !rvr_scalar(spec, "speed", &e->def.speed) ||
      !rvr_scalar(spec, "capacity", &e->def.capacity))
    goto fail;
  U64 count = n > (U64)definition->id ? n : (U64)definition->id + 1;
  PTR registered = su.new_array(object_class(donor), count);
  if (!registered)
    goto fail;
  e->roots[1] = su_hold(registered);
  if (!e->roots[1])
    goto fail;
  /* Reserved gaps have a valid, clearly named fallback, never null specs.
   * They have no offers. Their stable IDs can be filled in any load order. */
  PTR reserved = 0;
  if (count > n) {
    reserved = call(clone, donor, 0, &ok);
    if (!ok || !reserved)
      goto fail;
    e->roots[2] = su_hold(reserved);
    if (!e->roots[2])
      goto fail;
    if (!rvr_reference(reserved, "name",
                       su.string("Unavailable mod vehicle")) ||
        !rvr_reference(reserved, "hint",
                       su.string("Restore the matching vehicle mod set before "
                                 "saving this world.")))
      goto fail;
  }
  for (U64 i = 0; i < count; i++)
    rvr_barrier(registered, su.array_address(registered, 8, i),
                i == (U64)definition->id ? spec
                : i < n ? *(PTR *)su.array_address(original, 8, i)
                        : reserved);
  PTR registered_offers = offers;
  if (definition->garage) {
    PTR offer = *(PTR *)su.array_address(offers, 8, 0);
    if (!offer)
      goto fail;
    PTR own = call(clone, offer, 0, &ok);
    if (!ok || !own)
      goto fail;
    e->roots[3] = su_hold(own);
    if (!e->roots[3])
      goto fail;
    int price = definition->price;
    void *args[] = {&price};
    PTR money = call(decimal, 0, args, &ok);
    if (!ok || !money)
      goto fail;
    e->roots[5] = su_hold(money);
    if (!e->roots[5])
      goto fail;
    if (!rvr_scalar(own, "vehicle", &e->def.id) ||
        !rvr_scalar(own, "price", unbox(money)) ||
        !rvr_reference(own, "summary", su.string(definition->summary)))
      goto fail;
    registered_offers = su.new_array(object_class(offer), on + 1);
    if (!registered_offers)
      goto fail;
    e->roots[4] = su_hold(registered_offers);
    if (!e->roots[4])
      goto fail;
    for (U64 i = 0; i <= on; i++)
      rvr_barrier(registered_offers, su.array_address(registered_offers, 8, i),
                  i == on ? own : *(PTR *)su.array_address(offers, 8, i));
    e->offer = (int)on;
  }
  rvr_static_set(mv_catalog_field, registered);
  rvr_static_set(mv_offers_field, registered_offers);
  PTR check = 0, check_offers = 0;
  su.static_field(mv_catalog_field, &check);
  su.static_field(mv_offers_field, &check_offers);
  if (check != registered || check_offers != registered_offers) {
    rvr_static_set(mv_catalog_field, original);
    rvr_static_set(mv_offers_field, offers);
    goto fail;
  }
  SURoot next_catalog = e->roots[1],
         next_offers = definition->garage ? e->roots[4] : mv_offers_root;
  e->roots[1] = 0;
  if (definition->garage) {
    e->roots[4] = 0;
    su_drop(&mv_offers_root);
  }
  su_drop(&mv_catalog_root);
  mv_catalog_root = next_catalog;
  mv_offers_root = next_offers;
  /* New arrays retain all previous specs. Retain only the current roots. */
  mv_release_registration(e);
  e->used = 1;
  event("vehicle-registered");
  return 1;
fail:
  mv_release_registration(e);
  su_zero(e, sizeof(*e));
  return 0;
}
static int mv_offer_index(int id) {
  MVEntry *e = mv_entry(id);
  return e ? e->offer : -1;
}
#endif
