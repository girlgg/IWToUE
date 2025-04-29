#include "Widgets/MaterialSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FMaterialSettingsViewModel::FMaterialSettingsViewModel()
{
}

FText FMaterialSettingsViewModel::GetExportDirectory() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Material.ExportDirectory);
}

ECheckBoxState FMaterialSettingsViewModel::GetUseGlobalPathCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->Material.bUseGlobalMaterialPath ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FMaterialSettingsViewModel::GetGlobalMaterialPath() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Material.GlobalMaterialPath);
}

bool FMaterialSettingsViewModel::IsGlobalMaterialPathEnabled() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->Material.bUseGlobalMaterialPath;
}

FReply FMaterialSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const FString Title = LOC_SETTINGS("BrowseMatExportDirTitle", "Select Default Material Export Directory").
			ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (FString FolderName; DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Material.ExportDirectory,
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
	UWraithSettings* Settings = GetSettings();
	if (Settings->Material.ExportDirectory != NewText.ToString())
	{
		Settings->Material.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FMaterialSettingsViewModel::HandleUseGlobalPathCheckStateChanged(ECheckBoxState NewState)
{
	UWraithSettings* Settings = GetSettings();
	const bool bNewState = (NewState == ECheckBoxState::Checked);
	if (Settings->Material.bUseGlobalMaterialPath != bNewState)
	{
		Settings->Material.bUseGlobalMaterialPath = bNewState;
		SaveSettings();
	}
}

FReply FMaterialSettingsViewModel::HandleBrowseGlobalMaterialPathClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
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
	UWraithSettings* Settings = GetSettings();
	if (Settings->Material.GlobalMaterialPath != NewText.ToString())
	{
		Settings->Material.GlobalMaterialPath = NewText.ToString();
		SaveSettings();
	}
}

UWraithSettings* FMaterialSettingsViewModel::GetSettings() const
{
	if (GEditor)
	{
		if (UWraithSettingsManager* SettingsManager = GEditor->GetEditorSubsystem<UWraithSettingsManager>())
		{
			return SettingsManager->GetSettingsMutable();
		}
	}
	UE_LOG(LogTemp, Error,
	       TEXT("FMaterialSettingsViewModel::GetSettings - Could not get Settings Manager or Settings Object!"));
	return nullptr;
}

void FMaterialSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings()) Settings->Save();
}
