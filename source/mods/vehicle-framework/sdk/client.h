#ifndef MOON_VEHICLE_CLIENT_H
#define MOON_VEHICLE_CLIENT_H
#include "vehicle.h"
#include "../../../runtime/native/services.h"
__declspec(dllimport) DWORD __stdcall GetModuleFileNameA(PTR, char *, DWORD);
static const MoonVehicles *mv_connect(PTR domain, PTR game) {
    /* A recognized loader owns dependency startup and provider identity. A
     * missing/incompatible service fails closed instead of loading a second DLL. */
    if(moon_runtime_loader_present()){
        const MoonRuntimeApiV1 *runtime=moon_runtime_connect();
        const MoonVehicles *api=runtime?(const MoonVehicles *)runtime->find("vehicle-framework","vehicles",MV_ABI,sizeof(MoonVehicles)):0;
        return api&&api->abi==MV_ABI&&api->size>=sizeof(MoonVehicles)?api:0;
    }

  char path[1200];
  DWORD n = GetModuleFileNameA(0, path, 1000);
  if (!n || n >= 1000)
    return 0;
  while (n && path[n - 1] != '\\' && path[n - 1] != '/')
    n--;
  if (!n)
    return 0;
  cat(path + n, "MoonLab\\mods\\vehicle-framework\\mod.dll");
  PTR module = LoadLibraryA(path);
  if (!module)
    return 0;
  int (*start)(PTR, PTR) =
      (int (*)(PTR, PTR))GetProcAddress(module, "MoonLab_ModStart");
  MoonVehiclesGet get =
      (MoonVehiclesGet)GetProcAddress(module, "MoonLab_Vehicles_v1");
  if (!start || !get || !start(domain, game))
    return 0;
  return get(MV_ABI, sizeof(MoonVehicles));
}
#endif
