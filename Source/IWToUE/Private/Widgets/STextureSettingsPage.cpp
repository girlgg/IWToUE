#include "Widgets/STextureSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/TextureSettingsViewModel.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void STextureSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FTextureSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("TextureTopDesc", "Configure settings for exporting texture (image) assets."))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("TexturePathConfigCategory", "Path Configuration"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("TextureDefaultExportDirLabel", "Default Export Directory"),
						LOC_SETTINGS("TextureDefaultExportDirTooltip",
						             "Directory for exported textures if not using global path and model paths don't override."),
						TAttribute<FText>(ViewModel.Get(), &FTextureSettingsViewModel::GetExportDirectory),
						FOnTextChanged(),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FTextureSettingsViewModel::HandleExportDirectoryTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FTextureSettingsViewModel::HandleBrowseExportDirectoryClicked)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("TextureUseGlobalPathLabel", "Use Global Texture Path"),
						LOC_SETTINGS("TextureUseGlobalPathTooltip",
						             "If checked, export all textures to the single path specified below, overriding Model settings."),
						SNew(SCheckBox)
						.IsChecked(ViewModel.Get(), &FTextureSettingsViewModel::GetUseGlobalPathCheckState)
						.OnCheckStateChanged(ViewModel.Get(),
						                     &FTextureSettingsViewModel::HandleUseGlobalPathCheckStateChanged)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("TextureGlobalPathLabel", "Global Texture Path"),
						LOC_SETTINGS("TextureGlobalPathTooltip",
						             "The single directory used for all textures when 'Use Global Texture Path' is enabled."),
						TAttribute<FText>(ViewModel.Get(), &FTextureSettingsViewModel::GetGlobalTexturePath),
						FOnTextChanged(),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FTextureSettingsViewModel::HandleGlobalTexturePathTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FTextureSettingsViewModel::HandleBrowseGlobalTexturePathClicked)
					)
					// .IsEnabled(ViewModel.Get(), &FTextureSettingsViewModel::IsGlobalTexturePathEnabled)
				]
			)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("TextureBottomDesc", "Configure where texture assets are exported."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
