#include "WraithX/SWraithXWidget.h"

#include "Framework/Notifications/NotificationManager.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Widgets/Input/SSearchBox.h"
#include "Widgets/Layout/SUniformGridPanel.h"
#include "Widgets/Notifications/SNotificationList.h"
#include "Widgets/Notifications/SProgressBar.h"
#include "WraithX/SSettingsDialog.h"
#include "WraithX/WraithXViewModel.h"

void SWraithXWidget::Construct(const FArguments& InArgs)
{
	// --- Initialize ViewModel ---
	ViewModel->Initialize(AssetImportManager);

	// --- Bind View to ViewModel Delegates ---
	ViewModel->OnListChangedDelegate.BindSP(this, &SWraithXWidget::HandleListChanged);
	ViewModel->OnAssetCountChangedDelegate.BindSP(this, &SWraithXWidget::HandleAssetCountChanged);
	ViewModel->OnLoadingStateChangedDelegate.BindSP(this, &SWraithXWidget::HandleLoadingStateChanged);
	ViewModel->OnImportPathChangedDelegate.BindSP(this, &SWraithXWidget::HandleImportPathChanged);
	ViewModel->OnShowNotificationDelegate.BindSP(this, &SWraithXWidget::HandleShowNotification);
	ViewModel->OnPreviewAssetDelegate.BindSP(this, &SWraithXWidget::HandlePreviewAsset);

	ChildSlot
	[
		SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0.0f, 4.0f)
		[
			CreateTopToolbar()
		]

		+ SVerticalBox::Slot()
		.FillHeight(0.8f)
		.Padding(4.0f, 8.0f)
		[
			CreateMainArea()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.0f, 8.0f)
		[
			CreateBottomPanel()
		]

		+ SVerticalBox::Slot()
		.AutoHeight()
		[
			CreateStatusBar()
		]
	];

	HandleAssetCountChanged(ViewModel->GetTotalAssetCount(), ViewModel->GetFilteredAssetCount());
	HandleImportPathChanged(ViewModel->GetImportPath());
}

SWraithXWidget::~SWraithXWidget()
{
	if (ViewModel.IsValid())
	{
		ViewModel->OnListChangedDelegate.Unbind();
		ViewModel->OnAssetCountChangedDelegate.Unbind();
		ViewModel->OnLoadingStateChangedDelegate.Unbind();
		ViewModel->OnImportPathChangedDelegate.Unbind();
		ViewModel->OnShowNotificationDelegate.Unbind();
		ViewModel->OnPreviewAssetDelegate.Unbind();
	}
}

