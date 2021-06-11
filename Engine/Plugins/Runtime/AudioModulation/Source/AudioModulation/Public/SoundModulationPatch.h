// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "CoreMinimal.h"
#include "IAudioModulation.h"
#include "SoundModulationParameter.h"
#include "SoundModulationTransform.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "SoundModulationPatch.generated.h"

// Forward Declarations
class USoundControlBus;


USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundControlModulationInput
{
	GENERATED_USTRUCT_BODY()

	FSoundControlModulationInput();

	/** Get the modulated input value on parent patch initialization and hold that value for its lifetime */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input, meta = (DisplayName = "Sample-And-Hold"))
	uint8 bSampleAndHold : 1;

	/** Transform to apply to the input prior to mix phase */
	UPROPERTY(EditAnywhere, Category = Input)
	FSoundModulationTransform Transform;

	/** The input bus */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Input)
	USoundControlBus* Bus = nullptr;

	const USoundControlBus* GetBus() const;
	const USoundControlBus& GetBusChecked() const;
};

USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundControlModulationPatch
{
	GENERATED_USTRUCT_BODY()

	/** Whether or not patch is bypassed (patch is still active, but always returns output parameter default value when modulated) */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs)
	bool bBypass = true;

	UPROPERTY(EditAnywhere, BlueprintReadOnly, Category = Output, meta = (DisplayName = "Parameter"))
	USoundModulationParameter* OutputParameter = nullptr;

	/** Modulation inputs */
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Inputs)
	TArray<FSoundControlModulationInput> Inputs;

};

UCLASS(config = Engine, editinlinenew, BlueprintType)
class AUDIOMODULATION_API USoundModulationPatch : public USoundModulatorBase
{
	GENERATED_UCLASS_BODY()

public:
	UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = Modulation, meta = (ShowOnlyInnerProperties))
	FSoundControlModulationPatch PatchSettings;

	virtual FName GetOutputParameterName() const override
	{
		if (PatchSettings.OutputParameter)
		{
			return PatchSettings.OutputParameter->GetFName();
		}

		return Super::GetOutputParameterName();
	}

#if WITH_EDITOR
	virtual void PostEditChangeProperty(FPropertyChangedEvent& InPropertyChangedEvent) override;
	virtual void PostEditChangeChainProperty(FPropertyChangedChainEvent& InPropertyChangedEvent) override;
#endif // WITH_EDITOR
};
