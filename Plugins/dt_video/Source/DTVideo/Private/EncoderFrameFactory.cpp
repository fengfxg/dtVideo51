// Copyright Epic Games, Inc. All Rights Reserved.

#include "EncoderFrameFactory.h"

#include "CudaModule.h"

#include "VideoEncoderInput.h"
#include "Templates/SharedPointer.h"




#include "VideoCommon.h"
#include "ShaderCore.h"
//#include <D3D11State.h>
#include "D3D11RHI/Public/D3D11State.h"
#include "D3D11RHI/Public/D3D11Resources.h"
//#include  "ID3D12DynamicRHI.h"
#include <D3D12RHI/Public/ID3D12DynamicRHI.h>
//#include "Windows/AllowWindowsPlatformTypes.h"
//THIRD_PARTY_INCLUDES_START
//#include <VersionHelpers.h>
//THIRD_PARTY_INCLUDES_END
//#include "Windows/HideWindowsPlatformTypes.h"




FEncoderFrameFactory::FEncoderFrameFactory()
{
}

FEncoderFrameFactory::~FEncoderFrameFactory()
{
	FlushFrames();
}



void FEncoderFrameFactory::FlushFrames()
{
	TextureToFrameMapping.Empty();
}


void FEncoderFrameFactory::RemoveStaleTextures()
{
	// Remove any textures whose only reference is the one held by this class

	TMap<FTextureRHIRef, TSharedPtr<AVEncoder::FVideoEncoderInputFrame>>::TIterator Iter = TextureToFrameMapping.CreateIterator();
	for (; Iter; ++Iter)
	{
		FTextureRHIRef& Tex = Iter.Key();

		if (Tex.GetRefCount() == 1)
		{
			Iter.RemoveCurrent();
		}
	}
}



TSharedPtr<AVEncoder::FVideoEncoderInputFrame> FEncoderFrameFactory::GetOrCreateFrame(const TRefCountPtr<FRHITexture> InTexture)
{
	check(EncoderInput.IsValid());

	RemoveStaleTextures();

	TSharedPtr<AVEncoder::FVideoEncoderInputFrame> OutFrame;

	if (TextureToFrameMapping.Contains(InTexture))
	{
		OutFrame = *(TextureToFrameMapping.Find(InTexture));
	}
	else
	{
		// A frame needs to be created
		// NOTE: This create and move is necessary as this constructor is the only way we can create a new TSharedPtr with a custom deleter.
		TSharedPtr<AVEncoder::FVideoEncoderInputFrame> NewFrame(
			EncoderInput->CreateBuffer([&EncoderInput = EncoderInput](const AVEncoder::FVideoEncoderInputFrame* ReleasedFrame) { /* OnReleased */ }),
			[&EncoderInput = EncoderInput](AVEncoder::FVideoEncoderInputFrame* InFrame) { EncoderInput->DestroyBuffer(InFrame); });
		OutFrame = MoveTemp(NewFrame);

		SetTexture(OutFrame, InTexture);
		TextureToFrameMapping.Add(InTexture, OutFrame);
	}

	auto w = InTexture->GetDesc().Extent.X;
	auto h = InTexture->GetDesc().Extent.Y;

	OutFrame->SetWidth(w);
	OutFrame->SetHeight(h);
	OutFrame->SetFrameID(++FrameId);
	return OutFrame;
}



TSharedPtr<AVEncoder::FVideoEncoderInputFrame> FEncoderFrameFactory::GetFrameAndSetTexture(TRefCountPtr<FRHITexture> InTexture)
{
	check(EncoderInput.IsValid());

	TSharedPtr<AVEncoder::FVideoEncoderInputFrame> Frame = GetOrCreateFrame(InTexture);

	return Frame;
}

TSharedPtr<AVEncoder::FVideoEncoderInput> FEncoderFrameFactory::GetOrCreateVideoEncoderInput()
{
	if (!EncoderInput.IsValid())
	{
		EncoderInput = CreateVideoEncoderInput();
	}

	return EncoderInput;
}

