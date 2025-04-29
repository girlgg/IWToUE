#pragma once
#include "Interface/IAssetImporter.h"

class ICastMeshImporter;
struct FCastMaterialInfo;
class ICastMaterialImporter;
struct FCastRoot;
struct FWraithXModel;

class FModelImporter final : public IAssetImporter
{
public:
	FModelImporter();
	virtual ~FModelImporter() override = default;

	virtual bool Import(const FAssetImportContext& Context, TArray<UObject*>& OutCreatedObjects,
	                    FOnAssetImportProgress ProgressDelegate) override;
	virtual UClass* GetSupportedUClass() const override;

private:
	/**
	 * @brief Processes dependent Textures and Materials for a model.
	 * Ensures textures are imported/cached.
	 * Builds the material list (FCastMaterialInfo) within the provided FCastRoot.
	 * Optionally creates UMaterialInstances (controlled by bCreateMaterialInstances).
	 * @param InOutModelData The model data read from the handler. Texture objects will be filled here.
	 * @param OutSceneRoot The CastRoot where the processed FCastMaterialInfo list will be stored.
	 * @param Context The overall import context.
	 * @param MaterialImporter Instance used to create material assets if requested.
	 * @param bCreateMaterialInstances If true, UMaterialInstances will be created and cached immediately. If false, only FCastMaterialInfo is prepared.
	 * @param ProgressDelegate Delegate for reporting sub-progress.
	 * @return True if dependencies were processed successfully (even if some textures failed), false on critical error.
	 */
	static bool ProcessModelDependencies(
		FWraithXModel& InOutModelData,
		FCastRoot& OutSceneRoot,
		const FAssetImportContext& Context,
		ICastMaterialImporter* MaterialImporter,
		bool bCreateMaterialInstances,
		const FOnAssetImportProgress& ProgressDelegate
	);

	/** Helper to get or create a material instance. */
	static UMaterialInterface* GetOrCreateMaterialInstance(
		const FCastMaterialInfo& PreparedMaterialInfo,
		const FAssetImportContext& Context,
		ICastMaterialImporter* MaterialImporter,
		const FString& PreferredPackagePath
	);


	// Dependencies needed for creating the final mesh
	TSharedPtr<ICastMeshImporter> MeshImporter;
	// Material importer instance (could be passed in constructor or created here)
	TSharedPtr<ICastMaterialImporter> MaterialImporterInternal;
};
