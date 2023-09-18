// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#include "DTVideoRecording.h"
#include "MediaPacket.h"
#include "DynamicRHI.h"
#include "ScreenRendering.h"
#include "CommonRenderResources.h"
#include "DTStopRecordAsyncBPLib.h"
#include "DTVideoBPLib.h"
#include "Engine/World.h"
#include "Rendering/SlateRenderer.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformFilemanager.h"
#include "Async/Async.h"

const uint32 HardcodedAudioSamplerate = 48000;
const uint32 HardcodedAudioNumChannels = 2;
const uint32 HardcodedAudioBitrate = 192000;
const uint32 HardcodedVideoFPS = 60;
const uint32 HardcodedVideoBitrate = 20000000;

FDTVideoRecording FDTVideoRecording::g_Singleton;

// 构造函数
FDTVideoRecording::FDTVideoRecording()
{
}

// 析构函数
FDTVideoRecording::~FDTVideoRecording()
{
}

// 解码视频帧
void FDTVideoRecording::OnEncodedVideoFrame(uint32 LayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InputFrame, const AVEncoder::FCodecPacket& Packet)
{
	AVEncoder::FMediaPacket packet(AVEncoder::EPacketType::Video);

	packet.Timestamp = InputFrame->GetTimestampUs();
	packet.Duration = 0;
	packet.Data = TArray<uint8>(Packet.Data.Get(), Packet.DataSize);
	packet.Video.bKeyFrame = Packet.IsKeyFrame;
	packet.Video.Width = InputFrame->GetWidth();
	packet.Video.Height = InputFrame->GetHeight();
	packet.Video.FrameAvgQP = Packet.VideoQP;
	packet.Video.Framerate = 60;

	FScopeLock Lock(&m_ListenersCS);
	OnMediaSample(packet);

	//InputFrame->Release();
}

// 缓冲帧数回调
void FDTVideoRecording::OnFrameBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer)
{
	// 录制中处理视频帧数
	if (m_bVideoRecording)
	{
		ProcessVideoFrame(FrameBuffer);
	}
}

// 处理视频帧
void FDTVideoRecording::ProcessVideoFrame(const FTexture2DRHIRef& FrameBuffer)
{
	// 无效解码器
	if (!CreateVideoEncoder())
	{
		return;
	}

	{
		FScopeLock Lock(&m_VideoProcessingCS);

		const FRHITextureDesc& desc = FrameBuffer->GetDesc();
		auto w = desc.Extent.X;
		auto h = desc.Extent.Y;

		//如果大小不一致
		if ((m_VideoConfig.Width != w) || (m_VideoConfig.Height != h)) {
			auto config = m_VideoEncoder->GetLayerConfig(0);

			m_VideoConfig.Width = w;
			m_VideoConfig.Height = h;

			config.Width = m_VideoConfig.Width;
			config.Height = m_VideoConfig.Height;
			config.MaxBitrate = m_VideoConfig.Bitrate;
			config.TargetBitrate = m_VideoConfig.Bitrate;
			config.MaxFramerate = m_VideoConfig.Framerate;

			m_VideoEncoder->UpdateLayerConfig(0, config);
		}

		FTimespan Now = FTimespan::FromSeconds(FPlatformTime::Seconds()) - m_StartTime;

		auto InputFrame = m_EncoderFrameFactory.GetFrameAndSetTexture(FrameBuffer);
		InputFrame->SetTimestampUs(Now.GetTicks());

		AVEncoder::FVideoEncoder::FEncodeOptions EncodeOptions;
		m_VideoEncoder->Encode(InputFrame, EncodeOptions);
	}
}


void FDTVideoRecording::OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double /*AudioClock*/)
{
	// 录制中处理视频帧数
	if (m_bVideoRecording)
	{
		ProcessAudioFrame(AudioData, NumSamples, NumChannels, SampleRate);
	}
}

// 音频解码回调
void FDTVideoRecording::OnEncodedAudioFrame(const AVEncoder::FMediaPacket& Packet)
{
	FScopeLock Lock(&m_ListenersCS);
	OnMediaSample(Packet);
}

