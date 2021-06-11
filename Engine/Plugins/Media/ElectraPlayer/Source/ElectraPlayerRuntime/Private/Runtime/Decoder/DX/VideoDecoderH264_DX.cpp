// Copyright Epic Games, Inc. All Rights Reserved.

#ifdef ELECTRA_ENABLE_MFDECODER

#include "PlayerCore.h"
#include "ElectraPlayerPrivate.h"

#include "ElectraPlayerPrivate_Platform.h"

#include "StreamAccessUnitBuffer.h"
#include "Decoder/VideoDecoderH264.h"
#include "Renderer/RendererBase.h"
#include "Renderer/RendererVideo.h"
#include "Player/PlayerSessionServices.h"
#include "Utilities/StringHelpers.h"

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/WindowsHWrapper.h"
#include "HAL/LowLevelMemTracker.h"

THIRD_PARTY_INCLUDES_START
#include "mftransform.h"
#include "mfapi.h"
#include "mferror.h"
#include "mfidl.h"
THIRD_PARTY_INCLUDES_END

#include "Windows/HideWindowsPlatformTypes.h"

#include "DecoderErrors_DX.h"
#include "VideoDecoderH264_DX.h"

namespace
{

static const int32 MaxHWDecodeW = 1920;
static const int32 MaxHWDecodeH = 1088;

//DEFINE_GUID(MF_MT_DEFAULT_STRIDE,    0x644b4e48, 0x1e02, 0x4516, 0xb0, 0xeb, 0xc0, 0x1c, 0xa9, 0xd4, 0x9a, 0xc6);
//static const GUID AVDecVideoMaxCodedWidth   = { 0x5ae557b8, 0x77af, 0x41f5, { 0x9f, 0xa6, 0x4d, 0xb2, 0xfe, 0x1d, 0x4b, 0xca } };
//static const GUID AVDecVideoMaxCodedHeight  = { 0x7262a16a, 0xd2dc, 0x4e75, { 0x9b, 0xa8, 0x65, 0xc0, 0xc6, 0xd3, 0x2b, 0x13 } };

//#define MFSampleExtension_ClosedCaption_CEA708_MAX_SIZE	256
//DEFINE_GUID(MFSampleExtension_ClosedCaption_CEA708, 0x26f09068, 0xe744, 0x47dc, 0xaa,0x03,0xdb,0xf2,0x04,0x03,0xbd,0xe6);
//DEFINE_GUID(MF_USER_DATA_PAYLOAD,	0xd1d4985d, 0xdc92, 0x457a, 0xb3,0xa0,0x65,0x1a,0x33,0xa3,0x10,0x47);

// `MF_LOW_LATENCY` is defined in "mfapi.h" for >= WIN8
// UE4 supports lower Windows versions at the moment and so `WINVER` is < `_WIN32_WINNT_WIN8`
// to be able to use `MF_LOW_LATENCY` with default UE4 build we define it ourselves and check actual
// Windows version in runtime
#if (WINVER < _WIN32_WINNT_WIN8)
	const GUID MF_LOW_LATENCY = { 0x9c27891a, 0xed7a, 0x40e1,{ 0x88, 0xe8, 0xb2, 0x27, 0x27, 0xa0, 0x24, 0xee } };
#endif

}

