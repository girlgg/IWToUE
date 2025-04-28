#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"

class FTextureSettingsViewModel;

class IWTOUE_API STextureSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(STextureSettingsPage)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FTextureSettingsViewModel> InViewModel);

private:
	TSharedPtr<FTextureSettingsViewModel> ViewModel;
};
