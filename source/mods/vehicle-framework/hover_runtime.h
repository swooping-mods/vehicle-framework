/* Shared runtime: one physics/input owner, isolated state per vehicle.
 * Providers own presentation and their fitted hull, never the control loop. */
#include "sdk/hover_input.h"
#include "../../runtime/native/input.h"
#include "hover_probe_cache.h"
static PTR world_field,walker_body_field,restoring,ui_blocked,typing,focused,find_objects,component_component,component_parent;
static PTR body_field,owner_method,driven_method,vehicle_enter;
static PTR rb_velocity,rb_get_velocity,rb_position,rb_move_rotation,rb_angular,rb_constraints,rb_get_constraints;
static PTR rb_ccd,rb_interpolate,rb_gravity,fixed_delta,hit_body,ray_hits,notify;
static PTR vehicle_grounded,vehicle_airborne,vehicle_wheels,vehicle_drive,vehicle_steer,vehicle_air_control;
static PTR get_grounded,get_airborne,get_wheels,get_air_control,body_cached_ptr;
/* The managed getter is stripped in this pin. UnityPlayer exposes the exact
 * native injected binding, whose self argument is Object.m_CachedPtr. */
static unsigned char (*mh_get_gravity)(PTR);
static PTR mh_native_body(PTR body){PTR native=0;field_value(body,body_cached_ptr,&native);return native;}
#include "sdk/hover_body.h"
static SURoot query_hits,world_root,walker_root,equipment_root;
static short (*key_state)(int);
static PTR (*foreground_window)(void);
static DWORD (*window_pid)(PTR,DWORD *);
static int mh_ready,mh_frames,mh_was_toggle,mh_was_hill,mh_test;
static RVVec center;
typedef struct {
    SURoot vehicle,body,transform,tracks,colliders[64];unsigned collider_count;
    int tracks_enabled,gravity,grounded,airborne,wheels,air_control,snapshot,failed;
    const MoonHoverProfile *profile;int id;
    RVFlight flight;RVConstraints constraints;RVOutput output;MHProbeCache probe_cache;
    RVVec position,measured,floor_up,previous_position;RVPreview preview;int has_position,has_floor;
    float clearance,terrain_min_clearance,terrain_nose_up;
    unsigned int terrain_ticks,preview_ticks,missing_floor_ticks;
    int query_count,query_status;float query_gap,query_normal;
} MHInstance;
static MHInstance mh_instances[128];
static inline RVVec rv_from(SPVec3 v){return rv_v(v.x,v.y,v.z);}
static inline SPVec3 rv_to(RVVec v){return su_v(v.x,v.y,v.z);}
static char *fixed_text(char *p,float value){
    if(!rv_finite(value))return cat(p,"null");if(value<0){*p++='-';value=-value;}
    U64 n=(U64)(value*1000+.5f);p=num(p,n/1000);*p++='.';*p++=(char)('0'+n/100%10);*p++=(char)('0'+n/10%10);*p++=(char)('0'+n%10);*p=0;return p;
}
static char *vector_text(char *p,RVVec v){*p++='[';p=fixed_text(p,v.x);*p++=',';p=fixed_text(p,v.y);*p++=',';p=fixed_text(p,v.z);*p++=']';*p=0;return p;}
static void sample(const char *name,MHInstance *v,const RVOutput *o,RVVec position,RVVec measured){
    char b[2048],*p=cat(b,"{\"event\":\"");p=cat(p,name);p=cat(p,"\",\"ticks\":");p=num(p,(U64)v->terrain_ticks);p=cat(p,",\"instance\":");p=num(p,(U64)(v-mh_instances));p=cat(p,",\"vehicle_id\":");p=num(p,(U64)v->id);p=cat(p,",\"seconds\":");p=fixed_text(p,su_number(su.time_unscaled,0));p=cat(p,",\"position\":");p=vector_text(p,position);
    p=cat(p,",\"measured_velocity\":");p=vector_text(p,measured);p=cat(p,",\"commanded_velocity\":");p=vector_text(p,o->velocity);p=cat(p,",\"pitch\":");p=fixed_text(p,o->pitch);
    p=cat(p,",\"measured_up\":");p=vector_text(p,rv_from(su_vector(su.tr_up,su_get(v->transform))));p=cat(p,",\"gravity_up\":");p=vector_text(p,v->flight.up);
    p=cat(p,",\"roll\":");p=fixed_text(p,o->roll);p=cat(p,",\"clearance\":");p=fixed_text(p,v->clearance);
    p=cat(p,",\"terrain_ticks\":");p=num(p,(U64)v->terrain_ticks);p=cat(p,",\"preview_ticks\":");p=num(p,(U64)v->preview_ticks);
    p=cat(p,",\"missing_floor_ticks\":");p=num(p,(U64)v->missing_floor_ticks);p=cat(p,",\"terrain_min_clearance\":");p=fixed_text(p,v->terrain_min_clearance);
    p=cat(p,",\"terrain_nose_up\":");p=fixed_text(p,v->terrain_nose_up);
    p=cat(p,",\"forward\":");p=vector_text(p,o->forward);p=cat(p,",\"floor_up\":");p=vector_text(p,v->floor_up);
    p=cat(p,",\"preview\":");p=num(p,(U64)v->preview.valid);p=cat(p,",\"preview_gap\":");p=fixed_text(p,v->preview.clearance);
    p=cat(p,",\"preview_distance\":");p=fixed_text(p,v->preview.distance);p=cat(p,",\"preview_up\":");p=vector_text(p,v->preview.normal);
    p=cat(p,",\"query_count\":");p=num(p,(U64)v->query_count);p=cat(p,",\"query_status\":");p=fixed_text(p,(float)v->query_status);p=cat(p,",\"query_gap\":");p=fixed_text(p,v->query_gap);p=cat(p,",\"query_normal\":");p=fixed_text(p,v->query_normal);p=cat(p,",\"support_count\":");p=num(p,v->profile->box_count*5);p=cat(p,",\"hit_stride\":");p=num(p,(U64)su.hit_size);
    p=cat(p,",\"floor_queries\":");p=num(p,v->probe_cache.queries);p=cat(p,",\"floor_cache_hits\":");p=num(p,v->probe_cache.hits);p=cat(p,",\"has_floor\":");p=num(p,(U64)v->has_floor);p=cat(p,",\"landing\":");p=num(p,(U64)v->flight.landing);p=cat(p,"}\n");DWORD n=0;WriteFile(mv_log,b,(DWORD)(p-b),&n,0);
}
static PTR game_component(PTR component,const char *name){int ok=1;PTR type=su_type("MoonCleaners",name);void *a[]={type};return call(component_component,component,a,&ok);}
static void notice(const char *message){int ok=1;PTR equipment=su_get(equipment_root);if(!su_alive(equipment))return;void *a[]={su.string(message)};call(notify,equipment,a,&ok);}
static int key(int code){return moon_input_key((unsigned)code);}
/* Physical polling only supplies exclusion/edge bookkeeping and private F1;
 * delivered gameplay keys always go through shared Input admission. */
