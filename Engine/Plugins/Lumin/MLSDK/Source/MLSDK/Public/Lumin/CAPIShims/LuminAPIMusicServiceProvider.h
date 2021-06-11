// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once

#if !defined(WITH_MLSDK) || WITH_MLSDK

#include "Lumin/CAPIShims/LuminAPI.h"

LUMIN_THIRD_PARTY_INCLUDES_START
#include <ml_music_service_common.h>
#include <ml_music_service_provider.h>
LUMIN_THIRD_PARTY_INCLUDES_END

namespace LUMIN_MLSDK_API
{

CREATE_DEPRECATED_MSG_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderCreate, "Replaced by MLMusicServiceProviderCreateEx.")
#define MLMusicServiceProviderCreate ::LUMIN_MLSDK_API::MLMusicServiceProviderCreateShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderCreateEx)
#define MLMusicServiceProviderCreateEx ::LUMIN_MLSDK_API::MLMusicServiceProviderCreateExShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderStart)
#define MLMusicServiceProviderStart ::LUMIN_MLSDK_API::MLMusicServiceProviderStartShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderSetAudioOutput)
#define MLMusicServiceProviderSetAudioOutput ::LUMIN_MLSDK_API::MLMusicServiceProviderSetAudioOutputShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderWriteAudioOutput)
#define MLMusicServiceProviderWriteAudioOutput ::LUMIN_MLSDK_API::MLMusicServiceProviderWriteAudioOutputShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderFlushAudioOutput)
#define MLMusicServiceProviderFlushAudioOutput ::LUMIN_MLSDK_API::MLMusicServiceProviderFlushAudioOutputShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderReleaseAudioOutput)
#define MLMusicServiceProviderReleaseAudioOutput ::LUMIN_MLSDK_API::MLMusicServiceProviderReleaseAudioOutputShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderSetVolume)
#define MLMusicServiceProviderSetVolume ::LUMIN_MLSDK_API::MLMusicServiceProviderSetVolumeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyPositionChange)
#define MLMusicServiceProviderNotifyPositionChange ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyPositionChangeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyPlaybackStateChange)
#define MLMusicServiceProviderNotifyPlaybackStateChange ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyPlaybackStateChangeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyRepeatStateChange)
#define MLMusicServiceProviderNotifyRepeatStateChange ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyRepeatStateChangeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyShuffleStateChange)
#define MLMusicServiceProviderNotifyShuffleStateChange ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyShuffleStateChangeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyStatus)
#define MLMusicServiceProviderNotifyStatus ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyStatusShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyError)
#define MLMusicServiceProviderNotifyError ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyErrorShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyMetadataChange)
#define MLMusicServiceProviderNotifyMetadataChange ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyMetadataChangeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderNotifyVolumeChange)
#define MLMusicServiceProviderNotifyVolumeChange ::LUMIN_MLSDK_API::MLMusicServiceProviderNotifyVolumeChangeShim
CREATE_FUNCTION_SHIM(ml_musicservice_provider, MLResult, MLMusicServiceProviderTerminate)
#define MLMusicServiceProviderTerminate ::LUMIN_MLSDK_API::MLMusicServiceProviderTerminateShim

}

#endif // !defined(WITH_MLSDK) || WITH_MLSDK
