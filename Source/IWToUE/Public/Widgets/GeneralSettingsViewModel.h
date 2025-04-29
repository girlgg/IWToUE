#pragma once

#include "CoreMinimal.h"

enum class EGameType : uint8;
class UWraithSettings;

class FGeneralSettingsViewModel : public TSharedFromThis<FGeneralSettingsViewModel>
{
public:
	FGeneralSettingsViewModel();

	ECheckBoxState GetLoadModelsCheckState() const;
	ECheckBoxState GetLoadAudioCheckState() const;
	ECheckBoxState GetLoadMaterialsCheckState() const;
	ECheckBoxState GetLoadAnimationsCheckState() const;
	ECheckBoxState GetLoadTexturesCheckState() const;
	ECheckBoxState GetLoadMapsCheckState() const;

	FText GetCordycepPath() const;
	FText GetGamePath() const;

	TSharedPtr<EGameType> GetCurrentGameType() const;
	TArray<TSharedPtr<EGameType>>* GetGameTypeOptions();
	FText GetGameTypeDisplayName(EGameType Value) const;

	FText GetLaunchParameters() const;

	void HandleLoadModelsCheckStateChanged(ECheckBoxState NewState);
	void HandleLoadAudioCheckStateChanged(ECheckBoxState NewState);
	void HandleLoadMaterialsCheckStateChanged(ECheckBoxState NewState);
	void HandleLoadAnimationsCheckStateChanged(ECheckBoxState NewState);
	void HandleLoadTexturesCheckStateChanged(ECheckBoxState NewState);
	void HandleLoadMapsCheckStateChanged(ECheckBoxState NewState);

	void HandleCordycepPathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void HandleGamePathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void HandleGameTypeSelectionChanged(TSharedPtr<EGameType> NewSelection, ESelectInfo::Type SelectInfo);
	void HandleLaunchParametersTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

	FReply HandleBrowseCordycepPathClicked();
	FReply HandleBrowseGamePathClicked();

private:
	UWraithSettings* GetSettings() const;
	void SaveSettings();

	TArray<TSharedPtr<EGameType>> GameTypeOptionsSource;
};
