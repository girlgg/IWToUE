#include "Widgets/AnimationSettingsViewModel.h"

#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "IContentBrowserSingleton.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FAnimationSettingsViewModel::FAnimationSettingsViewModel()
{
}

FText FAnimationSettingsViewModel::GetExportDirectory() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Animation.ExportDirectory);
}

FText FAnimationSettingsViewModel::GetTargetSkeletonPath() const
{
	const UWraithSettings* Settings = GetSettings();
	return FText::FromString(Settings->Animation.TargetSkeletonPath);
}

FReply FAnimationSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	UWraithSettings* Settings = GetSettings();
	if (IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get())
	{
		const FString Title = LOC_SETTINGS("BrowseAnimExportDirTitle", "Select Animation Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (FString FolderName; DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title,
		                                                             Settings->Animation.ExportDirectory,
		                                                             FolderName))
		{
			if (Settings->Animation.ExportDirectory != FolderName)
			{
				Settings->Animation.ExportDirectory = FolderName;
				SaveSettings();
			}
		}
	}
	return FReply::Handled();
}

void FAnimationSettingsViewModel::HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	UWraithSettings* Settings = GetSettings();
	if (Settings->Animation.ExportDirectory != NewText.ToString())
	{
		Settings->Animation.ExportDirectory = NewText.ToString();
		SaveSettings();
	}
}

FReply FAnimationSettingsViewModel::HandleBrowseTargetSkeletonClicked()
{
	FContentBrowserModule& ContentBrowserModule = FModuleManager::LoadModuleChecked<FContentBrowserModule>(
		"ContentBrowser");
	FAssetPickerConfig AssetPickerConfig;
	AssetPickerConfig.Filter.ClassPaths.Add(USkeleton::StaticClass()->GetClassPathName());
	AssetPickerConfig.Filter.bRecursiveClasses = true;
	AssetPickerConfig.InitialAssetViewType = EAssetViewType::List;
	AssetPickerConfig.bAllowNullSelection = true;
	AssetPickerConfig.OnAssetSelected = FOnAssetSelected::CreateLambda([this](const FAssetData& AssetData)
	{
		UWraithSettings* Settings = GetSettings();
		FString NewPath = AssetData.IsValid() ? AssetData.GetObjectPathString() : TEXT("");
		if (Settings->Animation.TargetSkeletonPath != NewPath)
		{
			Settings->Animation.TargetSkeletonPath = NewPath;
			SaveSettings();
			FSlateApplication::Get().DismissAllMenus();
		}
	});
	AssetPickerConfig.OnAssetEnterPressed = FOnAssetEnterPressed::CreateLambda(
		[this](const TArray<FAssetData>& SelectedAssets)
		{
			UWraithSettings* Settings = GetSettings();
			if (SelectedAssets.Num() > 0)
			{
				FString NewPath = SelectedAssets[0].IsValid() ? SelectedAssets[0].GetObjectPathString() : TEXT("");
				if (Settings->Animation.TargetSkeletonPath != NewPath)
				{
					Settings->Animation.TargetSkeletonPath = NewPath;
					SaveSettings();
				}
			}
			FSlateApplication::Get().DismissAllMenus();
		});
	UWraithSettings* Settings = GetSettings();
	if (!Settings->Animation.TargetSkeletonPath.IsEmpty())
	{
		AssetPickerConfig.InitialAssetSelection = FAssetData(
			FindObject<USkeleton>(nullptr, *Settings->Animation.TargetSkeletonPath));
	}


	TSharedRef<SWidget> PickerWidget = ContentBrowserModule.Get().CreateAssetPicker(AssetPickerConfig);

	FSlateApplication::Get().PushMenu(
		FSlateApplication::Get().GetActiveTopLevelWindow().ToSharedRef(),
		FWidgetPath(),
		PickerWidget,
		FSlateApplication::Get().GetCursorPos(),
		FPopupTransitionEffect(FPopupTransitionEffect::ContextMenu)
	);


	return FReply::Handled();
}

void FAnimationSettingsViewModel::HandleTargetSkeletonTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo)
{
	UWraithSettings* Settings = GetSettings();
	if (Settings->Animation.TargetSkeletonPath != NewText.ToString())
	{
		Settings->Animation.TargetSkeletonPath = NewText.ToString();
		SaveSettings();
	}
}

UWraithSettings* FAnimationSettingsViewModel::GetSettings() const
{
	if (GEditor)
	{
		if (UWraithSettingsManager* SettingsManager = GEditor->GetEditorSubsystem<UWraithSettingsManager>())
		{
			return SettingsManager->GetSettingsMutable();
		}
	}
	UE_LOG(LogTemp, Error,
	       TEXT("FAnimationSettingsViewModel::GetSettings - Could not get Settings Manager or Settings Object!"));
	return nullptr;
}

void FAnimationSettingsViewModel::SaveSettings()
{
	if (UWraithSettings* Settings = GetSettings()) Settings->Save();
}
