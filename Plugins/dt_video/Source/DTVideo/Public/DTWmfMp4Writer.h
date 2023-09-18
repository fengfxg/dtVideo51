// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#pragma once

#include "DTWmfPrivate.h"
#include "Math/NumericLimits.h"
#include "MediaPacket.h"
#include "Misc/Optional.h"
#include "Templates/RefCounting.h"

class FDTWmfMp4Writer final
{
public:
	bool Initialize(const TCHAR* Filename);

	/**
	 * Create an audio stream and return the its index on success
	 */
	TOptional<DWORD> CreateAudioStream(const FString& Codec, const AVEncoder::FAudioConfig& Config);

	/**
	 * Create a video stream and return the its index on success
	 */
	TOptional<DWORD> CreateVideoStream(const FString& Codec, const AVEncoder::FVideoConfig& Config);

	bool Start();
	bool Write(const AVEncoder::FMediaPacket& InSample, DWORD StreamIndex);
	bool Finalize();

private:
	TRefCountPtr<IMFSinkWriter> Writer;
};
