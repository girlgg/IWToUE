#pragma once
#include "Interface/IAssetImporter.h"

class ICastMaterialImporter;

class FMaterialImporter final : public IAssetImporter
{
public:
	FMaterialImporter();
	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) override;
	virtual UClass* GetSupportedUClass() const override;

private:
	TSharedPtr<ICastMaterialImporter> MaterialImporterInternal;
};
