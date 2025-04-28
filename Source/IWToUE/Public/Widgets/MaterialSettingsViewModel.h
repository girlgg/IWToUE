#pragma once

#include "CoreMinimal.h"

class UWraithSettings;

class FMaterialSettingsViewModel : public TSharedFromThis<FMaterialSettingsViewModel>
{
public:
	FMaterialSettingsViewModel(UWraithSettings* InSettings);

	FText GetExportDirectory() const;
	ECheckBoxState GetUseGlobalPathCheckState() const;
	FText GetGlobalMaterialPath() const;
	bool IsGlobalMaterialPathEnabled() const;

	FReply HandleBrowseExportDirectoryClicked();
	void HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void HandleUseGlobalPathCheckStateChanged(ECheckBoxState NewState);
	FReply HandleBrowseGlobalMaterialPathClicked();
	void HandleGlobalMaterialPathTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

private:
	void SaveSettings();
	UWraithSettings* Settings;
};

