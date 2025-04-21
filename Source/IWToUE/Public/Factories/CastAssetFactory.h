#pragma once

#include "CoreMinimal.h"
#include "Factories/Factory.h"
#include "Utils/CastImporter.h"
#include "CastAssetFactory.generated.h"

class FCastToUnrealConverter;
class ICastImportUIHandler;
class FCastReader;
class FLargeMemoryReader;

#define RETURN_IF_PASS(condition, message) \
if (condition) { \
UE_LOG(LogCast, Error, TEXT("%s"), message); \
GEditor->GetEditorSubsystem<UImportSubsystem>()->BroadcastAssetPostImport(this, nullptr); \
return nullptr; \
}

class FSceneCleanupGuard
{
public:
	FSceneCleanupGuard(FCastImporter* Importer) : ImporterPtr(Importer)
	{
	}

	~FSceneCleanupGuard() { if (ImporterPtr) ImporterPtr->ReleaseScene(); }

private:
	FCastImporter* ImporterPtr;
};

UCLASS()
class IWTOUE_API UCastAssetFactory : public UFactory
{
	GENERATED_BODY()

public:
	UCastAssetFactory(const FObjectInitializer& ObjectInitializer);

	//~ UObject Interface
	virtual void PostInitProperties() override;
	//~ End of UObject Interface

	//~ Begin UFactor Interface
	virtual bool ConfigureProperties() override;
	virtual bool DoesSupportClass(UClass* Class) override;
	virtual FText GetDisplayName() const override;
	virtual UClass* ResolveSupportedClass() override;
	UObject* HandleExistingAsset(UObject* InParent, FName InName, const FString& InFilename);
	static void HandleMaterialImport(FString&& ParentPath, const FString& InFilename, FCastImporter* CastImporter,
	                                 FCastImportOptions* ImportOptions);
	static UObject* ExecuteImportProcess(UObject* InParent, FName InName, EObjectFlags Flags, const FString& InFilename,
	                                     FCastImporter* CastImporter, FCastImportOptions* ImportOptions,
	                                     FString InCurrentFilename);
	virtual UObject* FactoryCreateFile(UClass* InClass, UObject* InParent, FName InName, EObjectFlags Flags,
	                                   const FString& Filename, const TCHAR* Parms, FFeedbackContext* Warn,
	                                   bool& bOutOperationCanceled) override;
	//~ End of UFactor Interface

	bool DetectImportType(const FString& InFilename);

protected:
	FText GetImportTaskText(const FText& TaskText) const;

private:
	void InitializeDependencies();
	void SaveCreatedPackages(const TArray<UObject*>& CreatedObjects);
	UObject* HandleImportFailure(const FString& ErrorMessage, UFactory* Factory, FFeedbackContext* Warn,
	                             bool& bOutOperationCanceled);

public:
	UPROPERTY()
	TObjectPtr<UCastImportUI> ImportUI;

private:
	FCastImportOptions ImportOptions;

	TSharedPtr<FCastReader> Reader;
	TSharedPtr<FCastToUnrealConverter> Converter;
	TSharedPtr<ICastImportUIHandler> UIHandler;

	bool bShowOption;
	bool bDetectImportTypeOnImport;
	bool bOperationCanceled;
	bool bImportAll = false;
};
