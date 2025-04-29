#include "Widgets/AudioSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FAudioSettingsViewModel::FAudioSettingsViewModel()
{
}

void FAudioSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings()) Settings->Save();
}

FText FAudioSettingsViewModel::GetExportDirectory() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Audio.ExportDirectory);
}

FReply FAudioSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseAudioExportDirTitle", "Select Audio Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->
			OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Audio.ExportDirectory, FolderName))
		{
			if (Settings->Audio.ExportDirectory != FolderName)
			{
				Settings->Audio.ExportDirectory = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FAudioSettingsViewModel::HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	UWraithSettings* Settings = GetSettings();
	if (Settings->Audio.ExportDirectory != NewText.ToString())
	{
		Settings->Audio.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

UWraithSettings* FAudioSettingsViewModel::GetSettings() const
{
	if (GEditor)
	{
		if (UWraithSettingsManager* SettingsManager = GEditor->GetEditorSubsystem<UWraithSettingsManager>())
		{
			return SettingsManager->GetSettingsMutable();
		}
	}
	UE_LOG(LogTemp, Error,
	       TEXT("FAudioSettingsViewModel::GetSettings - Could not get Settings Manager or Settings Object!"));
	return nullptr;
}
