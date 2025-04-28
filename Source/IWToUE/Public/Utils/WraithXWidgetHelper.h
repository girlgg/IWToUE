#pragma once

#include "CoreMinimal.h"

namespace WraithXWidgetHelper
{
	TSharedRef<SWidget> CreateCategory(const FText& CategoryTitle, TSharedRef<SVerticalBox> CategoryContent);
	TSharedRef<SWidget> CreateSettingRow(const FText& SettingName, const FText& TooltipText,
	                                     TSharedRef<SWidget> SettingWidget);
	TSharedRef<SWidget> CreatePathSettingRow(const FText& SettingName, const FText& TooltipText,
	                                         const TAttribute<FText>& PathAttribute,
	                                         FOnTextChanged PathTextChangedHandler,
	                                         FOnTextCommitted PathTextCommittedHandler, FOnClicked BrowseClickHandler);
}
