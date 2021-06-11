// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "IDisplayClusterProjection.h"
#include "Render/Projection/IDisplayClusterProjectionPolicyFactory.h"

class IDisplayClusterProjectionPolicyFactory;
class USceneComponent;
class UStaticMeshComponent;


class FDisplayClusterProjectionModule
	: public IDisplayClusterProjection
{
public:
	FDisplayClusterProjectionModule();
	virtual ~FDisplayClusterProjectionModule();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IModuleInterface
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void StartupModule() override;
	virtual void ShutdownModule() override;

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjection
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void GetSupportedProjectionTypes(TArray<FString>& OutProjectionTypes) override;
	virtual TSharedPtr<IDisplayClusterProjectionPolicyFactory> GetProjectionFactory(const FString& InProjectionType) override;

	virtual bool AssignWarpMeshToViewport(const FString& ViewportId, UStaticMeshComponent* MeshComponent, USceneComponent* OriginComponent) override;

private:
	// Available factories
	TMap<FString, TSharedPtr<IDisplayClusterProjectionPolicyFactory>> ProjectionPolicyFactories;
};
