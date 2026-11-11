#include "vehicle_input.h"
typedef int (*MoonVehicleCallback)(int,Context *,MoonVehicleInput *);
__declspec(dllexport) int MoonLab_VehicleBridgeAbi(void){return 1;}
static int install_vehicle_adapter(int role,int id,MoonVehicleCallback callback){
    PTR loader=GetModuleHandleA("version.dll");
    if(!loader||!GetProcAddress(loader,"MoonLab_LoaderAbi"))return 1; /* Standalone single-mod probe. */
    const MoonHookApiV1 *api=loader_hook_api(loader);
    if(!api||(role!=1&&role!=2))return 0;
    for(int i=0;i<hook_count;i++)if(hooks[i].id==id){
        MoonHookSubscriptionV1 spec={sizeof(spec),MOON_HOOK_VEHICLE_FIXED,role==1?MOON_ROLE_INPUT:MOON_ROLE_PHYSICS,MOON_PHASE_NORMAL,0,id,0,callback};
        if(!api->subscribe(&spec))return 0;
        for(int j=i+1;j<hook_count;j++)hooks[j-1]=hooks[j];
        hook_count--;return 1;
    }
    return 0;
}
