#ifndef RUSTBOUND_BODY_ADAPTER_H
#define RUSTBOUND_BODY_ADAPTER_H
/* Constraints are borrowed for one flight, rather than captured when a vehicle
 * first appears. Vanilla can change them while parked or transferring control. */
typedef struct {int held,original;} RVConstraints;
static inline int rv_constraints_release(RVConstraints *state,PTR body){
    if(!state->held)return 1;
    if(su_alive(body)){
        int ok=1;void *args[]={&state->original};call(rb_constraints,body,args,&ok);
        if(!ok)return 0;
    }
    state->held=0;return 1;
}
static inline int rv_constraints_acquire(RVConstraints *state,PTR body){
    if(state->held||!su_alive(body))return 0;
    int ok=1,original=integer(rb_get_constraints,body,&ok);if(!ok)return 0;
    *state=(RVConstraints){1,original};int unconstrained=0;void *args[]={&unconstrained};
    call(rb_constraints,body,args,&ok);
    // A managed setter may fail after mutation. Keep the snapshot until a
    // successful restore (or destruction), including a later cleanup retry.
    if(!ok)rv_constraints_release(state,body);
    return ok;
}
#endif
