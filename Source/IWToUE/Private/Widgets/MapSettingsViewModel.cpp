#include "Widgets/MapSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FMapSettingsViewModel::FMapSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
	check(Settings != nullptr);
}

FText FMapSettingsViewModel::GetExportDirectory() const
{
	return FText::FromString(Settings->Map.ExportDirectory);
}

FReply FMapSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseMapExportDirTitle", "Select Map Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->
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
	if (Settings->Map.ExportDirectory != NewText.ToString())
	{
		Settings->Map.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FMapSettingsViewModel::SaveSettings()
{
	if (Settings) Settings->Save();
}
