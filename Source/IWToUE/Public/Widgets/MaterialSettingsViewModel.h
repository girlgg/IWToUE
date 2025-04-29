#pragma once

#include "CoreMinimal.h"

class UWraithSettings;

class FMaterialSettingsViewModel : public TSharedFromThis<FMaterialSettingsViewModel>
{
public:
	FMaterialSettingsViewModel();

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
	UWraithSettings* GetSettings() const;
	void SaveSettings();
};