namespace Electra {

IVideoDecoderH264::FSystemConfiguration			FVideoDecoderH264::SystemConfig;

#if ELECTRA_ENABLE_SWDECODE
bool FVideoDecoderH264::bDidCheckHWSupport = false;
bool FVideoDecoderH264::bIsHWSupported = false;
#endif

/*********************************************************************************************************************/
/*********************************************************************************************************************/
/*********************************************************************************************************************/

bool IVideoDecoderH264::Startup(const IVideoDecoderH264::FSystemConfiguration& InConfig)
{
	return FVideoDecoderH264::Startup(InConfig);
}

void IVideoDecoderH264::Shutdown()
{
	FVideoDecoderH264::Shutdown();
}

bool IVideoDecoderH264::GetStreamDecodeCapability(FStreamDecodeCapability& OutResult, const FStreamDecodeCapability& InStreamParameter)
{
	return FVideoDecoderH264::GetStreamDecodeCapability(OutResult, InStreamParameter);
}

IVideoDecoderH264::FSystemConfiguration::FSystemConfiguration()
{
	ThreadConfig.Decoder.Priority 	= TPri_Normal;
	ThreadConfig.Decoder.StackSize	= 256 << 10;	// how much do we need?
	ThreadConfig.Decoder.CoreAffinity = -1;
	// Not needed, set to useful values anyway
	ThreadConfig.PassOn.Priority  	= TPri_Normal;
	ThreadConfig.PassOn.StackSize 	= 65536;
	ThreadConfig.PassOn.CoreAffinity  = -1;
}

IVideoDecoderH264::FInstanceConfiguration::FInstanceConfiguration()
	: MaxDecodedFrames(8)														// FIXME: how many do we need exactly?
	, ThreadConfig(FVideoDecoderH264::SystemConfig.ThreadConfig)
{
}

/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/
/***************************************************************************************************************************************************/


//-----------------------------------------------------------------------------
/**
 * Decoder system startup
 *
 * @param config
 *
 * @return
 */
bool FVideoDecoderH264::Startup(const IVideoDecoderH264::FSystemConfiguration& config)
{
	SystemConfig = config;
#ifdef ELECTRA_ENABLE_SWDECODE
	bDidCheckHWSupport = false;
	bIsHWSupported = false;
#endif
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Decoder system shutdown.
 */
void FVideoDecoderH264::Shutdown()
{
}


//-----------------------------------------------------------------------------
/**
 * Constructor
 */
FVideoDecoderH264::FVideoDecoderH264()
	: FMediaThread("Electra::H264 decoder")
	, bThreadStarted(false)
	, PlayerSessionServices(nullptr)
	, Renderer(nullptr)
	, CurrentAccessUnit(nullptr)
	, InputBufferListener(nullptr)
	, ReadyBufferListener(nullptr)
	, bIsHardwareAccelerated(true)
	, bRequiresReconfigurationForSW(false)
	, NumFramesInDecoder(0)
	, bDecoderFlushPending(false)
	, bError(false)
	, CurrentRenderOutputBuffer(nullptr)
	, bHaveDecoder(false)
	, MaxDecodeBufferSize(0)
{
}


//-----------------------------------------------------------------------------
/**
 * Destructor
 */
FVideoDecoderH264::~FVideoDecoderH264()
{
	Close();
}


//-----------------------------------------------------------------------------
/**
 * Sets an AU input buffer listener.
 *
 * @param InListener
 */
void FVideoDecoderH264::SetAUInputBufferListener(IAccessUnitBufferListener* InListener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
	InputBufferListener = InListener;
}


//-----------------------------------------------------------------------------
/**
 * Sets a buffer-ready listener.
 *
 * @param InListener
 */
void FVideoDecoderH264::SetReadyBufferListener(IDecoderOutputBufferListener* InListener)
{
	FMediaCriticalSection::ScopedLock lock(ListenerMutex);
	ReadyBufferListener = InListener;
}


//-----------------------------------------------------------------------------
/**
 * Sets the owning player's session service interface.
 *
 * @param InSessionServices
 *
 * @return
 */
void FVideoDecoderH264::SetPlayerSessionServices(IPlayerSessionServices* SessionServices)
{
	PlayerSessionServices = SessionServices;
}


//-----------------------------------------------------------------------------
/**
 * Opens a decoder instance
 *
 * @param InConfig
 *
 * @return
 */
void FVideoDecoderH264::Open(const IVideoDecoderH264::FInstanceConfiguration& InConfig)
{
	Config = InConfig;

	MaxDecodeDim = FIntPoint(Config.MaxFrameWidth, Config.MaxFrameHeight);

	// Set a large enough size to hold a single access unit. Since we will be asking for a new AU on
	// demand there is no need to overly restrict ourselves here. But it must be large enough to
	// hold at least the largest expected access unit.
	AccessUnits.CapacitySet(FAccessUnitBuffer::FConfiguration(16 << 20, 60.0));

	StartThread();
}


//-----------------------------------------------------------------------------
/**
 * Closes the decoder instance.
 */
void FVideoDecoderH264::Close()
{
	StopThread();
}


//-----------------------------------------------------------------------------
/**
 * Sets a new decoder limit.
 * As soon as a new sequence starts matching this limit the decoder will be
 * destroyed and recreated to conserve memory.
 * The assumption is that the data being streamed will not exceed this limit,
 * but if it does, any access unit requiring a better decoder will take
 * precendence and force a decoder capable of decoding it!
 *
 * @param MaxWidth
 * @param MaxHeight
 * @param MaxProfile
 * @param MaxProfileLevel
 * @param AdditionalOptions
 */
void FVideoDecoderH264::SetMaximumDecodeCapability(int32 MaxWidth, int32 MaxHeight, int32 MaxProfile, int32 MaxProfileLevel, const FParamDict& AdditionalOptions)
{
	// Recall max resolution so we can allocate properly sized sample buffers
	MaxDecodeDim = FIntPoint(MaxWidth, MaxHeight);
}


//-----------------------------------------------------------------------------
/**
 * Sets a new renderer.
 *
 * @param InRenderer
 */
void FVideoDecoderH264::SetRenderer(TSharedPtr<IMediaRenderer, ESPMode::ThreadSafe> InRenderer)
{
	Renderer = InRenderer;
}


void FVideoDecoderH264::SetResourceDelegate(const TSharedPtr<IVideoDecoderResourceDelegate, ESPMode::ThreadSafe>& InResourceDelegate)
{
	ResourceDelegate = InResourceDelegate;
}

//-----------------------------------------------------------------------------
/**
 * Creates and runs the decoder thread.
 */
void FVideoDecoderH264::StartThread()
{
	ThreadSetPriority(Config.ThreadConfig.Decoder.Priority);
	ThreadSetCoreAffinity(Config.ThreadConfig.Decoder.CoreAffinity);
	ThreadSetStackSize(Config.ThreadConfig.Decoder.StackSize);
	ThreadStart(Electra::MakeDelegate(this, &FVideoDecoderH264::WorkerThread));
	bThreadStarted = true;
}


//-----------------------------------------------------------------------------
/**
 * Stops the decoder thread.
 */
void FVideoDecoderH264::StopThread()
{
	if (bThreadStarted)
	{
		TerminateThreadSignal.Signal();
		ThreadWaitDone();
		bThreadStarted = false;
	}
}


//-----------------------------------------------------------------------------
/**
 * Posts an error to the session service error listeners.
 *
 * @param ApiReturnValue
 * @param Message
 * @param Code
 * @param Error
 */
void FVideoDecoderH264::PostError(int32 ApiReturnValue, const FString& Message, uint16 Code, UEMediaError Error)
{
	bError = true;
	check(PlayerSessionServices);
	if (PlayerSessionServices)
	{
		FErrorDetail err;
		err.SetError(Error != UEMEDIA_ERROR_OK ? Error : UEMEDIA_ERROR_DETAIL);
		err.SetFacility(Facility::EFacility::H264Decoder);
		err.SetCode(Code);
		err.SetMessage(Message);
		err.SetPlatformMessage(FString::Printf(TEXT("%d (0x%08x)"), ApiReturnValue, ApiReturnValue));
		PlayerSessionServices->PostError(err);
	}
}


//-----------------------------------------------------------------------------
/**
 * Sends a log message to the session service log.
 *
 * @param Level
 * @param Message
 */
void FVideoDecoderH264::LogMessage(IInfoLog::ELevel Level, const FString& Message)
{
	if (PlayerSessionServices)
	{
		PlayerSessionServices->PostLog(Facility::EFacility::H264Decoder, Level, Message);
	}
}


//-----------------------------------------------------------------------------
/**
 * Create a pool of decoded images for the decoder.
 *
 * @return
 */
bool FVideoDecoderH264::CreateDecodedImagePool()
{
	check(Renderer);
	FParamDict poolOpts;

	poolOpts.Set("num_buffers", FVariantValue((int64) Config.MaxDecodedFrames));
	if (!bIsHardwareAccelerated)
	{
		poolOpts.Set("sw_texture", FVariantValue(true));
	}
	UEMediaError Error = Renderer->CreateBufferPool(poolOpts);
	check(Error == UEMEDIA_ERROR_OK);

	MaxDecodeBufferSize = (int32) Renderer->GetBufferPoolProperties().GetValue("max_buffers").GetInt64();

	if (Error != UEMEDIA_ERROR_OK)
	{
		PostError(0, "Failed to create image pool", ERRCODE_INTERNAL_COULD_NOT_CREATE_IMAGE_POOL, Error);
	}

	return Error == UEMEDIA_ERROR_OK;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the pool of decoded images.
 */
void FVideoDecoderH264::DestroyDecodedImagePool()
{
	Renderer->ReleaseBufferPool();
}


//-----------------------------------------------------------------------------
/**
 * Called to receive a new input access unit for decoding.
 *
 * @param InAccessUnit
 */
IAccessUnitBufferInterface::EAUpushResult FVideoDecoderH264::AUdataPushAU(FAccessUnit* InAccessUnit)
{
	InAccessUnit->AddRef();
	bool bOk = AccessUnits.Push(InAccessUnit);
	if (!bOk)
	{
		FAccessUnit::Release(InAccessUnit);
	}

	return bOk ? IAccessUnitBufferInterface::EAUpushResult::Ok : IAccessUnitBufferInterface::EAUpushResult::Full;
}


//-----------------------------------------------------------------------------
/**
 * "Pushes" an End Of Data marker indicating no further access units will be added.
 */
void FVideoDecoderH264::AUdataPushEOD()
{
	AccessUnits.PushEndOfData();
}


//-----------------------------------------------------------------------------
/**
 * Flushes the decoder and clears the input access unit buffer.
 */
void FVideoDecoderH264::AUdataFlushEverything()
{
	FlushDecoderSignal.Signal();
	DecoderFlushedSignal.WaitAndReset();
}


//-----------------------------------------------------------------------------
/**
 *	Switches Windows 8+ to multithreaded DX for software decoding.
 */
#ifdef ELECTRA_ENABLE_SWDECODE
bool FVideoDecoderH264::FallbackToSwDecoding(FString Reason)
{
	if (!bIsHardwareAccelerated)
	{
		return false;
	}

	UE_LOG(LogElectraPlayer, Log, TEXT("FVideoDecoderH264::FallbackToSwDecoding: %s"), *Reason);

	bIsHardwareAccelerated = false;

	if (Electra::IsWindows8Plus())
	{
		// NOTE: the following doesn't apply to Windows 7 as it doesn't use DX11 device in decoding thread
		// as we don't use a dedicated DirextX device for s/w decoding, UE4's rendering device will be used from inside the decoder
		// to produce output samples, which means access from render and decoding threads. We need to enable multithread protection
		// for the device. Multithread protection can have performance impact, though its affect is expected to be negligible in most cases.
		// WARNING:
		// Once multithread protection is enabled we don't disable it, so UE4's rendering device stays protected for the rest of its lifetime.
		// Some other system could enable multithread protection after we did it, we have no means to know about this, and so disabling it
		// at the end of playback can cause GPU driver crash
		if (Electra::FDXDeviceInfo::s_DXDeviceInfo->RenderingDx11Device)
		{
			HRESULT res;
			TRefCountPtr<ID3D10Multithread> DxMultithread;
			VERIFY_HR(Electra::FDXDeviceInfo::s_DXDeviceInfo->RenderingDx11Device->QueryInterface(__uuidof(ID3D10Multithread), (void**)DxMultithread.GetInitReference()), "Failed to set video decoder into software mode (multithread protected)", ERRCODE_INTERNAL_FAILED_TO_SWITCH_TO_SOFTWARE_MODE);
			DxMultithread->SetMultithreadProtected(1);
		}
	}
	return true;
}
#else
bool FVideoDecoderH264::FallbackToSwDecoding(FString Reason)
{
	// Software decoding not supported. It's not even being called but we need to return something.
	return false;
}
#endif


//-----------------------------------------------------------------------------
/**
 *	Reconfigures decoder for software mode.
 */
bool FVideoDecoderH264::ReconfigureForSwDecoding(FString Reason)
{
	bRequiresReconfigurationForSW = true;
	if (!FallbackToSwDecoding(MoveTemp(Reason)))
	{
		return false;
	}

	// Nullify D3D Manager to switch decoder to software mode.
	HRESULT res;
	check(DecoderTransform.GetReference());
	VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_SET_D3D_MANAGER, 0), "Failed to set video decoder into software mode (unset D3D manager)", ERRCODE_INTERNAL_FAILED_TO_SWITCH_TO_SOFTWARE_MODE);
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Configures the decoder input and output media types.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH264::Configure()
{
	// Setup media input type
	if (!DecoderSetInputType())
	{
		return false;
	}
	if (bRequiresReconfigurationForSW)
	{
		return false;
	}

	// Setup media output type
	if (!DecoderSetOutputType())
	{
		return false;
	}
	if (bRequiresReconfigurationForSW)
	{
		return false;
	}

	// Verify status
	if (!DecoderVerifyStatus())
	{
		return false;
	}
	if (bRequiresReconfigurationForSW)
	{
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Starts decoder processing.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH264::StartStreaming()
{
	HRESULT res;
	// Start it!
	VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), "Failed to notify video decoder of stream begin", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_BEGIN);
	VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), "Failed to notify video decoder of start start", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_START);
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Configures the decoder input media type.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH264::DecoderSetInputType()
{
	TRefCountPtr<IMFMediaType>	InputMediaType;
	HRESULT					res;

	// https://docs.microsoft.com/en-us/windows/desktop/medfound/h-264-video-decoder
	VERIFY_HR(MFCreateMediaType(InputMediaType.GetInitReference()), "Failed to create video decoder input media type", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE);
	VERIFY_HR(InputMediaType->SetGUID(MF_MT_MAJOR_TYPE, MFMediaType_Video), "Failed to set video decoder input media type major", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE_MAJOR);
	VERIFY_HR(InputMediaType->SetGUID(MF_MT_SUBTYPE, MFVideoFormat_H264), "Failed to set video decoder input media sub type", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE_SUBTYPE);

	int32 configW, configH;
	if (bIsHardwareAccelerated)
	{
		configW = MaxHWDecodeW;
		configH = MaxHWDecodeH;
	}
	else
	{
		configW = CurrentSampleInfo.GetResolution().Width;
		configH = CurrentSampleInfo.GetResolution().Height;
	}

	if (CurrentAccessUnit && CurrentAccessUnit->AUCodecData.IsValid() && CurrentAccessUnit->AUCodecData->CodecSpecificData.Num())
	{
		VERIFY_HR(InputMediaType->SetBlob(MF_MT_USER_DATA, CurrentAccessUnit->AUCodecData->CodecSpecificData.GetData(), (UINT32) CurrentAccessUnit->AUCodecData->CodecSpecificData.Num()), "Failed to set video decoder input media type codec blob", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_MEDIA_TYPE_CODEC_BLOB);
	}

	VERIFY_HR(MFSetAttributeSize(InputMediaType, MF_MT_FRAME_SIZE, configW, configH), "Failed to set video decoder input media type resolution", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_MEDIA_TYPE_RESOLUTION);
	VERIFY_HR(InputMediaType->SetUINT32(MF_MT_INTERLACE_MODE, MFVideoInterlace_MixedInterlaceOrProgressive), "Failed to set video decoder interlace mode", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_MEDIA_PROGRESSIVE_MODE);

	res = DecoderTransform->SetInputType(0, InputMediaType, 0);

#ifdef ELECTRA_ENABLE_SWDECODE
	if (bIsHardwareAccelerated && res == MF_E_UNSUPPORTED_D3D_TYPE)
		// h/w acceleration is not supported, e.g. unsupported resolution (4K), fall back to s/w decoding
	{
		return ReconfigureForSwDecoding(TEXT("MF_E_UNSUPPORTED_D3D_TYPE"));
	}
	else
#endif

	if (FAILED(res))
	{
		VERIFY_HR(res, "Failed to set video decoder input type", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_MEDIA_TYPE)
		return false;
	}

	return true;
}


//-----------------------------------------------------------------------------
/**
 * Configures the decoder output media type.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH264::DecoderSetOutputType()
{
	TRefCountPtr<IMFMediaType>	OutputMediaType;
	GUID					OutputMediaMajorType;
	GUID					OutputMediaSubtype;
	HRESULT					res;

	// Supposedly calling GetOutputAvailableType() returns following output media subtypes:
	// MFVideoFormat_NV12, MFVideoFormat_YV12, MFVideoFormat_IYUV, MFVideoFormat_I420, MFVideoFormat_YUY2
	for(int32 TypeIndex=0; ; ++TypeIndex)
	{
		VERIFY_HR(DecoderTransform->GetOutputAvailableType(0, TypeIndex, OutputMediaType.GetInitReference()), "Failed to get video decoder available output type", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_MEDIA_TYPE);
		VERIFY_HR(OutputMediaType->GetGUID(MF_MT_MAJOR_TYPE, &OutputMediaMajorType), "Failed to get video decoder available output media type", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_MEDIA_TYPE_MAJOR);
		VERIFY_HR(OutputMediaType->GetGUID(MF_MT_SUBTYPE, &OutputMediaSubtype), "Failed to get video decoder available output media subtype", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_MEDIA_TYPE_SUBTYPE);
		if (OutputMediaMajorType == MFMediaType_Video && OutputMediaSubtype == MFVideoFormat_NV12)
		{
			VERIFY_HR(DecoderTransform->SetOutputType(0, OutputMediaType, 0), "Failed to set video decoder output type", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_MEDIA_TYPE);
			CurrentOutputMediaType = OutputMediaType;
			return true;
		}
	}
	PostError(S_OK, "Failed to set video decoder output type to desired format", ERRCODE_INTERNAL_COULD_NOT_SET_OUTPUT_DESIRED_MEDIA_TYPE);
	return false;
}


//-----------------------------------------------------------------------------
/**
 * Checks that the decoder is in the state we think it should be and good to go.
 *
 * @return true if successful, false on error
 */
bool FVideoDecoderH264::DecoderVerifyStatus()
{
	HRESULT		res;
	DWORD		NumInputStreams;
	DWORD		NumOutputStreams;

	VERIFY_HR(DecoderTransform->GetStreamCount(&NumInputStreams, &NumOutputStreams), "Failed to get video decoder stream count", ERRCODE_INTERNAL_COULD_NOT_GET_STREAM_COUNT);
	if (NumInputStreams != 1 || NumOutputStreams != 1)
	{
		PostError(S_OK, FString::Printf(TEXT("Unexpected number of streams: input %lu, output %lu"), NumInputStreams, NumOutputStreams), ERRCODE_INTERNAL_UNEXPECTED_NUMBER_OF_STREAMS);
		return false;
	}

	DWORD DecoderStatus = 0;
	VERIFY_HR(DecoderTransform->GetInputStatus(0, &DecoderStatus), "Failed to get video decoder input status", ERRCODE_INTERNAL_COULD_NOT_GET_INPUT_STATUS);
	if (MFT_INPUT_STATUS_ACCEPT_DATA != DecoderStatus)
	{
		PostError(S_OK, FString::Printf(TEXT("Decoder doesn't accept data, status %lu"), DecoderStatus), ERRCODE_INTERNAL_DECODER_NOT_ACCEPTING_DATA);
		return false;
	}

	VERIFY_HR(DecoderTransform->GetOutputStreamInfo(0, &DecoderOutputStreamInfo), "Failed to get video decoder output stream info", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_STREAM_INFO);
	if (!(DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_FIXED_SAMPLE_SIZE))
	{
		return ReconfigureForSwDecoding(TEXT("Incompatible H.264 decoder: fixed sample size expected"));
	}
	if (!(DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_WHOLE_SAMPLES))
	{
		return ReconfigureForSwDecoding(TEXT("Incompatible H.264 decoder: whole samples expected"));
	}
	if (bIsHardwareAccelerated && !(DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
	{
		// theoretically we can handle this situation with H/W decoder, but we can't reproduce it locally for testing so we aren't sure if H/W
		// decoder would work in this case
		return ReconfigureForSwDecoding(TEXT("Incompatible H.264 decoder: h/w accelerated decoder is expected to provide output samples"));
	}
	if (!bIsHardwareAccelerated && (DecoderOutputStreamInfo.dwFlags & MFT_OUTPUT_STREAM_PROVIDES_SAMPLES))
	{
		PostError(S_OK, "Incompatible H.264 decoder: s/w decoder is expected to require preallocated output samples", ERRCODE_INTERNAL_INCOMPATIBLE_DECODER);
		return false;
	}
	return true;
}


//-----------------------------------------------------------------------------
/**
 * Destroys the current decoder instance.
 */
void FVideoDecoderH264::InternalDecoderDestroy()
{
	DecoderTransform = nullptr;
	CurrentOutputMediaType = nullptr;
	bHaveDecoder = false;
}


//-----------------------------------------------------------------------------
/**
 * Notifies the buffer-ready listener that we will now be producing output data
 * and need an output buffer.
 * This call is not intended to create or obtain an output buffer. It is merely
 * indicating that new output will be produced.
 *
 * @param bHaveOutput
 */
void FVideoDecoderH264::NotifyReadyBufferListener(bool bHaveOutput)
{
	if (ReadyBufferListener)
	{
		IDecoderOutputBufferListener::FDecodeReadyStats stats;
		stats.MaxDecodedElementsReady = MaxDecodeBufferSize;
		stats.NumElementsInDecoder    = NumFramesInDecoder;
		stats.bOutputStalled		  = !bHaveOutput;
		stats.bEODreached   		  = AccessUnits.IsEndOfData() && stats.NumDecodedElementsReady == 0 && stats.NumElementsInDecoder == 0;
		ListenerMutex.Lock();
		if (ReadyBufferListener)
		{
			ReadyBufferListener->DecoderOutputReady(stats);
		}
		ListenerMutex.Unlock();
	}
}



void FVideoDecoderH264::PrepareAU(FAccessUnit* AccessUnit)
{
	if (!AccessUnit->bHasBeenPrepared)
	{
		AccessUnit->bHasBeenPrepared = true;

		// Process NALUs
		bool bIsDiscardable = true;
		bool bIsKeyframe = false;
		//AccessUnit->mIsDiscardable = 1;
		// Replace the NALU lengths with the startcode.
		uint32* pNALU = (uint32 *)AccessUnit->AUData;
		uint32* pEnd  = (uint32 *)Electra::AdvancePointer(pNALU, AccessUnit->AUSize);
		while(pNALU < pEnd)
		{
			// Check the nal_ref_idc in the NAL unit for dependencies.
			uint8 nal = *(const uint8 *)(pNALU + 1);
			check((nal & 0x80) == 0);
			if ((nal >> 5) != 0)
			{
				bIsDiscardable = false;		//AccessUnit->mIsDiscardable = 0;
			}
			// IDR frame?
			if ((nal & 0x1f) == 5)
			{
				bIsKeyframe = true;			//AccessUnit->mIsKeyframe = 1;
			}

			// SEI message(s)?
			if ((nal & 0x1f) == 6)
			{
				// ...
			}

			uint32 naluLen = MEDIA_FROM_BIG_ENDIAN(*pNALU) + 4;
			*pNALU = MEDIA_TO_BIG_ENDIAN(0x00000001U);
			pNALU = Electra::AdvancePointer(pNALU, naluLen);
		}

		// Is there codec specific data?
		if (AccessUnit->AUCodecData.IsValid())
		{
			// Yes.
			if (bIsKeyframe || AccessUnit->bIsSyncSample || AccessUnit->bIsFirstInSequence)
			{
				NewSampleInfo = AccessUnit->AUCodecData->ParsedInfo;

				// Have to re-allocate the AU memory to preprend the codec data
				uint64 nb = AccessUnit->AUSize + AccessUnit->AUCodecData->CodecSpecificData.Num();
				void* pD = AccessUnit->AllocatePayloadBuffer(nb);
				check(pD);
				void* pP = pD;
				FMemory::Memcpy(pP, AccessUnit->AUCodecData->CodecSpecificData.GetData(), AccessUnit->AUCodecData->CodecSpecificData.Num());
				pP = Electra::AdvancePointer(pP, AccessUnit->AUCodecData->CodecSpecificData.Num());
				FMemory::Memcpy(pP, AccessUnit->AUData, AccessUnit->AUSize);
				AccessUnit->AdoptNewPayloadBuffer(pD, nb);
				// New codec data makes this AU non-discardable.
				bIsDiscardable = false;
			}
		}
	}
}


void FVideoDecoderH264::SetupBufferAcquisitionProperties()
{
	BufferAcquireOptions.Clear();
	int32 w = CurrentSampleInfo.GetResolution().Width;
	int32 h = CurrentSampleInfo.GetResolution().Height;
	BufferAcquireOptions.Set("width",  FVariantValue((int64) w));
	BufferAcquireOptions.Set("height", FVariantValue((int64) h));
}



void FVideoDecoderH264::ReturnUnusedFrame()
{
	CurrentDecoderOutputBuffer.Reset();
	if (CurrentRenderOutputBuffer)
	{
		Electra::FParamDict DummySampleProperties;
		Renderer->ReturnBuffer(CurrentRenderOutputBuffer, false, DummySampleProperties);
		CurrentRenderOutputBuffer = nullptr;
	}
}


bool FVideoDecoderH264::AcquireOutputBuffer()
{
	// If we still have one we're done.
	if (CurrentRenderOutputBuffer)
	{
		return true;
	}
	while(!TerminateThreadSignal.IsSignaled())
	{
		if (FlushDecoderSignal.IsSignaled())
		{
			break;
		}

		UEMediaError bufResult = Renderer->AcquireBuffer(CurrentRenderOutputBuffer, 0, BufferAcquireOptions);
		check(bufResult == UEMEDIA_ERROR_OK || bufResult == UEMEDIA_ERROR_INSUFFICIENT_DATA);
		if (bufResult != UEMEDIA_ERROR_OK && bufResult != UEMEDIA_ERROR_INSUFFICIENT_DATA)
		{
			PostError(S_OK, "Failed to acquire sample buffer", ERRCODE_INTERNAL_COULD_NOT_GET_SAMPLE_BUFFER, bufResult);
			return false;
		}

		bool bHaveOutputBuffer = CurrentRenderOutputBuffer != nullptr;
		NotifyReadyBufferListener(bHaveOutputBuffer);
		if (bHaveOutputBuffer)
		{
			break;
		}
		else
		{
			// No available buffer. Sleep for a bit on the flush signal. This gets set before the termination signal.
			FlushDecoderSignal.WaitTimeout(1000 * 5);
		}
	}
	return true;

}


bool FVideoDecoderH264::ConvertDecodedImage(const TRefCountPtr<IMFSample>& DecodedOutputSample)
{
	FParamDict* OutputBufferSampleProperties = new FParamDict();
	if (!OutputBufferSampleProperties)
	{
		return false;
	}

	HRESULT					res;
	LONGLONG				llTimeStamp = 0;
	MFVideoArea				videoArea = {};
	UINT32					uiPanScanEnabled = 0;
	FTimeValue				TimeStamp, Duration;

	//PVS reports warning 773 (possible memory leak) for OutputBufferSampleProperties in the line below. It's a false positive as passing it into SetupDecodeOutputData() later on will pass on ownership implicitly.
	VERIFY_HR(DecodedOutputSample->GetSampleTime(&llTimeStamp), "Failed to get video decoder output sample timestamp", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_SAMPLE_TIME); //-V773
	TimeStamp.SetFromHNS((int64) llTimeStamp);
	OutputBufferSampleProperties->Set("pts", FVariantValue(TimeStamp));

	VERIFY_HR(DecodedOutputSample->GetSampleDuration(&llTimeStamp), "Failed to get video decoder output sample duration", ERRCODE_INTERNAL_COULD_NOT_GET_OUTPUT_SAMPLE_DURATION);
	Duration.SetFromHNS((int64) llTimeStamp);
	OutputBufferSampleProperties->Set("duration", FVariantValue(Duration));

	if (SUCCEEDED(res = CurrentOutputMediaType->GetUINT32(MF_MT_PAN_SCAN_ENABLED, &uiPanScanEnabled)) && uiPanScanEnabled)
	{
		res = CurrentOutputMediaType->GetBlob(MF_MT_PAN_SCAN_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr);
	}

	UINT32	dwInputWidth = 0;
	UINT32	dwInputHeight = 0;
	res = MFGetAttributeSize(CurrentOutputMediaType.GetReference(), MF_MT_FRAME_SIZE, &dwInputWidth, &dwInputHeight);
	check(SUCCEEDED(res));
	res = CurrentOutputMediaType->GetBlob(MF_MT_MINIMUM_DISPLAY_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr);
	if (FAILED(res))
	{
		res = CurrentOutputMediaType->GetBlob(MF_MT_GEOMETRIC_APERTURE, (UINT8*)&videoArea, sizeof(MFVideoArea), nullptr);
		if (FAILED(res))
		{
			videoArea.OffsetX.fract = 0;
			videoArea.OffsetX.value = 0;
			videoArea.OffsetY.fract = 0;
			videoArea.OffsetY.value = 0;
			videoArea.Area.cx = dwInputWidth;
			videoArea.Area.cy = dwInputHeight;
		}
	}

	// Width and height is the cropped area!
	OutputBufferSampleProperties->Set("width",  FVariantValue((int64) videoArea.Area.cx));
	OutputBufferSampleProperties->Set("height", FVariantValue((int64) videoArea.Area.cy));

	if (!bIsHardwareAccelerated || (FDXDeviceInfo::s_DXDeviceInfo->DxVersion == FDXDeviceInfo::ED3DVersion::Version11Win8))
	{
		// Cropping is applied to immediate decoder output and never visible to outside code
		OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64)0));
	}
	else
	{
		// Note: WMF decoder will always crop only at the lower, right borders
		OutputBufferSampleProperties->Set("crop_left", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_right", FVariantValue((int64)(dwInputWidth - videoArea.Area.cx)));
		OutputBufferSampleProperties->Set("crop_top", FVariantValue((int64)0));
		OutputBufferSampleProperties->Set("crop_bottom", FVariantValue((int64)(dwInputHeight - videoArea.Area.cy)));
	}

	// Try to get the stride. Defaults to 0 should it not be obtainable.
	UINT32 stride = MFGetAttributeUINT32(CurrentOutputMediaType, MF_MT_DEFAULT_STRIDE, 0);
	OutputBufferSampleProperties->Set("pitch",  FVariantValue((int64) stride));

	// Try to get the frame rate ratio
	UINT32 num=0, denom=0;
	MFGetAttributeRatio(CurrentOutputMediaType, MF_MT_FRAME_RATE, &num, &denom);
	OutputBufferSampleProperties->Set("fps_num",   FVariantValue((int64) num ));
	OutputBufferSampleProperties->Set("fps_denom", FVariantValue((int64) denom ));

	// Try to get the pixel aspect ratio
	num=0, denom=0;
	MFGetAttributeRatio(CurrentOutputMediaType, MF_MT_PIXEL_ASPECT_RATIO, &num, &denom);
	if (!num || !denom)
	{
		num   = 1;
		denom = 1;
	}
	OutputBufferSampleProperties->Set("aspect_ratio", FVariantValue((double) num / (double)denom));
	OutputBufferSampleProperties->Set("aspect_w",     FVariantValue((int64) num));
	OutputBufferSampleProperties->Set("aspect_h",     FVariantValue((int64) denom));

	OutputBufferSampleProperties->Set("pixelfmt",	  FVariantValue((int64)EPixelFormat::PF_NV12));

	if (CurrentRenderOutputBuffer != nullptr)
	{
		if (!SetupDecodeOutputData(FIntPoint(videoArea.Area.cx, videoArea.Area.cy), DecodedOutputSample, OutputBufferSampleProperties))
		{
			return false;
		}

		Renderer->ReturnBuffer(CurrentRenderOutputBuffer, true, *OutputBufferSampleProperties);
		CurrentRenderOutputBuffer = nullptr;

	}
	return true;
}


bool FVideoDecoderH264::Decode(FAccessUnit* AccessUnit, bool bResolutionChanged)
{
	// No decoder, nothing to do.
	if (DecoderTransform.GetReference() == nullptr)
	{
		return true;
	}
	// Draining the transform for resolution changes is only required for software transforms
	if (bIsHardwareAccelerated && bResolutionChanged)
	{
		return true;
	}

	HRESULT res;
	TRefCountPtr<IMFSample>	InputSample;
	bool bFlush = AccessUnit == nullptr;

	if (bFlush)
	{
		if (bResolutionChanged)
		{
			VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0), "Failed to issue video decoder drain command for resolution change", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_DRAINCOMMAND);
		}
		else
		{
			VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_END_OF_STREAM, 0), "Failed to set video decoder end of stream notification", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_ENDOFSTREAM);
			VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_DRAIN, 0), "Failed to issue video decoder drain command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_DRAINCOMMAND);
		}
	}
	else
	{
		// Create the input sample.
		TRefCountPtr<IMFMediaBuffer> InputSampleBuffer;
		BYTE*					pbNewBuffer = nullptr;
		DWORD					dwMaxBufferSize = 0;
		DWORD					dwSize = 0;
		LONGLONG				llSampleTime = 0;

		CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);

		VERIFY_HR(MFCreateSample(InputSample.GetInitReference()), "Failed to create video decoder input sample", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUT_SAMPLE);
		VERIFY_HR(MFCreateMemoryBuffer((DWORD) AccessUnit->AUSize, InputSampleBuffer.GetInitReference()), "Failed to create video decoder input sample memory buffer", ERRCODE_INTERNAL_COULD_NOT_CREATE_INPUTBUFFER);
		VERIFY_HR(InputSample->AddBuffer(InputSampleBuffer.GetReference()), "Failed to set video decoder input buffer with sample", ERRCODE_INTERNAL_COULD_NOT_ADD_INPUT_BUFFER_TO_SAMPLE);
		VERIFY_HR(InputSampleBuffer->Lock(&pbNewBuffer, &dwMaxBufferSize, &dwSize), "Failed to lock video decoder input sample buffer", ERRCODE_INTERNAL_COULD_NOT_LOCK_INPUT_BUFFER);
		FMemory::Memcpy(pbNewBuffer, AccessUnit->AUData, AccessUnit->AUSize);
		VERIFY_HR(InputSampleBuffer->Unlock(), "Failed to unlock video decoder input sample buffer", ERRCODE_INTERNAL_COULD_NOT_UNLOCK_INPUT_BUFFER);
		VERIFY_HR(InputSampleBuffer->SetCurrentLength((DWORD) AccessUnit->AUSize), "Failed to set video decoder input sample buffer length", ERRCODE_INTERNAL_COULD_NOT_SET_BUFFER_CURRENT_LENGTH);
		// Set sample attributes
		llSampleTime = AccessUnit->PTS.GetAsHNS();
		VERIFY_HR(InputSample->SetSampleTime(llSampleTime), "Failed to set video decoder input sample presentation time", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_TIME);
		llSampleTime = AccessUnit->Duration.GetAsHNS();
		VERIFY_HR(InputSample->SetSampleDuration(llSampleTime), "Failed to set video decode intput sample duration", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_DURATION);
		llSampleTime = AccessUnit->DTS.GetAsHNS();
		VERIFY_HR(InputSample->SetUINT64(MFSampleExtension_DecodeTimestamp, llSampleTime), "Failed to set video decoder input sample decode time", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_DECODETIME);
		VERIFY_HR(InputSample->SetUINT32(MFSampleExtension_CleanPoint, AccessUnit->bIsSyncSample ? 1 : 0), "Failed to set video decoder input sample clean point", ERRCODE_INTERNAL_COULD_NOT_SET_INPUT_SAMPLE_KEYFRAME);
	}

	while(!TerminateThreadSignal.IsSignaled())
	{
#ifdef ELECTRA_ENABLE_SWDECODE
		// For SW decoding we need to get a buffer from the renderer right now.
		if (!bIsHardwareAccelerated)
		{
			if (!AcquireOutputBuffer())
			{
				return false;
			}

			// Check if we got an output buffer. When flushing or terminating AcquireOutputBuffer() may return with no buffer!
			if (CurrentRenderOutputBuffer)
			{
				// Setup texture sample to receive data during decode
				PreInitDecodeOutputForSW(FIntPoint(CurrentSampleInfo.GetResolution().Width, CurrentSampleInfo.GetResolution().Height));
			}
			else
			{
				break;
			}
		}
#endif

		// Check if to be flushed now. This may have resulted in AcquireOutputBuffer() to return with no buffer!
		if (FlushDecoderSignal.IsSignaled())
		{
			break;
		}

		if (!CurrentDecoderOutputBuffer.IsValid())
		{
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);

			if (!CreateDecoderOutputBuffer())
			{
				return false;
			}
		}

		CurrentDecoderOutputBuffer->PrepareForProcess();
		DWORD	dwStatus = 0;
		res = DecoderTransform->ProcessOutput(0, 1, &CurrentDecoderOutputBuffer->mOutputBuffer, &dwStatus);
		CurrentDecoderOutputBuffer->UnprepareAfterProcess();

		if (res == MF_E_TRANSFORM_NEED_MORE_INPUT)
		{
			// Draining for resolution change?
			if (bResolutionChanged || bDecoderFlushPending)
			{
				bDecoderFlushPending = false;
				CurrentSampleInfo = NewSampleInfo;
				SetupBufferAcquisitionProperties();
				ReturnUnusedFrame();
				NumFramesInDecoder = 0;
				if (!DecoderSetInputType())
				{
					return false;
				}
				return DecoderSetOutputType();
			}
			else if (bFlush)
			{
				// Yes. This means we have received all pending output and are done now.
				// Issue a flush for completeness sake.
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0), "Failed to issue video decoder flush command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_FLUSHCOMMAND);
				// And start over.
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_BEGIN_STREAMING, 0), "Failed to set video decoder stream begin", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_BEGIN);
				VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_NOTIFY_START_OF_STREAM, 0), "Failed to start video decoder", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_START);
				CurrentDecoderOutputBuffer.Reset();
				NumFramesInDecoder = 0;
				return true;
			}
			else if (InputSample.IsValid())
			{
				CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);

				VERIFY_HR(DecoderTransform->ProcessInput(0, InputSample.GetReference(), 0), "Failed to process video decoder input", ERRCODE_INTERNAL_COULD_NOT_PROCESS_INPUT);
				// Used this sample. Have no further input data for now, but continue processing to produce output if possible.
				InputSample = nullptr;
				++NumFramesInDecoder;
			}
			else
			{
				// Need more input but have none right now.
				return true;
			}
		}
		else if (res == MF_E_TRANSFORM_STREAM_CHANGE)
		{
			//if((CurrentDecoderOutputBuffer->mOutputBuffer.dwFlags & MFT_OUTPUT_DATA_BUFFER_FORMAT_CHANGE) != 0)
			{
				// Update output type.
				if(!DecoderSetOutputType())
				{
					return false;
				}
			}
		}
		else if (SUCCEEDED(res))
		{
			TRefCountPtr<IMFSample> DecodedOutputSample = CurrentDecoderOutputBuffer->DetachOutputSample();
			//LogMessage(IInfoLog::ELevel::Info, StringHelpers::SPrintf(TEXT("OUT: %p"), DecodedOutputSample.Get()));
			CurrentDecoderOutputBuffer.Reset();

			if (DecodedOutputSample)
			{
				// HW accelerated decoding needs to get a buffer from the renderer only now after decoding.
				// SW decoding required it earlier for the call to ProcessOutput().
				if (bIsHardwareAccelerated)
				{
					if (!AcquireOutputBuffer())
					{
						return false;
					}
				}

				CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);

				// Check if to be flushed now. This may have resulted in AcquireOutputBuffer() to return with no buffer!
				if (FlushDecoderSignal.IsSignaled())
				{
					break;
				}
				if (!ConvertDecodedImage(DecodedOutputSample))
				{
					return false;
				}
			}
			--NumFramesInDecoder;
		}
		else
		{
			// Error!
			VERIFY_HR(res, "Failed to process video decoder output", ERRCODE_INTERNAL_COULD_NOT_PROCESS_OUTPUT);
			return false;
		}
	}
	return true;
}


