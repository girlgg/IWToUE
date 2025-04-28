#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FMapSettingsViewModel;

class IWTOUE_API SMapSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMapSettingsPage)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FMapSettingsViewModel> InViewModel);
private:
	TSharedPtr<FMapSettingsViewModel> ViewModel;
};
