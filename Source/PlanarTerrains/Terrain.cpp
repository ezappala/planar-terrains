// Fill out your copyright notice in the Description page of Project Settings.

#include "PlanarTerrains/Terrain.h"

ATerrain::ATerrain() {
    root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(root);

    terrain = CreateDefaultSubobject<UUDLODTerrainComponent>(TEXT("TerrainComp"));
    terrain->SetupAttachment(root);
}