TSharedPtr<AVEncoder::FVideoEncoderInput> FEncoderFrameFactory::CreateVideoEncoderInput() const
{
	if (!GDynamicRHI)
	{
		UE_LOG(LogTemp, Error, TEXT("GDynamicRHI not valid for some reason."));
		return nullptr;
	}

	const ERHIInterfaceType RHIType = RHIGetInterfaceType();

	// Consider if we want to support runtime resolution changing?
	bool bIsResizable = false;

	if (RHIType == ERHIInterfaceType::D3D11)
	{
		if (IsRHIDeviceAMD())
		{
			return AVEncoder::FVideoEncoderInput::CreateForD3D11(GDynamicRHI->RHIGetNativeDevice(), bIsResizable, true);
		}
		else if (IsRHIDeviceNVIDIA())
		{
			return AVEncoder::FVideoEncoderInput::CreateForD3D11(GDynamicRHI->RHIGetNativeDevice(), bIsResizable, false);
		}
	}
	else if (RHIType == ERHIInterfaceType::D3D12)
	{
		if (IsRHIDeviceAMD())
		{
			return AVEncoder::FVideoEncoderInput::CreateForD3D12(GDynamicRHI->RHIGetNativeDevice(), bIsResizable, false);
		}
		else if (IsRHIDeviceNVIDIA())
		{
			return AVEncoder::FVideoEncoderInput::CreateForCUDA(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext(), bIsResizable);
		}
	}


	UE_LOG(LogTemp, Error, TEXT("Current RHI %s is not supported in Pixel Streaming"), GDynamicRHI->GetName());
	return nullptr;
}

void FEncoderFrameFactory::SetTexture(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InputFrame, const TRefCountPtr<FRHITexture>& Texture)
{
	const ERHIInterfaceType RHIType = RHIGetInterfaceType();

	if (RHIType == ERHIInterfaceType::D3D11)
	{
		InputFrame->SetTexture((ID3D11Texture2D*)Texture->GetNativeResource(), [](ID3D11Texture2D* NativeTexture) { /* Do something with released texture if needed */ });
	}
	else if (RHIType == ERHIInterfaceType::D3D12)
	{
		if (IsRHIDeviceAMD())
		{
			InputFrame->SetTexture((ID3D12Resource*)Texture->GetNativeResource(), [](ID3D12Resource* NativeTexture) { /* Do something with released texture if needed */ });
		}
		else if (IsRHIDeviceNVIDIA())
		{
			SetTextureCUDAD3D12(InputFrame, Texture);
		}
		else
		{
			UE_LOG(LogTemp, Error, TEXT("Pixel Streaming only supports AMD and NVIDIA devices, this device is neither of those."));
		}
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("Pixel Streaming does not support this RHI - %s"), GDynamicRHI->GetName());
	}
}




