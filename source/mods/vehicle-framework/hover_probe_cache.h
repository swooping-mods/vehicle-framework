#ifndef MOON_HOVER_PROBE_CACHE_H
#define MOON_HOVER_PROBE_CACHE_H
/* Only quiet unattended surface hover may reuse a floor sample. No persistent
 * world-visibility key is assumed: revalidate within 100ms even without motion. */
typedef struct {RVVec position,forward,up,normal;float age,clearance;int valid;unsigned queries,hits;} MHProbeCache;
static inline int mh_probe_reuse(MHProbeCache *c,int driven,int surface,int landing,float dt,RVVec position,RVVec velocity,RVVec forward,RVVec up,float hover){
    if(!rv_finite(dt)||dt<=0||dt>.051f)return 0;
    c->age+=dt;
    if(!c->valid||driven||!surface||landing||c->age+1e-6f>=.1f||
       rv_length(velocity)>.08f||rv_length(rv_sub(position,c->position))>.015f||
       rv_dot(forward,c->forward)<.99999f||rv_dot(up,c->up)<.99999f||rv_abs(c->clearance-hover)>.035f)return 0;
    c->hits++;return 1;
}
static inline void mh_probe_commit(MHProbeCache *c,int has_floor,RVVec position,RVVec forward,RVVec up,RVVec normal,float clearance){
    c->position=position;c->forward=forward;c->up=up;c->normal=normal;c->clearance=clearance;c->valid=has_floor;c->age=0;c->queries++;
}
#endif
