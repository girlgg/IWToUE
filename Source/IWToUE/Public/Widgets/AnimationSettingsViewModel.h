#pragma once

#include "CoreMinimal.h"

class UWraithSettings;

class FAnimationSettingsViewModel : public TSharedFromThis<FAnimationSettingsViewModel>
{
public:
	FAnimationSettingsViewModel();

	FText GetExportDirectory() const;
	FText GetTargetSkeletonPath() const;

	FReply HandleBrowseExportDirectoryClicked();
	void HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	FReply HandleBrowseTargetSkeletonClicked();
	void HandleTargetSkeletonTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);

private:
	UWraithSettings* GetSettings() const;
	void SaveSettings();
};