// 处理音频帧数
void FDTVideoRecording::ProcessAudioFrame(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate)
{
	// 音频解码没有创建
	if (!m_AudioEncoder.IsValid())
	{
		return;
	}

	// 获取音频数据
	Audio::AlignedFloatBuffer InData;
	InData.Append(AudioData, NumSamples);
	Audio::TSampleBuffer<float> FloatBuffer(InData, NumChannels, SampleRate);

	// 更新声道
	if (FloatBuffer.GetNumChannels() != HardcodedAudioNumChannels)
	{
		FloatBuffer.MixBufferToChannels(HardcodedAudioNumChannels);
	}

	// 解码音频
	AVEncoder::FAudioFrame Frame;
	Frame.Timestamp = FTimespan::FromSeconds(m_AudioClock);
	Frame.Duration = FTimespan::FromSeconds(FloatBuffer.GetSampleDuration());
	FloatBuffer.Clamp();
	Frame.Data = FloatBuffer;
	m_AudioEncoder->Encode(Frame);

	// 添加流失时间
	m_AudioClock += FloatBuffer.GetSampleDuration();
}

// 初始化解码配置
bool FDTVideoRecording::CreateVideoConfig(uint32 nWidth, uint32 nHeight)
{

	if (m_VideoEncoderInput.IsValid())m_VideoEncoderInput.Reset();

	// 解码配置
	m_VideoConfig.Codec = "h264";
	m_VideoConfig.Width = nWidth;
	m_VideoConfig.Height = nHeight;
	m_VideoConfig.Framerate = HardcodedVideoFPS;
	m_VideoConfig.Bitrate = HardcodedVideoBitrate;

	m_VideoEncoderInput = m_EncoderFrameFactory.GetOrCreateVideoEncoderInput();
	return true;
}

// 创建视频解码器
bool FDTVideoRecording::CreateVideoEncoder()
{
	// 分辨率不一致，需要重新创建解码器
	if (m_VideoEncoder.IsValid())
	{
		return true;
	}

	// 无效配置
	if (m_VideoConfig.Width == 0 || m_VideoConfig.Height == 0)
	{
		return false;
	}

	// 解码器配置
	AVEncoder::FVideoEncoder::FLayerConfig LayerConfig;
	LayerConfig.Width = m_VideoConfig.Width;
	LayerConfig.Height = m_VideoConfig.Height;
	LayerConfig.MaxBitrate = m_VideoConfig.Bitrate;
	LayerConfig.TargetBitrate = m_VideoConfig.Bitrate;
	LayerConfig.MaxFramerate = m_VideoConfig.Framerate;

	
	// 获取解码激活队列
	auto& Available = AVEncoder::FVideoEncoderFactory::Get().GetAvailable();

	// 创建解码器
	m_VideoEncoder = AVEncoder::FVideoEncoderFactory::Get().Create(Available[0].ID, m_VideoEncoderInput, LayerConfig);

	// 设置解码回调
	m_VideoEncoder->SetOnEncodedPacket([this](uint32 LayerIndex, const TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const AVEncoder::FCodecPacket& Packet)
		{ OnEncodedVideoFrame(LayerIndex, Frame, Packet); });

	return m_VideoEncoder.IsValid();
}


// 创建音频解码
bool FDTVideoRecording::CreateAudioEncoder()
{
	// 解码器已经创建
	if (m_AudioEncoder.IsValid())
	{
		return true;
	}

	// 获取音频解码工厂
	AVEncoder::FAudioEncoderFactory* AudioEncoderFactory = AVEncoder::FAudioEncoderFactory::FindFactory("aac");
	if (!AudioEncoderFactory)
	{
		return false;
	}

	// 获取解码器
	m_AudioEncoder = AudioEncoderFactory->CreateEncoder("aac");
	if (!m_AudioEncoder)
	{
		return false;
	}

	// 创建解码器
	m_AudioConfig.Samplerate = HardcodedAudioSamplerate;
	m_AudioConfig.NumChannels = HardcodedAudioNumChannels;
	m_AudioConfig.Bitrate = HardcodedAudioBitrate;
	if (!m_AudioEncoder->Initialize(m_AudioConfig))
	{
		return false;
	}
	m_AudioConfig.Codec = m_AudioEncoder->GetType();

	// 注册回调接口
	m_AudioEncoder->RegisterListener(*this);

	return true;
}

