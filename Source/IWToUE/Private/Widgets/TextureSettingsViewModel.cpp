#include "Widgets/TextureSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"


#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FTextureSettingsViewModel::FTextureSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
	check(Settings != nullptr);
}

void FTextureSettingsViewModel::SaveSettings()
{
	if (Settings) Settings->Save();
}

FText FTextureSettingsViewModel::GetExportDirectory() const
{
	return FText::FromString(Settings->Texture.ExportDirectory);
}

ECheckBoxState FTextureSettingsViewModel::GetUseGlobalPathCheckState() const
{
	return Settings->Texture.bUseGlobalTexturePath ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FTextureSettingsViewModel::GetGlobalTexturePath() const
{
	return FText::FromString(Settings->Texture.GlobalTexturePath);
}

bool FTextureSettingsViewModel::IsGlobalTexturePathEnabled() const { return Settings->Texture.bUseGlobalTexturePath; }

// --- Handlers --- (Implementation is almost identical to Material ViewModel handlers, just operating on Settings->Texture)
FReply FTextureSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseTexExportDirTitle", "Select Default Texture Export Directory").
			ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Texture.ExportDirectory,
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
	if (Settings->Texture.ExportDirectory != NewText.ToString())
	{
		Settings->Texture.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FTextureSettingsViewModel::HandleUseGlobalPathCheckStateChanged(ECheckBoxState NewState)
{
	bool bNewState = (NewState == ECheckBoxState::Checked);
	if (Settings->Texture.bUseGlobalTexturePath != bNewState)
	{
		Settings->Texture.bUseGlobalTexturePath = bNewState;
		SaveSettings();
		// Note: The Model Settings page's combobox IsEnabled binding will automatically update.
	}
}

FReply FTextureSettingsViewModel::HandleBrowseGlobalTexturePathClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
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
	if (Settings->Texture.GlobalTexturePath != NewText.ToString())
	{
		Settings->Texture.GlobalTexturePath = NewText.ToString();
		SaveSettings();
	}
}
