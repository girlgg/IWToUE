#include "Widgets/SMaterialSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/MaterialSettingsViewModel.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMaterialSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FMaterialSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	const FLinearColor ImportantTextColor = FCoreStyle::Get().GetColor("ErrorReporting.WarningBackgroundColor");
	// Or a custom color
	const FSlateFontInfo ImportantFont = FAppStyle::GetFontStyle("BoldFont");

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[
			SNew(SBorder)
			             .BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
			             .Padding(FMargin(8.0f))
			             .BorderBackgroundColor(ImportantTextColor.CopyWithNewOpacity(0.6f))
			[
				SNew(STextBlock)
				                .Text(LOC_SETTINGS("MaterialImportantNote",
				                                   "Important: Textures referenced by materials might need to be exported alongside them. Check Model and Texture path settings to ensure textures are found correctly."))
				                .Font(ImportantFont)
				                .ColorAndOpacity(FSlateColor::UseForeground())
				                .AutoWrapText(true)
			]
		]

		// Path Configuration Category
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("MaterialPathConfigCategory", "Path Configuration"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("MaterialDefaultExportDirLabel", "Default Export Directory"),
						LOC_SETTINGS("MaterialDefaultExportDirTooltip",
						             "Directory for exported materials if not using global path and model paths don't override."),
						TAttribute<FText>(ViewModel.Get(), &FMaterialSettingsViewModel::GetExportDirectory),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FMaterialSettingsViewModel::HandleExportDirectoryTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FMaterialSettingsViewModel::HandleBrowseExportDirectoryClicked)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("MaterialUseGlobalPathLabel", "Use Global Material Path"),
						LOC_SETTINGS("MaterialUseGlobalPathTooltip",
						             "If checked, export all materials to the single path specified below, overriding Model settings."),
						SNew(SCheckBox)
						.IsChecked(ViewModel.Get(), &FMaterialSettingsViewModel::GetUseGlobalPathCheckState)
						.OnCheckStateChanged(ViewModel.Get(),
						                     &FMaterialSettingsViewModel::HandleUseGlobalPathCheckStateChanged)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("MaterialGlobalPathLabel", "Global Material Path"),
						LOC_SETTINGS("MaterialGlobalPathTooltip",
						             "The single directory used for all materials when 'Use Global Material Path' is enabled."),
						TAttribute<FText>(ViewModel.Get(), &FMaterialSettingsViewModel::GetGlobalMaterialPath),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FMaterialSettingsViewModel::HandleGlobalMaterialPathTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FMaterialSettingsViewModel::HandleBrowseGlobalMaterialPathClicked)
					)
					// ->IsEnabled(ViewModel.Get(), &FMaterialSettingsViewModel::IsGlobalMaterialPathEnabled)
				]
			)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("MaterialBottomDesc", "Configure where material assets are exported."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