void FEncoderFrameFactory::SetTextureCUDAD3D11(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InputFrame, const TRefCountPtr<FRHITexture>& Texture)
{
	FD3D11Texture* D3D11Texture = GetD3D11TextureFromRHITexture(Texture);
	unsigned long long TextureMemorySize = D3D11Texture->GetMemorySize();

	ID3D11Texture2D* D3D11NativeTexture = static_cast<ID3D11Texture2D*>(D3D11Texture->GetResource());

	TRefCountPtr<IDXGIResource> DXGIResource;
	HRESULT QueryResult = D3D11NativeTexture->QueryInterface(IID_PPV_ARGS(DXGIResource.GetInitReference()));
	if (QueryResult != S_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get DX texture handle for importing memory to CUDA: %d"), QueryResult);
	}

	HANDLE D3D11TextureHandle;
	DXGIResource->GetSharedHandle(&D3D11TextureHandle);
	DXGIResource->Release();

	FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

	CUexternalMemory MappedExternalMemory = nullptr;

	{
		// generate a cudaExternalMemoryHandleDesc
		CUDA_EXTERNAL_MEMORY_HANDLE_DESC CudaExtMemHandleDesc = {};
		CudaExtMemHandleDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D11_RESOURCE_KMT;
		CudaExtMemHandleDesc.handle.win32.name = NULL;
		CudaExtMemHandleDesc.handle.win32.handle = D3D11TextureHandle;
		CudaExtMemHandleDesc.size = TextureMemorySize;
		// Necessary for committed resources (DX11 and committed DX12 resources)
		CudaExtMemHandleDesc.flags |= CUDA_EXTERNAL_MEMORY_DEDICATED;

		// import external memory
		CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&MappedExternalMemory, &CudaExtMemHandleDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to import external memory from vulkan error: %d"), Result);
		}
	}

	CUmipmappedArray MappedMipArray = nullptr;
	CUarray MappedArray = nullptr;

	{
		CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC MipmapDesc = {};
		MipmapDesc.numLevels = 1;
		MipmapDesc.offset = 0;
		MipmapDesc.arrayDesc.Width = Texture->GetDesc().Extent.X;
		MipmapDesc.arrayDesc.Height = Texture->GetDesc().Extent.Y;
		MipmapDesc.arrayDesc.Depth = 1;
		MipmapDesc.arrayDesc.NumChannels = 4;
		MipmapDesc.arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
		MipmapDesc.arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST | CUDA_ARRAY3D_COLOR_ATTACHMENT;

		// get the CUarray from the external memory
		CUresult Result = FCUDAModule::CUDA().cuExternalMemoryGetMappedMipmappedArray(&MappedMipArray, MappedExternalMemory, &MipmapDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to bind mipmappedArray error: %d"), Result);
		}
	}

	// get the CUarray from the external memory
	CUresult MipMapArrGetLevelErr = FCUDAModule::CUDA().cuMipmappedArrayGetLevel(&MappedArray, MappedMipArray, 0);
	if (MipMapArrGetLevelErr != CUDA_SUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to bind to mip 0."));
	}

	FCUDAModule::CUDA().cuCtxPopCurrent(NULL);

	InputFrame->SetTexture(MappedArray, AVEncoder::FVideoEncoderInputFrame::EUnderlyingRHI::D3D11, nullptr, [MappedArray, MappedMipArray, MappedExternalMemory](CUarray NativeTexture) {
		// free the cuda types
		FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

		if (MappedArray)
		{
			CUresult Result = FCUDAModule::CUDA().cuArrayDestroy(MappedArray);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy MappedArray: %d"), Result);
			}
		}

		if (MappedMipArray)
		{
			CUresult Result = FCUDAModule::CUDA().cuMipmappedArrayDestroy(MappedMipArray);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy MappedMipArray: %d"), Result);
			}
		}

		if (MappedExternalMemory)
		{
			CUresult Result = FCUDAModule::CUDA().cuDestroyExternalMemory(MappedExternalMemory);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy MappedExternalMemoryArray: %d"), Result);
			}
		}

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
		});
}


