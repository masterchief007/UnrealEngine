// Copyright Epic Games, Inc. All Rights Reserved.

#include "AnimationBoneTrail.h"
#include "TrailHierarchy.h"
#include "SequencerTrailHierarchy.h"

#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "Sequencer/ControlRigLayerInstance.h"
#include "MovieSceneToolHelpers.h"

#include "ISequencer.h"
#include "MovieSceneSequence.h"
#include "Exporters/AnimSeqExportOption.h"

namespace UE 
{
namespace MotionTrailEditor	
{

float GetIntervalPerKey(int32 NumFrames, float SequenceLength)
{
	return (NumFrames > 1) ? (SequenceLength / (NumFrames - 1)) : MINIMUM_ANIMATION_LENGTH;
}

void FAnimTrajectoryCache::Evaluate(FTrajectoryCache* ParentTransformCache)
{
	TSharedPtr<ISequencer> Sequencer = WeakSequencer.Pin();
	check(Sequencer);

	if (!SkeletalMeshComponent.IsValid() || !SkeletalMeshComponent->SkeletalMesh || !SkeletalMeshComponent->SkeletalMesh->Skeleton)
	{
		return;
	}

	SkeletalMeshComponent->MasterPoseComponent = nullptr;

	CachedAnimSequence->SetSkeleton(SkeletalMeshComponent->SkeletalMesh->Skeleton);

	// TODO: for some reason SkeletalMeshComponent becomes invalid sometimes when evaluating, no clue why yet
	FMovieSceneSequenceTransform MovieSceneSequenceTransform;
	UAnimSeqExportOption* AnimSeqExportOption = NewObject<UAnimSeqExportOption>(GetTransientPackage(), NAME_None);
	MovieSceneToolHelpers::ExportToAnimSequence(CachedAnimSequence, AnimSeqExportOption,Sequencer->GetFocusedMovieSceneSequence()->GetMovieScene(), Sequencer.Get(), SkeletalMeshComponent.Get(),
		Sequencer->GetFocusedTemplateID(), MovieSceneSequenceTransform);
	AnimSeqExportOption->MarkPendingKill();
	Sequencer->ForceEvaluate();

	GetSpaceBasedAnimationData(GlobalBoneTransforms);
	ComponentBoneTransforms.SetNum(GlobalBoneTransforms.Num());

	Spacing = 1.0 / CachedAnimSequence->GetFrameRate();
	const double AnimLength = CachedAnimSequence->GetPlayLength();
	AnimRange = TRange<double>(0.0, AnimLength);
	for (int32 TrackIndex = 0; TrackIndex < GlobalBoneTransforms.Num(); TrackIndex++)
	{
		ComponentBoneTransforms[TrackIndex].SetNum(GlobalBoneTransforms[TrackIndex].Num());
		double TimeSeconds = 0.0;
		for (int32 TimeIndex = 0; TimeIndex < GlobalBoneTransforms[TrackIndex].Num(); TimeIndex++, TimeSeconds += Spacing)
		{
			ComponentBoneTransforms[TrackIndex][TimeIndex] = GlobalBoneTransforms[TrackIndex][TimeIndex].GetRelativeTransform(ParentTransformCache->GetInterp(TimeSeconds));
			//ComponentBoneTransforms[TrackIndex][TimeIndex] = GlobalBoneTransforms[TrackIndex][TimeIndex].GetRelativeTransformReverse(ParentTransformCache->GetInterp(TimeSeconds));
		}
	}

	bDirty = false;
}

void FAnimTrajectoryCache::UpdateRange(const TRange<double>& EvalRange, FTrajectoryCache* ParentTransformCache, const int32 BoneIdx)
{
	const int32 StartIdx = (EvalRange.GetLowerBoundValue() - AnimRange.GetLowerBoundValue()) / Spacing;
	const int32 EndIdx = (EvalRange.GetUpperBoundValue() - AnimRange.GetLowerBoundValue()) / Spacing;

	double TimeSeconds = EvalRange.GetLowerBoundValue();
	for (int32 TimeIndex = StartIdx; TimeIndex < EndIdx; TimeIndex++, TimeSeconds += Spacing)
	{
		GlobalBoneTransforms[BoneIdx][TimeIndex] = ComponentBoneTransforms[BoneIdx][TimeIndex] * ParentTransformCache->GetInterp(TimeSeconds);
	}
}

void FAnimTrajectoryCache::AddReferencedObjects(FReferenceCollector& Collector)
{
	Collector.AddReferencedObject(CachedAnimSequence);
}

void FAnimTrajectoryCache::GetSpaceBasedAnimationData(TArray<TArray<FTransform>>& OutAnimationDataInComponentSpace)
{
	USkeleton* MySkeleton = CachedAnimSequence->GetSkeleton();

	check(MySkeleton);
	const FReferenceSkeleton& RefSkeleton = MySkeleton->GetReferenceSkeleton();

	int32 NumBones = RefSkeleton.GetNum();

	OutAnimationDataInComponentSpace.Empty(NumBones);
	OutAnimationDataInComponentSpace.AddZeroed(NumBones);

	// 2d array of animated time [boneindex][time key]
	int32 NumKeys = CachedAnimSequence->GetNumberOfFrames();
	float Interval = UE::MotionTrailEditor::GetIntervalPerKey(NumKeys, CachedAnimSequence->SequenceLength);

	// allocate arrays
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		OutAnimationDataInComponentSpace[BoneIndex].AddUninitialized(NumKeys);
	}

