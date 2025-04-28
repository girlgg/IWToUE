#include "Widgets/AudioSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FAudioSettingsViewModel::FAudioSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
	check(Settings != nullptr);
}

void FAudioSettingsViewModel::SaveSettings()
{
	if (Settings) Settings->Save();
}

FText FAudioSettingsViewModel::GetExportDirectory() const
{
	return FText::FromString(Settings->Audio.ExportDirectory);
}

FReply FAudioSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
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
	if (Settings->Audio.ExportDirectory != NewText.ToString())
	{
		Settings->Audio.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}
