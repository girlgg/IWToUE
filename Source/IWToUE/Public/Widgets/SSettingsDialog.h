#pragma once

#include "CoreMinimal.h"
#include "SGeneralSettingsPage.h"
#include "WraithX/WraithSettingsManager.h"
#include "Widgets/SCompoundWidget.h"

class FMapSettingsViewModel;
class FAudioSettingsViewModel;
class FTextureSettingsViewModel;
class FMaterialSettingsViewModel;
class FAnimationSettingsViewModel;
class FModelSettingsViewModel;
class FGeneralSettingsViewModel;
class UWraithSettings;

namespace WraithXSettings
{
	namespace TabIds
	{
		static const FName General(TEXT("GeneralSettings"));
		static const FName Model(TEXT("ModelSettings"));
		static const FName Animation(TEXT("AnimationSettings"));
		static const FName Material(TEXT("MaterialSettings"));
		static const FName Texture(TEXT("TextureSettings"));
		static const FName Audio(TEXT("AudioSettings"));
		static const FName Map(TEXT("MapSettings"));
	}

	static const FName LayoutVersion(TEXT("SettingsLayout_v1"));
}

class IWTOUE_API SSettingsDialog : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SSettingsDialog)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);

private:
	void RegisterTabSpawners(UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnGeneralSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnModelSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnAnimationSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnMaterialSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnTextureSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnAudioSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);
	TSharedRef<SDockTab> SpawnMapSettingsTab(const FSpawnTabArgs& Args, UWraithSettingsManager* SettingsManager);

	FReply CloseDialog();

	TSharedPtr<FTabManager> TabManager;
	TSharedPtr<FTabManager::FLayout> SettingsLayout;

	// TSharedPtr<FGeneralSettingsViewModel> GeneralViewModel;
	// TSharedPtr<FModelSettingsViewModel> ModelViewModel;
	// TSharedPtr<FAnimationSettingsViewModel> AnimationViewModel;
	// TSharedPtr<FMaterialSettingsViewModel> MaterialViewModel;
	// TSharedPtr<FTextureSettingsViewModel> TextureViewModel;
	// TSharedPtr<FAudioSettingsViewModel> AudioViewModel;
	// TSharedPtr<FMapSettingsViewModel> MapViewModel;
};
