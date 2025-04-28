#include "WraithX/WraithSettingsManager.h"

#include "WraithX/WraithSettings.h"

const UWraithSettings* UWraithSettingsManager::GetSettings() const
{
	return SettingsObject;
}

UWraithSettings* UWraithSettingsManager::GetSettingsMutable()
{
	return SettingsObject;
}

void UWraithSettingsManager::Initialize(FSubsystemCollectionBase& Collection)
{
	Super::Initialize(Collection);
	if (!SettingsObject)
	{
		SettingsObject = GetMutableDefault<UWraithSettings>();
		if (!SettingsObject)
		{
			SettingsObject = NewObject<UWraithSettings>(GetTransientPackage(), UWraithSettings::StaticClass());
			SettingsObject->LoadConfig();
		}
		SettingsObject->AddToRoot();
	}
}

void UWraithSettingsManager::Deinitialize()
{
	if (SettingsObject)
	{
		SettingsObject->SaveConfig();
		SettingsObject->RemoveFromRoot();
		SettingsObject = nullptr;
	}
	
	Super::Deinitialize();
}
