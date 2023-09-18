// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "DTVideoBPLib.generated.h"

UENUM()
enum class EBP_SuccessOrFailure : uint8
{
    Success,
    Failure,
};


/**
 * 
 */
UCLASS(Blueprintable, BlueprintType, meta = (DisplayName = "DT Record Video"), Category = "DT Video")
class DTVIDEO_API UDTVideoBPLib : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()

public:
	/*
	Function: start recording game screen
	Parame Width: the resolution width of recording and saving, if it is 0, it is the current actual display pixel width
	Parame Height: the resolution height of recording and saving, if it is 0, it is the current actual display pixel height
	Return ErrorMessage: If it fails at the beginning, an error message will be output
	*/
    UFUNCTION(BlueprintCallable, meta = (WorldContext = "WorldContextObject", DisplayName = "StartRecord", ExpandEnumAsExecs = "Result"), Category = "DT Video")
    static void StartRecord(UObject* WorldContextObject, EBP_SuccessOrFailure& Result, FString& ErrorMessage, int32 Width = 0, int32 Height = 0);
};
