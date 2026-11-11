/* Engine-independent spherical hover control. Metres and seconds; yaw uses
 * radians, while pitch/roll are gravity-relative body-up slopes.
 * The adapter supplies measured position/velocity and the current gravity up;
 * collisions remain authoritative and are never replaced by stored velocity. */
#ifndef MOON_VEHICLE_HOVER_CORE_H
#define MOON_VEHICLE_HOVER_CORE_H
typedef struct {float x,y,z;} RVVec;
/* Defaults preserve the qualified Rustbound response. Each instance carries
 * its own validated copy; changing another vehicle never changes this state. */
typedef struct {
    float hover_height,cruise_speed,boost_speed,reverse_speed,strafe_speed,vertical_speed;
    float acceleration,boost_acceleration,brake_acceleration,jerk;
    float turn_rate,turn_falloff,turn_response,bank_factor;
    float tilt_scale,max_tilt,terrain_tilt,attitude_response,terrain_response;
    float heave_acceleration,heave_response,height_response,probe_max;
} RVConfig;
#define RV_CONFIG_DEFAULT { .hover_height=.85f,.cruise_speed=32,.boost_speed=64,.reverse_speed=12,.strafe_speed=15,.vertical_speed=12, .acceleration=10,.boost_acceleration=14,.brake_acceleration=20,.jerk=65, .turn_rate=1.05f,.turn_falloff=.018f,.turn_response=7,.bank_factor=.24f, .tilt_scale=29,.max_tilt=.46f,.terrain_tilt=1.73f,.attitude_response=7.5f,.terrain_response=9, .heave_acceleration=18,.heave_response=12,.height_response=3,.probe_max=165 }
static inline RVConfig rv_default_config(void){return (RVConfig)RV_CONFIG_DEFAULT;}
typedef struct {
    RVConfig config;
    RVVec heading,velocity,acceleration,up;
    float radius,pitch,roll,pitch_rate,roll_rate,yaw_rate,phase,last_rise;
    int active,landing,landed,vertical_braking,was_landing,surface;
    float landing_height,last_floor_clearance;int had_floor;
} RVFlight;
typedef struct {float throttle,steer,strafe,rise;int boost,brake,enabled;} RVInput;
typedef struct {RVVec velocity,forward,up;float pitch,roll,speed,power;int landed;} RVOutput;
/* A visible rising surface, measured against the whole current hull footprint.
 * It is separate from the supporting floor: a distant hill must not lower or
 * replace the floor holding the parked craft up. */
