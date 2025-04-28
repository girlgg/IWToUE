#include "Widgets/GeneralSettingsViewModel.h"

#include "DesktopPlatformModule.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FGeneralSettingsViewModel::FGeneralSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
	check(Settings != nullptr);

	UEnum* GameTypeEnum = StaticEnum<EGameType>();
	if (GameTypeEnum)
	{
		for (int32 i = 0; i < GameTypeEnum->NumEnums() - 1; ++i)
		{
			GameTypeOptionsSource.Add(MakeShared<EGameType>((EGameType)GameTypeEnum->GetValueByIndex(i)));
		}
	}
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadModelsCheckState() const
{
	return Settings->General.bLoadModels ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadAudioCheckState() const
{
	return Settings->General.bLoadAudio ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadMaterialsCheckState() const
{
	return Settings->General.bLoadMaterials ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadAnimationsCheckState() const
{
	return Settings->General.bLoadAnimations ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadTexturesCheckState() const
{
	return Settings->General.bLoadTextures ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

ECheckBoxState FGeneralSettingsViewModel::GetLoadMapsCheckState() const
{
	return Settings->General.bLoadMaps ? ECheckBoxState::Checked : ECheckBoxState::Unchecked;
}

FText FGeneralSettingsViewModel::GetCordycepPath() const
{
	return FText::FromString(Settings->General.CordycepToolPath);
}

FText FGeneralSettingsViewModel::GetGamePath() const
{
	return FText::FromString(Settings->General.GamePath);
}

TSharedPtr<EGameType> FGeneralSettingsViewModel::GetCurrentGameType() const
{
	for (const auto& Option : GameTypeOptionsSource)
	{
		if (*Option == Settings->General.GameType)
		{
			return Option;
		}
	}
	return GameTypeOptionsSource.Num() > 0 ? GameTypeOptionsSource[0] : nullptr;
}

TArray<TSharedPtr<EGameType>>* FGeneralSettingsViewModel::GetGameTypeOptions()
{
	return &GameTypeOptionsSource;
}

FText FGeneralSettingsViewModel::GetGameTypeDisplayName(EGameType Value) const
{
	UEnum* Enum = StaticEnum<EGameType>();
	if (Enum)
	{
		return Enum->GetDisplayNameTextByValue(static_cast<int64>(Value));
	}
	return LOC_SETTINGS("GameType_Unknown", "Unknown Type"); // Fallback localization
}

void FGeneralSettingsViewModel::HandleLoadModelsCheckStateChanged(ECheckBoxState NewState)
{
	Settings->General.bLoadModels = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleLoadAudioCheckStateChanged(ECheckBoxState NewState)
{
	Settings->General.bLoadAudio = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleLoadMaterialsCheckStateChanged(ECheckBoxState NewState)
{
	Settings->General.bLoadMaterials = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleLoadAnimationsCheckStateChanged(ECheckBoxState NewState)
{
	Settings->General.bLoadAnimations = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleLoadTexturesCheckStateChanged(ECheckBoxState NewState)
{
	Settings->General.bLoadTextures = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleLoadMapsCheckStateChanged(ECheckBoxState NewState)
{
	Settings->General.bLoadMaps = (NewState == ECheckBoxState::Checked);
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleCordycepPathTextChanged(const FText& NewText)
{
}

void FGeneralSettingsViewModel::HandleCordycepPathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	Settings->General.CordycepToolPath = NewText.ToString();
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleGamePathTextChanged(const FText& NewText)
{
}

void FGeneralSettingsViewModel::HandleGamePathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	Settings->General.GamePath = NewText.ToString();
	SaveSettings();
}

void FGeneralSettingsViewModel::HandleGameTypeSelectionChanged(TSharedPtr<EGameType> NewSelection,
                                                               ESelectInfo::Type SelectInfo)
{
	if (NewSelection.IsValid())
	{
		Settings->General.GameType = *NewSelection;
		SaveSettings();
	}
}

void FGeneralSettingsViewModel::HandleLaunchParametersTextChanged(const FText& NewText)
{
}

void FGeneralSettingsViewModel::HandleLaunchParametersTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	Settings->General.LaunchParameters = NewText.ToString();
	SaveSettings();
}

FReply FGeneralSettingsViewModel::HandleBrowseCordycepPathClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseCordycepTitle", "Select Cordycep Tool Folder").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->General.CordycepToolPath,
		                                         FolderName))
		{
			Settings->General.CordycepToolPath = FolderName;
			SaveSettings();
		}
	}
	return FReply::Handled();
}

FReply FGeneralSettingsViewModel::HandleBrowseGamePathClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseGamePathTitle", "Select Game Folder").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);

		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->General.GamePath, FolderName))
		{
			Settings->General.GamePath = FolderName;
			SaveSettings();
		}
	}
	return FReply::Handled();
}

FText FGeneralSettingsViewModel::GetLaunchParameters() const
{
	return FText::FromString(Settings->General.LaunchParameters);
}

void FGeneralSettingsViewModel::SaveSettings()
{
	if (Settings)
	{
		Settings->Save();
	}
}
