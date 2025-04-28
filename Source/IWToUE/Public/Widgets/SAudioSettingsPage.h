#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FAudioSettingsViewModel;

class IWTOUE_API SAudioSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAudioSettingsPage)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FAudioSettingsViewModel> InViewModel);

private:
	TSharedPtr<FAudioSettingsViewModel> ViewModel;
};