static int raw_key(int code){return (key_state(code)&0x8000)!=0;}
static float mh_keyboard_axis(int positive,int negative,int *native_allowed){
    int p=key(positive),n=key(negative);
    return rv_keyboard_axis(p,n,raw_key(positive),raw_key(negative),native_allowed);
}
static int input_allowed(PTR walker){
    int ok=1;DWORD pid=0;window_pid(foreground_window(),&pid);
    return pid==GetCurrentProcessId()&&boolean(focused,0,&ok)&&!boolean(ui_blocked,walker,&ok)&&!boolean(typing,0,&ok)&&ok;
}

static void find_equipment(PTR walker){
    if(su_alive(su_get(equipment_root)))return;su_drop(&equipment_root);
    PTR equipment=game_component(walker,"MoonPrototypeEquipment");
    if(!equipment){
        int ok=1,inactive=0,sort=0;PTR type=su_type("MoonCleaners","MoonPrototypeEquipment");void *a[]={type,&inactive,&sort};PTR array=call(find_objects,0,a,&ok);
        U64 n=array?su.array_length(array):0;if(n>64)return;
        for(U64 i=0;i<n;i++){PTR candidate=*(PTR *)su.array_address(array,8,i);PTR player=call(method("MoonCleaners","MoonPrototypeEquipment","get_Player",0),candidate,0,&ok);if(ok&&player==walker){equipment=candidate;break;}}
    }
    if(equipment){equipment_root=su_hold(equipment);event("equipment-ready");}
}