TSharedRef<ITableRow> SWraithXWidget::GenerateListRow(TSharedPtr<FCoDAsset> Item,
                                                      const TSharedRef<STableViewBase>& OwnerTable)
{
	return SNew(STableRow<TSharedPtr<FCoDAsset>>, OwnerTable)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(0.4f)
			[
				SNew(STextBlock).Text(FText::FromString(Item->AssetName))
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			[
				SNew(STextBlock).Text(Item->GetAssetStatusText())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			[
				SNew(STextBlock).Text(Item->GetAssetTypeText())
			]
			+ SHorizontalBox::Slot()
			.FillWidth(0.2f)
			[
				SNew(STextBlock).Text(FText::AsNumber(Item->AssetSize))
			]
		];
}

void SWraithXWidget::OnSortColumnChanged(const EColumnSortPriority::Type /*SortPriority*/, const FName& ColumnId,
                                         const EColumnSortMode::Type /*NewSortMode*/)
{
	if (ViewModel.IsValid())
	{
		ViewModel->SetSortColumn(ColumnId);
	}
}

EColumnSortMode::Type SWraithXWidget::GetSortMode(const FName ColumnId) const
{
	if (ViewModel.IsValid())
	{
		return ViewModel->GetSortModeForColumn(ColumnId);
	}
	return EColumnSortMode::None;
}

void SWraithXWidget::SortData()
{
	if (CurrentSortColumn == FName("AssetName"))
	{
		FilteredItems.Sort([this](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
		{
			return CurrentSortMode == EColumnSortMode::Ascending
				       ? A->AssetName.Compare(B->AssetName) < 0
				       : A->AssetName.Compare(B->AssetName) > 0;
		});
	}
	else if (CurrentSortColumn == FName("Status"))
	{
		FilteredItems.Sort([this](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
		{
			return CurrentSortMode == EColumnSortMode::Ascending
				       ? A->GetAssetStatusText().CompareTo(B->GetAssetStatusText()) < 0
				       : A->GetAssetStatusText().CompareTo(B->GetAssetStatusText()) > 0;
		});
	}
	else if (CurrentSortColumn == FName("Type"))
	{
		FilteredItems.Sort([this](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
		{
			return CurrentSortMode == EColumnSortMode::Ascending
				       ? A->GetAssetTypeText().CompareTo(B->GetAssetTypeText()) < 0
				       : A->GetAssetTypeText().CompareTo(B->GetAssetTypeText()) > 0;
		});
	}
	else if (CurrentSortColumn == FName("Size"))
	{
		FilteredItems.Sort([this](const TSharedPtr<FCoDAsset>& A, const TSharedPtr<FCoDAsset>& B)
		{
			return CurrentSortMode == EColumnSortMode::Ascending
				       ? A->AssetSize < B->AssetSize
				       : A->AssetSize > B->AssetSize;
		});
	}
}

TSharedRef<SWidget> SWraithXWidget::CreateTopToolbar()
{
	const FSlateBrush* ToolbarBg = FAppStyle::GetBrush("ToolPanel.GroupBorder");
	const FButtonStyle& ToolbarButton = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("FlatButton.Default");

	return SNew(SBorder)
		.BorderImage(ToolbarBg)
		.Padding(FMargin(8.0f, 4.0f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			[
				SAssignNew(SearchBox, SSearchBox)
				.HintText(FIWToUELocalizationManager::Get().GetText("SearchBoxHint"))
				.OnTextChanged(this, &SWraithXWidget::HandleSearchTextChanged)
				.OnTextCommitted(this, &SWraithXWidget::HandleSearchTextCommitted)
				.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.Padding(4.0f, 0.0f)
			[
				SNew(SButton)
				.ButtonStyle(&ToolbarButton)
				.OnClicked(this, &SWraithXWidget::HandleClearSearchClicked)
				.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
				.ToolTipText(FIWToUELocalizationManager::Get().GetText("ClearBoxHint"))
				.ContentPadding(FMargin(2.0f))
				[
					SNew(SImage).Image(FAppStyle::GetBrush("Cross"))
				]
			]

			+ SHorizontalBox::Slot()
			.FillWidth(0.1f)
			[
				SNew(SSpacer)
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			.HAlign(HAlign_Right)
			.Padding(8.f, 0.f)
			[
				SAssignNew(AssetCountText, STextBlock)
				.TextStyle(FAppStyle::Get(), "SmallText")
				.Text(TAttribute<FText>::CreateSP(this, &SWraithXWidget::GetAssetCountText))
			]

			+ SHorizontalBox::Slot()
			.AutoWidth()
			.VAlign(VAlign_Center)
			[
				SNew(SComboButton)
				.ButtonStyle(&ToolbarButton)
				.HasDownArrow(true)
				.ButtonContent()
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(SImage).Image(FAppStyle::GetBrush("Icons.Filter"))
					]
					+ SHorizontalBox::Slot()
					.AutoWidth()
					.Padding(4.0f, 0.0f, 0.0f, 0.0f)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "SmallText")
						.Text(FIWToUELocalizationManager::Get().GetText("FilterButton"))
					]
				]
				.MenuContent()
				[
					CreateFilterMenuContent()
				]
				.IsEnabled(TAttribute<bool>::Create(
					TAttribute<bool>::FGetter::CreateSP(this, &SWraithXWidget::IsUIEnabled)))
			]
		];
}

TSharedRef<SWidget> SWraithXWidget::CreateMainArea()
{
	return SNew(SSplitter)
			.Orientation(Orient_Horizontal)
			+ SSplitter::Slot()
			.Value(0.7f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(2.0f)
				[
					SNew(SVerticalBox)
					// List View
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SAssignNew(ListView, SListView<TSharedPtr<FCoDAsset>>)
						.ListItemsSource(&ViewModel->GetFilteredItems())
						.OnGenerateRow(this, &SWraithXWidget::GenerateListRow)
						.OnSelectionChanged(this, &SWraithXWidget::HandleListSelectionChanged)
						.OnMouseButtonDoubleClick(this, &SWraithXWidget::HandleListDoubleClick)
						.SelectionMode(ESelectionMode::Multi)
						.HeaderRow(
							SNew(SHeaderRow)
							+ SHeaderRow::Column(EWraithXSortColumnToFName(EWraithXSortColumn::AssetName))
							.DefaultLabel(FIWToUELocalizationManager::Get().GetText("AssetName"))
							.FillWidth(0.4f)
							.SortMode(this, &SWraithXWidget::GetSortMode,
							          EWraithXSortColumnToFName(EWraithXSortColumn::AssetName))
							.OnSort(this, &SWraithXWidget::OnSortColumnChanged)
							+ SHeaderRow::Column(EWraithXSortColumnToFName(EWraithXSortColumn::Status))
							.DefaultLabel(FIWToUELocalizationManager::Get().GetText("Status"))
							.FillWidth(0.2f)
							.SortMode(this, &SWraithXWidget::GetSortMode,
							          EWraithXSortColumnToFName(EWraithXSortColumn::Status))
							.OnSort(this, &SWraithXWidget::OnSortColumnChanged)
							+ SHeaderRow::Column(EWraithXSortColumnToFName(EWraithXSortColumn::Type))
							.DefaultLabel(FIWToUELocalizationManager::Get().GetText("Type"))
							.FillWidth(0.2f)
							.SortMode(this, &SWraithXWidget::GetSortMode,
							          EWraithXSortColumnToFName(EWraithXSortColumn::Type))
							.OnSort(this, &SWraithXWidget::OnSortColumnChanged)
							+ SHeaderRow::Column(EWraithXSortColumnToFName(EWraithXSortColumn::Size))
							.DefaultLabel(FIWToUELocalizationManager::Get().GetText("Size"))
							.FillWidth(0.2f)
							.SortMode(this, &SWraithXWidget::GetSortMode,
							          EWraithXSortColumnToFName(EWraithXSortColumn::Size))
							.OnSort(this, &SWraithXWidget::OnSortColumnChanged)
							.HAlignHeader(HAlign_Right)
							.HAlignCell(HAlign_Right)
						)
						.ScrollbarVisibility(EVisibility::Visible)
					]
				]
			]
			// Right Slot: Details Panel
			+ SSplitter::Slot()
			.Value(0.3f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(8.0f)
				[
					SNew(SVerticalBox)
					+ SVerticalBox::Slot()
					.AutoHeight()
					.Padding(0.0f, 0.0f, 0.0f, 8.0f)
					[
						SNew(STextBlock)
						.TextStyle(FAppStyle::Get(), "HeadingExtraSmall")
						.Text(NSLOCTEXT("WraithX", "AssetDetails", "Asset Details"))
					]
					+ SVerticalBox::Slot()
					.FillHeight(1.0f)
					[
						SNew(SScrollBox)
						+ SScrollBox::Slot()
						[

							SAssignNew(AssetInfoPanel, SAssetInfoPanel)
							.AssetItems(TAttribute<TArray<TSharedPtr<FCoDAsset>>>::Create(
								[VM = ViewModel]() { return VM->GetSelectedItems(); }))
						]
					]
				]
			];
}

TSharedRef<SWidget> SWraithXWidget::CreateBottomPanel()
{
	const FButtonStyle& ActionButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("PrimaryButton");
	const FButtonStyle& DefaultButtonStyle = FAppStyle::Get().GetWidgetStyle<FButtonStyle>("Button");
	const FSlateBrush* PanelBg = FAppStyle::GetBrush("ToolPanel.DarkGroupBorder");

	return SNew(SBorder)
		.BorderImage(PanelBg)
		.Padding(8.0f)
		[
			SNew(SVerticalBox)

			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(0.0f, 0.0f, 0.0f, 8.0f)
			[
				SNew(SBorder)
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				.Padding(6.0f)
				[
					SNew(SHorizontalBox)

					+ SHorizontalBox::Slot()
					.FillWidth(0.65f)
					.Padding(0.0f, 0.0f, 8.0f, 0.0f)
					.VAlign(VAlign_Bottom)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "SmallText")
							.Text(FIWToUELocalizationManager::Get().GetText("ImportPathLabel"))
						]
						+ SVerticalBox::Slot()
						.AutoHeight()
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.FillWidth(1.0f)
							[
								SAssignNew(ImportPathInput, SEditableTextBox)
								.Style(FAppStyle::Get(), "FlatEditableTextBox")
								.HintText(FIWToUELocalizationManager::Get().GetText("ImportPathHint"))
								.Text(TAttribute<FText>::CreateSP(this, &SWraithXWidget::GetImportPathText))
								.OnTextCommitted(this, &SWraithXWidget::HandleImportPathCommitted)
								.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
							]
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.Padding(4.0f, 0.0f, 0.0f, 0.0f)
							[
								SNew(SButton)
								.ButtonStyle(&ActionButtonStyle)
								.Text(FIWToUELocalizationManager::Get().GetText("Browse"))
								.OnClicked(this, &SWraithXWidget::HandleBrowseImportPathClicked)
								.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
								.ToolTipText(NSLOCTEXT("WraithX", "BrowseTooltip", "Browse for Import Directory"))
							]
						]
					]

					+ SHorizontalBox::Slot()
					.FillWidth(0.35f)
					[
						SNew(SVerticalBox)
						+ SVerticalBox::Slot()
						.AutoHeight()
						.Padding(0.0f, 0.0f, 0.0f, 4.0f)
						[
							SNew(STextBlock)
							.TextStyle(FAppStyle::Get(), "SmallText")
							.Text(FIWToUELocalizationManager::Get().GetText("AdvancedSettings"))
						]
						+ SVerticalBox::Slot()
						[
							SAssignNew(OptionalParamsInput, SEditableTextBox)
							.Style(FAppStyle::Get(), "FlatEditableTextBox")
							.HintText(FIWToUELocalizationManager::Get().GetText("OptionalParameters"))
							.Text(TAttribute<FText>::CreateSP(this, &SWraithXWidget::GetOptionalParamsText))
							.OnTextCommitted(this, &SWraithXWidget::HandleOptionalParamsCommitted)
							.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
							.ToolTipText(NSLOCTEXT("WraithX", "OptionalParamsTooltip",
							                       "Optional command-line parameters for the import process."))
						]
					]
				]
			]

			+ SVerticalBox::Slot()
			.AutoHeight()
			[
				SNew(SHorizontalBox)

				+ SHorizontalBox::Slot()
				.FillWidth(1.0f)
				[
					SNew(SUniformGridPanel)
					.SlotPadding(FMargin(4.0f, 0.0f))
					.MinDesiredSlotWidth(100.f)

					+ SUniformGridPanel::Slot(0, 0)
					[
						SNew(SButton)
						.ButtonStyle(&ActionButtonStyle)
						.Text(FIWToUELocalizationManager::Get().GetText("LoadGame"))
						.OnClicked(this, &SWraithXWidget::HandleLoadGameClicked)
						.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
						.HAlign(HAlign_Center)
						.ToolTipText(NSLOCTEXT("WraithX", "LoadGameTooltip",
						                       "Load asset list from the configured game source."))
					]
					+ SUniformGridPanel::Slot(1, 0)
					[
						SNew(SButton)
						.ButtonStyle(&DefaultButtonStyle)
						.Text(FIWToUELocalizationManager::Get().GetText("RefreshFile"))
						.OnClicked(this, &SWraithXWidget::HandleRefreshGameClicked)
						.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::IsUIEnabled))
						.HAlign(HAlign_Center)
						.ToolTipText(NSLOCTEXT("WraithX", "RefreshGameTooltip",
						                       "Reload and refresh the asset list from the source."))
					]
					+ SUniformGridPanel::Slot(2, 0)
					[
						SNew(SButton)
						.ButtonStyle(&ActionButtonStyle)
						.OnClicked(this, &SWraithXWidget::HandleImportSelectedClicked)
						.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::CanImportSelected))
						.HAlign(HAlign_Center)
						.ToolTipText(NSLOCTEXT("WraithX", "ImportSelectedTooltip",
						                       "Import the currently selected assets into the project."))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Download"))
								.DesiredSizeOverride(FVector2D(16, 16))
							]
							+ SHorizontalBox::Slot()
							.Padding(4, 0, 0, 0)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FIWToUELocalizationManager::Get().GetText("ImportSelected"))
							]
						]
					]
					+ SUniformGridPanel::Slot(3, 0)
					[
						SNew(SButton)
						.ButtonStyle(&DefaultButtonStyle)
						.OnClicked(this, &SWraithXWidget::HandleImportAllClicked)
						.IsEnabled(TAttribute<bool>::CreateSP(this, &SWraithXWidget::CanImportAll))
						.HAlign(HAlign_Center)
						.ToolTipText(NSLOCTEXT("WraithX", "ImportAllTooltip",
						                       "Import all assets currently shown in the list (respecting filters)."))
						[
							SNew(SHorizontalBox)
							+ SHorizontalBox::Slot()
							.AutoWidth()
							.VAlign(VAlign_Center)
							[
								SNew(SImage)
								.Image(FAppStyle::Get().GetBrush("Icons.Download"))
								.DesiredSizeOverride(FVector2D(16, 16))
							]
							+ SHorizontalBox::Slot()
							.Padding(4, 0, 0, 0)
							.VAlign(VAlign_Center)
							[
								SNew(STextBlock)
								.Text(FIWToUELocalizationManager::Get().GetText("ImportAll"))
							]
						]
					]
				]

				+ SHorizontalBox::Slot()
				.AutoWidth()
				.Padding(8.0f, 0.0f, 0.0f, 0.0f)
				.VAlign(VAlign_Center)
				[
					SNew(SButton)
					.ButtonStyle(FAppStyle::Get(), "SimpleButton")
					.OnClicked(this, &SWraithXWidget::HandleSettingsClicked)
					.ToolTipText(NSLOCTEXT("WraithX", "SettingsButtonTooltip", "Open Settings"))
					.ContentPadding(FMargin(2.0f))
					[
						SNew(SImage)
						.Image(FAppStyle::GetBrush("Icons.Settings"))
						.ColorAndOpacity(FSlateColor::UseForeground())
					]
				]
			]
		];
}

