#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FMaterialSettingsViewModel;

class IWTOUE_API SMaterialSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SMaterialSettingsPage) {}
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FMaterialSettingsViewModel> InViewModel);

private:
	TSharedPtr<FMaterialSettingsViewModel> ViewModel;
};