static MHInstance *mh_instance(PTR vehicle){for(unsigned i=0;i<128;i++)if(su_get(mh_instances[i].vehicle)==vehicle)return mh_instances+i;return 0;}
static int mh_release(MHInstance *v){
    v->flight.active=0;
    if(!rv_constraints_release(&v->constraints,su_get(v->body))){event("hover-constraint-restore-failed");return 0;}
    if(v->snapshot&&su_alive(su_get(v->body))){
        int ok=1;unsigned char gravity=(unsigned char)v->gravity;void *a[]={&gravity};call(rb_gravity,su_get(v->body),a,&ok);
        unsigned char grounded=(unsigned char)v->grounded,airborne=(unsigned char)v->airborne,control=(unsigned char)v->air_control;
        void *g[]={&grounded},*b[]={&airborne},*c[]={&control},*w[]={&v->wheels};
        call(vehicle_grounded,su_get(v->vehicle),g,&ok);call(vehicle_airborne,su_get(v->vehicle),b,&ok);
        call(vehicle_air_control,su_get(v->vehicle),c,&ok);call(vehicle_wheels,su_get(v->vehicle),w,&ok);
        PTR native=mh_native_body(su_get(v->body));if(!ok||!native||mh_get_gravity(native)!=(unsigned char)v->gravity)return 0;
    }
    v->snapshot=0;v->probe_cache.valid=0;return 1;
}
static int mh_acquire(MHInstance *v){
    if(v->snapshot)return 1;int ok=1;PTR body=su_get(v->body),vehicle=su_get(v->vehicle);
    PTR native=mh_native_body(body);if(!native)return 0;v->gravity=(int)mh_get_gravity(native);v->grounded=boolean(get_grounded,vehicle,&ok);
    v->airborne=boolean(get_airborne,vehicle,&ok);v->wheels=integer(get_wheels,vehicle,&ok);v->air_control=boolean(get_air_control,vehicle,&ok);
    if(!ok)return 0;v->snapshot=1;unsigned char disabled=0;void *a[]={&disabled};call(rb_gravity,body,a,&ok);
    if(!ok||mh_get_gravity(native)!=0){mh_release(v);return 0;}return 1;
}
static int mh_drop(MHInstance *v){
    if(!mh_release(v))return 0;
    if(su_alive(su_get(v->tracks)))su_bool(method("UnityEngine","Behaviour","set_enabled",1),su_get(v->tracks),v->tracks_enabled);
    su_drop(&v->tracks);
    for(unsigned i=0;i<v->collider_count;i++){
        PTR c=su_get(v->colliders[i]);if(su_alive(c))su_bool(su.collider_enabled,c,1);su_drop(v->colliders+i);
    }
    su_drop(&v->transform);su_drop(&v->body);su_drop(&v->vehicle);su_zero(v,sizeof(*v));return 1;
}
static int mh_prepare(PTR vehicle,const MoonHoverProfile *profile,int id){
    if(!profile)return 1;if(mh_instance(vehicle))return 1;
    MHInstance *v=0;for(unsigned i=0;i<128;i++)if(!mh_instances[i].vehicle){v=mh_instances+i;break;}if(!v)return 0;
    PTR body=0;field_value(vehicle,body_field,&body);if(!su_alive(body))return 0;
    PTR transform=rv_transform(vehicle);if(!su_alive(transform))return 0;
    v->vehicle=su_hold(vehicle);v->body=su_hold(body);v->transform=su_hold(transform);v->profile=profile;v->id=id;v->terrain_min_clearance=1e6f;
    if(!v->vehicle||!v->body||!v->transform)goto fail;
    if(profile->replace_native_solids){
        int ok=1;unsigned char typed=1,recursive=1,inactive=1,reverse=0;
        void *a[]={su_type("UnityEngine","Collider"),&typed,&recursive,&inactive,&reverse,0};
        PTR list=call(su_method("UnityEngine","GameObject","GetComponentsInternal","System.Type;System.Boolean;System.Boolean;System.Boolean;System.Boolean;System.Object"),rv_object(vehicle),a,&ok);
        U64 count=list?su.array_length(list):0;if(!ok||!list||count>64)goto fail;
        for(U64 i=0;i<count;i++){
            PTR c=*(PTR *)su.array_address(list,8,i);if(!su_alive(c))continue;
            int enabled=boolean(method("UnityEngine","Collider","get_enabled",0),c,&ok),trigger=boolean(method("UnityEngine","Collider","get_isTrigger",0),c,&ok);
            PTR attached=call(method("UnityEngine","Collider","get_attachedRigidbody",0),c,0,&ok);if(!ok)goto fail;
            if(!enabled||trigger||attached!=body)continue;
            SURoot root=su_hold(c);if(!root)goto fail;v->colliders[v->collider_count++]=root;su_bool(su.collider_enabled,c,0);
        }
    }
    PTR tracks=game_component(vehicle,"MoonVehicleTracks");
    if(tracks){int ok=1;v->tracks_enabled=boolean(method("UnityEngine","Behaviour","get_enabled",0),tracks,&ok);v->tracks=su_hold(tracks);if(!ok||!v->tracks)goto fail;su_bool(method("UnityEngine","Behaviour","set_enabled",1),tracks,0);}
    su_int(rb_ccd,body,2);su_int(rb_interpolate,body,1);event("hover-instance-ready");return 1;
fail:mh_drop(v);return 0;
}
static int mh_state(void *vehicle,MoonHoverState *state,unsigned int size){
    if(!state||size!=sizeof(*state)||mv_thread!=GetCurrentThreadId())return 0;
    MHInstance *v=mh_instance(vehicle);if(!v)return 0;int ok=1;PTR owner=call(owner_method,vehicle,0,&ok);
    *state=(MoonHoverState){.abi=MV_HOVER_ABI,.size=sizeof(*state),.flight=v->flight,.output=v->output,
        .position=v->position,.measured=v->measured,.center=center,.floor_up=v->floor_up,.preview=v->preview,
        .clearance=v->clearance,.min_clearance=v->terrain_min_clearance,.max_nose_up=v->terrain_nose_up,
        .fixed_ticks=v->terrain_ticks,.preview_ticks=v->preview_ticks,.missing_floor_ticks=v->missing_floor_ticks,
        .has_floor=v->has_floor,.local_owner=ok&&owner==su_get(walker_root)};return 1;
}

