#ifndef MOON_VEHICLE_FLEET_H
#define MOON_VEHICLE_FLEET_H
/* Read-only fleet queries. Storage strings identify an instance; model IDs and
 * owner IDs deliberately never select a vehicle. Returned managed references are borrowed;
 * retain independent roots when keeping them beyond the immediate query. */
#define MOON_FLEET_LIMIT 256u
typedef struct {
  void *record, *storage, *owner, *holder;
  int model, garage, deployed;
} MoonFleetRecord;
typedef struct {
  int (*equal)(void *, void *);
  int (*read)(void *, MoonFleetRecord *);
  int (*occupied)(void *, int, int *);
} MoonFleetQueries;
static inline int moon_fleet_local(const MoonFleetQueries *q,
                                   const MoonFleetRecord *r, void *player) {
  return q && q->equal && r && r->record && r->owner && player &&
         q->equal(r->owner, player);
}
static inline int moon_fleet_find(const MoonFleetQueries *q, void *const *records,
                                  unsigned count, void *storage,
                                  MoonFleetRecord *result) {
  if (!q || !q->read || !q->equal || !records || !storage || !result ||
      count > MOON_FLEET_LIMIT) return 0;
  int found = 0;
  MoonFleetRecord candidate = {0};
  for (unsigned i = 0; i < count; i++) {
    MoonFleetRecord r = {0};
    if (!q->read(records[i], &r)) return 0;
    if (r.storage && q->equal(r.storage, storage)) {
      /* Corrupt duplicate storage identities must not choose an arbitrary car. */
      if (found) return 0;
      candidate = r;
      found = 1;
    }
  }
  if (found) *result = candidate;
  return found;
}
/* Native passenger seats only; driver activity remains a live-vehicle query. */
static inline int moon_fleet_unoccupied(const MoonFleetQueries *q,
                                        const MoonFleetRecord *r) {
  int occupied = 1;
  return q && q->occupied && r && r->storage && r->model >= 0 &&
         q->occupied(r->storage, r->model, &occupied) && !occupied;
}
#endif