TSharedRef<SWidget> SWraithXWidget::CreateStatusBar()
{
	return SNew(SBorder)
		.BorderImage(FAppStyle::GetBrush("StatusBar.Background"))
		.Padding(FMargin(4.f, 2.f))
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.FillWidth(1.0f)
			.VAlign(VAlign_Center)
			[
				SAssignNew(LoadingIndicator, SProgressBar)
				.Percent(TAttribute<TOptional<float>>::CreateSP(this, &SWraithXWidget::GetLoadingProgress))
				.Visibility(TAttribute<EVisibility>::CreateSP(this, &SWraithXWidget::GetLoadingIndicatorVisibility))
			]
		];
}

TSharedRef<SWidget> SWraithXWidget::CreateFilterMenuContent()
{
	return SNew(SVerticalBox)
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(4.f)
		[
			SNew(STextBlock).Text(NSLOCTEXT("WraithX", "FilterOptions WIP", "Filter Options (Work In Progress)"))
		];
}

void SWraithXWidget::HandleSearchTextChanged(const FText& NewText)
{
	if (ViewModel.IsValid())
	{
		ViewModel->SetSearchText(NewText.ToString());
	}
}

void SWraithXWidget::HandleSearchTextCommitted(const FText& NewText, ETextCommit::Type CommitType)
{
	if (ViewModel.IsValid())
	{
		ViewModel->SetSearchText(NewText.ToString());
	}
}

