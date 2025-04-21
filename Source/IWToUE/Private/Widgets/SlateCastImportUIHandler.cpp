#include "Widgets/SlateCastImportUIHandler.h"

#include "SeLogChannels.h"
#include "Interfaces/IMainFrameModule.h"
#include "Widgets/CastOptionWindow.h"
#include "Windows/WindowsPlatformApplicationMisc.h"

bool FSlateCastImportUIHandler::GetImportOptions(FCastImportOptions& OutOptions, bool bShowDialog,
                                                 bool bIsAutomated,
                                                 const FString& FullPath, bool& bOutImportAll)
{
	bOutImportAll = false;

	if (!bShowDialog || bIsAutomated)
	{
		return true;
	}

	TSharedPtr<SWindow> ParentWindow;
	if (FModuleManager::Get().IsModuleLoaded("MainFrame"))
	{
		IMainFrameModule& MainFrame = FModuleManager::LoadModuleChecked<IMainFrameModule>("MainFrame");
		ParentWindow = MainFrame.GetParentWindow();
	}

	const float ImportWindowWidth = 450.0f;
	const float ImportWindowHeight = 750.0f;
	FVector2D ImportWindowSize = FVector2D(ImportWindowWidth, ImportWindowHeight);

	FSlateRect WorkAreaRect = FSlateApplicationBase::Get().GetPreferredWorkArea();
	FVector2D DisplayTopLeft(WorkAreaRect.Left, WorkAreaRect.Top);
	FVector2D DisplaySize(WorkAreaRect.Right - WorkAreaRect.Left, WorkAreaRect.Bottom - WorkAreaRect.Top);

	float ScaleFactor = FPlatformApplicationMisc::GetDPIScaleFactorAtPoint(DisplayTopLeft.X, DisplayTopLeft.Y);
	ImportWindowSize *= ScaleFactor;

	FVector2D WindowPosition = (DisplayTopLeft + (DisplaySize - ImportWindowSize) / 2.0f) / ScaleFactor;

	TSharedRef<SWindow> Window = SNew(SWindow)
		.Title(NSLOCTEXT("UnrealEd", "CastImportOpionsTitleRefactored", "Cast Import Options"))
		.SizingRule(ESizingRule::Autosized)
		.AutoCenter(EAutoCenter::None)
		.ClientSize(ImportWindowSize)
		.ScreenPosition(WindowPosition);

	UCastImportUI* ImportUI = NewObject<UCastImportUI>();
	TSharedPtr<SCastOptionWindow> OptionsWidget;
	Window->SetContent(
		SAssignNew(OptionsWidget, SCastOptionWindow)
		.ImportUI(ImportUI)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FullPath))
		.MaxWindowHeight(ImportWindowHeight)
		.MaxWindowWidth(ImportWindowWidth)
	);
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (OptionsWidget->ShouldImport())
	{
		OutOptions.PhysicsAsset = ImportUI->PhysicsAsset;
		OutOptions.Skeleton = ImportUI->Skeleton;
		OutOptions.bImportMaterial = ImportUI->bImportMaterial;
		OutOptions.TexturePathType = ImportUI->TexturePathType;
		OutOptions.GlobalMaterialPath = ImportUI->GlobalMaterialPath;
		OutOptions.TextureFormat = ImportUI->TextureFormat;
		OutOptions.bImportAsSkeletal = ImportUI->bImportAsSkeletal;
		OutOptions.bImportMesh = ImportUI->bImportMesh;
		OutOptions.bImportAnimations = ImportUI->bImportAnimations;
		OutOptions.bConvertRefPosition = ImportUI->bConvertRefPosition;
		OutOptions.bConvertRefAnim = ImportUI->bConvertRefAnim;
		OutOptions.bReverseFace = ImportUI->bReverseFace;
		OutOptions.bImportAnimationNotify = ImportUI->bImportAnimationNotify;
		OutOptions.bDeleteRootNodeAnim = ImportUI->bDeleteRootNodeAnim;
		OutOptions.MaterialType = ImportUI->MaterialType;

		bOutImportAll = OptionsWidget->ShouldImportAll();
		return true;
	}
	return false;
}
