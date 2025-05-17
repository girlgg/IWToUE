#pragma once
#include "Interface/IAssetImporter.h"

class ICastAnimationImporter;

class FAnimationImporter final : public IAssetImporter
{
public:
	FAnimationImporter();
	virtual ~FAnimationImporter() override = default;
	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) override;
	virtual UClass* GetSupportedUClass() const override;

private:
	TSharedPtr<ICastAnimationImporter> AnimImporter;
};
