#include "Widgets/SGeneralSettingsPage.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Utils/WraithXWidgetHelper.h"
#include "Widgets/GeneralSettingsViewModel.h"
#include "WraithX/WraithSettings.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)
BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION


void SGeneralSettingsPage::Construct(const FArguments& InArgs, TSharedRef<FGeneralSettingsViewModel> InViewModel)
{
	ViewModel = InViewModel;

	ChildSlot
	[
		SNew(SVerticalBox)

		// Optional: Top description text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5, 0, 10)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("GeneralTopDesc", "Configure general import settings, paths, and game launch options."))
			.AutoWrapText(true)
		]

		// --- Loading Types Category ---
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("LoadTypesCategory", "Asset Load Types"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(ViewModel.Get(), &FGeneralSettingsViewModel::GetLoadModelsCheckState)
					.OnCheckStateChanged(ViewModel.Get(),
					                     &FGeneralSettingsViewModel::HandleLoadModelsCheckStateChanged)
					.ToolTipText(LOC_SETTINGS("LoadModelsTooltip", "Enable loading of 3D Models."))
					[
						SNew(STextBlock).Text(LOC_SETTINGS("LoadModelsLabel", "Models"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(ViewModel.Get(), &FGeneralSettingsViewModel::GetLoadAudioCheckState)
					.OnCheckStateChanged(ViewModel.Get(),
					                     &FGeneralSettingsViewModel::HandleLoadAudioCheckStateChanged)
					.ToolTipText(LOC_SETTINGS("LoadAudioTooltip", "Enable loading of Audio assets."))
					[
						SNew(STextBlock).Text(LOC_SETTINGS("LoadAudioLabel", "Audio"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(ViewModel.Get(), &FGeneralSettingsViewModel::GetLoadMaterialsCheckState)
					.OnCheckStateChanged(ViewModel.Get(),
					                     &FGeneralSettingsViewModel::HandleLoadMaterialsCheckStateChanged)
					.ToolTipText(LOC_SETTINGS("LoadMaterialsTooltip", "Enable loading of Materials."))
					[
						SNew(STextBlock).Text(LOC_SETTINGS("LoadMaterialsLabel", "Materials"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(ViewModel.Get(), &FGeneralSettingsViewModel::GetLoadAnimationsCheckState)
					.OnCheckStateChanged(ViewModel.Get(),
					                     &FGeneralSettingsViewModel::HandleLoadAnimationsCheckStateChanged)
					.ToolTipText(LOC_SETTINGS("LoadAnimationsTooltip", "Enable loading of Animations."))
					[
						SNew(STextBlock).Text(LOC_SETTINGS("LoadAnimationsLabel", "Animations"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(ViewModel.Get(), &FGeneralSettingsViewModel::GetLoadTexturesCheckState)
					.OnCheckStateChanged(ViewModel.Get(),
					                     &FGeneralSettingsViewModel::HandleLoadTexturesCheckStateChanged)
					.ToolTipText(LOC_SETTINGS("LoadTexturesTooltip", "Enable loading of Textures."))
					[
						SNew(STextBlock).Text(LOC_SETTINGS("LoadTexturesLabel", "Textures"))
					]
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					SNew(SCheckBox)
					.IsChecked(ViewModel.Get(), &FGeneralSettingsViewModel::GetLoadMapsCheckState)
					.OnCheckStateChanged(ViewModel.Get(),
					                     &FGeneralSettingsViewModel::HandleLoadMapsCheckStateChanged)
					.ToolTipText(LOC_SETTINGS("LoadMapsTooltip", "Enable loading of Maps."))
					[
						SNew(STextBlock).Text(LOC_SETTINGS("LoadMapsLabel", "Maps"))
					]
				]
			)
		]

		// --- Paths Category ---
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("PathsCategory", "Path Settings"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("CordycepPathLabel", "Cordycep Tool Path"),
						LOC_SETTINGS("CordycepPathTooltip",
						             "Path to the Cordycep executable or root folder."),
						TAttribute<FText>(ViewModel.Get(), &FGeneralSettingsViewModel::GetCordycepPath),
						FOnTextChanged::CreateSP(ViewModel.Get(),
						                         &FGeneralSettingsViewModel::HandleCordycepPathTextChanged),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FGeneralSettingsViewModel::HandleCordycepPathTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FGeneralSettingsViewModel::HandleBrowseCordycepPathClicked)
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreatePathSettingRow(
						LOC_SETTINGS("GamePathLabel", "Game Path"),
						LOC_SETTINGS("GamePathTooltip", "Path to the game's root directory."),
						TAttribute<FText>(ViewModel.Get(), &FGeneralSettingsViewModel::GetGamePath),
						FOnTextChanged::CreateSP(ViewModel.Get(),
						                         &FGeneralSettingsViewModel::HandleGamePathTextChanged),
						FOnTextCommitted::CreateSP(ViewModel.Get(),
						                           &FGeneralSettingsViewModel::HandleGamePathTextCommitted),
						FOnClicked::CreateSP(ViewModel.Get(),
						                     &FGeneralSettingsViewModel::HandleBrowseGamePathClicked)
					)
				]
			)
		]


		// --- Game Configuration Category ---
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 5)
		[
			WraithXWidgetHelper::CreateCategory(
				LOC_SETTINGS("GameConfigCategory", "Game Configuration"),
				SNew(SVerticalBox)
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("GameTypeLabel", "Game Type"),
						LOC_SETTINGS("GameTypeTooltip",
						             "Select the target game type for specific configurations."),
						SNew(SComboBox<TSharedPtr<EGameType>>)
						.OptionsSource(ViewModel->GetGameTypeOptions())
						.OnSelectionChanged(
							ViewModel.Get(),
							&FGeneralSettingsViewModel::HandleGameTypeSelectionChanged)
						.OnGenerateWidget_Lambda(
							[](TSharedPtr<EGameType> InItem)
							{
								// How each item looks in dropdown
								return SNew(STextBlock).Text(
										UWraithSettings::GetGameTypeDisplayName(
											*InItem));
							})
						.InitiallySelectedItem(
							ViewModel->GetCurrentGameType())
						[
							// Content of the combobox button when closed
							SNew(STextBlock)
							.Text_Lambda([this]()
							{
								// Dynamic text based on current selection
								TSharedPtr<EGameType> Selected = ViewModel->GetCurrentGameType();
								return Selected.IsValid()
									       ? UWraithSettings::GetGameTypeDisplayName(*Selected)
									       : LOC_SETTINGS("InvalidSelection", "Select...");
							})
						]
					)
				]
				+ SVerticalBox::Slot().AutoHeight().Padding(2)
				[
					WraithXWidgetHelper::CreateSettingRow(
						LOC_SETTINGS("LaunchParamsLabel", "Launch Parameters"),
						LOC_SETTINGS("LaunchParamsTooltip",
						             "Additional command-line parameters when launching the game."),
						SNew(SEditableTextBox)
						.Text(ViewModel.Get(), &FGeneralSettingsViewModel::GetLaunchParameters)
						.OnTextChanged(ViewModel.Get(),
						               &FGeneralSettingsViewModel::HandleLaunchParametersTextChanged)
						.OnTextCommitted(ViewModel.Get(),
						                 &FGeneralSettingsViewModel::HandleLaunchParametersTextCommitted)
						.HintText(LOC_SETTINGS("LaunchParamsHint", "e.g., -windowed -log"))
					)
				]
			)
		]

		// Optional: Bottom description text
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(0, 10, 0, 5)
		[
			SNew(STextBlock)
			.Text(LOC_SETTINGS("GeneralBottomDesc", "These settings affect asset loading and game interaction."))
			.AutoWrapText(true)
			.ColorAndOpacity(FSlateColor::UseSubduedForeground()) // Make it less prominent
		]
	];
}


END_SLATE_FUNCTION_BUILD_OPTIMIZATION
