#include "Widgets/TextureSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"


#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FTextureSettingsViewModel::FTextureSettingsViewModel()
{
}

void FTextureSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings()) Settings->Save();
}

FText FTextureSettingsViewModel::GetExportDirectory() const
{
	UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Texture.ExportDirectory);
}

ECheckBoxState FTextureSettingsViewModel::GetUseGlobalPathCheckState() const
{
	UWraithSettings* Settings = GetSettings();
	return Settings->Texture.bUseGlobalTexturePath ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FTextureSettingsViewModel::GetGlobalTexturePath() const
{
	UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Texture.GlobalTexturePath);
}

bool FTextureSettingsViewModel::IsGlobalTexturePathEnabled() const
{
	UWraithSettings* Settings = GetSettings();
	return Settings->Texture.bUseGlobalTexturePath;
}

FReply FTextureSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const FString Title = LOC_SETTINGS("BrowseTexExportDirTitle", "Select Default Texture Export Directory").
			ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (FString FolderName; DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Texture.ExportDirectory,
		                                                             FolderName))
		{
			if (Settings->Texture.ExportDirectory != FolderName)
			{
				Settings->Texture.ExportDirectory = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FTextureSettingsViewModel::HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	UWraithSettings* Settings = GetSettings();
	if (Settings->Texture.ExportDirectory != NewText.ToString())
	{
		Settings->Texture.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FTextureSettingsViewModel::HandleUseGlobalPathCheckStateChanged(ECheckBoxState NewState)
{
	UWraithSettings* Settings = GetSettings();
	if (const bool bNewState = NewState == ECheckBoxState::Checked; Settings->Texture.bUseGlobalTexturePath != bNewState)
	{
		Settings->Texture.bUseGlobalTexturePath = bNewState;
		SaveSettings();
	}
}

FReply FTextureSettingsViewModel::HandleBrowseGlobalTexturePathClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseGlobalTexPathTitle", "Select Global Texture Export Path").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Texture.GlobalTexturePath,
		                                         FolderName))
		{
			if (Settings->Texture.GlobalTexturePath != FolderName)
			{
				Settings->Texture.GlobalTexturePath = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FTextureSettingsViewModel::HandleGlobalTexturePathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	UWraithSettings* Settings = GetSettings();
	if (Settings->Texture.GlobalTexturePath != NewText.ToString())
	{
		Settings->Texture.GlobalTexturePath = NewText.ToString();
		SaveSettings();
	}
}

UWraithSettings* FTextureSettingsViewModel::GetSettings() const
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
