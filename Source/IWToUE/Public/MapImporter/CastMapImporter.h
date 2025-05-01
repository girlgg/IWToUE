#pragma once

#include "CoreMinimal.h"
#include "Services/AssetImportManager.h"
#include "UObject/Object.h"
#include "Widgets/CastImportUI.h"
#include "WraithX/WraithXViewModel.h"

class FCastMapImporter
{
public:
	void RegisterMenus();

	TSharedRef<SWidget> CreateToolbarDropdown();

private:
	void ImportMapFromJson();
	void ImportMapFronCord();
	void ImportAssetFronCord();

	FReply OnConfirmImport(TSharedRef<SWindow> DialogWindow, TSharedPtr<SEditableTextBox> JsonFilePathBox,
					   TSharedPtr<SEditableTextBox> ModelFolderPathBox, TSharedPtr<SEditableTextBox> ModelPathBox,
					   TSharedPtr<SEditableTextBox> InGlobalMaterialPathBox);
	FReply OnCancelImport(TSharedRef<SWindow> DialogWindow);
	FReply OnOpenJsonFile(TSharedPtr<SEditableTextBox> JsonFilePathBox);
	FReply OnOpenModelFolder(TSharedPtr<SEditableTextBox> ModelFolderPathBox);

	void HandleImportTypeChanged(ECastTextureImportType NewImportType);
	void OnImportTypeSelectionChanged(TSharedPtr<FString> NewSelection, ESelectInfo::Type SelectInfo);
	TSharedRef<SWidget> GenerateImportTypeWidget(TSharedPtr<FString> Item);
	FText GetSelectedImportTypeText() const;

	TSharedPtr<SEditableTextBox> GlobalMaterialPathBox;
	ECastTextureImportType CurrentImportType{ECastTextureImportType::CastTIT_Default};

	TSharedRef<FWraithXViewModel> ViewModel = MakeShared<FWraithXViewModel>();
};