	// If skeleton to track map isn't initialized yet, initialize it
	if (SkelToTrackIdx.Num() == 0)
	{
		SkelToTrackIdx.SetNumUninitialized(NumBones);
		for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
		{
			FName BoneName = MySkeleton->GetReferenceSkeleton().GetBoneName(BoneIndex);
			int32 TrackIndex = CachedAnimSequence->GetAnimationTrackNames().Find(BoneName);
			SkelToTrackIdx[BoneIndex] = TrackIndex;
		}
	}

	// Set component transform for each bone
	for (int32 BoneIndex = 0; BoneIndex < NumBones; ++BoneIndex)
	{
		int32 TrackIndex = SkelToTrackIdx[BoneIndex];
		int32 ParentBoneIndex = MySkeleton->GetReferenceSkeleton().GetParentIndex(BoneIndex);

		if (TrackIndex != INDEX_NONE)
		{
			auto& RawAnimation = CachedAnimSequence->GetRawAnimationData()[TrackIndex];
			// fill up keys - calculate PK1 * K1
			for (int32 Key = 0; Key < NumKeys; ++Key)
			{
				FTransform AnimatedLocalKey;
				CachedAnimSequence->ExtractBoneTransform(CachedAnimSequence->GetRawAnimationData(), AnimatedLocalKey, TrackIndex, Interval*Key);

				if (ParentBoneIndex != INDEX_NONE)
				{
					OutAnimationDataInComponentSpace[BoneIndex][Key] = AnimatedLocalKey * OutAnimationDataInComponentSpace[ParentBoneIndex][Key];
				}
				else
				{
					OutAnimationDataInComponentSpace[BoneIndex][Key] = AnimatedLocalKey;
				}
			}
		}
		else
		{
			// get local spaces from refpose and use that to fill it up
			FTransform LocalTransform = MySkeleton->GetReferenceSkeleton().GetRefBonePose()[BoneIndex];

			for (int32 Key = 0; Key < NumKeys; ++Key)
			{
				if (ParentBoneIndex != INDEX_NONE)
				{
					OutAnimationDataInComponentSpace[BoneIndex][Key] = LocalTransform * OutAnimationDataInComponentSpace[ParentBoneIndex][Key];
				}
				else
				{
					OutAnimationDataInComponentSpace[BoneIndex][Key] = LocalTransform;
				}
			}
		}
	}
}

FTransform FAnimBoneTrajectoryCache::GetInterp(const double InTime) const
{
	if (AnimTrajectoryCache->GlobalBoneTransforms.Num() == 0)
	{
		return Default;
	}

	const double T = (InTime / AnimTrajectoryCache->Spacing) - FMath::FloorToDouble(InTime / AnimTrajectoryCache->Spacing);
	const int32 LowIdx = FMath::Clamp(int32((InTime - AnimTrajectoryCache->AnimRange.GetLowerBoundValue()) / AnimTrajectoryCache->Spacing), 0, AnimTrajectoryCache->GlobalBoneTransforms[BoneIdx].Num() - 1);
	const int32 HighIdx = FMath::Clamp(LowIdx + 1, 0, AnimTrajectoryCache->GlobalBoneTransforms[BoneIdx].Num() - 1);

	FTransform TempBlended;
	TempBlended.Blend(AnimTrajectoryCache->GlobalBoneTransforms[BoneIdx][LowIdx], AnimTrajectoryCache->GlobalBoneTransforms[BoneIdx][HighIdx], T);
	return TempBlended;
}

TArray<double> FAnimBoneTrajectoryCache::GetAllTimesInRange(const TRange<double>& InRange) const
{
	if (AnimTrajectoryCache->Spacing == 0.0)
	{
		return TArray<double>();
	}

	TRange<double> GenRange = TRange<double>::Intersection({ AnimTrajectoryCache->AnimRange, InRange });

	TArray<double> AllTimesInRange;
	AllTimesInRange.Reserve(int(GenRange.Size<double>() / AnimTrajectoryCache->Spacing) + 1);
	const double FirstTick = FMath::FloorToDouble((GenRange.GetLowerBoundValue()) / AnimTrajectoryCache->Spacing) * AnimTrajectoryCache->Spacing;
	for (double TickItr = FirstTick; TickItr < GenRange.GetUpperBoundValue(); TickItr += AnimTrajectoryCache->Spacing)
	{
		AllTimesInRange.Add(TickItr);
	}

	return AllTimesInRange;
}

ETrailCacheState FAnimationBoneTrail::UpdateTrail(const FSceneContext& InSceneContext)
{
	if (TrajectoryCache->GetAnimCache()->IsDirty())
	{
		return ETrailCacheState::NotUpdated;
	}

	checkf(InSceneContext.TrailHierarchy->GetHierarchy()[InSceneContext.YourNode].Parents.Num() == 1, TEXT("AnimationBoneTrails only support one parent"));
	const FGuid ParentGuid = InSceneContext.TrailHierarchy->GetHierarchy()[InSceneContext.YourNode].Parents[0];
	const TUniquePtr<FTrail>& Parent = InSceneContext.TrailHierarchy->GetAllTrails()[ParentGuid];

	USkeletalMeshComponent* SkeletalMeshComponent = TrajectoryCache->GetAnimCache()->GetSkeletalMeshComponent();
	FSequencerTrailHierarchy* SequencerTrailHierarchy = static_cast<FSequencerTrailHierarchy*>(InSceneContext.TrailHierarchy);
	const FGuid SkelMeshCompGuid = SequencerTrailHierarchy->GetObjectsTracked().FindChecked(SkeletalMeshComponent);
	FTrajectoryCache* SkelMeshCompTrajectoryCache = SequencerTrailHierarchy->GetAllTrails().FindChecked(SkelMeshCompGuid)->GetTrajectoryTransforms();

	ETrailCacheState ParentCacheState = InSceneContext.ParentCacheStates[ParentGuid];

	if (ParentCacheState == ETrailCacheState::Dead || !TrajectoryCache->IsValid())
	{
		return ETrailCacheState::Dead;
	}

	const bool bParentChanged = ParentCacheState != ETrailCacheState::UpToDate;
	if (bParentChanged || bForceEvaluateNextTick)
	{
		CachedEffectiveRange = TRange<double>::Hull({ Parent->GetEffectiveRange(), TrajectoryCache->GetAnimCache()->GetRange() });

		if (bParentChanged)
		{
			const FDateTime StartTime = FDateTime::Now();
			TrajectoryCache->GetAnimCache()->UpdateRange(InSceneContext.EvalTimes.Range, SkelMeshCompTrajectoryCache, TrajectoryCache->GetBoneIndex());
			const FTimespan Timespan = FDateTime::Now() - StartTime;
			InSceneContext.TrailHierarchy->GetTimingStats().Add("FAnimTrajectoryCache::Update", Timespan);
		}

		bForceEvaluateNextTick = false;
		return ETrailCacheState::Stale;
	}
	else
	{
		return ETrailCacheState::UpToDate;
	}
}

} // namespace MovieScene
} // namespace UE
