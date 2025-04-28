#include "Widgets/SAnimationSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/AnimationSettingsViewModel.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SAnimationSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FAnimationSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("AnimTopDesc", "Configure settings for exporting animation sequences."))
			.AutoWrapText(true)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("AnimExportSettingsCategory", "Export Settings"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("AnimExportDirLabel", "Export Directory"),
						LOC_SETTINGS("AnimExportDirTooltip", "Default directory for exported animations."),
						TAttribute<FText>(ViewModel.Get(), &FAnimationSettingsViewModel::GetExportDirectory),
						FOnTextChanged(),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FAnimationSettingsViewModel::HandleExportDirectoryTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FAnimationSettingsViewModel::HandleBrowseExportDirectoryClicked)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SHorizontalBox)
					.ToolTipText(LOC_SETTINGS("AnimTargetSkeletonTooltip",
					                          "Optional: Path to the Skeleton asset in your project to target animations to. Click browse to pick from Content Browser."))
					+ SHorizontalBox::Slot()
					.FillWidth(0.4f)
					.VAlign(VAlign_Center)
					.Padding(0, 0, 10, 0)
					[
						SNew(STextBlock).Text(LOC_SETTINGS("AnimTargetSkeletonLabel", "Target Skeleton"))
					]
					+ SHorizontalBox::Slot()
					.FillWidth(0.6f)
					.VAlign(VAlign_Center)
					[
						SNew(SHorizontalBox)
						+ SHorizontalBox::Slot()
						.FillWidth(1.0f)
						[
							SNew(SEditableTextBox)
							.Text(ViewModel.Get(), &FAnimationSettingsViewModel::GetTargetSkeletonPath)
							.OnTextCommitted(ViewModel.Get(),
							                 &FAnimationSettingsViewModel::HandleTargetSkeletonTextCommitted)
							.HintText(LOC_SETTINGS("AnimTargetSkeletonHint",
							                       "Optional: /Game/Path/To/Skeleton.Skeleton"))
						]
						+ SHorizontalBox::Slot()
						.AutoWidth()
						.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
						.VAlign(VAlign_Center)
						[
							SNew(SButton)
							.ButtonStyle(FAppStyle::Get(), "SimpleButton")
							.OnClicked(ViewModel.Get(),
							           &FAnimationSettingsViewModel::HandleBrowseTargetSkeletonClicked)
							.ContentPadding(FMargin(1, 0))
							.ToolTipText(LOC_SETTINGS("BrowseAssetButtonTooltip", "Browse for asset"))
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Search"))
								.ColorAndOpacity(FSlateColor::UseForeground())
							]
						]
					]
				]
			)
		]

		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("AnimBottomDesc",
			                   "Set the output location and optional retargeting skeleton for animations."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
