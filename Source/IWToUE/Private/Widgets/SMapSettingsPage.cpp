#include "Widgets/SMapSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/MapSettingsViewModel.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SMapSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FMapSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("MapTopDesc",
			                   "Configure settings for exporting map assets."))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("MapExportSettingsCategory", "Export Settings"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("MapExportDirLabel", "Export Directory"),
						LOC_SETTINGS("MapExportDirTooltip",
						             "Default directory for exported map assets."),
						TAttribute<FText>(
							ViewModel.Get(),
							&FMapSettingsViewModel::GetExportDirectory),
						FOnTextChanged(),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FMapSettingsViewModel::HandleExportDirectoryTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FMapSettingsViewModel::HandleBrowseExportDirectoryClicked)
					)
				]
			)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("MapBottomDesc", "Set the output location for map files."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
