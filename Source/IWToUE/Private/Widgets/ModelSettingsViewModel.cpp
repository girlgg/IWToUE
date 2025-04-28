#include "Widgets/ModelSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "IDesktopPlatform.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FModelSettingsViewModel::FModelSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
	check(Settings != nullptr);
	PopulateEnumOptions();
}

FText FModelSettingsViewModel::GetExportDirectory() const
{
	return FText::FromString(Settings->Model.ExportDirectory);
}

ECheckBoxState FModelSettingsViewModel::GetExportLoDsCheckState() const
{
	return Settings->Model.bExportLODs ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TOptional<int32> FModelSettingsViewModel::GetMaxLODLevel() const
{
	return Settings->Model.MaxLODLevel;
}

bool FModelSettingsViewModel::IsMaxLODLevelEnabled() const
{
	return Settings->Model.bExportLODs;
}

ECheckBoxState FModelSettingsViewModel::GetExportVertexColorCheckState() const
{
	return Settings->Model.bExportVertexColor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

TSharedPtr<ETextureExportPathMode> FModelSettingsViewModel::GetCurrentTexturePathMode() const
{
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
	return !Settings->Texture.bUseGlobalTexturePath;
}

TSharedPtr<EMaterialExportPathMode> FModelSettingsViewModel::GetCurrentMaterialPathMode() const
{
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
	return !Settings->Material.bUseGlobalMaterialPath;
}

FReply FModelSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseModelExportDirTitle", "Select Model Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (DesktopPlatform->
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
	if (Settings->Model.ExportDirectory != NewText.ToString())
	{
		Settings->Model.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleExportLODsCheckStateChanged(ECheckBoxState NewState)
{
	bool bNewState = (NewState == ECheckBoxState::Checked);
	if (Settings->Model.bExportLODs != bNewState)
	{
		Settings->Model.bExportLODs = bNewState;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleMaxLODLevelChanged(int32 NewValue)
{
	int32 ClampedValue = FMath::Clamp(NewValue, 0, 10); // Clamp between 0 and an arbitrary upper limit (e.g., 10)
	if (Settings->Model.MaxLODLevel != ClampedValue)
	{
		Settings->Model.MaxLODLevel = ClampedValue;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleExportVertexColorCheckStateChanged(ECheckBoxState NewState)
{
	bool bNewState = (NewState == ECheckBoxState::Checked);
	if (Settings->Model.bExportVertexColor != bNewState)
	{
		Settings->Model.bExportVertexColor = bNewState;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleTexturePathModeSelectionChanged(TSharedPtr<ETextureExportPathMode> NewSelection,
                                                                    ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid() && Settings->Model.TexturePathMode != *NewSelection)
	{
		Settings->Model.TexturePathMode = *NewSelection;
		SaveSettings();
	}
}

void FModelSettingsViewModel::HandleMaterialPathModeSelectionChanged(TSharedPtr<EMaterialExportPathMode> NewSelection,
                                                                     ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid() && Settings->Model.MaterialPathMode != *NewSelection)
	{
		Settings->Model.MaterialPathMode = *NewSelection;
		SaveSettings();
	}
}

void FModelSettingsViewModel::SaveSettings()
{
	if (Settings) Settings->Save();
}

void FModelSettingsViewModel::PopulateEnumOptions()
{
	UEnum* TextureEnum = StaticEnum<ETextureExportPathMode>();
	if (TextureEnum)
	{
		TexturePathModeOptionsSource.Empty(TextureEnum->NumEnums() - 1);
		for (int32 i = 0; i < TextureEnum->NumEnums() - 1; ++i) // Skip _MAX
		{
			TexturePathModeOptionsSource.Add(
				MakeShared<ETextureExportPathMode>((ETextureExportPathMode)TextureEnum->GetValueByIndex(i)));
		}
	}
	// Material Path Modes
	UEnum* MaterialEnum = StaticEnum<EMaterialExportPathMode>();
	if (MaterialEnum)
	{
		MaterialPathModeOptionsSource.Empty(MaterialEnum->NumEnums() - 1);
		for (int32 i = 0; i < MaterialEnum->NumEnums() - 1; ++i) // Skip _MAX
		{
			MaterialPathModeOptionsSource.Add(
				MakeShared<EMaterialExportPathMode>((EMaterialExportPathMode)MaterialEnum->GetValueByIndex(i)));
		}
	}
}
