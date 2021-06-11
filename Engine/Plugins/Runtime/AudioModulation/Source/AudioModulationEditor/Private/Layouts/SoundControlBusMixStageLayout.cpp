// Copyright Epic Games, Inc. All Rights Reserved.
#include "SoundControlBusMixStageLayout.h"

#include "Audio.h"
#include "AudioModulationStyle.h"
#include "DetailCategoryBuilder.h"
#include "DetailLayoutBuilder.h"
#include "DetailWidgetRow.h"
#include "EditorStyleSet.h"
#include "IAudioModulation.h"
#include "IDetailChildrenBuilder.h"
#include "IDetailPropertyRow.h"
#include "InputCoreTypes.h"
#include "Misc/Attribute.h"
#include "PropertyRestriction.h"
#include "SCurveEditor.h"
#include "ScopedTransaction.h"
#include "Sound/SoundModulationDestination.h"
#include "SoundControlBus.h"
#include "SoundControlBusMix.h"
#include "SoundModulationParameterSettingsLayout.h"
#include "UObject/ReflectedTypeAccessors.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Layout/SSeparator.h"
#include "Widgets/Layout/SWidgetSwitcher.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Text/SRichTextBlock.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Views/SListView.h"


#define LOCTEXT_NAMESPACE "AudioModulation"


namespace AudioModulationEditorUtils
{
	USoundModulationParameter* GetParameterFromBus(TSharedRef<IPropertyHandle> BusHandle)
	{
		UObject* Object = nullptr;
		if (BusHandle->GetValue(Object) == FPropertyAccess::Success)
		{
			if (USoundControlBus* ControlBus = Cast<USoundControlBus>(Object))
			{
				return ControlBus->Parameter;
			}
		}

		return nullptr;
	}

	void HandleConvertNormalizedToUnit(TSharedRef<IPropertyHandle> BusHandle, TSharedRef<IPropertyHandle> NormalizedValueHandle, TSharedRef<IPropertyHandle> UnitValueHandle)
	{
		if (USoundModulationParameter* Parameter = AudioModulationEditorUtils::GetParameterFromBus(BusHandle))
		{
			if (Parameter->RequiresUnitConversion())
			{
				float NormalizedValue = 1.0f;
				if (NormalizedValueHandle->GetValue(NormalizedValue) == FPropertyAccess::Success)
				{
					const float UnitValue = Parameter->ConvertNormalizedToUnit(NormalizedValue);
					UnitValueHandle->SetValue(UnitValue, EPropertyValueSetFlags::NotTransactable);
				}
			}
		}
	}

	void HandleConvertUnitToNormalized(TSharedRef<IPropertyHandle> BusHandle, TSharedRef<IPropertyHandle> UnitValueHandle, TSharedRef<IPropertyHandle> NormalizedValueHandle)
	{
		if (USoundModulationParameter* Parameter = AudioModulationEditorUtils::GetParameterFromBus(BusHandle))
		{
			if (Parameter->RequiresUnitConversion())
			{
				float UnitValue = 1.0f;
				if (UnitValueHandle->GetValue(UnitValue) == FPropertyAccess::Success)
				{
					const float NormalizedValue = Parameter->ConvertUnitToNormalized(UnitValue);
					NormalizedValueHandle->SetValue(NormalizedValue);
				}
			}
		}
	}
}

void FSoundControlBusMixStageLayoutCustomization::CustomizeHeader(TSharedRef<IPropertyHandle> StructPropertyHandle, FDetailWidgetRow& HeaderRow, IPropertyTypeCustomizationUtils& CustomizationUtils)
{
	HeaderRow.NameContent()
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				StructPropertyHandle->CreatePropertyNameWidget()
			]
		]
		.ValueContent()
		.MinDesiredWidth(300.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			.Padding(2.0f, 0.0f, 0.0f, 0.0f)
			.VAlign(VAlign_Center)
			[
				SNew(STextBlock)
				.Font(IDetailLayoutBuilder::GetDetailFont())
				.Text(FText::Format(LOCTEXT("BusMixStageHeader_Format", "Bus Stage {0}"), StructPropertyHandle->GetPropertyDisplayName()))
				.ToolTipText(StructPropertyHandle->GetToolTipText())
			]
		];
}

