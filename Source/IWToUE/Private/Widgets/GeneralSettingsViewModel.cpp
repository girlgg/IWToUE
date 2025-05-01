#include "Widgets/GeneralSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FGeneralSettingsViewModel::FGeneralSettingsViewModel()
{
	if (const UEnum* GameTypeEnum = StaticEnum<EGameType>())
	{
		for (int32 i = 0; i < GameTypeEnum->NumEnums() - 1; ++i)
		{
			GameTypeOptionsSource.Add(MakeShared<EGameType>((EGameType)GameTypeEnum->GetValueByIndex(i)));
		}
	}
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadModelsCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings
		       ? (Settings->General.bLoadModels ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		       : ECheckBoxState::Undetermined;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadAudioCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings
		       ? (Settings->General.bLoadAudio ? ECheckBoxState::Checked : ECheckBoxState::Unchecked)
		       : ECheckBoxState::Undetermined;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadMaterialsCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->General.bLoadMaterials ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadAnimationsCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->General.bLoadAnimations ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadTexturesCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->General.bLoadTextures ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadMapsCheckState() const
{
	const UWraithSettings* Settings = GetSettings();
	return Settings->General.bLoadMaps ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FGeneralSettingsViewModel::GetCordycepPath() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->General.CordycepToolPath);
}

FText FGeneralSettingsViewModel::GetGamePath() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->General.GamePath);
}

TSharedPtr<EGameType> FGeneralSettingsViewModel::GetCurrentGameType() const
{
	const UWraithSettings* Settings = GetSettings();
	if (!Settings) return GameTypeOptionsSource.Num() > 0 ? GameTypeOptionsSource[0] : nullptr;

	for (const auto& Option : GameTypeOptionsSource)
	{
		if (*Option == Settings->General.GameType) return Option;
	}
	return GameTypeOptionsSource.Num() > 0 ? GameTypeOptionsSource[0] : nullptr;
}

TArray<TSharedPtr<EGameType>>* FGeneralSettingsViewModel::GetGameTypeOptions()
{
	return &GameTypeOptionsSource;
}

FText FGeneralSettingsViewModel::GetGameTypeDisplayName(EGameType Value) const
{
	return UWraithSettings::GetGameTypeDisplayName(Value);
}

FText FGeneralSettingsViewModel::GetLaunchParameters() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->General.LaunchParameters);
}

void FGeneralSettingsViewModel::HandleLoadModelsCheckStateChanged(ECheckBoxState NewState)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.bLoadModels = (NewState == ECheckBoxState::Checked);
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleLoadAudioCheckStateChanged(ECheckBoxState NewState)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.bLoadAudio = (NewState == ECheckBoxState::Checked);
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleLoadMaterialsCheckStateChanged(ECheckBoxState NewState)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.bLoadMaterials = (NewState == ECheckBoxState::Checked);
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleLoadAnimationsCheckStateChanged(ECheckBoxState NewState)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.bLoadAnimations = (NewState == ECheckBoxState::Checked);
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleLoadTexturesCheckStateChanged(ECheckBoxState NewState)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.bLoadTextures = (NewState == ECheckBoxState::Checked);
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleLoadMapsCheckStateChanged(ECheckBoxState NewState)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.bLoadMaps = (NewState == ECheckBoxState::Checked);
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleCordycepPathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.CordycepToolPath = NewText.ToString();
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleGamePathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.GamePath = NewText.ToString();
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleGameTypeSelectionChanged(TSharedPtr<EGameType> NewSelection,
                                                               ESelectInfo::Type SelectInfo)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		if (NewSelection.IsValid())
		{
			Settings->General.GameType = *NewSelection;
			SaveSettings();
		}
	}
}

void FGeneralSettingsViewModel::HandleLaunchParametersTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->General.LaunchParameters = NewText.ToString();
		SaveSettings();
	}
}

FReply FGeneralSettingsViewModel::HandleBrowseCordycepPathClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (!Settings) return FReply::Handled();

	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseCordycepTitle", "Select Cordycep Tool Folder").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->General.CordycepToolPath,
		                                         FolderName))
		{
			if (Settings->General.CordycepToolPath != FolderName)
			{
				Settings->General.CordycepToolPath = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

FReply FGeneralSettingsViewModel::HandleBrowseGamePathClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (!Settings) return FReply::Handled();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const FString Title = LOC_SETTINGS("BrowseGamePathTitle", "Select Game Folder").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (FString FolderName;
			DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->General.GamePath, FolderName))
		{
			if (Settings->General.GamePath != FolderName)
			{
				Settings->General.GamePath = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

UWraithSettings* FGeneralSettingsViewModel::GetSettings() const
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

void FGeneralSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings())
	{
		Settings->Save();
	}
	else
	{
		UE_LOG(LogTemp, Error, TEXT("FGeneralSettingsViewModel::SaveSettings - Cannot save, Settings object is null."));
	}
}