FReply SWraithXWidget::HandleClearSearchClicked()
{
	if (ViewModel.IsValid())
	{
		SearchBox->SetText(FText::GetEmpty());
		ViewModel->ClearSearchText();
	}
	return FReply::Handled();
}

FReply SWraithXWidget::HandleLoadGameClicked()
{
	if (ViewModel.IsValid())
	{
		ViewModel->LoadGame();
	}
	return FReply::Handled();
}

FReply SWraithXWidget::HandleRefreshGameClicked()
{
	if (ViewModel.IsValid())
	{
		ViewModel->RefreshGame();
	}
	return FReply::Handled();
}

FReply SWraithXWidget::HandleImportSelectedClicked()
{
	if (ViewModel.IsValid())
	{
		ViewModel->ImportSelectedAssets();
	}
	return FReply::Handled();
}

FReply SWraithXWidget::HandleImportAllClicked()
{
	if (ViewModel.IsValid())
	{
		ViewModel->ImportAllAssets();
	}
	return FReply::Handled();
}

FReply SWraithXWidget::HandleSettingsClicked()
{
	TSharedPtr<SWindow> ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();

	TSharedRef<SWindow> SettingsWindow = SNew(SWindow)
		.Title(NSLOCTEXT("WraithX", "SettingsTitle", "WraithX Settings"))
		.ClientSize(FVector2D(600, 400))
		.SupportsMaximize(false)
		.SupportsMinimize(false)
		.SizingRule(ESizingRule::FixedSize)
		.IsTopmostWindow(true);

	TSharedRef<SSettingsDialog> SettingsDialog = SNew(SSettingsDialog);
	// .ViewModel(ViewModel)

	SettingsWindow->SetContent(SettingsDialog);

	if (ActiveWindow.IsValid())
	{
		FSlateApplication::Get().AddModalWindow(SettingsWindow, ActiveWindow);
	}
	else
	{
		FSlateApplication::Get().AddWindow(SettingsWindow);
	}

	return FReply::Handled();
}

