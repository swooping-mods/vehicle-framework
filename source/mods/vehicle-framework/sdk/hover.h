#ifndef MOON_VEHICLE_HOVER_H
#define MOON_VEHICLE_HOVER_H
#include "hover_core.h"
#define MV_HOVER_ABI 1u
#define MV_HOVER_BOX_LIMIT 8u
typedef struct { const char *name; RVVec center,size; } MoonHoverBox;
/* Boxes describe the actual fitted collision hull in vehicle-root space.
 * Their five underside samples drive both clearance and predictive sensing.
 * The framework copies this profile/box table during registration. */
typedef struct {
    unsigned int abi,size;
    RVConfig tuning;
    const MoonHoverBox *boxes;
    unsigned int box_count;
    float reference;
    int replace_native_solids,flight_enabled;
} MoonHoverProfile;
typedef struct {
    unsigned int abi,size;
    RVFlight flight;
    RVOutput output;
    RVVec position,measured,center,floor_up;
    RVPreview preview;
    float clearance,min_clearance,max_nose_up;
    unsigned int fixed_ticks,preview_ticks,missing_floor_ticks;
    int has_floor,local_owner;
} MoonHoverState;
enum { MV_HOVER_TOGGLE=1, MV_HOVER_TEST_FACE_HILL=2 };
static inline RVVec mv_hover_support(const MoonHoverProfile *p,unsigned int index){
    const MoonHoverBox *b=p->boxes+index/5;unsigned int corner=index%5;
    RVVec v=b->center;v.y-=b->size.y*.5f;
    if(corner){v.x+=((corner-1)&1?1.f:-1.f)*b->size.x*.5f;v.z+=((corner-1)&2?1.f:-1.f)*b->size.z*.5f;}
    return v;
}
static inline int mv_hover_profile_valid(const MoonHoverProfile *p){
    if(!p||p->abi!=MV_HOVER_ABI||p->size!=sizeof(*p)||!rv_config_valid(&p->tuning)||
       !p->boxes||p->box_count<1||p->box_count>MV_HOVER_BOX_LIMIT||
       !rv_finite(p->reference)||rv_abs(p->reference)>20||p->reference+p->tuning.hover_height<.25f||
       (p->replace_native_solids!=0&&p->replace_native_solids!=1)||
       (p->flight_enabled!=0&&p->flight_enabled!=1))return 0;
    for(unsigned i=0;i<p->box_count;i++){
        const MoonHoverBox *b=p->boxes+i;
        if(!rv_valid(b->center)||rv_abs(b->center.x)>50||rv_abs(b->center.y)>50||rv_abs(b->center.z)>50||
           !rv_valid(b->size)||b->size.x<.05f||b->size.y<.05f||b->size.z<.05f||
           b->size.x>100||b->size.y>100||b->size.z>100)return 0;
    }
    return 1;
}
#endif