static int ground(MHInstance *v,RVVec p,RVVec up,float *clearance){
    v->query_count=v->query_status=0;v->query_gap=1e6f;v->query_normal=0;
    // A centre-only ray can miss the slope supporting a wide cabin or fender.
    // Sample the actual hull undersides, retaining other vehicles/buildings.
    PTR transform=su_get(v->transform),walker_body=0;field_value(su_get(walker_root),walker_body_field,&walker_body);
    RVVec right=rv_from(su_vector(su.tr_right,transform)),forward=rv_from(su_vector(su.tr_forward,transform)),body_up=rv_from(su_vector(su.tr_up,transform));
    SPVec3 direction=rv_to(rv_mul(up,-1));float length=28.f,best=1e6f;int mask=~(1<<2),triggers=1,ok=1;PTR hits=su_get(query_hits);
    PTR type=su_type("MoonCleaners","MoonPrototypeVehicle");unsigned char inactive=0;void *parent_args[]={type,&inactive};
    for(unsigned support_index=0;support_index<(v->profile->box_count*5);support_index++){
        RVVec local=mv_hover_support(v->profile,support_index);RVVec support=rv_add(p,rv_add(rv_mul(right,local.x),rv_add(rv_mul(body_up,local.y),rv_mul(forward,local.z))));
        SPVec3 origin=rv_to(rv_add(support,rv_mul(up,2.5f)));void *args[]={&origin,&direction,hits,&length,&mask,&triggers};PTR result=call(ray_hits,0,args,&ok);
        if(!ok||!result){v->query_status=-1;return 0;}int n=*(int *)unbox(result);v->query_count+=n;if(n<0||n>=64){v->query_status=-2;return 0;}
        for(int i=0;i<n;i++){
            PTR hit=su.array_address(hits,su.hit_size,(U64)i),body=call(hit_body,hit,0,&ok);
            if(body&&(body==su_get(v->body)||body==walker_body))continue;
            PTR collider=call(su.hit_collider,hit,0,&ok);if(!collider||!ok){v->query_status=-3;return 0;}
            if(call(component_parent,collider,parent_args,&ok)==su_get(v->vehicle))continue;if(!ok){v->query_status=-4;return 0;}
            SPVec3 point,normal;su_copy(&point,(char *)hit+su.hit_point,12);su_copy(&normal,(char *)hit+su.hit_normal,12);
            float c=rv_dot(rv_sub(support,rv_from(point)),up)-v->profile->reference;
            if(c<v->query_gap){v->query_gap=c;v->query_normal=rv_dot(rv_from(normal),up);}
            if(c<-.3f||rv_dot(rv_from(normal),up)<.35f)continue;if(c<best){best=c;v->floor_up=rv_normal(rv_from(normal),up);}
        }
    }
    if(best==1e6f)return 0;v->query_status=1;*clearance=best;return 1;
}
static RVPreview terrain_ahead(MHInstance *v,RVVec p,RVVec up,RVVec measured,RVInput input){
    RVPreview preview={0};RVVec travel=rv_plane(measured,up);float speed=rv_length(travel);
    if(speed<.1f){travel=rv_add(rv_mul(v->flight.heading,input.throttle),rv_mul(rv_cross(up,v->flight.heading),input.strafe));if(rv_length(travel)<.1f)return preview;}
    travel=rv_normal(travel,v->flight.heading);RVVec side=rv_normal(rv_cross(up,travel),rv_v(1,0,0));
    PTR transform=su_get(v->transform),walker_body=0;field_value(su_get(walker_root),walker_body_field,&walker_body);
    RVVec right=rv_from(su_vector(su.tr_right,transform)),forward=rv_from(su_vector(su.tr_forward,transform)),body_up=rv_from(su_vector(su.tr_up,transform));
    float span=0;for(unsigned i=0;i<v->profile->box_count*5;i++){RVVec local=mv_hover_support(v->profile,i);float width=rv_abs(rv_dot(side,right)*local.x+rv_dot(side,forward)*local.z);if(width>span)span=width;}span*=.85f;
    float length=rv_probe_distance_profile(speed,&v->profile->tuning);SPVec3 direction=rv_to(travel);int mask=~(1<<2),triggers=1,ok=1;
    PTR hits=su_get(query_hits),type=su_type("MoonCleaners","MoonPrototypeVehicle");unsigned char inactive=0;void *parent_args[]={type,&inactive};
    for(int lane=-1;lane<=1;lane++){
        SPVec3 origin=rv_to(rv_add(p,rv_add(rv_mul(up,v->profile->reference+.3f),rv_mul(side,span*(float)lane))));
        void *args[]={&origin,&direction,hits,&length,&mask,&triggers};PTR result=call(ray_hits,0,args,&ok);
        if(!ok||!result)return (RVPreview){0};int count=*(int *)unbox(result);if(count<0||count>=64)return (RVPreview){0};
        float nearest=length+1;RVVec point={0},normal={0};
        for(int i=0;i<count;i++){
            PTR hit=su.array_address(hits,su.hit_size,(U64)i),body=call(hit_body,hit,0,&ok);
            if(body&&(body==su_get(v->body)||body==walker_body))continue;
            PTR collider=call(su.hit_collider,hit,0,&ok);if(!ok||!collider)return (RVPreview){0};
            if(call(component_parent,collider,parent_args,&ok)==su_get(v->vehicle))continue;if(!ok)return (RVPreview){0};
            float distance=0;su_copy(&distance,(char *)hit+su.hit_distance,4);
            if(distance<nearest){nearest=distance;su_copy(&point,(char *)hit+su.hit_point,12);su_copy(&normal,(char *)hit+su.hit_normal,12);}
        }
        // A wall/ceiling is an occluder, not an uphill floor visible behind it.
        float ny=rv_dot(normal,up);if(nearest>length||ny<.35f||rv_dot(normal,travel)>-.025f)continue;
        float gap=1e6f;
        for(unsigned i=0;i<(v->profile->box_count*5);i++){
            RVVec local=mv_hover_support(v->profile,i);RVVec support=rv_add(p,rv_add(rv_mul(right,local.x),rv_add(rv_mul(body_up,local.y),rv_mul(forward,local.z))));
            float height=rv_dot(rv_sub(support,point),normal)/ny-v->profile->reference;if(height<gap)gap=height;
        }
        if(!preview.valid||gap<preview.clearance)preview=(RVPreview){rv_normal(normal,up),gap,nearest,1};
    }
    return preview;
}

