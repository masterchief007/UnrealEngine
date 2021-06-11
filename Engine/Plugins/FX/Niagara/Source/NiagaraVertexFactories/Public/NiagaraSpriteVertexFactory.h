// Copyright Epic Games, Inc. All Rights Reserved.

/*=============================================================================
	ParticleVertexFactory.h: Particle vertex factory definitions.
=============================================================================*/

#pragma once

#include "CoreMinimal.h"
#include "RenderResource.h"
#include "UniformBuffer.h"
#include "NiagaraVertexFactory.h"
#include "NiagaraDataSet.h"
#include "SceneView.h"

class FMaterial;

/**
 * Uniform buffer for particle sprite vertex factories.
 */
BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT( FNiagaraSpriteUniformParameters, NIAGARAVERTEXFACTORIES_API)
	SHADER_PARAMETER( uint32, bLocalSpace)
	SHADER_PARAMETER_EX( FVector4, TangentSelector, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector4, NormalsSphereCenter, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector4, NormalsCylinderUnitDirection, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector4, SubImageSize, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector, CameraFacingBlend, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, RemoveHMDRoll, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER( FVector4, MacroUVParameters )
	SHADER_PARAMETER_EX( float, RotationScale, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, RotationBias, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, NormalsType, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( float, DeltaSeconds, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER_EX( FVector2D, PivotOffset, EShaderPrecisionModifier::Half )
	SHADER_PARAMETER(int, PositionDataOffset)
	SHADER_PARAMETER(int, VelocityDataOffset)
	SHADER_PARAMETER(int, RotationDataOffset)
	SHADER_PARAMETER(int, SizeDataOffset)
	SHADER_PARAMETER(int, SubimageDataOffset)
	SHADER_PARAMETER(int, ColorDataOffset)
	SHADER_PARAMETER(uint32, MaterialParamValidMask)
	SHADER_PARAMETER(int, MaterialParamDataOffset)
	SHADER_PARAMETER(int, MaterialParam1DataOffset)
	SHADER_PARAMETER(int, MaterialParam2DataOffset)
	SHADER_PARAMETER(int, MaterialParam3DataOffset)
	SHADER_PARAMETER(int, FacingDataOffset)
	SHADER_PARAMETER(int, AlignmentDataOffset)
	SHADER_PARAMETER(int, SubImageBlendMode)
	SHADER_PARAMETER(int, CameraOffsetDataOffset)
	SHADER_PARAMETER(int, UVScaleDataOffset)
	SHADER_PARAMETER(int, NormalizedAgeDataOffset)
	SHADER_PARAMETER(int, MaterialRandomDataOffset)
	SHADER_PARAMETER(FVector4, DefaultPos)
	SHADER_PARAMETER(FVector2D, DefaultSize)
	SHADER_PARAMETER(FVector2D, DefaultUVScale)
	SHADER_PARAMETER(FVector, DefaultVelocity)
	SHADER_PARAMETER(float, DefaultRotation)
	SHADER_PARAMETER(FVector4, DefaultColor)
	SHADER_PARAMETER(float, DefaultMatRandom)
	SHADER_PARAMETER(float, DefaultCamOffset)
	SHADER_PARAMETER(float, DefaultNormAge)
	SHADER_PARAMETER(float, DefaultSubImage)
	SHADER_PARAMETER(FVector4, DefaultFacing)
	SHADER_PARAMETER(FVector4, DefaultAlignment)
	SHADER_PARAMETER(FVector4, DefaultDynamicMaterialParameter0)
	SHADER_PARAMETER(FVector4, DefaultDynamicMaterialParameter1)
	SHADER_PARAMETER(FVector4, DefaultDynamicMaterialParameter2)
	SHADER_PARAMETER(FVector4, DefaultDynamicMaterialParameter3)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FNiagaraSpriteUniformParameters> FNiagaraSpriteUniformBufferRef;

BEGIN_GLOBAL_SHADER_PARAMETER_STRUCT(FNiagaraSpriteVFLooseParameters, NIAGARAVERTEXFACTORIES_API)
	SHADER_PARAMETER(uint32, NumCutoutVerticesPerFrame)
	SHADER_PARAMETER(uint32, NiagaraFloatDataStride)
	SHADER_PARAMETER(uint32, ParticleAlignmentMode)
	SHADER_PARAMETER(uint32, ParticleFacingMode)
	SHADER_PARAMETER(uint32, SortedIndicesOffset)
	SHADER_PARAMETER(uint32, IndirectArgsOffset)
	SHADER_PARAMETER_SRV(Buffer<float2>, CutoutGeometry)
	SHADER_PARAMETER_SRV(Buffer<float>, NiagaraParticleDataFloat)
	SHADER_PARAMETER_SRV(Buffer<float>, NiagaraParticleDataHalf)
	SHADER_PARAMETER_SRV(Buffer<int>, SortedIndices)
	SHADER_PARAMETER_SRV(Buffer<uint>, IndirectArgsBuffer)
END_GLOBAL_SHADER_PARAMETER_STRUCT()

typedef TUniformBufferRef<FNiagaraSpriteVFLooseParameters> FNiagaraSpriteVFLooseParametersRef;

/**
 * Vertex factory for rendering particle sprites.
 */
class NIAGARAVERTEXFACTORIES_API FNiagaraSpriteVertexFactory : public FNiagaraVertexFactoryBase
{
	DECLARE_VERTEX_FACTORY_TYPE(FNiagaraSpriteVertexFactory);

public:

	/** Default constructor. */
	FNiagaraSpriteVertexFactory(ENiagaraVertexFactoryType InType, ERHIFeatureLevel::Type InFeatureLevel )
		: FNiagaraVertexFactoryBase(InType, InFeatureLevel),
		LooseParameterUniformBuffer(nullptr),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		AlignmentMode(0),
		FacingMode(0),
		SortedIndicesOffset(0)
	{}

	FNiagaraSpriteVertexFactory()
		: FNiagaraVertexFactoryBase(NVFT_MAX, ERHIFeatureLevel::Num),
		LooseParameterUniformBuffer(nullptr),
		NumCutoutVerticesPerFrame(0),
		CutoutGeometrySRV(nullptr),
		AlignmentMode(0),
		FacingMode(0),
		SortedIndicesOffset(0)
		
	{}

	// FRenderResource interface.
	virtual void InitRHI() override;

	virtual bool RendersPrimitivesAsCameraFacingSprites() const override { return true; }

	/**
	 * Should we cache the material's shadertype on this platform with this vertex factory? 
	 */
	static bool ShouldCompilePermutation(const FVertexFactoryShaderPermutationParameters& Parameters);

	/**
	 * Can be overridden by FVertexFactory subclasses to modify their compile environment just before compilation occurs.
	 */
	static void ModifyCompilationEnvironment(const FVertexFactoryShaderPermutationParameters& Parameters, FShaderCompilerEnvironment& OutEnvironment);
	
	void SetTexCoordBuffer(const FVertexBuffer* InTexCoordBuffer);
	
	/**
	 * Set the uniform buffer for this vertex factory.
	 */
	FORCEINLINE void SetSpriteUniformBuffer( const FNiagaraSpriteUniformBufferRef& InSpriteUniformBuffer )
	{
		SpriteUniformBuffer = InSpriteUniformBuffer;
	}

	/**
	 * Retrieve the uniform buffer for this vertex factory.
	 */
	FORCEINLINE FRHIUniformBuffer* GetSpriteUniformBuffer()
	{
		return SpriteUniformBuffer;
	}

	void SetCutoutParameters(int32 InNumCutoutVerticesPerFrame, FRHIShaderResourceView* InCutoutGeometrySRV)
	{
		NumCutoutVerticesPerFrame = InNumCutoutVerticesPerFrame;
		CutoutGeometrySRV = InCutoutGeometrySRV;
	}

	inline int32 GetNumCutoutVerticesPerFrame() const { return NumCutoutVerticesPerFrame; }
	inline FRHIShaderResourceView* GetCutoutGeometrySRV() const { return CutoutGeometrySRV; }

	void SetSortedIndices(const FShaderResourceViewRHIRef& InSortedIndicesSRV, uint32 InSortedIndicesOffset)
	{
		SortedIndicesSRV = InSortedIndicesSRV;
		SortedIndicesOffset = InSortedIndicesOffset;
	}

	FORCEINLINE FRHIShaderResourceView* GetSortedIndicesSRV()
	{
		return SortedIndicesSRV;
	}

	FORCEINLINE int32 GetSortedIndicesOffset()
	{
		return SortedIndicesOffset;
	}

	void SetFacingMode(uint32 InMode)
	{
		FacingMode = InMode;
	}

	uint32 GetFacingMode()
	{
		return FacingMode;
	}

	void SetAlignmentMode(uint32 InMode)
	{
		AlignmentMode = InMode;
	}

	uint32 GetAlignmentMode()
	{
		return AlignmentMode;
	}

	void SetVertexBufferOverride(const FVertexBuffer* InVertexBufferOverride)
	{
		VertexBufferOverride = InVertexBufferOverride;
	}

	FUniformBufferRHIRef LooseParameterUniformBuffer;

protected:
	/** Initialize streams for this vertex factory. */
	void InitStreams();

private:

	const FVertexBuffer* VertexBufferOverride = nullptr;

	/** Uniform buffer with sprite parameters. */
	FUniformBufferRHIRef SpriteUniformBuffer;

	int32 NumCutoutVerticesPerFrame;
	FShaderResourceViewRHIRef CutoutGeometrySRV;
	uint32 AlignmentMode;
	uint32 FacingMode;

	FShaderResourceViewRHIRef SortedIndicesSRV;
	uint32 SortedIndicesOffset;
};
