#include "Widgets/SAudioSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/AudioSettingsViewModel.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAudioSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FAudioSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("AudioTopDesc",
			                   "Configure settings for exporting audio assets (Sound Waves, Cues etc.)."))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("AudioExportSettingsCategory", "Export Settings"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("AudioExportDirLabel", "Export Directory"),
						LOC_SETTINGS("AudioExportDirTooltip",
						             "Default directory for exported audio assets."),
						TAttribute<FText>(
							ViewModel.Get(),
							&FAudioSettingsViewModel::GetExportDirectory),
						FOnTextChanged(),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FAudioSettingsViewModel::HandleExportDirectoryTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FAudioSettingsViewModel::HandleBrowseExportDirectoryClicked)
					)
				]
			)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("AudioBottomDesc", "Set the output location for audio files."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