void FSoundControlBusMixStageLayoutCustomization::CustomizeChildren(TSharedRef<IPropertyHandle> StructPropertyHandle, IDetailChildrenBuilder& ChildBuilder, IPropertyTypeCustomizationUtils& StructCustomizationUtils)
{
	uint32 NumChildren;
	StructPropertyHandle->GetNumChildren(NumChildren);

	TMap<FName, TSharedPtr<IPropertyHandle>> PropertyHandles;

	for (uint32 ChildIndex = 0; ChildIndex < NumChildren; ++ChildIndex)
	{
		TSharedRef<IPropertyHandle> ChildHandle = StructPropertyHandle->GetChildHandle(ChildIndex).ToSharedRef();
		const FName PropertyName = ChildHandle->GetProperty()->GetFName();
		PropertyHandles.Add(PropertyName, ChildHandle);
	}

	TSharedRef<IPropertyHandle> BusHandle = PropertyHandles.FindChecked(GET_MEMBER_NAME_CHECKED(FSoundControlBusMixStage, Bus)).ToSharedRef();

	TAttribute<EVisibility> BusInfoVisibility = TAttribute<EVisibility>::Create([BusHandle]()
	{
		UObject* Bus = nullptr;
		BusHandle->GetValue(Bus);
		return Bus && Bus->IsA<USoundControlBus>() ? EVisibility::Visible : EVisibility::Hidden;
	});

	ChildBuilder.AddProperty(BusHandle);

	TSharedRef<IPropertyHandle> NormalizedValueHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSoundModulationMixValue, TargetValue)).ToSharedRef();
	TSharedRef<IPropertyHandle> UnitValueHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSoundModulationMixValue, TargetUnitValue)).ToSharedRef();

	// When editor opens, set unit value in case bus unit has changed while editor was closed.
	AudioModulationEditorUtils::HandleConvertNormalizedToUnit(BusHandle, NormalizedValueHandle, UnitValueHandle);

	NormalizedValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([BusHandle, NormalizedValueHandle, UnitValueHandle]()
	{
		AudioModulationEditorUtils::HandleConvertNormalizedToUnit(BusHandle, NormalizedValueHandle, UnitValueHandle);
	}));

	UnitValueHandle->SetOnPropertyValueChanged(FSimpleDelegate::CreateLambda([BusHandle, UnitValueHandle, NormalizedValueHandle]()
	{
		AudioModulationEditorUtils::HandleConvertUnitToNormalized(BusHandle, UnitValueHandle, NormalizedValueHandle);
		AudioModulationEditorUtils::HandleConvertNormalizedToUnit(BusHandle, NormalizedValueHandle, UnitValueHandle);
	}));

	ChildBuilder.AddCustomRow(StructPropertyHandle->GetPropertyDisplayName())
	.NameContent()
	[
		SNew(STextBlock)
		.Font(IDetailLayoutBuilder::GetDetailFont())
		.Text(LOCTEXT("BusModulationValue_MixValueName", "Value"))
		.ToolTipText(StructPropertyHandle->GetToolTipText())
	]
	.ValueContent()
		.MinDesiredWidth(300.0f)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				.Padding(2.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					UnitValueHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(TAttribute<FText>::Create([BusHandle]()
					{
						USoundModulationParameter* Parameter = AudioModulationEditorUtils::GetParameterFromBus(BusHandle);
						if (Parameter && Parameter->RequiresUnitConversion())
						{
							return Parameter->Settings.UnitDisplayName;
						}

						return FText();
					}))
				]
			+ SHorizontalBox::Slot()
				.FillWidth(0.4f)
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					NormalizedValueHandle->CreatePropertyValueWidget()
				]
			+ SHorizontalBox::Slot()
				.Padding(4.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				.AutoWidth()
				[
					SNew(STextBlock)
					.Font(IDetailLayoutBuilder::GetDetailFont())
					.Text(LOCTEXT("SoundModulationControl_UnitValueNormalized", "Normalized"))
				]
	]
	.Visibility(TAttribute<EVisibility>::Create([BusHandle]()
	{
		if (USoundModulationParameter* Parameter = AudioModulationEditorUtils::GetParameterFromBus(BusHandle))
		{
			return Parameter->RequiresUnitConversion() ? EVisibility::Visible : EVisibility::Hidden;
		}

		return EVisibility::Hidden;
	}));

	ChildBuilder.AddProperty(NormalizedValueHandle)
	.Visibility(TAttribute<EVisibility>::Create([BusHandle]()
	{
		if (USoundModulationParameter* Parameter = AudioModulationEditorUtils::GetParameterFromBus(BusHandle))
		{
			return Parameter->RequiresUnitConversion() ? EVisibility::Hidden : EVisibility::Visible;
		}

		return EVisibility::Visible;
	}));

	TSharedRef<IPropertyHandle> AttackTimeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSoundModulationMixValue, AttackTime)).ToSharedRef();
	ChildBuilder.AddProperty(AttackTimeHandle);

	TSharedRef<IPropertyHandle> ReleaseTimeHandle = StructPropertyHandle->GetChildHandle(GET_MEMBER_NAME_CHECKED(FSoundModulationMixValue, ReleaseTime)).ToSharedRef();
	ChildBuilder.AddProperty(ReleaseTimeHandle);
}
#undef LOCTEXT_NAMESPACE // SoundModulationFloat
