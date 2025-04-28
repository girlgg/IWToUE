#include "Widgets/AnimationSettingsViewModel.h"

#include "ContentBrowserDelegates.h"
#include "ContentBrowserModule.h"
#include "DesktopPlatformModule.h"
#include "IContentBrowserSingleton.h"
#include "Localization/IWToUELocalizationManager.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

FAnimationSettingsViewModel::FAnimationSettingsViewModel(UWraithSettings* InSettings)
	: Settings(InSettings)
{
	check(Settings != nullptr);
}

FText FAnimationSettingsViewModel::GetExportDirectory() const
{
	return FText::FromString(Settings->Animation.ExportDirectory);
}

FText FAnimationSettingsViewModel::GetTargetSkeletonPath() const
{
	return FText::FromString(Settings->Animation.TargetSkeletonPath);
}

FReply FAnimationSettingsViewModel::HandleBrowseExportDirectoryClicked()
{
	IDesktopPlatform* DesktopPlatform = FDesktopPlatformModule::Get();
	if (DesktopPlatform)
	{
		FString FolderName;
		const FString Title = LOC_SETTINGS("BrowseAnimExportDirTitle", "Select Animation Export Directory").ToString();
		const void* ParentWindowHandle = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(nullptr);
		if (DesktopPlatform->OpenDirectoryDialog(ParentWindowHandle, Title, Settings->Animation.ExportDirectory,
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
	if (Settings->Animation.TargetSkeletonPath != NewText.ToString())
	{
		Settings->Animation.TargetSkeletonPath = NewText.ToString();
		SaveSettings();
	}
}

void FAnimationSettingsViewModel::SaveSettings()
{
	if (Settings) Settings->Save();
}
