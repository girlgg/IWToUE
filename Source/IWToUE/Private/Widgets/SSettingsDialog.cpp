#include "Widgets/SSettingsDialog.h"

#include "SlateOptMacros.h"
#include "Localization/IWToUELocalizationManager.h"
#include "Widgets/AnimationSettingsViewModel.h"
#include "Widgets/AudioSettingsViewModel.h"
#include "Widgets/GeneralSettingsViewModel.h"
#include "Widgets/MapSettingsViewModel.h"
#include "Widgets/MaterialSettingsViewModel.h"
#include "Widgets/ModelSettingsViewModel.h"
#include "Widgets/SAnimationSettingsPage.h"
#include "Widgets/SAudioSettingsPage.h"
#include "Widgets/SMapSettingsPage.h"
#include "Widgets/SMaterialSettingsPage.h"
#include "Widgets/SModelSettingsPage.h"
#include "Widgets/STextureSettingsPage.h"
#include "Widgets/TextureSettingsViewModel.h"

#define LOC_SETTINGS(Key, Text) FIWToUELocalizationManager::Get().GetText(Key, Text)

BEGIN_SLATE_FUNCTION_BUILD_OPTIMIZATION

void SSettingsDialog::Construct(const FArguments& InArgs)
{
	UWraithSettingsManager* SettingsManager = GEditor ? GEditor->GetEditorSubsystem<UWraithSettingsManager>() : nullptr;
	if (!SettingsManager)
	{
		UE_LOG(LogTemp, Error, TEXT("SSettingsDialog: Could not get UWraithSettingsManager subsystem!"));
		// Handle error, maybe show a message and disable content
		ChildSlot
		[
			SNew(STextBlock).Text(LOC_SETTINGS("ErrorNoManager", "Error: Settings Subsystem not found!"))
		];
		return;
	}

	TSharedRef<SDockTab> OwnerTab = SNew(SDockTab).TabRole(ETabRole::NomadTab);
	TabManager = FGlobalTabmanager::Get()->NewTabManager(OwnerTab);

	RegisterTabSpawners(SettingsManager);

	SettingsLayout = FTabManager::NewLayout(WraithXSettings::LayoutVersion)
		->AddArea(
			FTabManager::NewPrimaryArea()
			->SetOrientation(Orient_Vertical)
			->Split(
				FTabManager::NewStack()
				->SetSizeCoefficient(1.0f)
				->SetHideTabWell(false)
				->AddTab(WraithXSettings::TabIds::General, ETabState::OpenedTab)
				->AddTab(WraithXSettings::TabIds::Model, ETabState::OpenedTab)
				->AddTab(WraithXSettings::TabIds::Animation, ETabState::OpenedTab)
				->AddTab(WraithXSettings::TabIds::Material, ETabState::OpenedTab)
				->AddTab(WraithXSettings::TabIds::Texture, ETabState::OpenedTab)
				->AddTab(WraithXSettings::TabIds::Audio, ETabState::OpenedTab)
				->AddTab(WraithXSettings::TabIds::Map, ETabState::OpenedTab)
				->SetForegroundTab(WraithXSettings::TabIds::General)
			)
		);

	// 构建主界面
	ChildSlot
	[
		SNew(SVerticalBox)

		// 标签内容区域
		+ SVerticalBox::Slot()
		.AutoHeight()
		.Padding(FMargin(5.0f))

		+ SVerticalBox::Slot().FillHeight(1.0f).Padding(FMargin(5.0f, 0.0f, 5.0f, 0.0f))
		[
			TabManager->RestoreFrom(SettingsLayout.ToSharedRef(), TSharedPtr<SWindow>()).ToSharedRef()
		]

		// 底部按钮栏
		+ SVerticalBox::Slot()
		  .AutoHeight()
		  .Padding(10.0f) // More padding around buttons
		  .HAlign(HAlign_Right)
		[
			SNew(SHorizontalBox)
			+ SHorizontalBox::Slot()
			.AutoWidth()
			[
				SNew(SButton)
				.Text(LOC_SETTINGS("CloseButton", "Close"))
				.ToolTipText(LOC_SETTINGS("CloseButton_Tooltip",
				                          "Close the settings window. Changes are saved automatically."))
				.OnClicked(this, &SSettingsDialog::CloseDialog)
				.ButtonStyle(FAppStyle::Get(), "PrimaryButton")
			]
		]
	];
}