FReply SWraithXWidget::HandleBrowseImportPathClicked()
{
	if (ViewModel.IsValid())
	{
		TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared());
		ViewModel->BrowseImportPath(ParentWindow);
	}
	return FReply::Handled();
}

void SWraithXWidget::HandleImportPathCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	if (ViewModel.IsValid() && (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus))
	{
		ViewModel->SetImportPath(InText.ToString());
	}
	else if (CommitType == ETextCommit::OnCleared)
	{
		ImportPathInput->SetText(FText::FromString(ViewModel->GetImportPath()));
	}
}

void SWraithXWidget::HandleListSelectionChanged(TSharedPtr<FCoDAsset> /*Item*/, ESelectInfo::Type /*SelectInfo*/)
{
	if (ViewModel.IsValid() && ListView.IsValid())
	{
		ViewModel->SetSelectedItems(ListView->GetSelectedItems());
	}
}

void SWraithXWidget::HandleListDoubleClick(TSharedPtr<FCoDAsset> Item)
{
	if (Item.IsValid())
	{
		ViewModel->RequestPreviewAsset(Item);
	}
}

void SWraithXWidget::HandleOptionalParamsCommitted(const FText& InText, ETextCommit::Type CommitType)
{
	if (ViewModel.IsValid() && (CommitType == ETextCommit::OnEnter || CommitType == ETextCommit::OnUserMovedFocus))
	{
		ViewModel->SetOptionalParams(InText.ToString());
	}
	else if (CommitType == ETextCommit::OnCleared)
	{
		OptionalParamsInput->SetText(FText::FromString(ViewModel->GetOptionalParams()));
	}
}

