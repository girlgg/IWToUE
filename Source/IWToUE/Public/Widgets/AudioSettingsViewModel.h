﻿#pragma once

#include "CoreMinimal.h"

class UWraithSettings;

class FAudioSettingsViewModel : public TSharedFromThis<FAudioSettingsViewModel>
{
public:
	FAudioSettingsViewModel();

	FText GetExportDirectory() const;

	FReply HandleBrowseExportDirectoryClicked();
	void HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

private:
	UWraithSettings* GetSettings() const;
	void SaveSettings();
};