typedef struct {RVVec normal;float clearance,distance;int valid;} RVPreview;
static inline RVVec rv_v(float x,float y,float z){return (RVVec){x,y,z};}
static inline RVVec rv_add(RVVec a,RVVec b){return rv_v(a.x+b.x,a.y+b.y,a.z+b.z);}
static inline RVVec rv_sub(RVVec a,RVVec b){return rv_v(a.x-b.x,a.y-b.y,a.z-b.z);}
static inline RVVec rv_mul(RVVec a,float s){return rv_v(a.x*s,a.y*s,a.z*s);}
static inline float rv_dot(RVVec a,RVVec b){return a.x*b.x+a.y*b.y+a.z*b.z;}
static inline RVVec rv_cross(RVVec a,RVVec b){return rv_v(a.y*b.z-a.z*b.y,a.z*b.x-a.x*b.z,a.x*b.y-a.y*b.x);}
static inline float rv_abs(float x){return x<0?-x:x;}
static inline float rv_clamp(float v,float lo,float hi){return v<lo?lo:v>hi?hi:v;}
static inline int rv_finite(float v){return v==v&&v<1e20f&&v>-1e20f;}
static inline int rv_valid(RVVec v){return rv_finite(v.x)&&rv_finite(v.y)&&rv_finite(v.z)&&rv_abs(v.x)<=1e7f&&rv_abs(v.y)<=1e7f&&rv_abs(v.z)<=1e7f;}
static inline float rv_sqrt(float x){
    if(x<=0)return 0;
    // An exponent-scaled seed keeps convergence independent of moon/world size.
    union {float f;unsigned int bits;} seed={x};seed.bits=(seed.bits>>1)+0x1fc00000u;
    float y=seed.f;for(int i=0;i<5;i++)y=.5f*(y+x/y);return y;
}
static inline float rv_length(RVVec a){return rv_sqrt(rv_dot(a,a));}
static inline RVVec rv_normal(RVVec a,RVVec fallback){float l=rv_length(a);return l>1e-6f?rv_mul(a,1/l):fallback;}
static inline RVVec rv_plane(RVVec a,RVVec up){return rv_sub(a,rv_mul(up,rv_dot(a,up)));}
static inline RVVec rv_tangent(RVVec direction,RVVec up){
    RVVec projected=rv_plane(direction,up);
    RVVec axis=rv_abs(up.y)<.9f?rv_v(0,1,0):rv_v(0,0,1);
    return rv_normal(projected,rv_normal(rv_plane(axis,up),rv_v(1,0,0)));
}
static inline RVVec rv_limit(RVVec a,float limit){float l=rv_length(a);return l>limit?rv_mul(a,limit/l):a;}
static inline float rv_sine(float a){float q=a*a;return a*(1-q/6+q*q/120-q*q*q/5040);}
static inline void rv_spring(float *value,float *rate,float target,float omega,float dt){
    float x=omega*dt,e=1/(1+x+.48f*x*x+.235f*x*x*x),delta=*value-target,t=(*rate+omega*delta)*dt;
    *rate=(*rate-omega*t)*e;*value=target+(delta+t)*e;
}
static inline int rv_config_valid(const RVConfig *c){
    if(!c)return 0;
#define RV_RANGE(F,L,H) if(!(c->F>=L&&c->F<=H))return 0
    RV_RANGE(hover_height,.25f,5);RV_RANGE(cruise_speed,1,100);RV_RANGE(boost_speed,1,120);
    RV_RANGE(reverse_speed,1,60);RV_RANGE(strafe_speed,1,60);RV_RANGE(vertical_speed,2,30);
    RV_RANGE(acceleration,1,30);RV_RANGE(boost_acceleration,1,40);RV_RANGE(brake_acceleration,5,40);RV_RANGE(jerk,10,180);
    RV_RANGE(turn_rate,.1f,3);RV_RANGE(turn_falloff,0,.2f);RV_RANGE(turn_response,1,20);RV_RANGE(bank_factor,0,1);
    RV_RANGE(tilt_scale,10,100);RV_RANGE(max_tilt,.05f,1);RV_RANGE(terrain_tilt,.5f,2.68f);
    RV_RANGE(attitude_response,1,15);RV_RANGE(terrain_response,2,15);RV_RANGE(heave_acceleration,5,35);
    RV_RANGE(heave_response,3,20);RV_RANGE(height_response,1,5);RV_RANGE(probe_max,30,250);
#undef RV_RANGE
    return c->boost_speed>=c->cruise_speed&&c->terrain_tilt>=c->max_tilt;
}
static inline int rv_begin(RVFlight *s,RVVec p,RVVec center,RVVec up,RVVec forward,RVVec measured){
    if(!s||!rv_valid(p)||!rv_valid(center)||!rv_valid(up)||!rv_valid(forward)||!rv_valid(measured)||rv_length(up)<.5f)return 0;
    *s=(RVFlight){0};s->config=rv_default_config();s->up=rv_normal(up,rv_v(0,1,0));
    s->heading=rv_tangent(forward,s->up);
    s->velocity=measured;s->radius=rv_length(rv_sub(p,center));s->active=1;return 1;
}
static inline int rv_begin_profile(RVFlight *s,const RVConfig *config,RVVec p,RVVec center,RVVec up,RVVec forward,RVVec measured){
    if(!rv_config_valid(config))return 0;
    if(!rv_begin(s,p,center,up,forward,measured))return 0;
    s->config=*config;s->landing_height=config->hover_height;return 1;
}
static inline float rv_probe_distance(float speed){return rv_clamp(3.f+speed*.7f+speed*speed/36.f,3.f,165.f);}
static inline float rv_probe_distance_profile(float speed,const RVConfig *c){
    return rv_clamp(3.f+speed*.7f+speed*speed/(c->brake_acceleration*1.8f),3.f,c->probe_max);
}
static inline int rv_step_terrain(RVFlight *s,RVInput input,float dt,RVVec position,RVVec center,RVVec measured,RVVec up,float floor_clearance,int has_floor,RVVec surface_up,RVPreview preview,RVOutput *out){
    if(!s||!out||!s->active||!rv_finite(dt)||dt<=0||dt>.051f||!rv_valid(position)||!rv_valid(center)||!rv_valid(measured)||!rv_valid(up)||rv_length(up)<.5f)return 0;
    if(!rv_finite(input.throttle)||!rv_finite(input.steer)||!rv_finite(input.strafe)||!rv_finite(input.rise)||(has_floor&&(!rv_finite(floor_clearance)||!rv_valid(surface_up)||rv_length(surface_up)<.5f)))return 0;
    if(preview.valid&&(!rv_valid(preview.normal)||rv_length(preview.normal)<.5f||!rv_finite(preview.clearance)||!rv_finite(preview.distance)||preview.distance<0))return 0;
    const RVConfig *cfg=&s->config;
    up=rv_normal(up,s->up);RVVec heading=rv_tangent(s->heading,up);
    if(s->surface)input.rise=0;
    if(!input.enabled||s->landing){input.throttle=0;input.strafe=0;input.steer=0;input.rise=0;input.boost=0;input.brake=1;}
    input.throttle=rv_clamp(input.throttle,-1,1);input.steer=rv_clamp(input.steer,-1,1);input.strafe=rv_clamp(input.strafe,-1,1);input.rise=rv_clamp(input.rise,-1,1);
    float speed=rv_length(rv_plane(measured,up)),turn=cfg->turn_rate/(1+speed*cfg->turn_falloff);
    s->yaw_rate+=(input.steer*turn-s->yaw_rate)*rv_clamp(dt*cfg->turn_response,0,1);
    RVVec right=rv_normal(rv_cross(up,heading),rv_v(1,0,0));float angle=s->yaw_rate*dt;
    heading=rv_normal(rv_add(rv_mul(heading,1-angle*angle*.5f),rv_mul(right,rv_sine(angle))),heading);right=rv_cross(up,heading);
    float cruise=input.throttle>=0?(input.boost?cfg->boost_speed:cfg->cruise_speed):cfg->reverse_speed;
    RVVec target=rv_add(rv_mul(heading,input.throttle*cruise),rv_mul(right,input.strafe*cfg->strafe_speed));target=rv_limit(target,input.boost?cfg->boost_speed:cfg->cruise_speed);
    if(input.brake)target=rv_v(0,0,0);
    RVVec planar=rv_plane(measured,up);
    /* Released pedals allow bounded drag rather than requesting full powered
     * braking. Explicit brake, landing and lost input authority still stop.
     * Keep measured drift in world space when the pilot turns while coasting. */
    if(input.enabled&&!input.brake&&!s->landing&&input.throttle==0&&input.strafe==0){
        float drag=rv_clamp(.7f+.06f*speed+.0015f*speed*speed,0,cfg->acceleration*.30f);
        target=speed>.0001f?rv_mul(planar,rv_clamp(1-drag/(2*speed),0,1)):rv_v(0,0,0);
    }
    RVVec travel=rv_normal(planar,rv_normal(target,heading));
    float clearance_rate=has_floor&&s->had_floor?(floor_clearance-s->last_floor_clearance)/dt:0;
    float floor_rise=0,terrain_pitch=0,terrain_roll=0,terrain_weight=0,preview_lift=0;
    int terrain_brake=0;
    if(has_floor){
        surface_up=rv_normal(surface_up,up);
        float ny=rv_clamp(rv_dot(surface_up,up),.35f,1);
        // Uphill feed-forward prevents lag into a face. Falling ground uses
        // the existing stopping-distance descent envelope instead of pulling
        // the craft down a ramp into the following flat floor.
        floor_rise=rv_clamp(-rv_dot(planar,surface_up)/ny,0,1000);
        float slope=rv_length(rv_plane(surface_up,up));
        if(!s->landing&&slope>.025f){
            terrain_weight=s->surface?rv_clamp((5.f-floor_clearance)/4.f,0,1):rv_clamp((2.f-floor_clearance)/1.15f,0,1);
            terrain_pitch=rv_dot(surface_up,heading)/ny;terrain_roll=rv_dot(surface_up,right)/ny;
        }
    }
    if(!s->landing){
        // Reserve enough travel to brake and build heave before the leading
        // corners arrive at a rising plane. This works for reverse and strafe
        // too; input direction cannot hide residual sideways motion.
        for(int i=0;i<2;i++){
            if((i==0&&(!has_floor||floor_clearance>5.f))||(i==1&&!preview.valid))continue;
            RVVec normal=i?rv_normal(preview.normal,up):surface_up;
            float ny=rv_dot(normal,up);if(ny<.35f)continue;
            float grade=-rv_dot(travel,normal)/ny;if(grade<.025f)continue;
            float gap=i?preview.clearance:floor_clearance;
            float distance=rv_clamp((gap-cfg->hover_height)/grade,0,10000);
            float hill_speed=(cfg->vertical_speed*(10.f/12.f))/grade;
            float safe=rv_sqrt(hill_speed*hill_speed+(cfg->brake_acceleration*1.8f)*rv_clamp(distance-speed*.65f-1.f,0,10000));
            float requested=rv_length(target);
            if(safe<requested){target=rv_mul(target,safe/requested);terrain_brake=1;}
            if(speed>safe)terrain_brake=1;
            if(i&&speed>.1f){
                float arrival=distance/speed,blend=rv_clamp((1.4f-arrival)/.6f,0,1);
                // Once this plane is the supporting floor, its clearance
                // feedback owns height again; prediction must not lock in an
                // unnecessarily high approach margin for the whole climb.
                int supporting=has_floor&&rv_dot(normal,surface_up)>.995f&&rv_abs(gap-floor_clearance)<.3f;
                preview_lift=supporting?0:rv_clamp(speed*grade,0,cfg->vertical_speed)*blend;
                float tilt=rv_clamp((1.6f-arrival)/.9f,0,1);
                if(tilt>terrain_weight){terrain_weight=tilt;terrain_pitch=rv_dot(normal,heading)/ny;terrain_roll=rv_dot(normal,right)/ny;}
            }
        }
    }
    float radius=rv_length(rv_sub(position,center));
    int spherical=radius>1&&rv_dot(rv_mul(rv_sub(position,center),1/radius),up)>.99f;
    // A constant-height chord is tangent to the mean of its endpoint ups,
    // not the current up. Otherwise fast travel looks like a persistent climb
    // and the vertical braking state never finishes after releasing lift.
    RVVec heave_up=spherical?rv_normal(rv_add(s->up,up),up):up;
    float measured_vertical=rv_dot(measured,heave_up);
    if(s->surface&&has_floor){s->radius=radius+(cfg->hover_height-floor_clearance);s->vertical_braking=0;}
    else if(s->landing){s->radius=radius-(has_floor?rv_clamp(floor_clearance-s->landing_height-.035f,0,1.2f):2.f)/3.f;s->vertical_braking=0;}
    else if(input.rise!=0){s->radius=radius;s->vertical_braking=0;}
    else if(s->last_rise!=0||s->was_landing)s->vertical_braking=1;
    if(s->vertical_braking){
        // Brake the measured motion, then hold where it actually stops. A
        // theoretical stopping point ignores jerk/response and can cause a
        // pronounced reversal after climb release or landing cancellation.
        s->radius=radius;
        if(rv_abs(measured_vertical)<.025f){s->vertical_braking=0;s->acceleration=rv_plane(s->acceleration,up);}
    }
    // Floor following raises the held altitude only as required for clearance.
    if(has_floor&&!s->landing&&floor_clearance<cfg->hover_height)s->radius=radius+(cfg->hover_height-floor_clearance);
    float vertical=!s->landing&&input.rise!=0?input.rise*cfg->vertical_speed:s->vertical_braking?0:rv_clamp((s->radius-radius)*cfg->height_response,-cfg->vertical_speed,cfg->vertical_speed);
    if(has_floor&&!s->landing&&s->surface)vertical=rv_clamp(floor_rise+(s->radius-radius)*cfg->height_response,-cfg->vertical_speed,cfg->vertical_speed);
    if(!s->landing&&preview_lift>0&&preview_lift>vertical)vertical=preview_lift;
    if(has_floor&&!s->landing){
        float available=rv_clamp(floor_clearance-cfg->hover_height,0,1000);
        // Reserve stopping distance for 350ms of velocity/jerk response as
        // well as braking. The instantaneous sqrt(2*a*d) envelope reacted
        // too late and bounced through most of the intended hover clearance.
        float safe_down=24.f*available/(rv_sqrt(17.64f+24.f*available)+4.2f);
        float following=s->surface?rv_clamp(floor_rise,-cfg->vertical_speed,cfg->vertical_speed):0;
        if(vertical<following-safe_down)vertical=following-safe_down;
        if(floor_clearance<cfg->hover_height){float repel=rv_clamp(following+(cfg->hover_height-floor_clearance)*4.f,0,cfg->vertical_speed);if(vertical<repel)vertical=repel;}
    }
    if(has_floor&&floor_clearance<.5f&&vertical<0)vertical=rv_clamp(vertical,-.65f,0);
    target=rv_add(target,rv_mul(up,vertical));
    RVVec planar_error=rv_plane(rv_sub(target,measured),up);
    RVVec accel=rv_limit(rv_mul(planar_error,input.brake||terrain_brake?3.5f:2.f),input.brake||terrain_brake?cfg->brake_acceleration:input.boost?cfg->boost_acceleration:cfg->acceleration);
    // Close to a floor, build descent gently enough that the jerk limiter can
    // reverse the thrust before the clearance is consumed. Landing owns its
    // separate low-speed profile and must still reach below hover height.
    float down_limit=has_floor&&!s->landing?rv_clamp((floor_clearance-cfg->hover_height)*2.f,0,cfg->heave_acceleration):cfg->heave_acceleration;
    float vertical_accel=rv_clamp((vertical-measured_vertical)*cfg->heave_response,-down_limit,cfg->heave_acceleration);
    accel=rv_add(accel,rv_mul(up,vertical_accel));
    float jerk=cfg->jerk;
    if(has_floor&&!s->landing){
        float closing=rv_dot(measured,surface_up);
        if(s->had_floor&&clearance_rate*rv_dot(surface_up,up)<closing)closing=clearance_rate*rv_dot(surface_up,up);
        float space=rv_clamp((floor_clearance-.2f)*rv_dot(surface_up,up),0,1000);
        // Reserve response time as well as stopping distance. Waiting until
        // the hull is almost touching cannot arrest a fast crest transition.
        float safe_close=36.f*space/(rv_sqrt(29.16f+36.f*space)+5.4f);
        if(closing<-safe_close){
            float force=rv_clamp((-safe_close-closing)*14.f+rv_clamp(.65f-floor_clearance,0,1)*35.f,0,45.f);
            accel=rv_add(accel,rv_mul(surface_up,force));jerk=120.f;
        }
    }
    s->acceleration=rv_add(s->acceleration,rv_limit(rv_sub(accel,s->acceleration),jerk*dt));
    RVVec velocity=rv_add(measured,rv_mul(s->acceleration,dt));
    // Jerk-limited braking must stop at zero, rather than briefly reversing a
    // released control while the old acceleration unwinds.
    RVVec axes[2]={heading,right};
    for(int i=0;i<2;i++)if(rv_abs(rv_dot(target,axes[i]))<.0001f){
        float before=rv_dot(measured,axes[i]),after=rv_dot(velocity,axes[i]);
        if(before*after<=0){velocity=rv_sub(velocity,rv_mul(axes[i],after));s->acceleration=rv_sub(s->acceleration,rv_mul(axes[i],rv_dot(s->acceleration,axes[i])));}
    }
    RVVec planar_velocity=rv_plane(velocity,up);
    float curvature=spherical?rv_dot(planar_velocity,planar_velocity)/radius:0;
    velocity=rv_sub(velocity,rv_mul(up,curvature*dt));
    float commanded_heave=rv_dot(velocity,up)+.5f*curvature*dt;
    if(s->vertical_braking&&measured_vertical*commanded_heave<=0){
        velocity=rv_sub(velocity,rv_mul(up,commanded_heave));s->acceleration=rv_plane(s->acceleration,up);s->vertical_braking=0;s->radius=radius;
    }
    if(input.brake&&speed<.03f)velocity=rv_mul(up,rv_dot(velocity,up));
    // Tilting follows actual lateral acceleration, including braking and drift.
    // Coordinated turn bank also anticipates the curvature of a fast turn.
    float pitch=rv_clamp(rv_dot(s->acceleration,heading)/cfg->tilt_scale,-.34f,.34f);
    float roll=rv_clamp((rv_dot(s->acceleration,right)+s->yaw_rate*speed*cfg->bank_factor)/cfg->tilt_scale,-.4f,.4f);
    if(terrain_weight>0){
        pitch=pitch*(1-terrain_weight)+terrain_pitch*terrain_weight;
        roll=roll*(1-terrain_weight)+terrain_roll*terrain_weight;
    }
    RVVec landing_up=up;
    if(s->landing){
        // Settle onto the supporting plane before native suspension takes over.
        // Level handoff on a hillside drops one side and produces a sharp roll.
        if(has_floor){
            float blend=rv_clamp((5.f-floor_clearance)/4.f,0,1);
            landing_up=rv_normal(rv_add(rv_mul(up,1-blend),rv_mul(rv_normal(surface_up,up),blend)),up);
        }
        float vertical_up=rv_clamp(rv_dot(landing_up,up),.35f,1);
        pitch=rv_dot(landing_up,heading)/vertical_up;roll=rv_dot(landing_up,right)/vertical_up;
    }
    // Limit the target before damping. Clamping the current attitude would
    // snap a slope-aligned landing back to the cruise envelope on cancellation.
    float target_tilt=rv_sqrt(pitch*pitch+roll*roll),limit=s->landing?2.68f:terrain_weight>0?cfg->terrain_tilt:cfg->max_tilt;
    if(target_tilt>limit){pitch*=limit/target_tilt;roll*=limit/target_tilt;}
    float omega=s->landing?4.f:terrain_weight>0?cfg->terrain_response:cfg->attitude_response;
    rv_spring(&s->pitch,&s->pitch_rate,pitch,omega,dt);rv_spring(&s->roll,&s->roll_rate,roll,omega,dt);
    RVVec body_up=rv_normal(rv_add(up,rv_add(rv_mul(heading,s->pitch),rv_mul(right,s->roll))),up);
    RVVec body_forward=rv_normal(rv_plane(heading,body_up),heading);
    s->landed=s->landing&&has_floor&&rv_abs(floor_clearance-s->landing_height)<.12f&&rv_abs(rv_dot(measured,up))<.3f&&speed<.6f&&rv_dot(body_up,landing_up)>.999f;
    if(s->landed)velocity=rv_v(0,0,0);
    s->last_floor_clearance=floor_clearance;s->had_floor=has_floor;
    s->heading=heading;s->up=up;s->velocity=velocity;s->last_rise=input.rise;s->was_landing=s->landing;
    *out=(RVOutput){velocity,body_forward,body_up,s->pitch,s->roll,speed,rv_clamp(speed/cfg->boost_speed+rv_abs(vertical)/(cfg->vertical_speed*(40.f/12.f)),.12f,1),s->landed};return 1;
}
static inline int rv_step_surface(RVFlight *s,RVInput input,float dt,RVVec position,RVVec center,RVVec measured,RVVec up,float floor_clearance,int has_floor,RVVec surface_up,RVOutput *out){
    return rv_step_terrain(s,input,dt,position,center,measured,up,floor_clearance,has_floor,surface_up,(RVPreview){0},out);
}
static inline int rv_step(RVFlight *s,RVInput input,float dt,RVVec position,RVVec center,RVVec measured,RVVec up,float floor_clearance,int has_floor,RVOutput *out){
    return rv_step_surface(s,input,dt,position,center,measured,up,floor_clearance,has_floor,up,out);
}
#endif
