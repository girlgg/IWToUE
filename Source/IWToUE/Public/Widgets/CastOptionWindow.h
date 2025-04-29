#pragma once
#include "CastImportUI.h"
#include "CastManager/CastImportOptions.h"

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

	FReply OnRequestImport()
	{
		bShouldImport = true;
		CloseWindow();
		return FReply::Handled();
	}

	FReply OnRequestImportAll()
	{
		bShouldImportAll = true;
		return OnRequestImport();
	}

	FReply OnRequestCancel()
	{
		bShouldImport = false;
		bShouldImportAll = false;
		CloseWindow();
		return FReply::Handled();
	}

	virtual FReply OnKeyDown(const FGeometry& MyGeometry, const FKeyEvent& InKeyEvent) override
	{
		if (InKeyEvent.GetKey() == EKeys::Escape) { return OnRequestCancel(); }
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
	void CloseWindow();
	EActiveTimerReturnType SetFocusPostConstruct(double InCurrentTime, float InDeltaTime);
	bool CanImport() const;
	FReply OnResetToDefaultClick() const;

	UCastImportUI* ImportUI;
	TSharedPtr<IDetailsView> DetailsView;
	TWeakPtr<SWindow> WidgetWindow;
	TSharedPtr<SButton> ImportAllButton;
	
	bool bShouldImport;
	bool bShouldImportAll;
};
