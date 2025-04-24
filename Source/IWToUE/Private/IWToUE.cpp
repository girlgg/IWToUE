#include "IWToUE.h"

#include "Interfaces/IPluginManager.h"
#include "MapImporter/CastMapImporter.h"

#define LOCTEXT_NAMESPACE "FIWToUEModule"

void FIWToUEModule::StartupModule()
{
	// 注册菜单
	UToolMenus::RegisterStartupCallback(FSimpleMulticastDelegate::FDelegate::CreateLambda([this]()
	{
		MapImporter->RegisterMenus();
	}));
	// 注册Shader
	FString ShaderDir = FPaths::Combine(IPluginManager::Get().FindPlugin(TEXT("IWToUE"))->GetBaseDir(), TEXT("Shaders"));
	AddShaderSourceDirectoryMapping(TEXT("/IWToUE"), ShaderDir);
}

void FIWToUEModule::ShutdownModule()
{
}

#undef LOCTEXT_NAMESPACE

IMPLEMENT_MODULE(FIWToUEModule, IWToUE)
