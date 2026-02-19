// Fill out your copyright notice in the Description page of Project Settings.

#include "PlanarTerrains/Terrain.h"

#include "subsys.h"
#include "Components/PrimitiveComponent.h"
#include "Components/StaticMeshComponent.h"

ATerrain::ATerrain() {
    root = CreateDefaultSubobject<USceneComponent>(TEXT("RootComp"));
    SetRootComponent(root);
    check(IsValid(root));

    terrain_visualizer = CreateDefaultSubobject<UStaticMeshComponent>(TEXT("TerrainVisualizer"));
    terrain_visualizer->SetupAttachment(root);
    check(IsValid(terrain_visualizer));

    terrain_component = CreateDefaultSubobject<UTerrainComponent>(TEXT("TerrainComponent"));
    terrain_component->SetupAttachment(terrain_visualizer);
    check(IsValid(terrain_component));
}

void ATerrain::BeginPlay() {
    Super::BeginPlay();
}
