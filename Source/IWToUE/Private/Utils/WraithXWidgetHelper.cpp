#include "Utils/WraithXWidgetHelper.h"

#include "Localization/IWToUELocalizationManager.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

TSharedRef<SWidget> WraithXWidgetHelper::CreateCategory(const FText& CategoryTitle,
                                                        TSharedRef<SVerticalBox> CategoryContent)
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
		.Padding(FMargin(10.0f, 5.0f))
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0, 0, 0, 5)
			[
				SNew(STextBlock)
				.Text(CategoryTitle)
				.Font(FAppStyle::GetFontStyle("BoldFont"))
				.TextStyle(FAppStyle::Get(), "LargeText")
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				CategoryContent
			]
		];
}

TSharedRef<SWidget> WraithXWidgetHelper::CreateSettingRow(const FText& SettingName, const FText& TooltipText,
                                                          TSharedRef<SWidget> SettingWidget)
{
	return SNew(SHorizontalBox)
		.ToolTipText(TooltipText)
		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(STextBlock)
			.Text(SettingName)
		]
		+ SHorizontalBox::Slot()
		.FillWidth(0.6f)
		.VAlign(VAlign_Center)
		[
			SettingWidget
		];
}

TSharedRef<SWidget> WraithXWidgetHelper::CreatePathSettingRow(const FText& SettingName, const FText& TooltipText,
                                                              const TAttribute<FText>& PathAttribute,
                                                              FOnTextChanged PathTextChangedHandler,
                                                              FOnTextCommitted PathTextCommittedHandler,
                                                              FOnClicked BrowseClickHandler)
{
	return SNew(SHorizontalBox)
		.ToolTipText(TooltipText)
		+ SHorizontalBox::Slot()
		.FillWidth(0.4f)
		.VAlign(VAlign_Center)
		.Padding(0, 0, 10, 0)
		[
			SNew(STextBlock).Text(SettingName)
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
				.Text(PathAttribute)
				.OnTextChanged(PathTextChangedHandler)
				.OnTextCommitted(PathTextCommittedHandler)
				.HintText(LOC_SETTINGS("PathHint", "Enter path or click browse..."))
			]
			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(FMargin(4.0f, 0.0f, 0.0f, 0.0f))
			.VAlign(VAlign_Center)
			[
				SNew(SButton)
				             .ButtonStyle(FAppStyle::Get(), "SimpleButton") // Less intrusive style for browse
				             .OnClicked(BrowseClickHandler)
				             .ContentPadding(FMargin(1, 0))
				             .ToolTipText(LOC_SETTINGS("BrowseButtonTooltip", "Browse for folder"))
				[
					SNew(SImage)
					            .Image(FAppStyle::Get().GetBrush("Icons.FolderOpen")) // Standard folder icon
					            .ColorAndOpacity(FSlateColor::UseForeground())
				]
			]
		];
}