/* BEGIN */
static int begin_hover(MHInstance *v){
    PTR body=su_get(v->body);RVVec p=rv_from(su_vector(rb_position,body)),up=rv_normal(rv_sub(p,center),rv_v(0,1,0));
    RVFlight flight;if(!rv_begin_profile(&flight,&v->profile->tuning,p,center,up,rv_from(su_vector(su.tr_forward,su_get(v->transform))),rv_from(su_vector(rb_get_velocity,body))))return 0;
    if(!mh_acquire(v))return 0;
    if(!rv_constraints_acquire(&v->constraints,body)){event("hover-constraint-acquire-failed");mh_release(v);v->failed=1;return 0;}
    flight.surface=1;flight.landing_height=v->profile->tuning.hover_height;v->flight=flight;event("surface-hover-start");return 1;
}
static void begin_flight(MHInstance *v){
    if(!v->flight.active&&!begin_hover(v))return;
    v->flight.surface=0;v->flight.landing=0;v->flight.landed=0;
    v->flight.radius=rv_length(rv_sub(rv_from(su_vector(rb_position,su_get(v->body))),center));
    notice("Hover flight | W/S fly, A/D turn, Space/Ctrl lift, Z/C strafe, Shift boost, Shift+F9 land");event("takeoff");
}

static int flight_fixed(PTR vehicle,const MoonVehicleInput *wheel){
    MHInstance *v=mh_instance(vehicle);if(!v)return 0;if(!mh_ready||v->failed)return mh_release(v)?0:1;int ok=1;
    PTR walker=su_get(walker_root),owner=call(owner_method,vehicle,0,&ok);
    if(!ok||!su_alive(walker)||owner!=walker){if(v->flight.active||v->snapshot||v->constraints.held){if(!mh_release(v))return 1;event("authority-released");}return 0;}
    if(!v->flight.active&&!begin_hover(v))return mh_release(v)?0:1;
    PTR body=su_get(v->body);if(!su_alive(body))return mh_release(v)?0:1;
    float dt=su_number(fixed_delta,0);if(!rv_finite(dt)||dt<=0||dt>.051f){event("flight-timestep-rejected");return mh_release(v)?0:1;}
    RVVec pos=rv_from(su_vector(rb_position,body)),measured=rv_from(su_vector(rb_get_velocity,body)),up=rv_normal(rv_sub(pos,center),v->flight.up);
    // Recall moves the native body discontinuously. Rebase heading/altitude
    // on its new pose before a stored flight target can turn it across a bay.
    if(v->has_position&&rv_length(rv_sub(pos,v->previous_position))>v->profile->tuning.boost_speed*dt+2.f){
        PTR boxed=call(method("UnityEngine","Rigidbody","get_rotation",0),body,0,&ok);SUQuat q={0,0,0,1};
        if(!ok||!boxed)return mh_release(v)?0:1;su_copy(&q,unbox(boxed),sizeof(q));
        RVVec forward={2*(q.x*q.z+q.w*q.y),2*(q.y*q.z-q.w*q.x),1-2*(q.x*q.x+q.y*q.y)};
        if(!rv_begin_profile(&v->flight,&v->profile->tuning,pos,center,up,forward,measured))return mh_release(v)?0:1;
        v->flight.surface=1;v->flight.landing_height=v->profile->tuning.hover_height;v->probe_cache.valid=0;event("hover-pose-reset");
    }
    v->previous_position=pos;v->has_position=1;
    int driven=boolean(driven_method,vehicle,&ok);if(!ok)return mh_release(v)?0:1;
    RVVec forward=rv_from(su_vector(su.tr_forward,su_get(v->transform))),body_up=rv_from(su_vector(su.tr_up,su_get(v->transform)));
    if(mh_probe_reuse(&v->probe_cache,driven,v->flight.surface,v->flight.landing,dt,pos,measured,forward,body_up,v->profile->tuning.hover_height)){
        v->has_floor=1;v->floor_up=v->probe_cache.normal;
        v->clearance=v->probe_cache.clearance+rv_dot(rv_sub(pos,v->probe_cache.position),up);
    }else{
        v->has_floor=ground(v,pos,up,&v->clearance);
        mh_probe_commit(&v->probe_cache,v->has_floor,pos,forward,body_up,v->floor_up,v->clearance);
    }
    int enabled=driven&&input_allowed(walker);
    RVInput input={0};input.enabled=enabled;
    if(enabled){
        int native_throttle,native_steer;
        input.throttle=mh_keyboard_axis(0x57,0x53,&native_throttle);input.steer=mh_keyboard_axis(0x44,0x41,&native_steer);input.strafe=(float)(key(0x43)-key(0x5a));input.rise=(float)(key(0x20)-key(0x11));input.boost=key(0x10);input.brake=key(0x58);
        if(!input.throttle&&native_throttle)input.throttle=su_number(vehicle_drive,vehicle);
        if(!input.steer&&native_steer)input.steer=su_number(vehicle_steer,vehicle);
        rv_wheel_input(&input,wheel);
    }
    if(!driven&&!v->flight.surface)v->flight.landing=1;
    v->preview=v->flight.landing?(RVPreview){0}:terrain_ahead(v,pos,up,measured,input);
    RVOutput out;if(!rv_step_terrain(&v->flight,input,dt,pos,center,measured,up,v->clearance,v->has_floor,v->floor_up,v->preview,&out)){
        event("flight-step-failed");v->failed=1;return mh_release(v)?0:1;
    }
    {
        v->terrain_ticks++;if(v->preview.valid)v->preview_ticks++;
        if(v->has_floor){if(v->clearance<v->terrain_min_clearance)v->terrain_min_clearance=v->clearance;}else v->missing_floor_ticks++;
        float nose=rv_dot(out.forward,up);if(nose>v->terrain_nose_up)v->terrain_nose_up=nose;
    }
    // Keep vanilla's cleaning, dust and presentation consumers consistent with
    // the replacement physics; stale grounded flags must not sweep from aloft.
    int near_ground=v->flight.surface&&v->has_floor&&v->clearance<3;
    // Grounded also drives tyre dust outside FixedUpdate. A hover vessel has
    // no wheel contact, while nearby surface hover is not ballistic airtime.
    su_bool(vehicle_grounded,vehicle,0);su_bool(vehicle_airborne,vehicle,!near_ground);su_bool(vehicle_air_control,vehicle,0);su_int(vehicle_wheels,vehicle,0);
    su_set_vector(rb_velocity,body,rv_to(out.velocity));su_set_vector(rb_angular,body,su_v(0,0,0));su_set_quat(rb_move_rotation,body,su_rotation(rv_to(out.forward),rv_to(out.up)));
    v->output=out;v->position=pos;v->measured=measured;if(mh_test&&v->terrain_ticks%20==0)sample("hover-flight-sample",v,&out,pos,measured);
    if(out.landed){v->flight.surface=1;v->flight.landing=0;v->flight.landed=0;v->flight.acceleration=rv_v(0,0,0);notice("Hovering | Shift+F9 lift off; normal vehicle exit available");event("landed");}
    return 1;
}