// 开始录制
FString FDTVideoRecording::StartVideo(uint32 nWidth, uint32 nHeight)
{
	// 录像中和保存中无法启动
	if (m_bVideoRecording)
	{
		return TEXT("Is Now Recording, Start Video Failure");
	}
	if (m_bVideoSaving)
	{
		return TEXT("Is Now Saving, Start Video Failure");
	}

	// 创建视频配置
	if (!CreateVideoConfig(nWidth, nHeight))
	{
		return TEXT("Create Video Config Failure");
	}

	// 创建音频解码
	if (!CreateAudioEncoder())
	{
		return TEXT("Create Audio Encoder Failure");
	}

	// 绑定音频回调
	FAudioDevice* pAudioDevice = FAudioDevice::GetMainAudioDevice().GetAudioDevice();
	if (!pAudioDevice)
	{
		return TEXT("Get Audio Device Failure");
	}
	pAudioDevice->RegisterSubmixBufferListener(this);

	// 绑定像素回调
	FSlateRenderer* pSlateRenderer = FSlateApplication::Get().GetRenderer();
	if (!pSlateRenderer)
	{
		return TEXT("Get Renderer Failure");
	}
	pSlateRenderer->OnBackBufferReadyToPresent().AddRaw(this, &FDTVideoRecording::OnFrameBufferReady);

	// 标记状态
	m_bVideoRecording = true;

	// 保存时间
	m_StartTime = FTimespan::FromSeconds(FPlatformTime::Seconds());
	m_AudioClock = 0;

	return TEXT("Success");
}

// 结束录制
bool FDTVideoRecording::StopVideo(const FString& FileName, UDTStopRecordAsyncBPLib* pDTStopRecordAsyncBPLib)
{
	// 录像中和保存中无法启动
	if (!m_bVideoRecording)
	{
		pDTStopRecordAsyncBPLib->OnFailure.Broadcast(TEXT("Not Recording, Stop Video Failure"));
		pDTStopRecordAsyncBPLib->RemoveFromRoot();
		return false;
	}
	if (m_bVideoSaving)
	{
		pDTStopRecordAsyncBPLib->OnFailure.Broadcast(TEXT("Is Now Saving, Stop Video Failure"));
		pDTStopRecordAsyncBPLib->RemoveFromRoot();
		return false;
	}

	// 标记状态
	m_bVideoRecording = false;
	m_bVideoSaving = true;

	// 获取保存目录
	FString FullFilename = FileName;

	// 无保存路径
	if (FullFilename.IsEmpty())
	{
		FullFilename = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::VideoCaptureDir() + FDateTime::Now().ToString(TEXT("%Y.%m.%d-%H.%M.%S.%s.mp4"))));
	}
	// 无指定硬盘
	else if (!FPaths::IsDrive(FullFilename.Left(2)))
	{
		FullFilename = FPlatformFileManager::Get().GetPlatformFile().ConvertToAbsolutePathForExternalAppForWrite(*(FPaths::VideoCaptureDir() + FullFilename));
	}

	// 添加后缀名
	if (!FullFilename.Right(4).Equals(TEXT(".mp4"), ESearchCase::IgnoreCase))
	{
		FullFilename += TEXT(".mp4");
	}

	// 开启线程保存
	m_BackgroundSaving.Reset(new FThread(TEXT("Highlight Saving"), [this, FullFilename, pDTStopRecordAsyncBPLib]()
		{
			// 保存文件
			FString SaveInfo = SaveFile(FullFilename);

			// 回调主线程
			AsyncTask(ENamedThreads::GameThread, [this, SaveInfo, pDTStopRecordAsyncBPLib]()
				{
					if (SaveInfo.Equals(TEXT("Success")))
					{
						pDTStopRecordAsyncBPLib->OnSuccess.Broadcast(TEXT("Success"));
					}
					else
					{
						pDTStopRecordAsyncBPLib->OnFailure.Broadcast(SaveInfo);
					}
					pDTStopRecordAsyncBPLib->RemoveFromRoot();
					m_bVideoSaving = false;
					m_BackgroundSaving.Release();
					m_BackgroundSaving.Reset();
				});
		}));

	return true;
}

