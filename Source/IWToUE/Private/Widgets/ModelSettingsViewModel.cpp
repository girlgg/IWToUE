#include "Widgets/ModelSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FModelSettingsViewModel::FModelSettingsViewModel()
{
	PopulateEnumOptions();
}

FText FModelSettingsViewModel::GetExportDirectory() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Model.ExportDirectory);
}

ECheckBoxState FModelSettingsViewModel::GetExportLoDsCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->Model.bExportLODs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TOptional<int32> FModelSettingsViewModel::GetMaxLODLevel() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->Model.MaxLODLevel;
}

bool FModelSettingsViewModel::IsMaxLODLevelEnabled() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->Model.bExportLODs;
}

ECheckBoxState FModelSettingsViewModel::GetExportVertexColorCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->Model.bExportVertexColor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TSharedPtr<ETextureExportPathMode> FModelSettingsViewModel::GetCurrentTexturePathMode() const
{
	const UWraithSettings* Settings = GetSettings();
	for (const auto& Option : TexturePathModeOptionsSource)
	{
		if (*Option == Settings->Model.TexturePathMode) return Option;
	}
	return TexturePathModeOptionsSource.Num() > 0 ? TexturePathModeOptionsSource[0] : nullptr;
}

TArray<TSharedPtr<ETextureExportPathMode>>* FModelSettingsViewModel::GetTexturePathModeOptions()
{
	return &TexturePathModeOptionsSource;
}

FText FModelSettingsViewModel::GetTexturePathModeDisplayName(ETextureExportPathMode Value) const
{
	return UWraithSettings::GetTextureExportPathModeDisplayName(Value);
}

bool FModelSettingsViewModel::IsTexturePathModeEnabled() const
{
	const UWraithSettings* Settings = GetSettings();
	return !Settings->Texture.bUseGlobalTexturePath;
}

TSharedPtr<EMaterialExportPathMode> FModelSettingsViewModel::GetCurrentMaterialPathMode() const
{
	const UWraithSettings* Settings = GetSettings();
	for (const auto& Option : MaterialPathModeOptionsSource)
	{
		if (*Option == Settings->Model.MaterialPathMode) return Option;
	}
	return MaterialPathModeOptionsSource.Num() > 0 ? MaterialPathModeOptionsSource[0] : nullptr;
}

TArray<TSharedPtr<EMaterialExportPathMode>>* FModelSettingsViewModel::GetMaterialPathModeOptions()
{
	return &MaterialPathModeOptionsSource;
}

FText FModelSettingsViewModel::GetMaterialPathModeDisplayName(EMaterialExportPathMode Value) const
{
	return UWraithSettings::GetMaterialExportPathModeDisplayName(Value);
}

bool FModelSettingsViewModel::IsMaterialPathModeEnabled() const
{
	UWraithSettings* Settings = GetSettings();
	return !Settings->Material.bUseGlobalMaterialPath;
}

FReply FModelSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const FString Title = LOC_SETTINGS("BrowseModelExportDirTitle", "Select Model Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (FString FolderName; DesktopPlatform->
			OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Model.ExportDirectory, FolderName))
		{
			if (Settings->Model.ExportDirectory != FolderName)
			{
				Settings->Model.ExportDirectory = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FModelSettingsViewModel::HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (UWraithSettings* Settings = GetSettings(); Settings->Model.ExportDirectory != NewText.ToString())
	{
		Settings->Model.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleExportLODsCheckStateChanged(ECheckBoxState NewState)
{
	const bool bNewState = NewState == ECheckBoxState::Checked;
	if (UWraithSettings* Settings = GetSettings(); Settings->Model.bExportLODs != bNewState)
	{
		Settings->Model.bExportLODs = bNewState;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleMaxLODLevelChanged(int32 NewValue)
{
	int32 ClampedValue = FMath::Clamp(NewValue, 0, 10);
	UWraithSettings* Settings = GetSettings();
	if (Settings->Model.MaxLODLevel != ClampedValue)
	{
		Settings->Model.MaxLODLevel = ClampedValue;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleExportVertexColorCheckStateChanged(ECheckBoxState NewState)
{
	const bool bNewState = NewState == ECheckBoxState::Checked;
	if (UWraithSettings* Settings = GetSettings(); Settings->Model.bExportVertexColor != bNewState)
	{
		Settings->Model.bExportVertexColor = bNewState;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleTexturePathModeSelectionChanged(TSharedPtr<ETextureExportPathMode> NewSelection,
                                                                    ESelectInfo::Type SelectInfo)
{
	if (UWraithSettings* Settings = GetSettings(); NewSelection.IsValid() && Settings->Model.TexturePathMode != *
		NewSelection)
	{
		Settings->Model.TexturePathMode = *NewSelection;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleMaterialPathModeSelectionChanged(TSharedPtr<EMaterialExportPathMode> NewSelection,
                                                                     ESelectInfo::Type SelectInfo)
{
	if (UWraithSettings* Settings = GetSettings(); NewSelection.IsValid() && Settings->Model.MaterialPathMode != *
		NewSelection)
	{
		Settings->Model.MaterialPathMode = *NewSelection;
		SaveSettings();
	}
}

UWraithSettings* FModelSettingsViewModel::GetSettings() const
{
	if (GEditor)
	{
		if (UWraithSettingsManager* SettingsManager = GEditor->GetEditorSubsystem<UWraithSettingsManager>())
		{
			return SettingsManager->GetSettingsMutable();
		}
	}
	UE_LOG(LogTemp, Error,
	       TEXT("FModelSettingsViewModel::GetSettings - Could not get Settings Manager or Settings Object!"));
	return nullptr;
}

void FModelSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings()) Settings->Save();
}

void FModelSettingsViewModel::PopulateEnumOptions()
{
	if (UEnum* TextureEnum = StaticEnum<ETextureExportPathMode>())
	{
		TexturePathModeOptionsSource.Empty(TextureEnum->NumEnums() - 1);
		for (int32 i = 0; i < TextureEnum->NumEnums() - 1; ++i) // Skip _MAX
		{
			TexturePathModeOptionsSource.Add(
				MakeShared<ETextureExportPathMode>(
					static_cast<ETextureExportPathMode>(TextureEnum->GetValueByIndex(i))));
		}
	}
	if (UEnum* MaterialEnum = StaticEnum<EMaterialExportPathMode>())
	{
		MaterialPathModeOptionsSource.Empty(MaterialEnum->NumEnums() - 1);
		for (int32 i = 0; i < MaterialEnum->NumEnums() - 1; ++i) // Skip _MAX
		{
			MaterialPathModeOptionsSource.Add(
				MakeShared<EMaterialExportPathMode>(
					static_cast<EMaterialExportPathMode>(MaterialEnum->GetValueByIndex(i))));
		}
	}
}
