#include "Widgets/MaterialSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FMaterialSettingsViewModel::FMaterialSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
}

FText FMaterialSettingsViewModel::GetExportDirectory() const
{
	return FText::FromString(Settings->Material.ExportDirectory);
}

ECheckBoxState FMaterialSettingsViewModel::GetUseGlobalPathCheckState() const
{
	return Settings->Material.bUseGlobalMaterialPath ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FMaterialSettingsViewModel::GetGlobalMaterialPath() const
{
	return FText::FromString(Settings->Material.GlobalMaterialPath);
}

bool FMaterialSettingsViewModel::IsGlobalMaterialPathEnabled() const
{
	return Settings->Material.bUseGlobalMaterialPath;
}

FReply FMaterialSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseMatExportDirTitle", "Select Default Material Export Directory").
			ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Material.ExportDirectory,
		                                         FolderName))
		{
			if (Settings->Material.ExportDirectory != FolderName)
			{
				Settings->Material.ExportDirectory = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FMaterialSettingsViewModel::HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (Settings->Material.ExportDirectory != NewText.ToString())
	{
		Settings->Material.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FMaterialSettingsViewModel::HandleUseGlobalPathCheckStateChanged(ECheckBoxState NewState)
{
	bool bNewState = (NewState == ECheckBoxState::Checked);
	if (Settings->Material.bUseGlobalMaterialPath != bNewState)
	{
		Settings->Material.bUseGlobalMaterialPath = bNewState;
		SaveSettings();
	}
}

FReply FMaterialSettingsViewModel::HandleBrowseGlobalMaterialPathClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseGlobalMatPathTitle", "Select Global Material Export Path").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Material.GlobalMaterialPath,
		                                         FolderName))
		{
			if (Settings->Material.GlobalMaterialPath != FolderName)
			{
				Settings->Material.GlobalMaterialPath = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}


void FMaterialSettingsViewModel::HandleGlobalMaterialPathTextCommitted(const FText& NewText,
                                                                       ETextCommit::Type CommitInfo)
{
	if (Settings->Material.GlobalMaterialPath != NewText.ToString())
	{
		Settings->Material.GlobalMaterialPath = NewText.ToString();
		SaveSettings();
	}
}

void FMaterialSettingsViewModel::SaveSettings()
{
	if (Settings) Settings->Save();
}
