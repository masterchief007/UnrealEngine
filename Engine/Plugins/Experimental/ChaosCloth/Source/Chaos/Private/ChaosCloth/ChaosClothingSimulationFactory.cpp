// Copyright Epic Games, Inc. All Rights Reserved.

#include "ChaosCloth/ChaosClothingSimulationFactory.h"
#include "ChaosCloth/ChaosClothConfig.h"
#include "ChaosCloth/ChaosClothingSimulation.h"
#include "ChaosCloth/ChaosWeightMapTarget.h"
#include "ChaosCloth/ChaosClothingSimulationInteractor.h"

IClothingSimulation* UChaosClothingSimulationFactory::CreateSimulation()
{
	IClothingSimulation* Simulation = new Chaos::FClothingSimulation();
	return Simulation;
}

void UChaosClothingSimulationFactory::DestroySimulation(IClothingSimulation* InSimulation)
{
    delete InSimulation;
}

bool UChaosClothingSimulationFactory::SupportsAsset(UClothingAssetBase* InAsset)
{
#if WITH_CHAOS
    return true;
#else
    return false;
#endif
}

bool UChaosClothingSimulationFactory::SupportsRuntimeInteraction()
{
    return true;
}

UClothingSimulationInteractor* UChaosClothingSimulationFactory::CreateInteractor()
{
	return NewObject<UChaosClothingSimulationInteractor>(GetTransientPackage());
}

TArrayView<const TSubclassOf<UClothConfigBase>> UChaosClothingSimulationFactory::GetClothConfigClasses() const
{
	static const TArray<TSubclassOf<UClothConfigBase>> ClothConfigClasses(
		{
			TSubclassOf<UClothConfigBase>(UChaosClothConfig::StaticClass()),
			TSubclassOf<UClothConfigBase>(UChaosClothSharedSimConfig::StaticClass())
		});
	return ClothConfigClasses;
}

const UEnum* UChaosClothingSimulationFactory::GetWeightMapTargetEnum() const
{
	return StaticEnum<EChaosWeightMapTarget>();
}
