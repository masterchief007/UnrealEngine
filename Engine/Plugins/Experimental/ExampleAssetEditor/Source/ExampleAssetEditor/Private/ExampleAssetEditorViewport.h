// Copyright Epic Games, Inc.All Rights Reserved.

#pragma once

#include "SAssetEditorViewport.h"

class UInputRouter;
class FSlateViewportInterfaceWrapper;
class FEditorViewportClient;

// Temporary Viewport Client class for a mode manager and sending through the tools context to the SAssetEditorViewport (SEditorViewport) - will probably want to subclass the SAssetEditorViewport here to pull in the input router?
class SExampleAssetEditorViewport
	: public SAssetEditorViewport
{
public:
	SLATE_BEGIN_ARGS(SExampleAssetEditorViewport) {}
		SLATE_ARGUMENT(TSharedPtr<FEditorViewportClient>, EditorViewportClient)
		SLATE_ARGUMENT(UInputRouter*, InputRouter)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

protected:
	UInputRouter* InputRouter;
	TSharedPtr<FSlateViewportInterfaceWrapper> SlateInputWrapper;
};
