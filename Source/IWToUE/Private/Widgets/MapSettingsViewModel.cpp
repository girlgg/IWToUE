#include "Widgets/MapSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FMapSettingsViewModel::FMapSettingsViewModel()
{
}

FText FMapSettingsViewModel::GetExportDirectory() const
{
	UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Map.ExportDirectory);
}

FReply FMapSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const FString Title = LOC_SETTINGS("BrowseMapExportDirTitle", "Select Map Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (FString FolderName; DesktopPlatform->
			OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Map.ExportDirectory, FolderName))
		{
			if (Settings->Map.ExportDirectory != FolderName)
			{
				Settings->Map.ExportDirectory = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FMapSettingsViewModel::HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	UWraithSettings* Settings = GetSettings();
	if (Settings->Map.ExportDirectory != NewText.ToString())
	{
		Settings->Map.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

UWraithSettings* FMapSettingsViewModel::GetSettings() const
{
	if (GEditor)
	{
		if (UWraithSettingsManager* SettingsManager = GEditor->GetEditorSubsystem<UWraithSettingsManager>())
		{
			return SettingsManager->GetSettingsMutable();
		}
	}
	UE_LOG(LogTemp, Error,
	       TEXT("FGeneralSettingsViewModel::GetSettings - Could not get Settings Manager or Settings Object!"));
	return nullptr;
}

void FMapSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings()) Settings->Save();
}
