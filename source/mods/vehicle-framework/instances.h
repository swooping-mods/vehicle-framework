/* Per-instance bookkeeping belongs to this runtime, not source prefabs. */
typedef struct {
  SURoot vehicle;
  MVEntry *entry;
  int remote, spawned;
} MVInstance;
static MVInstance mv_instances[256];
static PTR mv_find_objects, mv_local_index, mv_remote_index, mv_authored_field;
static int mv_configure(void *vehicle, void *authored, int remote) {
  int ok = 1,
      index = integer(remote ? mv_remote_index : mv_local_index, vehicle, &ok);
  MVEntry *entry = mv_entry(index);
  if (!ok || !entry)
    return 0;
  for (unsigned i = 0; i < 256; i++)
    if (su_get(mv_instances[i].vehicle) == vehicle)
      return 1;
  MVInstance *state = 0;
  for (unsigned i = 0; i < 256; i++)
    if (!mv_instances[i].vehicle) {
      state = mv_instances + i;
      break;
    }
  if (!state)
    return 0;
  if (!remote && entry->def.cockpit >= 0) {
    unsigned char closed = (unsigned char)entry->def.cockpit;
    if (!rvr_scalar(authored, "enclosedCab", &closed))
      return 0;
    if (entry->def.fit_seat) {
      PTR anchor = 0, seat = 0,
          af = get_field(object_class(authored), "driverAnchor"),
          sf = get_field(object_class(vehicle), "seat");
      if (!af || !sf)
        return 0;
      field_value(authored, af, &anchor);
      field_value(vehicle, sf, &seat);
      if (!su_alive(anchor) || !su_alive(seat))
        return 0;
      PTR root = rv_transform(vehicle);
      if (!root)
        return 0;
      if (seat == anchor) {
        PTR go = rv_child("Mod vehicle pilot origin", root);
        if (!go)
          return 0;
        seat = su_transform(go);
        if (!rvr_reference(vehicle, "seat", seat)) {
          su_destroy(go);
          return 0;
        }
      }
      su_parent(anchor, root);
      su_parent(seat, root);
      su_set_vector(su.tr_local_position, anchor,
                    su_v(entry->def.driver[0], entry->def.driver[1],
                         entry->def.driver[2]));
      su_set_vector(
          su.tr_local_position, seat,
          su_v(entry->def.seat[0], entry->def.seat[1], entry->def.seat[2]));
      su_set_quat(su.tr_local_rotation, anchor, (SUQuat){0, 0, 0, 1});
      su_set_quat(su.tr_local_rotation, seat, (SUQuat){0, 0, 0, 1});
    }
  }
  state->vehicle = su_hold(vehicle);
  if (!state->vehicle)
    return 0;
  state->entry = entry;
  state->remote = remote;
  return 1;
}
static void mv_scan_type(const char *name, int remote) {
  int ok = 1, inactive = 0, sort = 0;
  PTR type = su_type("MoonCleaners", name);
  void *args[] = {type, &inactive, &sort};
  PTR array = call(mv_find_objects, 0, args, &ok);
  U64 count = array ? su.array_length(array) : 0;
  if (!ok || !array || count > 2048)
    return;
  for (U64 i = 0; i < count; i++) {
    PTR vehicle = *(PTR *)su.array_address(array, 8, i);
    if (!su_alive(vehicle))
      continue;
    int index =
        integer(remote ? mv_remote_index : mv_local_index, vehicle, &ok);
    MVEntry *entry = mv_entry(index);
    if (!ok || !entry)
      continue;
    PTR authored = 0, field = remote
                                  ? get_field(object_class(vehicle),
                                              "<AuthoredView>k__BackingField")
                                  : mv_authored_field;
    if (!field)
      continue;
    field_value(vehicle, field, &authored);
    if (!su_alive(authored) || !mv_configure(vehicle, authored, remote))
      continue;
    for (unsigned j = 0; j < 256; j++)
      if (su_get(mv_instances[j].vehicle) == vehicle &&
          !mv_instances[j].spawned) {
        if(!remote&&!mh_prepare(vehicle,entry->def.hover,index))break;
        if (!entry->def.spawn || entry->def.spawn(vehicle, remote))
          mv_instances[j].spawned = 1;
        else {MHInstance *hover=mh_instance(vehicle);if(hover)mh_drop(hover);}
        break;
      }
  }
}
static void mv_instance_cleanup(void) {
  for (unsigned i = 0; i < 256; i++)
    if (mv_instances[i].vehicle && !su_alive(su_get(mv_instances[i].vehicle))) {
      MVInstance *v = mv_instances + i;
      MHInstance *hover=mh_instance(su_get(v->vehicle));if(hover&&!mh_drop(hover))continue;
      if (v->spawned && v->entry->def.destroy)
        v->entry->def.destroy(su_get(v->vehicle), v->remote);
      su_drop(&v->vehicle);
      su_zero(v, sizeof(*v));
    }
}
