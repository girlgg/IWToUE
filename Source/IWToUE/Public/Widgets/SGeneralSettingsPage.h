#pragma once

#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "WraithX/WraithSettingsManager.h"


class FGeneralSettingsViewModel;

class IWTOUE_API SGeneralSettingsPage : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SGeneralSettingsPage)
		{
		}

	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs, TSharedRef<FGeneralSettingsViewModel> InViewModel);

private:
	TSharedPtr<FGeneralSettingsViewModel> ViewModel;
};
