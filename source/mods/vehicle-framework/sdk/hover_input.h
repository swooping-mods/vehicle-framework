#include "../../../runtime/native/vehicle_input.h"
/* A captured physical keyboard key must not re-enter through the native
 * keyboard/gamepad aggregate. Unclaimed native axes and fresh wheels retain
 * their existing fallback/override order. */
static inline float rv_keyboard_axis(int positive,int negative,int physical_positive,int physical_negative,int *native_allowed){
    *native_allowed=!(physical_positive&&!positive)&&!(physical_negative&&!negative);
    return (float)(positive-negative);
}
/* Delivery masking cannot invent a press when a claim ends while the chord
 * remains held. Observe physical history even while UI/focus gates are shut. */
static inline int rv_hover_toggle_edge(int delivered,int physical,int allowed,int *was_physical){
    int pressed=delivered&&allowed&&!*was_physical;
    *was_physical=physical;return pressed;
}
/* Wheel axes own yaw/thrust while a fresh sample is eligible. Keyboard altitude
 * and strafe remain available. An invalid sample never replays an old value. */
static inline void rv_wheel_input(RVInput *input,const MoonVehicleInput *wheel){
    if(!input->enabled||!moon_vehicle_input_valid(wheel))return;
    input->steer=wheel->steer;input->throttle=wheel->brake?0:wheel->throttle;
    input->brake=(int)wheel->brake;input->boost=(int)wheel->boost;
}
