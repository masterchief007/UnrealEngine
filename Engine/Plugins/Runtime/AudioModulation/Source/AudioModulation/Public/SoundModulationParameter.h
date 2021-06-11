// Copyright Epic Games, Inc. All Rights Reserved.
#pragma once

#include "IAudioModulation.h"
#include "Math/Interval.h"
#include "Sound/SoundModulationDestination.h"
#include "UObject/ObjectMacros.h"
#include "UObject/Object.h"

#include "SoundModulationParameter.generated.h"

USTRUCT(BlueprintType)
struct AUDIOMODULATION_API FSoundModulationParameterSettings
{
	GENERATED_USTRUCT_BODY()

	/** 
	  * Normalized, unitless default value of modulator. To ensure bypass functionality of mixing, patching, and modulating 
	  * functions as anticipated, value should be selected such that GetMixFunction (see USoundModulationParameter)
	  * reduces to an identity function (i.e. function acts as a "pass-through" for all values in the range [0.0, 1.0]).
	  * If GetMixFunction performs the mathematical operation f(x1, x2), then the default ValueNormalized should result in
	  * f(x1, d) = x1 where d is ValueNormalized.
	  */
	UPROPERTY(EditAnywhere, Category = General, meta = (ClampMin = "0.0", ClampMax = "1.0"))
	float ValueNormalized = 1.0f;

#if WITH_EDITORONLY_DATA
	/** (Optional) Text name of parameter's unit */
	UPROPERTY(EditAnywhere, Category = General)
	FText UnitDisplayName;

	/** Default value of modulator in units (editor only) */
	UPROPERTY(Transient, EditAnywhere, Category = General)
	float ValueUnit = 1.0f;
#endif // WITH_EDITORONLY_DATA
};

UCLASS(BlueprintType)
class AUDIOMODULATION_API USoundModulationParameter : public UObject
{
	GENERATED_BODY()

public:
	UPROPERTY(EditAnywhere, Category = General, BlueprintReadOnly, meta = (DisplayName = "Parameter"))
	FSoundModulationParameterSettings Settings;

public:
	/** Whether or not the parameter requires a unit conversion. */
	virtual bool RequiresUnitConversion() const
	{
		return false;
	}

	/** Function used to mix modulator units together */
	virtual Audio::FModulationMixFunction GetMixFunction() const
	{
		static const Audio::FModulationMixFunction MixFunction = [](float* RESTRICT OutValueBuffer, const float* RESTRICT InValueBuffer, int32 InNumSamples)
		{
			for (int32 i = 0; i < InNumSamples; ++i)
			{
				OutValueBuffer[i] *= InValueBuffer[i];
			}
		};

		return MixFunction;
	}

	/** Function used to convert normalized, unitless value to unit value */
	virtual Audio::FModulationNormalizedConversionFunction  GetUnitConversionFunction() const
	{
		static const Audio::FModulationUnitConvertFunction ConversionFunction = [](float* RESTRICT OutValueBuffer, int32 InNumSamples)
		{
		};

		return ConversionFunction;
	}

	/** Function used to convert unit value to normalized, unitless value */
	virtual Audio::FModulationNormalizedConversionFunction GetNormalizedConversionFunction() const
	{
		static const Audio::FModulationNormalizedConversionFunction ConversionFunction = [](float* RESTRICT OutValueBuffer, int32 InNumSamples)
		{
		};

		return ConversionFunction;
	}

	/** Converts normalized, unitless value [0.0f, 1.0f] to unit value. */
	virtual float ConvertNormalizedToUnit(float InNormalizedValue) const final
	{
		float UnitValue = InNormalizedValue;
		GetUnitConversionFunction()(&UnitValue, 1);
		return UnitValue;
	}

	/** Converts unit value to unitless, normalized value [0.0f, 1.0f]. */
	virtual float ConvertUnitToNormalized(float InUnitValue) const final
	{
		float NormalizedValue = InUnitValue;
		GetNormalizedConversionFunction()(&NormalizedValue, 1);
		return NormalizedValue;
	}

	/** Returns default unit value (works with and without editor loaded) */
	virtual float GetUnitDefault() const
	{
		return ConvertNormalizedToUnit(Settings.ValueNormalized);
	}

	virtual float GetUnitMin() const
	{
		return 0.0f;
	}

	virtual float GetUnitMax() const 
	{
		return 1.0f;
	}

#if WITH_EDITOR
	void RefreshNormalizedValue();
	void RefreshUnitValue();
#endif // WITH_EDITOR
};

