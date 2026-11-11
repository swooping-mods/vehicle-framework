/* Each vehicle's identity selects exactly one provider. A false result keeps
 * vanilla physics/exit; other registered providers never see this vehicle. */
static void mv_update_callbacks(void *walker) {
  for (int i = 0; i < MV_LIMIT; i++)
    if (mv_entries[i].used && mv_entries[i].def.update)
      mv_entries[i].def.update(walker);
}
static int mv_physics_callback(int index, void *vehicle,
                               const MoonVehicleInput *input) {
  MVEntry *entry = mv_entry(index);
  return entry && entry->def.physics ? entry->def.physics(vehicle, input) : 0;
}
static int mv_exit_callback(int index, void *vehicle, int jump) {
  MVEntry *entry = mv_entry(index);
  return entry && entry->def.exit ? entry->def.exit(vehicle, jump) : 0;
}
