#pragma once

#include "CoreMinimal.h"

class UWraithSettings;

class FTextureSettingsViewModel : public TSharedFromThis<FTextureSettingsViewModel>
{
public:
	FTextureSettingsViewModel(UWraithSettings* InSettings);

	FText GetExportDirectory() const;
	ECheckBoxState GetUseGlobalPathCheckState() const;
	FText GetGlobalTexturePath() const;
	bool IsGlobalTexturePathEnabled() const;

	FReply HandleBrowseExportDirectoryClicked();
	void HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void HandleUseGlobalPathCheckStateChanged(ECheckBoxState NewState);
	FReply HandleBrowseGlobalTexturePathClicked();
	void HandleGlobalTexturePathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);


private:
	void SaveSettings();
	UWraithSettings* Settings;
};