void SSettingsDialog::RegisterTabSpawners(UWraithSettingsManager* SettingsManager)
{
	if (!TabManager.IsValid()) return;

	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::General,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnGeneralSettingsTab,
	                                                      SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("GeneralTabTitle", "General"));
	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::Model,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnModelSettingsTab,
	                                                      SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("ModelTabTitle", "Model"));
	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::Animation,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnAnimationSettingsTab,
	                                                      SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("AnimationTabTitle", "Animation"));
	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::Material,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnMaterialSettingsTab,
	                                                      SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("MaterialTabTitle", "Material"));
	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::Texture,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnTextureSettingsTab,
	                                                      SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("TextureTabTitle", "Texture"));
	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::Audio,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnAudioSettingsTab,
	                                                      SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("AudioTabTitle", "Audio"));
	TabManager->RegisterTabSpawner(WraithXSettings::TabIds::Map,
	                               FOnSpawnTab::CreateRaw(this, &SSettingsDialog::SpawnMapSettingsTab, SettingsManager))
	          .SetDisplayName(LOC_SETTINGS("MapTabTitle", "Map"));
}

TSharedRef<SDockTab> SSettingsDialog::SpawnGeneralSettingsTab(const FSpawnTabArgs& Args,
                                                              UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FGeneralSettingsViewModel> ViewModel = MakeShared<FGeneralSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("GeneralTabTitle", "General")).TabRole(PanelTab)
		[
			SNew(SGeneralSettingsPage, ViewModel)
		];
}

TSharedRef<SDockTab> SSettingsDialog::SpawnModelSettingsTab(const FSpawnTabArgs& Args,
                                                            UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FModelSettingsViewModel> ViewModel = MakeShared<FModelSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("ModelTabTitle", "Model")).TabRole(PanelTab)
		[
			SNew(SModelSettingsPage, ViewModel)
		];
}

TSharedRef<SDockTab> SSettingsDialog::SpawnAnimationSettingsTab(const FSpawnTabArgs& Args,
                                                                UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FAnimationSettingsViewModel> ViewModel = MakeShared<FAnimationSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("AnimTabTitle", "Animation")).TabRole(PanelTab)
		[
			SNew(SAnimationSettingsPage, ViewModel)
		];
}

TSharedRef<SDockTab> SSettingsDialog::SpawnMaterialSettingsTab(const FSpawnTabArgs& Args,
                                                               UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FMaterialSettingsViewModel> ViewModel = MakeShared<FMaterialSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("MaterialTabTitle", "Material")).TabRole(PanelTab)
		[
			SNew(SMaterialSettingsPage, ViewModel)
		];
}

TSharedRef<SDockTab> SSettingsDialog::SpawnTextureSettingsTab(const FSpawnTabArgs& Args,
                                                              UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FTextureSettingsViewModel> ViewModel = MakeShared<FTextureSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("TextureTabTitle", "Texture")).TabRole(PanelTab)
		[
			SNew(STextureSettingsPage, ViewModel)
		];
}

TSharedRef<SDockTab> SSettingsDialog::SpawnAudioSettingsTab(const FSpawnTabArgs& Args,
                                                            UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FAudioSettingsViewModel> ViewModel = MakeShared<FAudioSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("AudioTabTitle", "Audio")).TabRole(PanelTab)
		[
			SNew(SAudioSettingsPage, ViewModel)
		];
}

TSharedRef<SDockTab> SSettingsDialog::SpawnMapSettingsTab(const FSpawnTabArgs& Args,
                                                          UWraithSettingsManager* SettingsManager)
{
	TSharedRef<FMapSettingsViewModel> ViewModel = MakeShared<FMapSettingsViewModel>();
	return SNew(SDockTab).Label(LOC_SETTINGS("MapTabTitle", "Map")).TabRole(PanelTab)
		[
			SNew(SMapSettingsPage, ViewModel)
		];
}

FReply SSettingsDialog::CloseDialog()
{
	if (TSharedPtr<SWindow> ParentWindow = FSlateApplication::Get().FindWidgetWindow(AsShared()))
	{
		ParentWindow->RequestDestroyWindow();
	}
	return FReply::Handled();
}

END_SLATE_FUNCTION_BUILD_OPTIMIZATION
