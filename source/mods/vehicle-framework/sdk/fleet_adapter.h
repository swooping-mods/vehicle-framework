#ifndef MOON_VEHICLE_FLEET_ADAPTER_H
#define MOON_VEHICLE_FLEET_ADAPTER_H
#include "fleet.h"
/* Include after the checked reflection/GC adapter. This shared source adapter
 * adds no runtime DLL dependency and never mutates native ownership. */
typedef struct { DRoot array; unsigned count; } MoonFleetSnapshot;
static struct {
  PTR owned, storage, at, live_storage, local, equal, seats, taken;
  PTR owner, holder, model, garage, deployed, live_dictionary;
  PTR (*new_array)(PTR, U64);
} mf;
static inline int mf_equal(PTR a, PTR b) {
  int ok=1; void *args[]={a,b}; PTR value=call(mf.equal,0,args,&ok);
  return ok&&value&&*(unsigned char *)unbox(value);
}
static inline int mf_read(PTR owned, MoonFleetRecord *r) {
  if(!owned||!r)return 0;
  int ok=1; MoonFleetRecord value={0}; value.record=owned;
  value.storage=call(mf.storage,owned,0,&ok);
  field_value(owned,mf.owner,&value.owner);field_value(owned,mf.holder,&value.holder);
  field_value(owned,mf.model,&value.model);field_value(owned,mf.garage,&value.garage);
  unsigned char deployed=0;field_value(owned,mf.deployed,&deployed);value.deployed=deployed;
  if(!ok||!value.storage)return 0;
  *r=value;return 1;
}
static inline int mf_occupied(PTR storage,int model,int *occupied) {
  if(!storage||model<0||!occupied||!mf.seats||!mf.taken)return 0;
  int ok=1;void *args[]={&model};PTR box=call(mf.seats,0,args,&ok);
  int count=box?*(int *)unbox(box):-1;
  if(!ok||count<0||count>16)return 0;
  *occupied=0;
  for(int i=0;i<count;i++){
    void *seat[]={storage,&i};PTR taken=call(mf.taken,0,seat,&ok);
    if(!ok||!taken)return 0;
    if(*(unsigned char *)unbox(taken)){*occupied=1;break;}
  }
  return 1;
}
static const MoonFleetQueries mf_queries={mf_equal,mf_read,mf_occupied};
static inline void mf_close(MoonFleetSnapshot *s) {
  ddrop(&s->array);s->count=0;
}
static inline int mf_snapshot(PTR world,MoonFleetSnapshot *s) {
  if(!s)return 0;
  mf_close(s);if(!world)return 0;int ok=1;
  DRoot collection=dhold(call(mf.owned,world,0,&ok));PTR items=dget(collection);
  int count=items?integer(get_method(object_class(items),"get_Count",0),items,&ok):-1;
  if(!ok||count<0||count>(int)MOON_FLEET_LIMIT){ddrop(&collection);return 0;}
  PTR array=mf.new_array(find_class("MoonCleaners","MoonOwnedVehicle"),(U64)count);
  if(!array){ddrop(&collection);return 0;}
  s->array=dhold(array);int zero=0;void *args[]={array,&zero};
  if(s->array)call(get_method(object_class(items),"CopyTo",2),items,args,&ok);else ok=0;
  ddrop(&collection);
  if(!ok){mf_close(s);return 0;}s->count=(unsigned)count;return 1;
}
static inline PTR mf_item(const MoonFleetSnapshot *s,unsigned i) {
  PTR address=i<s->count?darray(dget(s->array),i):0;
  return address?*(PTR *)address:0;
}
static inline PTR mf_lookup(PTR world,PTR storage,int *ok) {
  if(!world||!storage){*ok=0;return 0;}void *args[]={storage};
  PTR record=call(mf.at,world,args,ok);if(!*ok||!record)return 0;
  MoonFleetRecord value={0};if(!mf_read(record,&value)||!mf_equal(value.storage,storage)){*ok=0;return 0;}return record;
}
static inline PTR mf_live(PTR world,PTR storage,int *ok) {
  PTR dictionary=0,live=0;
  if(!world||!storage){*ok=0;return 0;}
  field_value(world,mf.live_dictionary,&dictionary);
  if(dictionary){void *args[]={storage,&live};call(get_method(object_class(dictionary),"TryGetValue",2),dictionary,args,ok);}
  return live;
}
static inline PTR mf_vehicle_storage(PTR vehicle,int *ok) {
  return vehicle?call(mf.live_storage,vehicle,0,ok):0;
}
static inline int mf_bind(PTR game,int occupancy) {
#define MF(F,NS,T,N,S) mf.F=dmethod(NS,T,N,S);if(!mf.F)return 0
  MF(owned,"MoonCleaners","MoonWorld","get_OwnedVehicles","");
  MF(storage,"MoonCleaners","MoonOwnedVehicle","get_Storage","");
  MF(at,"MoonCleaners","MoonWorld","OwnedVehicleAt","System.String");
  MF(live_storage,"MoonCleaners","MoonPrototypeVehicle","get_StorageId","");
  MF(local,"MoonCleaners","MoonCrew","get_LocalPlayerId","");
  MF(equal,"System","String","op_Equality","System.String;System.String");
#undef MF
  PTR owned=find_class("MoonCleaners","MoonOwnedVehicle");
  mf.owner=get_field(owned,"owner");mf.holder=get_field(owned,"holder");
  mf.model=get_field(owned,"vehicle");mf.garage=get_field(owned,"garage");
  mf.deployed=get_field(owned,"inWorld");
  mf.live_dictionary=get_field(find_class("MoonCleaners","MoonWorld"),"garageVehicles");
  mf.new_array=(PTR(*)(PTR,U64))GetProcAddress(game,"il2cpp_array_new");
  if(!mf.owner||!mf.holder||!mf.model||!mf.garage||!mf.deployed||!mf.live_dictionary||!mf.new_array)return 0;
  if(occupancy){
    mf.seats=dmethod("MoonCleaners","MoonVehicleSeats","Count","System.Int32");
    mf.taken=dmethod("MoonCleaners","MoonVehicleSeats","Taken","System.String;System.Int32");
    if(!mf.seats||!mf.taken||method_token(mf.seats)!=100671591||method_token(mf.taken)!=100671595)return 0;
  }
  return 1;
}
#endif
