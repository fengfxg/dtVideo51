// Copyright 2021 Dexter.Wan. All Rights Reserved. 
// EMail: 45141961@qq.com

#pragma once

#include "CoreMinimal.h"
#include "Modules/ModuleManager.h"

class FDTVideoModule : public IModuleInterface
{
public:
	// 系统开始运行
	virtual void StartupModule() override;
	// 系统结束运行
	virtual void ShutdownModule() override;

};