void SWraithXWidget::HandleListChanged()
{
	if (ListView.IsValid())
	{
		ListView->RequestListRefresh();
	}
}

void SWraithXWidget::HandleAssetCountChanged(int32 TotalAssets, int32 FilteredAssets)
{
	// AssetCountText updates automatically via binding
	// if(AssetCountText.IsValid()) AssetCountText->SetText(GetAssetCountText());
}

void SWraithXWidget::HandleLoadingStateChanged(bool bIsLoading)
{
	// IsEnabled/Visibility attributes handle this automatically via their bindings.
}

void SWraithXWidget::HandleImportPathChanged(const FString& NewPath)
{
	// ImportPathInput updates automatically via binding
	// if (ImportPathInput.IsValid()) ImportPathInput->SetText(FText::FromString(NewPath));
}

void SWraithXWidget::HandleShowNotification(const FText& Message)
{
	ShowNotificationPopup(Message);
}

void SWraithXWidget::HandlePreviewAsset(TSharedPtr<FCoDAsset> AssetToPreview)
{
	if (!AssetToPreview.IsValid()) return;

	TSharedRef<SWindow> PreviewWindow = SNew(SWindow)
		.Title(FText::Format(
			NSLOCTEXT("WraithX", "PreviewTitle", "Preview: {0}"), FText::FromString(AssetToPreview->AssetName)))
		.ClientSize(FVector2D(800, 600))
		.SupportsMaximize(true)
		.SupportsMinimize(true);

	TSharedRef<SWraithXPreviewWindow> PreviewerWidget = SNew(SWraithXPreviewWindow);
	// .AssetItem(AssetToPreview);

	PreviewWindow->SetContent(PreviewerWidget);
}

