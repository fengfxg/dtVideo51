// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#include "DTVideoBPLib.h"
#include "VideoRecordingSystem.h"
#include "PlatformFeatures.h"
#include "DTVideoRecording.h"
#include "GameplayMediaEncoder.h"

void UDTVideoBPLib::StartRecord(UObject* WorldContextObject, EBP_SuccessOrFailure& Result, FString& ErrorMessage, int32 Width /*= 0*/, int32 Height /*= 0*/)
{
    // 获取世界指针
    UWorld* World = GEngine->GetWorldFromContextObject(WorldContextObject, EGetWorldErrorMode::LogAndReturnNull);

    // 无效世界
    if (World == nullptr)
    {
        Result = EBP_SuccessOrFailure::Failure;
        return;
    }

    // 获取分辨率
    if (Width <= 0 || Height <= 0)
    { 
        Width = World->GetGameViewport()->Viewport->GetSizeXY().X; 
        Height = World->GetGameViewport()->Viewport->GetSizeXY().Y;
    }
    
    // 开始录像
    ErrorMessage = FDTVideoRecording::Get().StartVideo(Width, Height);
    if (ErrorMessage.Equals(TEXT("Success")))
    {
        Result = EBP_SuccessOrFailure::Success; 
        return;
    }
    
    Result = EBP_SuccessOrFailure::Failure;
    return;
}
