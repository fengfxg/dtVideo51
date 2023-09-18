// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#include "DTStopRecordAsyncBPLib.h"
#include "DTVideoRecording.h"

UDTStopRecordAsyncBPLib* UDTStopRecordAsyncBPLib::StopRecord(UObject* WorldContextObject, const FString& Name)
{
    // 调用节点函数
    UDTStopRecordAsyncBPLib* NewRequest = NewObject<UDTStopRecordAsyncBPLib>();
    NewRequest->AddToRoot();
    NewRequest->OnStopRecording(Name);
    return NewRequest;
}

// 保存录像
void UDTStopRecordAsyncBPLib::OnStopRecording(const FString& Name)
{
    FDTVideoRecording::Get().StopVideo(Name, this);
}