bool FVideoDecoderH264::PerformFlush()
{
	// No decoder, nothing to do.
	if (DecoderTransform.GetReference() == nullptr)
	{
		return true;
	}

	HRESULT				res;
	bDecoderFlushPending = true;
	VERIFY_HR(DecoderTransform->ProcessMessage(MFT_MESSAGE_COMMAND_FLUSH, 0), "Failed to issue video decoder flush command", ERRCODE_INTERNAL_COULD_NOT_SET_DECODER_FLUSHCOMMAND);

	return true;
}


bool FVideoDecoderH264::DecodeDummy(FAccessUnit* AccessUnit)
{
	check(AccessUnit->Duration.IsValid());

	FParamDict OutputBufferSampleProperties;

	OutputBufferSampleProperties.Set("pts", FVariantValue(AccessUnit->PTS));
	OutputBufferSampleProperties.Set("duration", FVariantValue(AccessUnit->Duration));
	OutputBufferSampleProperties.Set("is_dummy", FVariantValue(true));

	if (!AcquireOutputBuffer())
	{
		return false;
	}

	if (CurrentRenderOutputBuffer != nullptr)
	{
//TODO: THIS BEING A DUMMY - THE DICT IN ANY DECODER OUTPUT BUFFER (is there any?) WILL BE OUTDATED THIS WAY!!
		Renderer->ReturnBuffer(CurrentRenderOutputBuffer, true, OutputBufferSampleProperties);
		CurrentRenderOutputBuffer = nullptr;
	}
	return true;
}


