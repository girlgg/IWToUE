#include "Widgets/SModelSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/ModelSettingsViewModel.h"
#include "Widgets/Input/SNumericEntryBox.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SModelSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FModelSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Top description
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5, 0, 10)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("ModelTopDesc",
			                   "Configure settings related to the export of static and skeletal meshes."))
			.AutoWrapText(true)
		]

		// Export Configuration Category
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("ModelExportConfigCategory", "Export Configuration"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("ModelExportDirLabel", "Export Directory"),
						LOC_SETTINGS("ModelExportDirTooltip",
						             "Default directory for exported models."),
						TAttribute<FText>(
							ViewModel.Get(),
							&FModelSettingsViewModel::GetExportDirectory),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FModelSettingsViewModel::HandleExportDirectoryTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FModelSettingsViewModel::HandleBrowseExportDirectoryClicked)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("ModelExportLODsLabel", "Export LODs"),
						LOC_SETTINGS("ModelExportLODsTooltip",
						             "Include Level of Detail meshes in the export."),
						SNew(SCheckBox)
						.IsChecked(ViewModel.Get(), &FModelSettingsViewModel::GetExportLoDsCheckState)
						.OnCheckStateChanged(ViewModel.Get(),
						                     &FModelSettingsViewModel::HandleExportLODsCheckStateChanged)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("ModelMaxLODLabel", "Max Export LOD Level"),
						LOC_SETTINGS("ModelMaxLODTooltip",
						             "Highest LOD index to export (0 is base mesh). Higher numbers are lower quality."),
						SNew(SNumericEntryBox<int32>)
						                             .Value(ViewModel.Get(),
						                                    &FModelSettingsViewModel::GetMaxLODLevel)
						                             .OnValueChanged(
							                             ViewModel.Get(),
							                             &FModelSettingsViewModel::HandleMaxLODLevelChanged)
						                             .MinValue(0)
						                             .MaxValue(10) // Set reasonable max
						                             .MinSliderValue(0)
						                             .MaxSliderValue(10)
						                             .AllowSpin(true)
						                             .IsEnabled(ViewModel.Get(),
						                                        &FModelSettingsViewModel::IsMaxLODLevelEnabled)
						// Bind IsEnabled
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("ModelExportVtxColorLabel",
						             "Export Vertex Colors"),
						LOC_SETTINGS("ModelExportVtxColorTooltip",
						             "Include vertex color data in the export."),
						SNew(SCheckBox)
						.IsChecked(ViewModel.Get(),
						           &FModelSettingsViewModel::GetExportVertexColorCheckState)
						.OnCheckStateChanged(ViewModel.Get(),
						                     &FModelSettingsViewModel::HandleExportVertexColorCheckStateChanged)
					)
				]
			)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("ModelPathStrategyCategory", "Path Strategy"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("ModelTexturePathModeLabel", "Texture Path Mode"),
						LOC_SETTINGS("ModelTexturePathModeTooltip",
						             "How texture paths are structured relative to the model export directory. Disabled if 'Use Global Texture Path' is checked in Texture Settings."),
						SNew(SComboBox<TSharedPtr<ETextureExportPathMode>>)
						.OptionsSource(ViewModel->GetTexturePathModeOptions())
						.OnSelectionChanged(ViewModel.Get(),
						                    &FModelSettingsViewModel::HandleTexturePathModeSelectionChanged)
						.OnGenerateWidget_Lambda(
							[this](TSharedPtr<ETextureExportPathMode> InItem)
							{
								return SNew(STextBlock).Text(
										ViewModel->GetTexturePathModeDisplayName(
											*InItem));
							})
						.InitiallySelectedItem(ViewModel->GetCurrentTexturePathMode())
						.IsEnabled(ViewModel.Get(),
						           &FModelSettingsViewModel::IsTexturePathModeEnabled)
						// Bind IsEnabled
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								TSharedPtr<ETextureExportPathMode> Selected = ViewModel
									->
									GetCurrentTexturePathMode();
								return Selected.IsValid()
									       ? ViewModel->GetTexturePathModeDisplayName(
										       *Selected)
									       : LOC_SETTINGS(
										       "InvalidSelection", "Select...");
							})
						]
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("ModelMaterialPathModeLabel",
						             "Material Path Mode"),
						LOC_SETTINGS("ModelMaterialPathModeTooltip",
						             "How material paths are structured relative to the model export directory. Disabled if 'Use Global Material Path' is checked in Material Settings."),
						SNew(SComboBox<TSharedPtr<EMaterialExportPathMode>>)
						.OptionsSource(ViewModel->GetMaterialPathModeOptions())
						.OnSelectionChanged(ViewModel.Get(),
						                    &FModelSettingsViewModel::HandleMaterialPathModeSelectionChanged)
						.OnGenerateWidget_Lambda(
							[this](TSharedPtr<EMaterialExportPathMode> InItem)
							{
								return SNew(STextBlock).Text(
										ViewModel->GetMaterialPathModeDisplayName(
											*InItem));
							})
						.InitiallySelectedItem(ViewModel->GetCurrentMaterialPathMode())
						.IsEnabled(ViewModel.Get(),
						           &FModelSettingsViewModel::IsMaterialPathModeEnabled)
						[
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								TSharedPtr<EMaterialExportPathMode> Selected = ViewModel
									->
									GetCurrentMaterialPathMode();
								return Selected.IsValid()
									       ? ViewModel->GetMaterialPathModeDisplayName(
										       *Selected)
									       : LOC_SETTINGS(
										       "InvalidSelection", "Select...");
							})
						]
					)
				]
			)
		]
		+ SVerticalBox::Slot().AutoHeight().Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("ModelBottomDesc",
			                   "These settings control the output format and structure for exported models."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground())
		]
	];
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION
