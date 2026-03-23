#include "terrain_actor.h"

ATerrainActor::ATerrainActor() {
    terrain = CreateDefaultSubobject<UTerrain>(TEXT("TerrainRoot"));
    SetRootComponent(terrain);
    check(IsValid(terrain));
}
