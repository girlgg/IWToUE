#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FAnimationSettingsViewModel;

class IWTOUE_API SAnimationSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SAnimationSettingsPage)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FAnimationSettingsViewModel> InViewModel);

private:
	TSharedPtr<FAnimationSettingsViewModel> ViewModel;
};
