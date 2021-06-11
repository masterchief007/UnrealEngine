// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "CRSimPoint.generated.h"

UENUM()
enum class ECRSimPointIntegrateType : uint8
{
	Verlet,
	SemiExplicitEuler
};

USTRUCT()
struct FCRSimPoint
{
	GENERATED_BODY()

	FCRSimPoint()
	{
		Mass = 1.f;
		Size = 0.f;
		LinearDamping = 0.01f;
		InheritMotion = 0.f;
		Position = LinearVelocity = FVector::ZeroVector;
	}

	/**
	 * The mass of the point
	 */
	UPROPERTY()
	float Mass;

	/**
	 * Size of the point - only used for collision
	 */
	UPROPERTY()
	float Size;

	/**
	 * The linear damping of the point
	 */
	UPROPERTY()
	float LinearDamping;

	/**
	 * Defines how much the point will inherit motion from its input.
	 * This does not have an effect on passive (mass == 0.0) points.
	 * Values can be higher than 1 due to timestep - but they are clamped internally.
	 */
	UPROPERTY()
	float InheritMotion;

	/**
	 * The position of the point
	 */
	UPROPERTY()
	FVector Position;

	/**
	 * The velocity of the point per second
	 */
	UPROPERTY()
	FVector LinearVelocity;

	FCRSimPoint IntegrateVerlet(const FVector& InForce, float InBlend, float InDeltaTime) const;
	FCRSimPoint IntegrateSemiExplicitEuler(const FVector& InForce, float InDeltaTime) const;
};