static int mh_face_hill(MHInstance *v){
    int ok=1;if(!v||boolean(driven_method,su_get(v->vehicle),&ok)||!ok||rv_length(rv_from(su_vector(rb_get_velocity,su_get(v->body))))>.5f)return 0;
    RVVec p=rv_from(su_vector(rb_position,su_get(v->body))),up=rv_normal(rv_sub(p,center),rv_v(0,1,0));
    RVVec forward=rv_tangent(v->flight.heading,up),right=rv_cross(up,forward),chosen={0};RVPreview best={0};float score=0;
    static const float directions[8][2]={{1,0},{.707f,.707f},{0,1},{-.707f,.707f},{-1,0},{-.707f,-.707f},{0,-1},{.707f,-.707f}};
    for(int i=0;i<8;i++){
        RVVec direction=rv_normal(rv_add(rv_mul(forward,directions[i][0]),rv_mul(right,directions[i][1])),forward);
        RVPreview found=terrain_ahead(v,p,up,rv_mul(direction,64),(RVInput){0});
        if(!found.valid||found.distance<6||found.distance>135)continue;
        float grade=-rv_dot(found.normal,direction)/rv_clamp(rv_dot(found.normal,up),.35f,1);
        float rank=grade/(1+found.distance*.005f);if(grade>.18f&&rank>score){score=rank;best=found;chosen=direction;}
    }
    if(!best.valid)return 0;
    PTR body=su_get(v->body);su_set_quat(method("UnityEngine","Rigidbody","set_rotation",1),body,su_rotation(rv_to(chosen),rv_to(up)));
    if(!rv_begin_profile(&v->flight,&v->profile->tuning,p,center,up,chosen,rv_v(0,0,0)))return 0;
    v->flight.surface=1;v->flight.landing_height=v->profile->tuning.hover_height;v->preview=best;v->previous_position=p;v->has_position=1;
    v->terrain_ticks=1;v->preview_ticks=v->missing_floor_ticks=0;v->terrain_min_clearance=1e6f;v->terrain_nose_up=0;
    char b[512],*out=cat(b,"{\"event\":\"test-uphill-route\",\"heading\":");out=vector_text(out,chosen);
    out=cat(out,",\"vehicle_id\":");out=num(out,(U64)v->id);out=cat(out,",\"normal\":");out=vector_text(out,best.normal);out=cat(out,",\"distance\":");out=fixed_text(out,best.distance);
    out=cat(out,",\"preview_gap\":");out=fixed_text(out,best.clearance);out=cat(out,"}\n");DWORD n=0;WriteFile(mv_log,b,(DWORD)(out-b),&n,0);return 1;
}

