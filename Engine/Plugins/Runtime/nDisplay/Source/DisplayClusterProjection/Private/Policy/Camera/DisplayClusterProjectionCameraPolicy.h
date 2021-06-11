// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Policy/DisplayClusterProjectionPolicyBase.h"

class UCameraComponent;
class UWorld;


/**
 * Implements math behind the native camera projection policy (use symmetric frustum of a camera)
 */
class FDisplayClusterProjectionCameraPolicy
	: public FDisplayClusterProjectionPolicyBase
{
public:
	FDisplayClusterProjectionCameraPolicy(const FString& ViewportId, const TMap<FString, FString>& Parameters);
	virtual ~FDisplayClusterProjectionCameraPolicy();

public:
	//////////////////////////////////////////////////////////////////////////////////////////////
	// IDisplayClusterProjectionPolicy
	//////////////////////////////////////////////////////////////////////////////////////////////
	virtual void StartScene(UWorld* World) override;
	virtual void EndScene() override;
	virtual bool HandleAddViewport(const FIntPoint& InViewportSize, const uint32 InViewsAmount) override;
	virtual void HandleRemoveViewport() override;

	virtual bool CalculateView(const uint32 ViewIdx, FVector& InOutViewLocation, FRotator& InOutViewRotation, const FVector& ViewOffset, const float WorldToMeters, const float NCP, const float FCP) override;
	virtual bool GetProjectionMatrix(const uint32 ViewIdx, FMatrix& OutPrjMatrix) override;

	virtual bool IsWarpBlendSupported() override
	{ return false; }

public:
	void SetCamera(UCameraComponent* NewCamera, float FOVMultiplier);

private:
	UWorld* World;
	// Camera to use for rendering
	UCameraComponent* AssignedCamera = nullptr;
	// FOV multiplier
	float CurrentFovMultiplier = 1.f;
};
