# Vehicle Framework: behavior and implementation

Describes the exported implementation **0.2.1**. The linked source and metadata define this version; gameplay qualification is a separate requirement.

## Behavior

Registers explicit custom vehicle IDs and native garage offers while preserving vanilla catalog entries. Providers supply definitions, model/cabin presentation, optional callbacks or shared hover profiles. Per-ID dispatch routes update/physics/exit to the matching provider; absent callbacks preserve native behavior. Shared hover/flight handles local authority, hull support probes, surface/free flight, landing and body restoration. The separate autonomous service admits bounded native unattended physics/cleaning requests for eligible ordinary cars.

## State and lifecycle

Stable IDs and owned/storage records use native saves/network identity; registration must not renumber them. Startup validates ABI/prefix sizes, IDs and duplicates. Rooted per-world catalog/instance data and shared assets release in ownership order; provider callbacks remain resident. Garage delivery revalidates footprint clearance and native bay/fallback anchors. Fleets must address exact storage IDs.

## Integration and configuration

No additional feature-mod runtime dependency is declared. Included shared headers are source reuse, not extra DLL dependencies.

The [module metadata](../source/mods/vehicle-framework/mod.json) owns the version and declared integration contract. Native modules start at `MoonLab_ModStart`. Game/Unity integrations target the pinned Steam Windows x64 IL2CPP build.

| Service | ABI | Table size |
|---|---:|---|
| Provides `vehicle-framework.vehicles` | 1 | 48 bytes |
| Provides `vehicle-framework.autonomous` | 1 | 48 bytes |

## Implementation sources

- [mod.c](../source/mods/vehicle-framework/mod.c)
- [definitions.h](../source/mods/vehicle-framework/definitions.h)
- [catalog_registry.h](../source/mods/vehicle-framework/catalog_registry.h)
- [instances.h](../source/mods/vehicle-framework/instances.h)
- [hover_runtime.h](../source/mods/vehicle-framework/hover_runtime.h)
- [autonomous_runtime.h](../source/mods/vehicle-framework/autonomous_runtime.h)
- [garage_ui.h](../source/mods/vehicle-framework/garage_ui.h)

## Verification boundary

No arbitrary asset importer, universal seat/animation system, lossless missing-provider recovery or cross-build promise. All peers need compatible IDs/providers. Native vehicle, hover, unattended control and collision paths each need scoped game qualification.

`python3 build.py --check` verifies the exported source hashes. It does not execute the mod or qualify the behaviors above. Installation and independent build prerequisites are in the [README](../README.md).
