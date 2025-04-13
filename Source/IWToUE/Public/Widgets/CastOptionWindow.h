﻿#pragma once
#include "CastImportUI.h"


class SCastOptionWindow final : public SCompoundWidget
{
public:
	SLATE_BEGIN_ARGS(SCastOptionWindow)
			: _ImportUI(NULL)
			  , _WidgetWindow()
			  , _FullPath()
			  , _MaxWindowHeight(0.0f)
			  , _MaxWindowWidth(0.0f)
		{
		}

		SLATE_ARGUMENT(UCastImportUI*, ImportUI)
		SLATE_ARGUMENT(TSharedPtr<SWindow>, WidgetWindow)
		SLATE_ARGUMENT(FText, FullPath)
		SLATE_ARGUMENT(float, MaxWindowHeight)
		SLATE_ARGUMENT(float, MaxWindowWidth)
	SLATE_END_ARGS()

	void Construct(const FArguments& InArgs);
	virtual bool SupportsKeyboardFocus() const override { return true; }

	FReply OnImport()
	{
		bShouldImport = true;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	FReply OnImportAll()
	{
		bShouldImportAll = true;
		return OnImport();
	}

	FReply OnCancel()
	{
		bShouldImport = false;
		bShouldImportAll = false;
		if (WidgetWindow.IsValid())
		{
			WidgetWindow.Pin()->RequestDestroyWindow();
		}
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape)
		{
			return OnCancel();
		}

		return FReply::Unhandled();
	}

	bool ShouldImport() const
	{
		return bShouldImport;
	}

	bool ShouldImportAll() const
	{
		return bShouldImportAll;
	}

	SCastOptionWindow()
		: ImportUI(nullptr)
		  , bShouldImport(false)
		  , bShouldImportAll(false)
	{
	}

private:
	EActiveTimerReturnType SetFocusPostConstruct(double InCurrentTime, float InDeltaTime);
	bool CanImport() const;
	FReply OnResetToDefaultClick() const;
	FText GetImportTypeDisplayText() const;
	
	UCastImportUI* ImportUI;
	TSharedPtr<IDetailsView> DetailsView;
	TWeakPtr<SWindow> WidgetWindow;
	TSharedPtr<SButton> ImportAllButton;
	bool bShouldImport;
	bool bShouldImportAll;
};