static int mh_request(void *vehicle,int action){
    if(mv_thread!=GetCurrentThreadId()||!mh_ready)return 0;MHInstance *v=mh_instance(vehicle);if(!v||v->failed)return 0;
    PTR walker=su_get(walker_root);int ok=1;if(!su_alive(walker)||call(owner_method,vehicle,0,&ok)!=walker||!ok||!input_allowed(walker))return 0;
    if(action==MV_HOVER_TEST_FACE_HILL)return mh_test&&mh_face_hill(v);
    if(action!=MV_HOVER_TOGGLE||!v->profile->flight_enabled||!boolean(driven_method,vehicle,&ok)||!ok)return 0;
    if(!v->flight.active||v->flight.surface)begin_flight(v);
    else{v->flight.landing=!v->flight.landing;notice(v->flight.landing?"Automatic landing | Shift+F9 cancel":"Altitude hold");event(v->flight.landing?"landing-start":"landing-cancel");}
    return v->flight.active;
}
static int mh_exit(PTR vehicle,int jump){
    MHInstance *v=mh_instance(vehicle);if(v&&v->flight.active&&!v->flight.surface){
        if(!jump)notice("Land first: Shift+F9 engages automatic landing");return 1;
    }
    return 0;
}
static int mh_world_drop(void){
    int ok=1;for(unsigned i=0;i<128;i++)if(mh_instances[i].vehicle&&!mh_drop(mh_instances+i))ok=0;
    if(!ok)return 0;su_drop(&world_root);su_drop(&walker_root);su_drop(&equipment_root);mh_frames=mh_ready=mh_was_toggle=mh_was_hill=0;return 1;
}
static void mh_cleanup(void){for(unsigned i=0;i<128;i++)if(mh_instances[i].vehicle&&!su_alive(su_get(mh_instances[i].vehicle)))mh_drop(mh_instances+i);}
static void mh_update(PTR walker,PTR world){
    if(world_root&&(!su_alive(su_get(world_root))||su_get(world_root)!=world||su_get(walker_root)!=walker))if(!mh_world_drop())return;
    if(!su_alive(world))return;int ok=1;
    if(boolean(restoring,world,&ok)||!ok){mh_ready=0;mh_frames=0;for(unsigned i=0;i<128;i++)if(mh_instances[i].vehicle)mh_release(mh_instances+i);return;}
    if(++mh_frames<120)return;
    if(!world_root){world_root=su_hold(world);walker_root=su_hold(walker);if(!world_root||!walker_root){mh_world_drop();return;}center=rv_from(su_vector(su.tr_get_position,rv_transform(world)));event("hover-world-ready");}
    mh_ready=1;find_equipment(walker);
    int allowed=input_allowed(walker),physical_toggle=raw_key(0x10)&&raw_key(0x78);
    int toggle=key(0x10)&&!raw_key(0x11)&&!raw_key(0x12)&&key(0x78);
    int toggle_edge=rv_hover_toggle_edge(toggle,physical_toggle,allowed,&mh_was_toggle),hill=mh_test&&raw_key(0x70);
    if(toggle_edge||(allowed&&hill&&!mh_was_hill)){
        MHInstance *driven=0,*near=0;float closest=7;RVVec player=rv_from(su_vector(su.tr_get_position,rv_transform(walker)));
        for(unsigned i=0;i<128;i++)if(mh_instances[i].vehicle){
            MHInstance *v=mh_instances+i;PTR vehicle=su_get(v->vehicle);if(!su_alive(vehicle)||call(owner_method,vehicle,0,&ok)!=walker||!ok)continue;
            if(boolean(driven_method,vehicle,&ok))driven=v;
            float distance=rv_length(rv_sub(player,rv_from(su_vector(rb_position,su_get(v->body)))));if(distance<closest){closest=distance;near=v;}
        }
        if(toggle_edge){
            if(driven)mh_request(su_get(driven->vehicle),MV_HOVER_TOGGLE);
            else if(near){int entered=boolean(vehicle_enter,su_get(near->vehicle),&ok);event(entered&&ok?"hover-vehicle-boarded":"hover-boarding-unavailable");if(entered&&ok)notice("Hover vehicle boarded | Shift+F9 lift off");}
            else notice("Buy a hover vehicle at a garage, then approach it to board");
        }
        if(hill&&!mh_was_hill)event(near&&mh_request(su_get(near->vehicle),MV_HOVER_TEST_FACE_HILL)?"test-uphill-staged":"test-uphill-unavailable");
    }
    mh_was_hill=hill;
}
static int mh_bind(PTR game_assembly){
    world_field=get_field(find_class("MoonCleaners","MoonWalker"),"world");walker_body_field=get_field(find_class("MoonCleaners","MoonWalker"),"body");body_field=get_field(find_class("MoonCleaners","MoonPrototypeVehicle"),"body");
#define MH_BIND(V,NS,T,N,S) V=su_method(NS,T,N,S);if(!V){event("hover-" T "::" N "-bind-failed");return 0;}
    MH_BIND(restoring,"MoonCleaners","MoonWorld","get_IsRestoringRendering","");
    MH_BIND(ui_blocked,"MoonCleaners","MoonWalker","get_UIBlocked","");
    MH_BIND(typing,"MoonCleaners","MoonInput","get_Typing","");
    MH_BIND(focused,"UnityEngine","Application","get_isFocused","");
    MH_BIND(owner_method,"MoonCleaners","MoonPrototypeVehicle","get_Owner","");
    MH_BIND(driven_method,"MoonCleaners","MoonPrototypeVehicle","get_IsDriven","");
    MH_BIND(vehicle_enter,"MoonCleaners","MoonPrototypeVehicle","Enter","");
    MH_BIND(vehicle_grounded,"MoonCleaners","MoonPrototypeVehicle","set_Grounded","System.Boolean");
    MH_BIND(vehicle_airborne,"MoonCleaners","MoonPrototypeVehicle","set_InAir","System.Boolean");
    MH_BIND(vehicle_wheels,"MoonCleaners","MoonPrototypeVehicle","set_WheelsOnGround","System.Int32");
    MH_BIND(vehicle_air_control,"MoonCleaners","MoonPrototypeVehicle","set_AirControl","System.Boolean");
    MH_BIND(vehicle_drive,"MoonCleaners","MoonPrototypeVehicle","get_DriveInput","");
    MH_BIND(vehicle_steer,"MoonCleaners","MoonPrototypeVehicle","get_Steering","");
    MH_BIND(component_component,"UnityEngine","Component","GetComponent","System.Type");
    MH_BIND(component_parent,"UnityEngine","Component","GetComponentInParent","System.Type;System.Boolean");
    MH_BIND(find_objects,"UnityEngine","Object","FindObjectsByType","System.Type;UnityEngine.FindObjectsInactive;UnityEngine.FindObjectsSortMode");
    MH_BIND(ray_hits,"UnityEngine","Physics","RaycastNonAlloc","UnityEngine.Vector3;UnityEngine.Vector3;UnityEngine.RaycastHit[];System.Single;System.Int32;UnityEngine.QueryTriggerInteraction");
    MH_BIND(rb_velocity,"UnityEngine","Rigidbody","set_linearVelocity","UnityEngine.Vector3");
    MH_BIND(rb_get_velocity,"UnityEngine","Rigidbody","get_linearVelocity","");
    MH_BIND(rb_position,"UnityEngine","Rigidbody","get_position","");
    MH_BIND(rb_move_rotation,"UnityEngine","Rigidbody","MoveRotation","UnityEngine.Quaternion");
    MH_BIND(rb_angular,"UnityEngine","Rigidbody","set_angularVelocity","UnityEngine.Vector3");
    MH_BIND(rb_constraints,"UnityEngine","Rigidbody","set_constraints","UnityEngine.RigidbodyConstraints");
    MH_BIND(rb_get_constraints,"UnityEngine","Rigidbody","get_constraints","");
    MH_BIND(rb_ccd,"UnityEngine","Rigidbody","set_collisionDetectionMode","UnityEngine.CollisionDetectionMode");
    MH_BIND(rb_interpolate,"UnityEngine","Rigidbody","set_interpolation","UnityEngine.RigidbodyInterpolation");
    MH_BIND(rb_gravity,"UnityEngine","Rigidbody","set_useGravity","System.Boolean");
    MH_BIND(fixed_delta,"UnityEngine","Time","get_fixedDeltaTime","");
    MH_BIND(hit_body,"UnityEngine","RaycastHit","get_rigidbody","");
    MH_BIND(notify,"MoonCleaners","MoonPrototypeEquipment","Notify","System.String");
    MH_BIND(get_grounded,"MoonCleaners","MoonPrototypeVehicle","get_Grounded","");
    MH_BIND(get_airborne,"MoonCleaners","MoonPrototypeVehicle","get_InAir","");
    MH_BIND(get_wheels,"MoonCleaners","MoonPrototypeVehicle","get_WheelsOnGround","");
    MH_BIND(get_air_control,"MoonCleaners","MoonPrototypeVehicle","get_AirControl","");
#undef MH_BIND
    PTR (*resolve)(const char *)=(PTR(*)(const char *))GetProcAddress(game_assembly,"il2cpp_resolve_icall");
    body_cached_ptr=get_field(find_class("UnityEngine","Object"),"m_CachedPtr");
    if(!resolve||!body_cached_ptr)return 0;
    mh_get_gravity=(unsigned char(*)(PTR))resolve("UnityEngine.Rigidbody::get_useGravity_Injected(System.IntPtr)");
    if(!mh_get_gravity)mh_get_gravity=(unsigned char(*)(PTR))resolve("UnityEngine.Rigidbody::get_useGravity_Injected");
    if(!mh_get_gravity)return 0;
    int (*value_size)(PTR,DWORD *)=(int(*)(PTR,DWORD *))GetProcAddress(game_assembly,"il2cpp_class_value_size");DWORD align=0;
    PTR hit_class=find_class("UnityEngine","RaycastHit"),entity_class=find_class("UnityEngine","EntityId");
    if(!world_field||!walker_body_field||!body_field||!value_size||!hit_class||!entity_class)return 0;
    su.hit_size=value_size(hit_class,&align);PTR collider_id_field=get_field(hit_class,"m_Collider");
    if(su.hit_size!=48||value_size(entity_class,&align)!=8||!collider_id_field||su.field_offset(collider_id_field)-16!=40)return 0;
    query_hits=su_hold(su.new_array(hit_class,64));if(!query_hits)return 0;
    PTR user=LoadLibraryA("user32.dll");if(!user)return 0;key_state=(short(*)(int))GetProcAddress(user,"GetAsyncKeyState");foreground_window=(PTR(*)(void))GetProcAddress(user,"GetForegroundWindow");window_pid=(DWORD(*)(PTR,DWORD *))GetProcAddress(user,"GetWindowThreadProcessId");
    char flag[8];mh_test=GetEnvironmentVariableA("MOONLAB_TEST_TOOLS",flag,8)==1&&flag[0]=='1';
    return key_state&&foreground_window&&window_pid&&ray_hits&&component_parent&&hit_body&&rb_velocity&&rb_get_velocity&&rb_move_rotation&&vehicle_grounded&&vehicle_airborne&&vehicle_wheels;
}
