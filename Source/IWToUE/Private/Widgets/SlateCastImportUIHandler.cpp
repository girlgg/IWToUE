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

	UCastImportUI* ImportUISession = NewObject<UCastImportUI>(GetTransientPackage());
	ImportUISession->LoadConfig();

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

	TSharedPtr<SCastOptionWindow> OptionsWidget;
	Window->SetContent(
		SAssignNew(OptionsWidget, SCastOptionWindow)
		.ImportUI(ImportUISession)
		.WidgetWindow(Window)
		.FullPath(FText::FromString(FullPath))
		.MaxWindowHeight(ImportWindowHeight)
		.MaxWindowWidth(ImportWindowWidth)
	);
	FSlateApplication::Get().AddModalWindow(Window, ParentWindow, false);

	if (OptionsWidget->ShouldImport())
	{
		OutOptions.bImportMesh = ImportUISession->bImportMesh;
		OutOptions.bImportAsSkeletal = ImportUISession->bImportAsSkeletal;
		OutOptions.Skeleton = ImportUISession->Skeleton;
		OutOptions.bReverseFace = ImportUISession->bReverseFace;
		OutOptions.bPhysicsAsset = ImportUISession->bPhysicsAsset;
		OutOptions.PhysicsAsset = ImportUISession->PhysicsAsset;
		OutOptions.bImportAnimations = ImportUISession->bImportAnimations;
		OutOptions.bImportAnimationNotify = ImportUISession->bImportAnimationNotify;
		OutOptions.bDeleteRootNodeAnim = ImportUISession->bDeleteRootNodeAnim;
		OutOptions.bConvertRefPosition = ImportUISession->bConvertRefPosition;
		OutOptions.bConvertRefAnim = ImportUISession->bConvertRefAnim;
		OutOptions.bImportMaterial = ImportUISession->bImportMaterial;
		OutOptions.MaterialType = ImportUISession->MaterialType;
		OutOptions.TexturePathType = ImportUISession->TexturePathType;
		OutOptions.GlobalMaterialPath = ImportUISession->GlobalMaterialPath;
		OutOptions.TextureFormat = ImportUISession->TextureFormat;

		ImportUISession->SaveConfig();

		bOutImportAll = OptionsWidget->ShouldImportAll();
		return true;
	}
	return false;
}