// Linearly scaled value between unit minimum and maximum.
UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterScaled : public USoundModulationParameter
{
	GENERATED_BODY()

public:
	/** Unit minimum of modulator. Minimum is only enforced at modulation destination. */
	UPROPERTY(EditAnywhere, Category = General)
	float UnitMin = 0.0f;

	/** Unit maximum of modulator. Maximum is only enforced at modulation destination. */
	UPROPERTY(EditAnywhere, Category = General)
	float UnitMax = 1.0f;

	virtual bool RequiresUnitConversion() const override;
	virtual Audio::FModulationUnitConvertFunction GetUnitConversionFunction() const override;
	virtual Audio::FModulationNormalizedConversionFunction GetNormalizedConversionFunction() const override;
	virtual float GetUnitMin() const override;
	virtual float GetUnitMax() const override;
};

// Modulation Parameter that scales normalized, unitless value to logarithmic frequency unit space.
UCLASS(BlueprintType, MinimalAPI, abstract)
class USoundModulationParameterFrequencyBase : public USoundModulationParameter
{
	GENERATED_BODY()

public:
	virtual bool RequiresUnitConversion() const override;
	virtual Audio::FModulationUnitConvertFunction GetUnitConversionFunction() const override;
	virtual Audio::FModulationNormalizedConversionFunction GetNormalizedConversionFunction() const override;
};

// Modulation Parameter that scales normalized, unitless value to logarithmic frequency unit space with provided minimum and maximum.
UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterFrequency : public USoundModulationParameterFrequencyBase
{
	GENERATED_BODY()

public:
	/** Unit minimum of modulator. Minimum is only enforced at modulation destination. */
	UPROPERTY(EditAnywhere, Category = General)
	float UnitMin = MIN_FILTER_FREQUENCY;

	/** Unit maximum of modulator. Maximum is only enforced at modulation destination. */
	UPROPERTY(EditAnywhere, Category = General)
	float UnitMax = MAX_FILTER_FREQUENCY;

	virtual float GetUnitMin() const override
	{
		return UnitMin;
	}

	virtual float GetUnitMax() const override
	{
		return UnitMax;
	}
};

// Modulation Parameter that scales normalized, unitless value to logarithmic frequency unit space with standard filter min and max frequency set.
UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterFilterFrequency : public USoundModulationParameterFrequencyBase
{
	GENERATED_BODY()

public:
	virtual float GetUnitMin() const override
	{
		return MIN_FILTER_FREQUENCY;
	}

	virtual float GetUnitMax() const override
	{
		return MAX_FILTER_FREQUENCY;
	}
};

// Modulation Parameter that scales normalized, unitless value to logarithmic frequency unit space with standard filter min and max frequency set.
// Mixes by taking the minimum (i.e. aggressive) filter frequency of all active modulators.
UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterLPFFrequency : public USoundModulationParameterFilterFrequency
{
	GENERATED_BODY()

public:
	virtual Audio::FModulationMixFunction GetMixFunction() const override;
};

// Modulation Parameter that scales normalized, unitless value to logarithmic frequency unit space with standard filter min and max frequency set.
// Mixes by taking the maximum (i.e. aggressive) filter frequency of all active modulators.
UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterHPFFrequency : public USoundModulationParameterFilterFrequency
{
	GENERATED_UCLASS_BODY()

public:
	virtual Audio::FModulationMixFunction GetMixFunction() const override;
};

// Modulation Parameter that scales normalized, unitless value to bipolar range. Mixes multiplicatively.
UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterBipolar : public USoundModulationParameter
{
	GENERATED_UCLASS_BODY()

public:
	/** Unit range of modulator. Range is only enforced at modulation destination. */
	UPROPERTY(EditAnywhere, Category = General, meta = (ClampMin = 0.00000001))
	float UnitRange = 2.0f;

	virtual bool RequiresUnitConversion() const override;
	virtual Audio::FModulationMixFunction GetMixFunction() const override;
	virtual Audio::FModulationUnitConvertFunction GetUnitConversionFunction() const override;
	virtual Audio::FModulationNormalizedConversionFunction GetNormalizedConversionFunction() const override;
	virtual float GetUnitMax() const override;
	virtual float GetUnitMin() const override;
};

UCLASS(BlueprintType, MinimalAPI)
class USoundModulationParameterVolume : public USoundModulationParameter
{
	GENERATED_BODY()

public:
	/** Minimum volume of parameter. Only enforced at modulation destination. */
	UPROPERTY(EditAnywhere, Category = General, meta = (ClampMax = 0.0))
	float MinVolume = -100.0f;

	virtual bool RequiresUnitConversion() const override;
	virtual Audio::FModulationUnitConvertFunction GetUnitConversionFunction() const override;
	virtual Audio::FModulationNormalizedConversionFunction GetNormalizedConversionFunction() const override;
	virtual float GetUnitMin() const override;
	virtual float GetUnitMax() const override;
};