void FVideoDecoderH264::DecoderCreate()
{
	if (InternalDecoderCreate())
	{
		// Configure the decoder with our default values.
		bRequiresReconfigurationForSW = false;
		Configure();
		if (!bError && bRequiresReconfigurationForSW)
		{
			// No failure yet, but a switch to software decoding is required. We do the configuration over one more time.
			check(!bIsHardwareAccelerated);	// must have been reset already!
			// Clear this out, we can't get another request for reconfiguration since we already did.
			bRequiresReconfigurationForSW = false;
			Configure();
			if (!bError && bRequiresReconfigurationForSW)
			{
				PostError(S_OK, "Failed to switch H.264 decoder transform to software decoding", ERRCODE_INTERNAL_FAILED_TO_SWITCH_TO_SOFTWARE_MODE);
			}
		}
		if (!bError)
		{
			StartStreaming();
			bHaveDecoder = true;
		}
		else
		{
			InternalDecoderDestroy();
		}
	}
	else
	{
		bError = true;
		PostError(S_OK, "Failed to create H.264 decoder transform", ERRCODE_INTERNAL_COULD_NOT_CREATE_DECODER);
	}
}


//-----------------------------------------------------------------------------
/**
 * Creates a decoder and checks if it will use hardware or software decoding.
 */