// 保存文件
FString FDTVideoRecording::SaveFile(const FString& FileName)
{
	// 没有缓冲
	if (m_Samples.Num() == 0)
	{
		return TEXT("No Recording Data");
	}

	// 定义开始变量
	int FirstSampleIndex = 0;
	FTimespan StartTime(0);

	// 判断是否有音频
	bool bHasAudio = false;
	for (int Idx = FirstSampleIndex; Idx != m_Samples.Num(); ++Idx)
	{
		if (m_Samples[Idx].Type == AVEncoder::EPacketType::Audio)
		{
			bHasAudio = true;
			break;
		}
	}

	// 初始化MP4文件
	if (!InitialiseMp4Writer(FileName, bHasAudio))
	{
		return TEXT("Initialise Mp4 File Failure");
	}

	// 写入MP4缓冲
	for (int Idx = FirstSampleIndex; Idx != m_Samples.Num(); ++Idx)
	{
		AVEncoder::FMediaPacket& Sample = m_Samples[Idx];
		Sample.Timestamp = Sample.Timestamp - StartTime;
		if (!m_Mp4Writer->Write(Sample, (Sample.Type == AVEncoder::EPacketType::Audio) ? m_AudioStreamIndex : m_VideoStreamIndex))
		{
			return TEXT("Write Mp4 File Failure");
		}
	}

	// 写入结束
	if (!m_Mp4Writer->Finalize())
	{
		return TEXT("Finalize Mp4 File Failure");
	}

	return TEXT("Success");
}


// 保存数据样本
void FDTVideoRecording::OnMediaSample(const AVEncoder::FMediaPacket& InSample)
{
	AVEncoder::FMediaPacket SampleCopy = InSample;
	m_Samples.Add(MoveTemp(SampleCopy));
}

// 初始化MP4文件
bool FDTVideoRecording::InitialiseMp4Writer(const FString& FullFilename, bool bHasAudio)
{
	// 创建目录
	FString FullDirName = FullFilename.Replace(TEXT("/"), TEXT("\\"));
	int nFind = FullDirName.Find(TEXT("\\"), ESearchCase::IgnoreCase, ESearchDir::FromStart);
	while (nFind != INDEX_NONE)
	{
		FString DirName = FullDirName.Left(nFind);
		auto& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		if (!PlatformFile.DirectoryExists(*DirName))
		{
			if (!PlatformFile.CreateDirectory(*DirName))
			{
				break;
			}
		}
		nFind = FullDirName.Find(TEXT("\\"), ESearchCase::IgnoreCase, ESearchDir::FromStart, nFind + 1);
	}

	// 重置文件写入
	m_Mp4Writer.Reset(new FDTWmfMp4Writer);

	// 写入头
	if (!m_Mp4Writer->Initialize(*FullFilename))
	{
		return false;
	}

	// 获取音频配置
	if (bHasAudio)
	{
		TPair<FString, AVEncoder::FAudioConfig> AudioConfig = TPair<FString, AVEncoder::FAudioConfig>(m_AudioConfig.Codec, m_AudioConfig);
		if (AudioConfig.Key == "")
		{
			return false;
		}

		TOptional<DWORD> Res = m_Mp4Writer->CreateAudioStream(AudioConfig.Key, AudioConfig.Value);
		if (Res.IsSet())
		{
			m_AudioStreamIndex = Res.GetValue();
		}
		else
		{
			return false;
		}
	}

	// 获取视频配置
	TPair<FString, AVEncoder::FVideoConfig> videoConfig = TPair<FString, AVEncoder::FVideoConfig>(m_VideoConfig.Codec, m_VideoConfig);
	if (videoConfig.Key == "")
	{
		return false;
	}

	TOptional<DWORD> Res = m_Mp4Writer->CreateVideoStream(videoConfig.Key, videoConfig.Value);
	if (Res.IsSet())
	{
		m_VideoStreamIndex = Res.GetValue();
	}
	else
	{
		return false;
	}

	if (!m_Mp4Writer->Start())
	{
		return false;
	}

	return true;
}
