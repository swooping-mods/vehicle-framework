/* Run the pinned native car physics once under temporary driven state. Vanilla
 * parking early-outs otherwise prevent an unattended car from accelerating.
 * The real owner/world remain untouched; custom physics retains its own owner. */
#include "sdk/autonomous.h"
static PTR mva_fixed,mva_driven,mva_owner,mva_world,mva_restoring,mva_grounded,mva_auto,mva_body;
static PTR mva_ui_field,mva_ui_blocked,mva_building;
static int (*mva_map_context)(void *owner);
static PTR mva_pending,mva_running;
static PTR mva_clean,mva_spec,mva_full,mva_faces,mva_time,mva_scale,mva_transform,mva_up;
static PTR mva_work_field,mva_next_work,mva_cleaning_field,mva_has_point,mva_normal,mva_ready;
static int mva_pending_work;
typedef struct {SURoot car;unsigned char has_point;} MVAWork;
static MVAWork mva_work_cars[MVA_CAR_LIMIT];
static void mva_clear_work(PTR car){
    for(int i=0;i<MVA_CAR_LIMIT;i++)if(mva_work_cars[i].car&&su_get(mva_work_cars[i].car)==car){su_drop(&mva_work_cars[i].car);mva_work_cars[i]=(MVAWork){0};}
}
static MVAWork *mva_work_car(PTR car){
    for(int i=0;i<MVA_CAR_LIMIT;i++)if(mva_work_cars[i].car&&su_get(mva_work_cars[i].car)==car)return mva_work_cars+i;
    MVAWork *free_slot=0;for(int i=0;i<MVA_CAR_LIMIT;i++){
        if(mva_work_cars[i].car&&!su_alive(su_get(mva_work_cars[i].car)))su_drop(&mva_work_cars[i].car);
        if(!mva_work_cars[i].car&&!free_slot)free_slot=mva_work_cars+i;
    }
    if(!free_slot)return 0;*free_slot=(MVAWork){su_hold(car),0};return free_slot->car?free_slot:0;
}
static void (*mva_set_field)(PTR,PTR,void *);
static MoonVehicleInput mva_input;
static int mva_stepping(void *car){return car&&car==mva_running;}
static int mva_request(void *car,const MoonVehicleInput *input){
    if(mv_thread!=GetCurrentThreadId()||mva_running)return 0;
    mva_pending=0;mva_pending_work=0;mva_input=(MoonVehicleInput){0};
    if(!input)return 1;if(!moon_vehicle_input_valid(input)||!su_alive(car))return 0;
    mva_pending=car;mva_input=*input;return 1;
}
static int mva_supports(void *car){
    if(mv_thread!=GetCurrentThreadId()||!su_alive(car))return 0;
    int ok=1,index=integer(mv_local_index,car,&ok);MVEntry *entry=mv_entry(index);
    if(!ok||index<0||index>MV_LAST_ID||(index>=MV_VANILLA_COUNT&&!entry)||(entry&&(entry->def.hover||entry->def.physics)))return 0;
    PTR owner=call(mva_owner,car,0,&ok),world=call(mva_world,car,0,&ok),body=0;field_value(car,mva_body,&body);
    return ok&&su_alive(owner)&&su_alive(world)&&su_alive(body);
}
static int mva_can_work(void *car){
    if(!mva_supports(car))return 0;int ok=1,kind=-1;PTR spec=call(mva_spec,car,0,&ok);if(!ok||!spec)return 0;
    if(!mva_work_field){mva_work_field=get_field(object_class(spec),"work");if(!mva_work_field||su.field_offset(mva_work_field)!=32){mva_work_field=0;return 0;}}
    field_value(spec,mva_work_field,&kind);return kind==0||kind==7;
}
static int mva_work(void *car,int enabled){
    if(mv_thread!=GetCurrentThreadId()||mva_running||(enabled!=0&&enabled!=1))return 0;
    if(!enabled){if(mva_pending==car)mva_pending_work=0;mva_clear_work(car);return 1;}
    if(mva_pending!=car)return 0;mva_pending_work=1;return 1;
}
static float mva_float(PTR method,PTR self,int *ok){float n=0;PTR v=call(method,self,0,ok);if(v)su_copy(&n,unbox(v),4);else *ok=0;return n;}
static void mva_clean_once(PTR car){
    unsigned char is_ready=0;field_value(car,mva_ready,&is_ready);
    if(!is_ready||mva_input.brake||mva_input.throttle<0){mva_clear_work(car);return;}
    int ok=1;float now=mva_float(mva_time,0,&ok),next=0;field_value(car,mva_next_work,&next);
    if(!ok||!(now>=next))return;
    /* Capability and geometry queries run only at the shared native 10 Hz
     * work deadline, rather than on every intervening physics tick. */
    if(!mva_can_work(car)){mva_clear_work(car);return;}
    if(!(mva_float(mva_scale,0,&ok)>0)||!boolean(mva_grounded,car,&ok)||!boolean(mva_faces,car,&ok)||boolean(mva_full,car,&ok)||!ok){mva_clear_work(car);return;}
    SPVec3 normal={0},up={0};field_value(car,mva_normal,&normal);PTR tr=call(mva_transform,car,0,&ok),v=tr?call(mva_up,tr,0,&ok):0;
    if(v)su_copy(&up,unbox(v),sizeof(up));else ok=0;
    if(!ok||!(su_dot(normal,up)>=.5f)){mva_clear_work(car);return;}
    MVAWork *state=mva_work_car(car);if(!state)return;
    unsigned char old_clean=0,old_point=0,yes=1;field_value(car,mva_cleaning_field,&old_clean);field_value(car,mva_has_point,&old_point);
    /* Native Update owns these transient presentation/admission flags and
     * clears the parked-car anchor every frame. Retain only our last native
     * sweep's continuity, letting Clean reset anchors after large movement. */
    mva_set_field(car,mva_has_point,&state->has_point);mva_set_field(car,mva_cleaning_field,&yes);
    next=now+.1f;mva_set_field(car,mva_next_work,&next);call(mva_clean,car,0,&ok);
    field_value(car,mva_has_point,&state->has_point);if(!ok)state->has_point=0;
    mva_set_field(car,mva_has_point,&old_point);mva_set_field(car,mva_cleaning_field,&old_clean);
}
static const MoonAutonomous mva_api={MVA_ABI,sizeof(MoonAutonomous),mva_request,mva_stepping,mva_supports,mva_work,mva_can_work};
__declspec(dllexport) const MoonAutonomous *MoonLab_Autonomous_v1(unsigned abi,unsigned size){return abi==MVA_ABI&&size==sizeof(mva_api)?&mva_api:0;}
static int mva_physics(PTR car,MVEntry *entry){
    if(mva_running)return 0;
    if(mva_pending!=car){mva_pending=0;return 0;}
    mva_pending=0;int work=mva_pending_work;mva_pending_work=0;
    if((entry&&(entry->def.hover||entry->def.physics))||!mva_supports(car))return 0;
    int ok=1;unsigned char old=0;field_value(car,mva_driven,&old);
    /* Native autopilot writes unrelated wandering inputs during Update. Never
     * borrow an existing autopilot session or another driver's car. */
    if(old||boolean(mva_auto,car,&ok)||!boolean(mva_grounded,car,&ok)||!ok)return 0;
    PTR owner=call(mva_owner,car,0,&ok),world=call(mva_world,car,0,&ok);
    if(!ok||!su_alive(owner)||!su_alive(world)||boolean(mva_restoring,world,&ok)||!ok)return 0;
    unsigned char old_ui=0;field_value(owner,mva_ui_field,&old_ui);
    /* The map sets the genuine owner's UI flag, which native car physics also
     * checks. Borrow only that flag after the map provider admits this frame;
     * native Held/Travelling/Pause/Building gates continue to apply. */
    int map_ui=old_ui&&mva_map_context&&mva_map_context(owner);
    if(map_ui){unsigned char no=0;mva_set_field(owner,mva_ui_field,&no);}
    int native_input=!boolean(mva_ui_blocked,owner,&ok)&&!boolean(mva_building,owner,&ok)&&ok;
    unsigned char yes=1;mva_running=car;mva_set_field(car,mva_driven,&yes);
    /* call() uses il2cpp_runtime_invoke's exception out parameter. Even when
     * managed physics throws it returns here, so restoration is unconditional.
     * Recursive loader dispatch sees stepping=true and executes vanilla only. */
    call(mva_fixed,car,0,&ok);
    if(ok&&work&&native_input)mva_clean_once(car);else mva_clear_work(car);
    mva_set_field(car,mva_driven,&old);
    if(map_ui)mva_set_field(owner,mva_ui_field,&old_ui);
    mva_running=0;
    return 1;
}
static int mva_bind(PTR game,int (*map_context)(void *)){
    mva_map_context=map_context;
    mva_set_field=(void(*)(PTR,PTR,void *))GetProcAddress(game,"il2cpp_field_set_value");
    mva_fixed=method("MoonCleaners","MoonPrototypeVehicle","FixedUpdate",0);
    PTR type=find_class("MoonCleaners","MoonPrototypeVehicle");mva_driven=get_field(type,"<IsDriven>k__BackingField");mva_body=get_field(type,"body");
    mva_owner=method("MoonCleaners","MoonPrototypeVehicle","get_Owner",0);mva_world=method("MoonCleaners","MoonPrototypeVehicle","get_World",0);mva_auto=method("MoonCleaners","MoonPrototypeVehicle","get_Autopilot",0);mva_grounded=method("MoonCleaners","MoonPrototypeVehicle","get_Grounded",0);
    mva_clean=method("MoonCleaners","MoonPrototypeVehicle","Clean",0);mva_spec=method("MoonCleaners","MoonPrototypeVehicle","get_Spec",0);mva_faces=method("MoonCleaners","MoonPrototypeVehicle","get_FacesUp",0);mva_full=method("MoonCleaners","MoonPrototypeVehicle","get_DustFull",0);
    mva_time=method("UnityEngine","Time","get_time",0);mva_scale=method("UnityEngine","Time","get_timeScale",0);mva_transform=method("UnityEngine","Component","get_transform",0);mva_up=method("UnityEngine","Transform","get_up",0);
#define MVA_FIELD(F,N,O) mva_##F=get_field(type,N);if(!mva_##F||su.field_offset(mva_##F)!=O)return 0
    MVA_FIELD(next_work,"nextWork",288);MVA_FIELD(cleaning_field,"<Cleaning>k__BackingField",113);MVA_FIELD(has_point,"hasCleanPoint",224);MVA_FIELD(normal,"groundNormal",176);MVA_FIELD(ready,"ready",302);
#undef MVA_FIELD
    if(!mva_clean||*(PTR *)mva_clean!=(unsigned char *)game+0x4cde60||!mva_spec||!mva_faces||!mva_full||!mva_time||!mva_scale||!mva_transform||!mva_up)return 0;
    mva_restoring=method("MoonCleaners","MoonWorld","get_IsRestoringRendering",0);
    PTR walker=find_class("MoonCleaners","MoonWalker");mva_ui_field=get_field(walker,"uiBlocked");
    mva_ui_blocked=method("MoonCleaners","MoonWalker","get_UIBlocked",0);mva_building=method("MoonCleaners","MoonWalker","get_Building",0);
    if(!mva_ui_field||su.field_offset(mva_ui_field)!=170||!mva_ui_blocked||!mva_building)return 0;
    return mva_set_field&&mva_fixed&&*(PTR *)mva_fixed==(unsigned char *)game+0x4d0c80&&mva_body&&su.field_offset(mva_body)==72&&mva_driven&&su.field_offset(mva_driven)==44&&mva_owner&&mva_world&&mva_auto&&mva_grounded&&mva_restoring;
}
