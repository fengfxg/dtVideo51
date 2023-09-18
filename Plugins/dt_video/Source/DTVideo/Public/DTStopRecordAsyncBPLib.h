// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintAsyncActionBase.h"
#include "DTStopRecordAsyncBPLib.generated.h"

// 返回代理
DECLARE_DYNAMIC_MULTICAST_DELEGATE_OneParam(FStopRecordDelegatge, const FString&, ErrorMessage);

/**
 * 
 */
UCLASS(meta = (DisplayName = "DT Record Video"), Category = "DT Video")
class DTVIDEO_API UDTStopRecordAsyncBPLib : public UBlueprintAsyncActionBase
{
	GENERATED_BODY()
	

public:
	/*
	Function: stop recording and save the file
	Param Name: The path and file name of the saved file, you need to add .mp4. If it is empty, save in [ Project name\Saved\VideoCaptures ]
	Return ErrorMessage: If the save fails, the error message will be output
	*/
	UFUNCTION(BlueprintCallable, BlueprintCosmetic, meta = (WorldContext = "WorldContextObject", DisplayName = "StopRecord", BlueprintInternalUseOnly = "true"), Category = "DT Video")
	static UDTStopRecordAsyncBPLib* StopRecord(UObject* WorldContextObject, const FString& Name);

	// Saved Successfully
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "Success"))
	FStopRecordDelegatge OnSuccess;

	// Save Failed
	UPROPERTY(BlueprintAssignable, meta = (DisplayName = "Failure"))
	FStopRecordDelegatge OnFailure;

	// 内部函数
private:
	// 保存录像
	void OnStopRecording(const FString& Name);
};
