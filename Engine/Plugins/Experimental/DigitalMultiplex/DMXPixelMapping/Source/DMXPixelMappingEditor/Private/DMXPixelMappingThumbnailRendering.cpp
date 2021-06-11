// Copyright Epic Games, Inc. All Rights Reserved.

#include "DMXPixelMappingThumbnailRendering.h"
#include "CanvasTypes.h"
#include "DMXPixelMapping.h"

bool UDMXPixelMappingThumbnailRendering::CanVisualizeAsset(UObject* Object)
{
	return GetThumbnailTextureFromObject(Object) != nullptr;
}

void UDMXPixelMappingThumbnailRendering::GetThumbnailSize(UObject* Object, float Zoom, uint32& OutWidth, uint32& OutHeight) const
{
	UTexture* ObjectTexture = GetThumbnailTextureFromObject(Object);
	if (ObjectTexture != nullptr && ObjectTexture->Resource != nullptr)
	{
		OutWidth = Zoom * ObjectTexture->Resource->GetSizeX();
		OutHeight = Zoom * ObjectTexture->Resource->GetSizeY();
	}
	else
	{
		OutWidth = 0;
		OutHeight = 0;
	}
}

void UDMXPixelMappingThumbnailRendering::Draw(UObject* Object, int32 X, int32 Y, uint32 Width, uint32 Height, FRenderTarget*, FCanvas* Canvas, bool bAdditionalViewFamily)
{
	UTexture* ObjectTexture = GetThumbnailTextureFromObject(Object);
	if (ObjectTexture != nullptr)
	{
		Canvas->DrawTile(X, Y, Width, Height, 0.0f, 0.0f, 1.0f, 1.0f, FLinearColor::White, ObjectTexture->Resource, false);
	}
}

UTexture* UDMXPixelMappingThumbnailRendering::GetThumbnailTextureFromObject(UObject* Object) const
{
	UDMXPixelMapping* DMXPixelMapping = Cast<UDMXPixelMapping>(Object);
	if (DMXPixelMapping != nullptr)
	{
		return DMXPixelMapping->ThumbnailImage;
	}
	return nullptr;
}
