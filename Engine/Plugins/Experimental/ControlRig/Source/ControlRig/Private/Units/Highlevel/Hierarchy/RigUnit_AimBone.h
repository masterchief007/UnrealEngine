// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "Units/Highlevel/RigUnit_HighlevelBase.h"
#include "RigUnit_AimBone.generated.h"

USTRUCT()
struct FRigUnit_AimBone_Target
{
	GENERATED_BODY()

	FRigUnit_AimBone_Target()
	{
		Weight = 1.f;
		Axis = FVector(1.f, 0.f, 0.f);
		Target = FVector(1.f, 0.f, 0.f);
		Kind = EControlRigVectorKind::Location;
		Space = NAME_None;
	}

	/**
	 * The amount of aim rotation to apply on this target.
	 */
	UPROPERTY(EditAnywhere, meta = (Input), Category = "AimTarget")
	float Weight;

	/**
	 * The axis to align with the aim on this target
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "Weight > 0.0"), Category = "AimTarget")
	FVector Axis;

	/**
	 * The target to aim at - can be a direction or location based on the Kind setting
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "Weight > 0.0"), Category = "AimTarget")
	FVector Target;

	/**
	 * The kind of target this is representing - can be a direction or a location
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "Weight > 0.0"), Category = "AimTarget")
	EControlRigVectorKind Kind;

	/**
	 * The space in which the target is expressed
	 */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Target Space", Input, EditCondition = "Weight > 0.0" ), Category = "AimTarget")
	FName Space;
};

USTRUCT()
struct FRigUnit_AimItem_Target
{
	GENERATED_BODY()

	FRigUnit_AimItem_Target()
	{
		Weight = 1.f;
		Axis = FVector(1.f, 0.f, 0.f);
		Target = FVector(1.f, 0.f, 0.f);
		Kind = EControlRigVectorKind::Location;
		Space = FRigElementKey(NAME_None, ERigElementType::Bone);
	}

	/**
	 * The amount of aim rotation to apply on this target.
	 */
	UPROPERTY(EditAnywhere, meta = (Input), Category = "AimTarget")
	float Weight;

	/**
	 * The axis to align with the aim on this target
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "Weight > 0.0"), Category = "AimTarget")
	FVector Axis;

	/**
	 * The target to aim at - can be a direction or location based on the Kind setting
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "Weight > 0.0"), Category = "AimTarget")
	FVector Target;

	/**
	 * The kind of target this is representing - can be a direction or a location
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "Weight > 0.0"), Category = "AimTarget")
	EControlRigVectorKind Kind;

	/**
	 * The space in which the target is expressed
	 */
	UPROPERTY(EditAnywhere, meta = (DisplayName = "Target Space", Input, EditCondition = "Weight > 0.0" ), Category = "AimTarget")
	FRigElementKey Space;
};

USTRUCT()
struct FRigUnit_AimBone_DebugSettings
{
	GENERATED_BODY()

	FRigUnit_AimBone_DebugSettings()
	{
		bEnabled = false;
		Scale = 10.f;
		WorldOffset = FTransform::Identity;
	}

	/**
	 * If enabled debug information will be drawn 
	 */
	UPROPERTY(EditAnywhere, meta = (Input), Category = "DebugSettings")
	bool bEnabled;

	/**
	 * The size of the debug drawing information
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "bEnabled"), Category = "DebugSettings")
	float Scale;

	/**
	 * The offset at which to draw the debug information in the world
	 */
	UPROPERTY(EditAnywhere, meta = (Input, EditCondition = "bEnabled"), Category = "DebugSettings")
	FTransform WorldOffset;
};

/**
 * Aligns the rotation of a primary and secondary axis of an input transform to a world target.
 * Note: This node operates in world space!
 */
USTRUCT(meta = (DisplayName = "Aim Math", Category = "Hierarchy", Keywords = "Lookat"))
struct FRigUnit_AimBoneMath : public FRigUnit_HighlevelBase
{
	GENERATED_BODY()

