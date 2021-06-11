// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#include "CoreMinimal.h"
#include "Styling/SlateStyle.h"

class FColorCorrectRegionsStyle
{
public:
	static void Initialize();
	static void Shutdown();
	static TSharedPtr<ISlateStyle> Get() { return CCRStyle; }

private:
	static TSharedPtr<FSlateStyleSet> CCRStyle;
};