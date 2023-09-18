// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#pragma once

#include "CoreMinimal.h"
#include "MediaPacket.h"
#include "AudioEncoder.h"
#include "DTWmfMp4Writer.h"
#include "AudioEncoderFactory.h"
#include "VideoEncoderFactory.h"
#include "GameplayMediaEncoder.h"
#include "EncoderFrameFactory.h"



/**
 *
 */
class DTVIDEO_API FDTVideoRecording : private ISubmixBufferListener, public AVEncoder::IAudioEncoderListener
{
	// 数据变量
public:
	AVEncoder::FVideoConfig						m_VideoConfig;								// 视频解码配置
	FCriticalSection							m_ListenersCS;								// 监听回调锁
	FCriticalSection							m_VideoProcessingCS;						// 视频处理锁
	TUniquePtr<AVEncoder::FVideoEncoder>		m_VideoEncoder;								// 视频解码组件
	TSharedPtr<AVEncoder::FVideoEncoderInput>	m_VideoEncoderInput;						// DX视频解码组件


	AVEncoder::FAudioConfig						m_AudioConfig;								// 音频解码配置
	TUniquePtr<AVEncoder::FAudioEncoder>		m_AudioEncoder;								// 音频解码

	TArray<AVEncoder::FMediaPacket>				m_Samples;									// 解码后样本
	TUniquePtr<FDTWmfMp4Writer>					m_Mp4Writer;								// MP4写入接口
	TUniquePtr<FThread>							m_BackgroundSaving;							// 保存线程

	FTimespan									m_StartTime = 0;							// 开始录制时间
	DWORD										m_AudioStreamIndex = 0;						// 音频流索引
	DWORD										m_VideoStreamIndex = 0;						// 视频流索引
	double										m_AudioClock = 0;							// 音频时间
	bool										m_bVideoRecording = false;					// 录制中
	bool										m_bVideoSaving = false;						// 保存中

	TMap<TSharedPtr<AVEncoder::FVideoEncoderInputFrame>, FTexture2DRHIRef>	m_BackBuffers;				// 视频解码缓冲
private:
	FEncoderFrameFactory						m_EncoderFrameFactory;

public:
	// 构造函数
	FDTVideoRecording();
	// 析构函数
	virtual ~FDTVideoRecording();

	// 内部变量
private:
	// 单例对象
	static FDTVideoRecording		g_Singleton;
 

	// 功能函数
public:
	// 获取实例
	static FDTVideoRecording& Get() { return g_Singleton; }

public:
	// 音频数据回调
	void OnNewSubmixBuffer(const USoundSubmix* OwningSubmix, float* AudioData, int32 NumSamples, int32 NumChannels, const int32 SampleRate, double AudioClock) override;
	// 音频解码回调
	void OnEncodedAudioFrame(const AVEncoder::FMediaPacket& Packet) override;
	// 处理音频帧数
	void ProcessAudioFrame(const float* AudioData, int32 NumSamples, int32 NumChannels, int32 SampleRate);

public:
	// 解码视频帧
	void OnEncodedVideoFrame(uint32 LayerIndex, const TSharedPtr< AVEncoder::FVideoEncoderInputFrame> Frame, const AVEncoder::FCodecPacket& Packet);
	// 缓冲帧数回调
	void OnFrameBufferReady(SWindow& SlateWindow, const FTexture2DRHIRef& FrameBuffer);
	// 处理视频帧
	void ProcessVideoFrame(const FTexture2DRHIRef& FrameBuffer);



	// 
public:
	// 初始化解码配置
	bool CreateVideoConfig(uint32 nWidth, uint32 nHeight);
	// 创建视频解码器
	bool CreateVideoEncoder();
	// 创建音频解码
	bool CreateAudioEncoder();

	// 
public:
	// 开始录制
	FString StartVideo(uint32 nWidth, uint32 nHeight);
	// 结束录制
	bool StopVideo(const FString& FileName, class UDTStopRecordAsyncBPLib* pDTStopRecordAsyncBPLib);
	// 保存文件
	FString SaveFile(const FString& FileName);

	// 
private:
	// 保存数据样本
	void OnMediaSample(const AVEncoder::FMediaPacket& InSample);
	// 初始化MP4文件
	bool InitialiseMp4Writer(const FString& FullFilename, bool bHasAudio);


};