	FRigUnit_AimBoneMath()
	{
		InputTransform = FTransform::Identity;
		Primary = FRigUnit_AimItem_Target();
		Secondary = FRigUnit_AimItem_Target();
		Primary.Axis = FVector(1.f, 0.f, 0.f);
		Secondary.Axis = FVector(0.f, 0.f, 1.f);
		Weight = 1.f;
		DebugSettings = FRigUnit_AimBone_DebugSettings();
		PrimaryCachedSpace = FCachedRigElement();
		SecondaryCachedSpace = FCachedRigElement();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/**
	 * The transform (in global space) before the aim was applied (optional)
	 */
	UPROPERTY(meta = (Input))
	FTransform InputTransform;

	/**
	 * The primary target for the aim
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigUnit_AimItem_Target Primary;

	/**
	 * The secondary target for the aim - also referred to as PoleVector / UpVector
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_AimItem_Target Secondary;

	/**
	 * The weight of the change - how much the change should be applied
	 */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
	float Weight;

	/**
	 * The resulting transform
	 */
	UPROPERTY(meta = (Output))
	FTransform Result;

	/** The debug setting for the node */
	UPROPERTY(meta = (Input, DetailsOnly))
	FRigUnit_AimBone_DebugSettings DebugSettings;

	UPROPERTY()
	FCachedRigElement PrimaryCachedSpace;

	UPROPERTY()
	FCachedRigElement SecondaryCachedSpace;
};

/**
 * Aligns the rotation of a primary and secondary axis of a bone to a global target.
 * Note: This node operates in global space!
 */
USTRUCT(meta=(DisplayName="Aim", Category="Hierarchy", Keywords="Lookat", Deprecated = "4.25"))
struct FRigUnit_AimBone : public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_AimBone()
	{
		Bone = NAME_None;
		Primary = FRigUnit_AimBone_Target();
		Secondary = FRigUnit_AimBone_Target();
		Primary.Axis = FVector(1.f, 0.f, 0.f);
		Secondary.Axis = FVector(0.f, 0.f, 1.f);
		Weight = 1.f;
		bPropagateToChildren = false;
		DebugSettings = FRigUnit_AimBone_DebugSettings();
		CachedBoneIndex = FCachedRigElement();
		PrimaryCachedSpace = FCachedRigElement();
		SecondaryCachedSpace = FCachedRigElement();
	}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (InPinPath.StartsWith(TEXT("Primary.Target")))
		{
			return FRigElementKey(Primary.Space, ERigElementType::Bone);
		}
		if (InPinPath.StartsWith(TEXT("Secondary.Target")))
		{
			return FRigElementKey(Secondary.Space, ERigElementType::Bone);
		}
		return FRigElementKey();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** 
	 * The name of the bone to align
	 */
	UPROPERTY(meta = (Input))
	FName Bone;

	/**
	 * The primary target for the aim 
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigUnit_AimBone_Target Primary;

	/**
	 * The secondary target for the aim - also referred to as PoleVector / UpVector
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_AimBone_Target Secondary;

	/**
	 * The weight of the change - how much the change should be applied
	 */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
	float Weight;

	/**
	 * If set to true all of the global transforms of the children
	 * of this bone will be recalculated based on their local transforms.
	 * Note: This is computationally more expensive than turning it off.
	 */
	UPROPERTY(meta = (Input, Constant))
	bool bPropagateToChildren;

	/** The debug setting for the node */
	UPROPERTY(meta = (Input, DetailsOnly))
	FRigUnit_AimBone_DebugSettings DebugSettings;

	UPROPERTY()
	FCachedRigElement CachedBoneIndex;

	UPROPERTY()
	FCachedRigElement PrimaryCachedSpace;

	UPROPERTY()
	FCachedRigElement SecondaryCachedSpace;
};

/**
 * Aligns the rotation of a primary and secondary axis of a bone to a global target.
 * Note: This node operates in global space!
 */
USTRUCT(meta=(DisplayName="Aim", Category="Hierarchy", Keywords="Lookat"))
struct FRigUnit_AimItem: public FRigUnit_HighlevelBaseMutable
{
	GENERATED_BODY()

	FRigUnit_AimItem()
	{
		Item = FRigElementKey(NAME_None, ERigElementType::Bone);
		Primary = FRigUnit_AimItem_Target();
		Secondary = FRigUnit_AimItem_Target();
		Primary.Axis = FVector(1.f, 0.f, 0.f);
		Secondary.Axis = FVector(0.f, 0.f, 1.f);
		Weight = 1.f;
		DebugSettings = FRigUnit_AimBone_DebugSettings();
		CachedItem = FCachedRigElement();
		PrimaryCachedSpace = FCachedRigElement();
		SecondaryCachedSpace = FCachedRigElement();
	}

	virtual FRigElementKey DetermineSpaceForPin(const FString& InPinPath, void* InUserContext) const override
	{
		if (InPinPath.StartsWith(TEXT("Primary.Target")))
		{
			return Primary.Space;
		}
		if (InPinPath.StartsWith(TEXT("Secondary.Target")))
		{
			return Secondary.Space;
		}
		return FRigElementKey();
	}

	RIGVM_METHOD()
	virtual void Execute(const FRigUnitContext& Context) override;

	/** 
	 * The name of the item to align
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigElementKey Item;

	/**
	 * The primary target for the aim 
	 */
	UPROPERTY(meta = (Input, ExpandByDefault))
	FRigUnit_AimItem_Target Primary;

	/**
	 * The secondary target for the aim - also referred to as PoleVector / UpVector
	 */
	UPROPERTY(meta = (Input))
	FRigUnit_AimItem_Target Secondary;

	/**
	 * The weight of the change - how much the change should be applied
	 */
	UPROPERTY(meta = (Input, UIMin = "0.0", UIMax = "1.0"))
	float Weight;

	/** The debug setting for the node */
	UPROPERTY(meta = (Input, DetailsOnly))
	FRigUnit_AimBone_DebugSettings DebugSettings;

	UPROPERTY()
	FCachedRigElement CachedItem;

	UPROPERTY()
	FCachedRigElement PrimaryCachedSpace;

	UPROPERTY()
	FCachedRigElement SecondaryCachedSpace;
};
