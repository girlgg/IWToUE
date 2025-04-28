#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FModelSettingsViewModel;

class IWTOUE_API SModelSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SModelSettingsPage)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FModelSettingsViewModel> InViewModel);

private:
	TSharedPtr<FModelSettingsViewModel> ViewModel;
};
