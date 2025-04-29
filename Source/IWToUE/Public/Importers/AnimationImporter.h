#pragma once
#include "Interface/IAssetImporter.h"

class FAnimationImporter final : public IAssetImporter
{
public:
	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) override;
	virtual UClass* GetSupportedUClass() const override;
};