void FVideoDecoderH264::TestHardwareDecoding()
{
	bRequiresReconfigurationForSW = false;
	CurrentSampleInfo.SetResolution(FStreamCodecInformation::FResolution(Align(Config.MaxFrameWidth, 16), Align(Config.MaxFrameHeight, 16)));
	NewSampleInfo = CurrentSampleInfo;
	bIsHardwareAccelerated = true;
	DecoderCreate();
	InternalDecoderDestroy();
}


//-----------------------------------------------------------------------------
/**
 * H264 video decoder main threaded decode loop
 */
void FVideoDecoderH264::WorkerThread()
{
	LLM_SCOPE(ELLMTag::ElectraPlayer);

	CurrentRenderOutputBuffer     = nullptr;
	CurrentAccessUnit   		  = nullptr;
	bHaveDecoder				  = false;
	NumFramesInDecoder  		  = 0;
	bDecoderFlushPending		  = false;
	bRequiresReconfigurationForSW = false;
	bError  					  = false;

	// Set the configured maximum resolution as the current resolution which is used to create the decoder transform.
	CurrentSampleInfo.SetResolution(FStreamCodecInformation::FResolution(Align(Config.MaxFrameWidth, 16), Align(Config.MaxFrameHeight, 16)));
	NewSampleInfo = CurrentSampleInfo;
	SetupBufferAcquisitionProperties();
	// Create a decoder transform. This will determine if we will be using a hardware or software decoder.
	bIsHardwareAccelerated = true;
	DecoderCreate();

	// Now that we know whether we will be running a hardware or software decoder we can create the decoded image pool.
	if (!CreateDecodedImagePool())
	{
		bError = true;
	}

	bool bDone = false;
	bool bInDummyDecodeMode = false;

	// Require a new media input type based on the actual first access unit.
	bool bNeedInitialReconfig = true;
	while(!TerminateThreadSignal.IsSignaled())
	{
		// Notify optional buffer listener that we will now be needing an AU for our input buffer.
		if (!bError && InputBufferListener && AccessUnits.Num() == 0)
		{
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);

			FAccessUnitBufferInfo	sin;
			IAccessUnitBufferListener::FBufferStats	stats;
			AccessUnits.GetStats(sin);
			stats.NumAUsAvailable  = sin.NumCurrentAccessUnits;
			stats.NumBytesInBuffer = sin.CurrentMemInUse;
			stats.MaxBytesOfBuffer = sin.MaxDataSize;
			stats.bEODSignaled     = sin.bEndOfData;
			stats.bEODReached      = sin.bEndOfData && sin.NumCurrentAccessUnits == 0;
			ListenerMutex.Lock();
			if (InputBufferListener)
			{
				InputBufferListener->DecoderInputNeeded(stats);
			}
			ListenerMutex.Unlock();
		}
		// Wait for data.
		bool bHaveData = AccessUnits.WaitForData(1000 * 5);
		// When there is data, even and especially after a previous EOD, we are no longer done and idling.
		if (bHaveData && bDone)
		{
			bDone = false;
		}

		// Tick output buffer pool to handle state progression of samples in flight
		Renderer->TickOutputBufferPool();
		// Tick platform code in case something needs doing
		PlatformTick();

		if (bHaveData && Renderer->CanReceiveOutputFrames(1)) // note: the check for full output buffers ignores the NumFramesInDecoder value as we don't seem to receive new data if we don't feed new input
		{
			CurrentAccessUnit = nullptr;
			bool bOk = AccessUnits.Pop(CurrentAccessUnit);
			MEDIA_UNUSED_VAR(bOk);
			check(bOk);

			if (!bError)
			{
				if (!CurrentAccessUnit->bIsDummyData)
				{
					{
					CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);
					PrepareAU(CurrentAccessUnit);
					}

					// Decode
					if (!bError)
					{
						// Update the buffer acquisition after a change in streams?
						bool bFormatChangedJustNow = CurrentSampleInfo.IsDifferentFromOtherVideo(NewSampleInfo);

						if (bInDummyDecodeMode)
						{
							bFormatChangedJustNow = true;
							bInDummyDecodeMode = false;
						}

						if (!bHaveDecoder)
						{
							CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);
							DecoderCreate();
							bNeedInitialReconfig = true;
						}

						if (bFormatChangedJustNow || bNeedInitialReconfig)
						{
							if (bNeedInitialReconfig)
							{
								bDecoderFlushPending = true;
							}
							if (!Decode(nullptr, bNeedInitialReconfig ? false : true))
							{
								// FIXME: Is this error fatal or safe to ignore?
							}
							CurrentSampleInfo = NewSampleInfo;
							SetupBufferAcquisitionProperties();
						}
						bNeedInitialReconfig = false;

						if (!Decode(CurrentAccessUnit, false))
						{
							bError = true;
						}
					}
				}
				else
				{
					if (!bInDummyDecodeMode)
					{
						bInDummyDecodeMode = true;
						Decode(nullptr, true);
					}

					if (!DecodeDummy(CurrentAccessUnit))
					{
						bError = true;
					}
				}
			}

			// If decoding was aborted and the AU not handed off, delete it now.
			FAccessUnit::Release(CurrentAccessUnit);
			CurrentAccessUnit = nullptr;
		}
		else
		{
			if (!bHaveData)
			{
				// No data. Is the buffer at EOD?
				if (AccessUnits.IsEndOfData())
				{
					NotifyReadyBufferListener(true);
					// Are we done yet?
					if (!bDone && !bError)
					{
						bError = !Decode(nullptr, false);
						NumFramesInDecoder = 0;
					}
					bDone = true;
					FMediaRunnable::SleepMilliseconds(10);
				}
			}
			else
			{
				// We could not decode new data as the output buffers are full... notify listener & wait a bit...
				NotifyReadyBufferListener(false);
				FMediaRunnable::SleepMilliseconds(5);
			}
		}

		// Flush?
		if (FlushDecoderSignal.IsSignaled())
		{
			CSV_SCOPED_TIMING_STAT(ElectraPlayer, FVideoDecoderH264_Worker);

			// Destroy and re-create.
			InternalDecoderDestroy();

			ReturnUnusedFrame();
			AccessUnits.Flush();

			FlushDecoderSignal.Reset();
			DecoderFlushedSignal.Signal();
			NumFramesInDecoder = 0;
			CurrentSampleInfo.SetResolution(FStreamCodecInformation::FResolution(Align(Config.MaxFrameWidth, 16), Align(Config.MaxFrameHeight, 16)));
			NewSampleInfo = CurrentSampleInfo;

			// Reset done state.
			bDone = false;
		}
	}

	InternalDecoderDestroy();
	ReturnUnusedFrame();
	DestroyDecodedImagePool();
	AccessUnits.Flush();
	AccessUnits.CapacitySet(0);
}

} // namespace Electra

#endif // ELECTRA_ENABLE_MFDECODER


