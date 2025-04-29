#pragma once

#include "Interface/IAssetImporter.h"

class FImageImporter final : public IAssetImporter
{
	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) override;

	virtual UClass* GetSupportedUClass() const override;
};
