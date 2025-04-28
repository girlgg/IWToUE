#pragma once

#include "CoreMinimal.h"

enum class EMaterialExportPathMode : uint8;
enum class ETextureExportPathMode : uint8;
class UWraithSettings;

class FModelSettingsViewModel : public TSharedFromThis<FModelSettingsViewModel>
{
public:
	FModelSettingsViewModel(UWraithSettings* InSettings);

	FText GetExportDirectory() const;
	ECheckBoxState GetExportLoDsCheckState() const;
	TOptional<int32> GetMaxLODLevel() const;
	bool IsMaxLODLevelEnabled() const;
	ECheckBoxState GetExportVertexColorCheckState() const;

	TSharedPtr<ETextureExportPathMode> GetCurrentTexturePathMode() const;
	TArray<TSharedPtr<ETextureExportPathMode>>* GetTexturePathModeOptions();
	FText GetTexturePathModeDisplayName(ETextureExportPathMode Value) const;
	bool IsTexturePathModeEnabled() const;

	TSharedPtr<EMaterialExportPathMode> GetCurrentMaterialPathMode() const;
	TArray<TSharedPtr<EMaterialExportPathMode>>* GetMaterialPathModeOptions();
	FText GetMaterialPathModeDisplayName(EMaterialExportPathMode Value) const;
	bool IsMaterialPathModeEnabled() const;

	FReply HandleBrowseExportDirectoryClicked();
	void HandleExportDirectoryTextCommitted(const FText& NewText, ETextCommit::Type CommitInfo);
	void HandleExportLODsCheckStateChanged(ECheckBoxState NewState);
	void HandleMaxLODLevelChanged(int32 NewValue);
	void HandleExportVertexColorCheckStateChanged(ECheckBoxState NewState);
	void HandleTexturePathModeSelectionChanged(TSharedPtr<ETextureExportPathMode> NewSelection,
	                                           ESelectInfo::Type SelectInfo);
	void HandleMaterialPathModeSelectionChanged(TSharedPtr<EMaterialExportPathMode> NewSelection,
	                                            ESelectInfo::Type SelectInfo);

private:
	void SaveSettings();
	void PopulateEnumOptions();

	UWraithSettings* Settings;

	TArray<TSharedPtr<ETextureExportPathMode>> TexturePathModeOptionsSource;
	TArray<TSharedPtr<EMaterialExportPathMode>> MaterialPathModeOptionsSource;
};
