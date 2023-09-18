// Copyright Epic Games, Inc. All Rights Reserved.

#pragma once
#include "CoreMinimal.h"
#include "VideoEncoderInput.h"
#include "Templates/RefCounting.h"
#include "Templates/SharedPointer.h"

class FRHITexture;


/*
* Factory to create `AVEncoder::FVideoEncoderInputFrame` for use in Pixel Streaming encoders.
* This class is responsible for creating/managing the underlying `AVEncoder::FVideoEncoderInput` required to create
* `AVEncoder::FVideoEncoderInputFrame`.
* This class is not threadsafe and should only be accessed from a single thread.
*/
class DTVIDEO_API FEncoderFrameFactory
{
public:
	FEncoderFrameFactory();
	~FEncoderFrameFactory();
	TSharedPtr<AVEncoder::FVideoEncoderInputFrame> GetFrameAndSetTexture(TRefCountPtr<FRHITexture> InTexture);
	TSharedPtr<AVEncoder::FVideoEncoderInput> GetOrCreateVideoEncoderInput();

private:
	TSharedPtr<AVEncoder::FVideoEncoderInputFrame> GetOrCreateFrame(const TRefCountPtr<FRHITexture> InTexture);
	void RemoveStaleTextures();
	void FlushFrames();
	TSharedPtr<AVEncoder::FVideoEncoderInput> CreateVideoEncoderInput() const;
	void SetTexture(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);

	void SetTextureCUDAD3D11(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);
	void SetTextureCUDAD3D12(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame, const TRefCountPtr<FRHITexture>& Texture);


private:
	uint64 FrameId = 0;
	TSharedPtr<AVEncoder::FVideoEncoderInput> EncoderInput;
	// Store a mapping between raw textures and the FVideoEncoderInputFrames that wrap them
	TMap<TRefCountPtr<FRHITexture>, TSharedPtr<AVEncoder::FVideoEncoderInputFrame>> TextureToFrameMapping;
};
