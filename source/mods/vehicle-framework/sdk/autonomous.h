#ifndef MOON_VEHICLE_AUTONOMOUS_H
#define MOON_VEHICLE_AUTONOMOUS_H
#include "../../../runtime/native/vehicle_input.h"
#define MVA_ABI 1u
#define MVA_CAR_LIMIT 64
/* The shared input broker requests exactly the immediately following physics
 * callback. A null input cancels. Only ordinary native cars are supported. */
typedef struct {
    unsigned int abi,size;
    int (*request)(void *vehicle,const MoonVehicleInput *input);
    int (*stepping)(void *vehicle);
    int (*supports)(void *vehicle);
    int (*work)(void *vehicle,int enabled);
    int (*can_work)(void *vehicle);
} MoonAutonomous;
typedef const MoonAutonomous *(*MoonAutonomousGet)(unsigned int,unsigned int);
#endif
