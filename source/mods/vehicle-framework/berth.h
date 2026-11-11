/* The query is bounded and rooted. A saturated result cannot establish a
 * clear bay. Only the exact recalled storage identity may be ignored. */
static SURoot mv_berth_hits;
static PTR mv_overlap_box,mv_collider_parent,mv_string_equal;
static int mv_berth_bind(void){
  mv_overlap_box=su_method("UnityEngine","Physics","OverlapBoxNonAlloc","UnityEngine.Vector3;UnityEngine.Vector3;UnityEngine.Collider[];UnityEngine.Quaternion;System.Int32;UnityEngine.QueryTriggerInteraction");
  mv_collider_parent=su_method("UnityEngine","Component","GetComponentInParent","System.Type;System.Boolean");
  mv_string_equal=su_method("System","String","op_Equality","System.String;System.String");
  mv_berth_hits=su_hold(su.new_array(find_class("UnityEngine","Collider"),64));
  return mv_overlap_box&&mv_collider_parent&&mv_string_equal&&mv_berth_hits;
}
static int mv_berth_clear(SPVec3 point,SPVec3 up,SUQuat rotation,MVEntry *e,PTR storage){
  SPVec3 half={e->def.width*.5f+.06f,e->def.height*.5f-.05f,e->def.depth*.5f+.06f};
  if(half.y<.05f)half.y=.05f;
  SPVec3 middle=su_add(point,su_mul(up,e->def.height*.5f+.08f));
  int mask=~(1<<2),triggers=1,ok=1;PTR hits=su_get(mv_berth_hits);
  void *a[]={&middle,&half,hits,&rotation,&mask,&triggers};PTR result=call(mv_overlap_box,0,a,&ok);
  if(!ok||!result)return 0;int n=*(int *)unbox(result);if(n<0||n>=64)return 0;
  unsigned char inactive=0;void *parent[]={su_type("MoonCleaners","MoonPrototypeVehicle"),&inactive};
  for(int i=0;i<n;i++){
    PTR collider=*(PTR *)su.array_address(hits,8,(U64)i),vehicle=call(mv_collider_parent,collider,parent,&ok);
    if(!ok)return 0;
    if(vehicle&&storage){
      PTR own=0;field_value(vehicle,get_field(object_class(vehicle),"<StorageId>k__BackingField"),&own);
      void *same[]={own,storage};PTR equal=call(mv_string_equal,0,same,&ok);if(ok&&equal&&*(unsigned char *)unbox(equal))continue;
    }
    return 0;
  }
  return 1;
}
static int mv_bay(PTR garage, MVEntry *entry,PTR storage,int refresh) {
  int rvr_bay_index = entry->offer;
  if (rvr_bay_index < 0)
    return 0;
  PTR field = get_field(object_class(garage), "bays"), bays = 0;
  if (!field)
    return 0;
  field_value(garage, field, &bays);
  U64 count = bays ? su.array_length(bays) : 0;
  if (count > MV_LIMIT + 32 || rvr_bay_index > MV_LIMIT + 32)
    return 0;
  if (!refresh && count > (U64)rvr_bay_index &&
      su_alive(*(PTR *)su.array_address(bays, 8, (U64)rvr_bay_index)))
    return 1;
  PTR tr = rv_transform(garage);
  SPVec3 origin=su_vector(su.tr_get_position,tr),up=su_vector(su.tr_up,tr);
  SPVec3 forward=su_vector(su.tr_forward,tr),right=su_vector(su.tr_right,tr);
  int mask=~(1<<2),triggers=1,ok=1,inside=0;PTR anchor=0;
  SPVec3 chosen={0};SUQuat rotation=su_rotation(forward,up);
  /* Spot applies the game's radial floor placement to its actual delivery
   * pad; Bay anchors can instead be showroom poses beside machinery. The
   * delivery context is already consumed, so this call reaches native Spot. */
  if(refresh){
    PTR pose=call(method("MoonCleaners","MoonGarage","Spot",0),garage,0,&ok);
    if(ok&&pose){
      SPVec3 point;SUQuat q;su_copy(&point,unbox(pose),12);su_copy(&q,(char *)unbox(pose)+12,16);
      if(mv_berth_clear(point,up,q,entry,storage)){chosen=point;rotation=q;inside=3;}
    }
  }
  /* Native bays already carry the garage's intended floor pose. Test each
   * actual hull-sized box, preserving all original anchors and occupants. */
  for(unsigned i=0;!inside&&i<mv_native_bays&&i<count;i++){
    PTR bay=*(PTR *)su.array_address(bays,8,i);if(!su_alive(bay))continue;
    SPVec3 point=su_vector(su.tr_get_position,bay),bay_up=su_vector(su.tr_up,bay);
    SUQuat q=su_rotation(su_vector(su.tr_forward,bay),bay_up);
    if(mv_berth_clear(point,bay_up,q,entry,storage)){chosen=point;rotation=q;inside=1;break;}
  }
  static const float candidates[][2]={{0,7},{0,-7},{7,0},{-7,0},{7,7},{-7,7},{7,-7},{-7,-7},{0,12},{0,-12},{0,18},{0,-18}};
  for(unsigned i=0;!inside&&i<sizeof(candidates)/sizeof(*candidates);i++){
    float span=entry->def.width>entry->def.depth?entry->def.width:entry->def.depth;
    float scale=span>4.5f?span/4.5f:1;
    SPVec3 point=su_add(origin,su_add(su_mul(right,candidates[i][0]*scale),su_mul(forward,candidates[i][1]*scale)));
    SPVec3 start=su_add(point,su_mul(up,20)),down=su_mul(up,-1);float length=40;
    union{U64 align;unsigned char data[96];}hit;su_zero(&hit,sizeof(hit));
    void *ray[]={&start,&down,hit.data,&length,&mask,&triggers};PTR found=call(su.physics_ray,0,ray,&ok);
    if(!ok||!found||!*(unsigned char *)unbox(found))continue;
    SPVec3 normal;su_copy(&point,hit.data+su.hit_point,12);su_copy(&normal,hit.data+su.hit_normal,12);
    if(su_dot(normal,up)<.9f)continue;
    point=su_add(point,su_mul(up,.15f));
    if(mv_berth_clear(point,up,rotation,entry,storage)){chosen=point;inside=2;}
  }
  if(inside){
    if(count>(U64)rvr_bay_index)anchor=*(PTR *)su.array_address(bays,8,(U64)rvr_bay_index);
    if(!su_alive(anchor)){PTR go=rv_child("Mod vehicle berth",tr);if(!go)return 0;anchor=su_transform(go);}
    su_set_vector(su.tr_position,anchor,chosen);su_set_quat(su.tr_rotation,anchor,rotation);
    event(inside==3?"vehicle-garage-delivery-ready":inside==1?"vehicle-garage-bay-ready":"vehicle-apron-berth-ready");
  }
  if (!anchor) {
    event("vehicle-berth-unavailable");
    return 0;
  }
  if(count>(U64)rvr_bay_index&&*(PTR *)su.array_address(bays,8,(U64)rvr_bay_index)==anchor)
    return 1;
  U64 total = count > (U64)rvr_bay_index ? count : (U64)rvr_bay_index + 1;
  SURoot anchor_root = su_hold(anchor);
  PTR array = su.new_array(find_class("UnityEngine", "Transform"), total);
  if (!array) {
    su_destroy(rv_object(anchor));
    su_drop(&anchor_root);
    return 0;
  }
  SURoot array_root = su_hold(array);
  for (U64 i = 0; i < count; i++)
    rvr_barrier(array, su.array_address(array, 8, i),
                *(PTR *)su.array_address(bays, 8, i));
  rvr_barrier(array, su.array_address(array, 8, (U64)rvr_bay_index), anchor);
  rvr_set(garage, field, array);
  PTR actual = 0;
  field_value(garage, field, &actual);
  su_drop(&array_root);
  su_drop(&anchor_root);
  if (actual != array)
    return 0;
  return 1;
}