void SWraithXWidget::ShowNotificationPopup(const FText& Message)
{
	FNotificationInfo Info(Message);
	Info.bFireAndForget = true;
	Info.ExpireDuration = 5.0f;
	Info.bUseThrobber = false;
	Info.bUseSuccessFailIcons = true;

	TSharedPtr<SNotificationItem> Notification = FSlateNotificationManager::Get().AddNotification(Info);
	if (Notification.IsValid())
	{
		Notification->SetCompletionState(SNotificationItem::CS_Success); // Or CS_Fail etc.
	}
}

bool SWraithXWidget::IsUIEnabled() const
{
	return ViewModel.IsValid() && !ViewModel->bIsLoading;
}

EVisibility SWraithXWidget::GetLoadingIndicatorVisibility() const
{
	return (ViewModel.IsValid() && ViewModel->IsLoading()) ? EVisibility::Visible : EVisibility::Collapsed;
}

TOptional<float> SWraithXWidget::GetLoadingProgress() const
{
	if (ViewModel.IsValid() && ViewModel->IsLoading())
	{
		return TOptional<float>(ViewModel->GetLoadingProgress());
	}
	return TOptional<float>();
}

FText SWraithXWidget::GetAssetCountText() const
{
	if (ViewModel.IsValid())
	{
		return FIWToUELocalizationManager::Get().GetFormattedText(
			"AssetCountFormat",
			"Counts unavailable",
			FText::AsNumber(ViewModel->GetTotalAssetCount()),
			FText::AsNumber(ViewModel->GetFilteredAssetCount())
		);
	}
	return FIWToUELocalizationManager::Get().GetFormattedText(
		"AssetCountFormat",
		"Counts unavailable",
		FText::AsNumber(0),
		FText::AsNumber(0)
	);
}

FText SWraithXWidget::GetImportPathText() const
{
	return ViewModel.IsValid() ? FText::FromString(ViewModel->GetImportPath()) : FText::GetEmpty();
}

FText SWraithXWidget::GetOptionalParamsText() const
{
	return ViewModel.IsValid() ? FText::FromString(ViewModel->GetOptionalParams()) : FText::GetEmpty();
}

bool SWraithXWidget::CanImportSelected() const
{
	return IsUIEnabled() && ViewModel.IsValid() && ViewModel->GetSelectedItems().Num() > 0;
}

bool SWraithXWidget::CanImportAll() const
{
	return IsUIEnabled() && ViewModel.IsValid() && ViewModel->GetFilteredItems().Num() > 0;
}
