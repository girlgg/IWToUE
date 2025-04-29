#pragma once

#include "CoreMinimal.h"

class UWraithSettings;

class FMapSettingsViewModel : public TSharedFromThis<FMapSettingsViewModel>
{
public:
	FMapSettingsViewModel();

	FText GetExportDirectory() const;

	FReply HandleBrowseExportDirectoryClicked();
	void HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

private:
	UWraithSettings* GetSettings() const;
	void SaveSettings();
};