void FEncoderFrameFactory::SetTextureCUDAD3D12(TSharedPtr<AVEncoder::FVideoEncoderInputFrame> InputFrame, const TRefCountPtr<FRHITexture>& Texture)
{
	ID3D12Resource* NativeD3D12Resource = GetID3D12DynamicRHI()->RHIGetResource(Texture);
	const int64 TextureMemorySize = GetID3D12DynamicRHI()->RHIGetResourceMemorySize(Texture);

	// Because we create our texture as RenderTargetable, it is created as a committed resource, which is what our current implementation here supports.
	// To prevent a mystery crash in future, check that our resource is a committed resource
	check(!GetID3D12DynamicRHI()->RHIIsResourcePlaced(Texture));

	TRefCountPtr<ID3D12Device> OwnerDevice;
	HRESULT QueryResult;
	if ((QueryResult = NativeD3D12Resource->GetDevice(IID_PPV_ARGS(OwnerDevice.GetInitReference()))) != S_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get DX texture handle for importing memory to CUDA: %d (Get Device)"), QueryResult);
	}

	//
	// ID3D12Device::CreateSharedHandle gives as an NT Handle, and so we need to call CloseHandle on it
	//
	HANDLE D3D12TextureHandle;
	if ((QueryResult = OwnerDevice->CreateSharedHandle(NativeD3D12Resource, NULL, GENERIC_ALL, NULL, &D3D12TextureHandle)) != S_OK)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to get DX texture handle for importing memory to CUDA: %d"), QueryResult);
	}

	FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

	CUexternalMemory MappedExternalMemory = nullptr;

	{
		// generate a cudaExternalMemoryHandleDesc
		CUDA_EXTERNAL_MEMORY_HANDLE_DESC CudaExtMemHandleDesc = {};
		CudaExtMemHandleDesc.type = CU_EXTERNAL_MEMORY_HANDLE_TYPE_D3D12_RESOURCE;
		CudaExtMemHandleDesc.handle.win32.name = NULL;
		CudaExtMemHandleDesc.handle.win32.handle = D3D12TextureHandle;
		CudaExtMemHandleDesc.size = TextureMemorySize;
		// Necessary for committed resources (DX11 and committed DX12 resources)
		CudaExtMemHandleDesc.flags |= CUDA_EXTERNAL_MEMORY_DEDICATED;

		// import external memory
		CUresult Result = FCUDAModule::CUDA().cuImportExternalMemory(&MappedExternalMemory, &CudaExtMemHandleDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to import external memory from vulkan error: %d"), Result);
		}
	}

	CUmipmappedArray MappedMipArray = nullptr;
	CUarray MappedArray = nullptr;

	{
		CUDA_EXTERNAL_MEMORY_MIPMAPPED_ARRAY_DESC MipmapDesc = {};
		MipmapDesc.numLevels = 1;
		MipmapDesc.offset = 0;
		MipmapDesc.arrayDesc.Width = Texture->GetDesc().Extent.X;
		MipmapDesc.arrayDesc.Height = Texture->GetDesc().Extent.Y;
		MipmapDesc.arrayDesc.Depth = 1;
		MipmapDesc.arrayDesc.NumChannels = 4;
		MipmapDesc.arrayDesc.Format = CU_AD_FORMAT_UNSIGNED_INT8;
		MipmapDesc.arrayDesc.Flags = CUDA_ARRAY3D_SURFACE_LDST | CUDA_ARRAY3D_COLOR_ATTACHMENT;

		// get the CUarray from the external memory
		CUresult Result = FCUDAModule::CUDA().cuExternalMemoryGetMappedMipmappedArray(&MappedMipArray, MappedExternalMemory, &MipmapDesc);
		if (Result != CUDA_SUCCESS)
		{
			UE_LOG(LogTemp, Error, TEXT("Failed to bind mipmappedArray error: %d"), Result);
		}
	}

	// get the CUarray from the external memory
	CUresult MipMapArrGetLevelErr = FCUDAModule::CUDA().cuMipmappedArrayGetLevel(&MappedArray, MappedMipArray, 0);
	if (MipMapArrGetLevelErr != CUDA_SUCCESS)
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to bind to mip 0."));
	}

	FCUDAModule::CUDA().cuCtxPopCurrent(NULL);

	InputFrame->SetTexture(MappedArray, AVEncoder::FVideoEncoderInputFrame::EUnderlyingRHI::D3D12, D3D12TextureHandle, [MappedArray, MappedMipArray, MappedExternalMemory](CUarray NativeTexture) {
		// free the cuda types
		FCUDAModule::CUDA().cuCtxPushCurrent(FModuleManager::GetModuleChecked<FCUDAModule>("CUDA").GetCudaContext());

		if (MappedArray)
		{
			CUresult Result = FCUDAModule::CUDA().cuArrayDestroy(MappedArray);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy MappedArray: %d"), Result);
			}
		}

		if (MappedMipArray)
		{
			CUresult Result = FCUDAModule::CUDA().cuMipmappedArrayDestroy(MappedMipArray);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy MappedMipArray: %d"), Result);
			}
		}

		if (MappedExternalMemory)
		{
			CUresult Result = FCUDAModule::CUDA().cuDestroyExternalMemory(MappedExternalMemory);
			if (Result != CUDA_SUCCESS)
			{
				UE_LOG(LogTemp, Error, TEXT("Failed to destroy MappedExternalMemoryArray: %d"), Result);
			}
		}

		FCUDAModule::CUDA().cuCtxPopCurrent(NULL);
		});
}

