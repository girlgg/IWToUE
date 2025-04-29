#include "Widgets/CastOptionWindow.h"

#include "IDocumentation.h"
#include "SPrimaryButton.h"
#include "Widgets/Layout/SUniformGridPanel.h"

#define LOCTEXT_NAMESPACE "CastOption"

void SCastOptionWindow::Construct(const FArguments& InArgs)
{
	ImportUI = InArgs._ImportUI;
	WidgetWindow = InArgs._WidgetWindow;
	check(ImportUI);

	TSharedPtr<SHorizontalBox> FbxHeaderButtons;
	TSharedPtr<SBox> InspectorBox;
	this->ChildSlot
	[
		SNew(SBox)
		.MaxDesiredHeight(InArgs._MaxWindowHeight)
		.MaxDesiredWidth(InArgs._MaxWindowWidth)
		[
			SNew(SVerticalBox)
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SBorder)
				.Padding(FMargin(3))
				.BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder"))
				[
					SNew(SHorizontalBox)
					+ SHorizontalBox::Slot()
					.AutoWidth()
					[
						SNew(STextBlock)
						.Text(LOCTEXT("Import_CurrentFileTitle", "Current Asset: "))
					]
					+ SHorizontalBox::Slot()
					.Padding(5, 0, 0, 0)
					.AutoWidth()
					.VAlign(VAlign_Center)
					[
						SNew(STextBlock)
						.Text(InArgs._FullPath)
						.ToolTipText(InArgs._FullPath)
					]
				]
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SAssignNew(InspectorBox, SBox)
				.MaxDesiredHeight(650.0f)
				.WidthOverride(400.0f)
			]
			+ SVerticalBox::Slot()
			.AutoHeight()
			.Padding(2.0f)
			[
				SNew(SUniformGridPanel)
				.SlotPadding(2.0f)
				+ SUniformGridPanel::Slot(1, 0)
				[
					SAssignNew(ImportAllButton, SPrimaryButton)
					.Text(LOCTEXT("CastOptionWindow_ImportAll", "Import All"))
					.ToolTipText(LOCTEXT("FbxOptionWindow_ImportAll_ToolTip",
					                     "Import all files with these same settings"))
					.IsEnabled(this, &SCastOptionWindow::CanImport)
					.OnClicked(this, &SCastOptionWindow::OnRequestImportAll)
				]
				+ SUniformGridPanel::Slot(2, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("CastOptionWindow_Import", "Import"))
					.IsEnabled(this, &SCastOptionWindow::CanImport)
					.OnClicked(this, &SCastOptionWindow::OnRequestImport)
				]
				+ SUniformGridPanel::Slot(3, 0)
				[
					SNew(SButton)
					.HAlign(HAlign_Center)
					.Text(LOCTEXT("CastOptionWindow_Cancel", "Cancel"))
					.ToolTipText(LOCTEXT("FbxOptionWindow_Cancel_ToolTip", "Cancels importing this Cast file"))
					.OnClicked(this, &SCastOptionWindow::OnRequestCancel)
				]
			]
		]
	];

	FPropertyEditorModule& PropertyEditorModule = FModuleManager::GetModuleChecked<FPropertyEditorModule>(
		"PropertyEditor");
	FDetailsViewArgs DetailsViewArgs;
	DetailsViewArgs.bAllowSearch = false;
	DetailsViewArgs.NameAreaSettings = FDetailsViewArgs::HideNameArea;
	DetailsView = PropertyEditorModule.CreateDetailView(DetailsViewArgs);

	InspectorBox->SetContent(DetailsView->AsShared());

	DetailsView->SetObject(ImportUI);

	RegisterActiveTimer(0.f, FWidgetActiveTimerDelegate::CreateSP(this, &SCastOptionWindow::SetFocusPostConstruct));
}

void SCastOptionWindow::CloseWindow()
{
	if (WidgetWindow.IsValid()) {
		WidgetWindow.Pin()->RequestDestroyWindow();
	}
}

EActiveTimerReturnType SCastOptionWindow::SetFocusPostConstruct(double InCurrentTime, float InDeltaTime)
{
	if (ImportAllButton.IsValid())
	{
		FSlateApplication::Get().SetKeyboardFocus(ImportAllButton, EFocusCause::SetDirectly);
	}

	return EActiveTimerReturnType::Stop;
}

bool SCastOptionWindow::CanImport() const
{
	return true;
}

FReply SCastOptionWindow::OnResetToDefaultClick() const
{
	if(ImportUI) {
		DetailsView->SetObject(ImportUI, true);
	}
	return FReply::Handled();	
}

#undef LOCTEXT_NAMESPACE
