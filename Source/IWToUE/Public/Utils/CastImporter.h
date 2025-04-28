#pragma once

#include "CastManager/CastImportOptions.h"
#include "Widgets/CastImportUI.h"
#include "CastManager/CastManager.h"
#include "CastManager/CastModel.h"
#include "CastManager/CastScene.h"

struct FCastCurveInfo;
class FCastFileTexture;
struct FCastScene;
struct FCastSceneInfo;
class FCastNode;

namespace CastScene
{
	class FImportSkeletalMeshArgs
	{
	public:
		FImportSkeletalMeshArgs()
			: InParent(nullptr)
			  , Name(NAME_None)
			  , Flags(RF_NoFlags)
			  , LodIndex(0)
			  , OutData(nullptr)
			  , bCreateRenderData(true)
			  , OrderedMaterialNames(nullptr)
			  , ImportMaterialOriginalNameData(nullptr)
			  , bMapMorphTargetToTimeZero(false)
		{
		}

		UObject* InParent;
		FName Name;
		EObjectFlags Flags;
		int32 LodIndex;
		FSkeletalMeshImportData* OutData;
		bool bCreateRenderData;
		TArray<FName>* OrderedMaterialNames;

		TArray<FName>* ImportMaterialOriginalNameData;
		bool bMapMorphTargetToTimeZero;
	};
}

struct FCastMaterial
{
	FCastMaterialInfo* CastMaterial{nullptr};
	UMaterialInterface* Material{nullptr};

	FString GetName() const
	{
		return CastMaterial ? CastMaterial->GetName() : Material ? Material->GetName() : TEXT("None");
	}
};

class FCastImporter
{
public:
	FCastImporter();
	~FCastImporter();
	void CleanUp();

	void ReleaseScene();

	int32 GetImportType(const FString& InFilename);
	bool OpenFile(FString Filename);
	bool GetSceneInfo(FString Filename);
	bool ImportFile(FString Filename);
	bool ImportFromFile(FString Filename);
	void AnalysisMaterial(const FString& ParentPath, FString MaterialPath, FString TexturePath,
	                      FString TextureFormat, bool bUseGlobalTexturePath = false);
	bool AnalysisTexture(FCastTextureInfo& Texture, FString ParentPath, FString TextureLineText, FString TexturePath,
	                     const FString& ImageFormat);

	bool AnalysisSetting(FCastSettingInfo& Setting, FString SettingLineText);

	bool ImportTexture(FCastTextureInfo& Texture, const FString& FilePath, const FString& ParentPath, bool bSRGB);
	static FString NoIllegalSigns(const FString& InString);
	UMaterialInterface* CreateMaterialInstance(const FCastMaterialInfo& Material, const UObject* ParentPackage);

	void ClearAllCaches();

	static FCastImporter* GetInstance(bool bDoNotCreate = false);
	static void DeleteInstance();

	static FString MakeName(const FString& Name);

	FCastImportOptions* GetImportOptions() const;
	FCastImportOptions* GetImportOptions(UCastImportUI* ImportUI, bool bShowOptionDialog,
	                                     bool bIsAutomated, const FString& FullPath,
	                                     bool& OutOperationCanceled,
	                                     bool& OutImportAll, const FString& InFilename);

	USkeletalMesh* ImportSkeletalMesh(CastScene::FImportSkeletalMeshArgs& ImportSkeletalMeshArgs);
	UStaticMesh* ImportStaticMesh(UObject* InParent, const FName InName, EObjectFlags Flags);
	UAnimSequence* ImportAnim(UObject* InParent, USkeleton* Skeleton);

	void BuildStaticMeshFromModel(UObject* ParentPackage, FCastRoot& SceneRoot, FCastModelInfo& Model, UStaticMesh* StaticMesh);

	static UObject* CreateAssetOfClass(UClass* AssetClass, FString ParentPackageName, FString ObjectName,
	                                   bool bAllowReplace = false);

	template <class T>
	static T* CreateAsset(FString ParentPackageName, FString ObjectName, bool bAllowReplace = false);

protected:
	enum IMPORTPHASE
	{
		NOTSTARTED,
		FILEOPENED,
		IMPORTED,
		FIXEDANDCONVERTED,
	};

	struct BoneCurve
	{
		TArray<float> PositionX;
		TArray<float> PositionY;
		TArray<float> PositionZ;

		TArray<FVector4> Rotation;

		TArray<float> ScaleX;
		TArray<float> ScaleY;
		TArray<float> ScaleZ;

		ECastAnimImportType AnimMode{ECastAnimImportType::CastAIT_Absolutely};
	};

	bool ImportAnimation(USkeleton* Skeleton, UAnimSequence* DestSeq, FCastAnimationInfo& Animation);

	void InitializeAnimationController(IAnimationDataController& Controller, const FCastAnimationInfo& Animation);
	void FinalizeController(IAnimationDataController& Controller, UAnimSequence* DestSeq);

	uint32 CalculateNumberOfFrames(const FCastAnimationInfo& Animation);
	TMap<FString, BoneCurve> ExtractCurves(USkeleton* Skeleton, const FCastAnimationInfo& Animation);
	void AssignCurveValues(BoneCurve& BoneCurveInfo, const FCastCurveInfo& Curve);
	void AddBoneCurves(IAnimationDataController& Controller, const FReferenceSkeleton& RefSkeleton);
	void PopulateBoneTracks(IAnimationDataController& Controller, const TMap<FString, BoneCurve>& BoneMap,
	                        const FReferenceSkeleton& RefSkeleton, uint32 NumberOfFrames);

	void SetPositionalKeys(TArray<FVector3f>& PositionalKeys, const BoneCurve& BoneCurveInfo,
	                       const FVector& BoneLocation, uint32 NumberOfFrames);
	void SetRotationalKeys(TArray<FQuat4f>& RotationalKeys, const BoneCurve& BoneCurveInfo,
	                       const FQuat& BoneRotation, const FReferenceSkeleton& RefSkeleton, int32 BoneID,
	                       uint32 NumberOfFrames);
	void SetScalingKeys(TArray<FVector3f>& ScalingKeys, const BoneCurve& BoneCurveInfo, const FVector& BoneScale,
	                    uint32 NumberOfFrames);

	void AddAnimationNotifies(UAnimSequence* DestSeq, const FCastAnimationInfo& Animation, float AnimationRate);

	template <typename T>
	void ExtendToFrameCount(TArray<T>& Keys, int32 NumberOfFrames);
	template <typename T>
	void InterpolateAnimationKeyframes(TArray<T>& BoneCurve, const TArray<uint32>& KeyFrameBuffer,
	                                   const TArray<FVariant>& KeyValueBuffer);
	template <typename T>
	T LerpValue(const uint32& LastKey,
	            const uint32& CurrentKey, const uint32& EndKey, const T& LastValue, const T& EndValue);

public:
	TSharedPtr<FCastImportOptions> ImportOptions;
	FCastSceneInfo SceneInfo;
	FString FileBasePath;

	FMD5Hash Md5Hash;

protected:
	static TSharedPtr<FCastImporter> StaticInstance;
	IMPORTPHASE CurPhase;

private:
	TArray<TWeakObjectPtr<UObject>> CreatedObjects;

	FCastManager* CastManager{nullptr};
	TArray<FCastMaterialInfo> ImportedMaterials;
};

template <class T>
T* FCastImporter::CreateAsset(FString ParentPackageName, FString ObjectName, bool bAllowReplace)
{
	return (T*)CreateAssetOfClass(T::StaticClass(), ParentPackageName, ObjectName, bAllowReplace);
}

template <typename T>
void FCastImporter::ExtendToFrameCount(TArray<T>& Keys, int32 NumberOfFrames)
{
	T LastKey = Keys.Last();
	while (Keys.Num() < NumberOfFrames) Keys.Add(LastKey);
}

template <typename T>
FORCEINLINE void FCastImporter::InterpolateAnimationKeyframes(TArray<T>& BoneCurve,
                                                              const TArray<uint32>& KeyFrameBuffer,
                                                              const TArray<FVariant>& KeyValueBuffer)
{
	uint32 CurrentFrame = 0;
	uint32 LastKeyFrame = 0;
	T LastKeyValue = T();
	for (int32 i = 0; i < KeyFrameBuffer.Num(); ++i)
	{
		if (i == 0)
		{
			BoneCurve.Add(KeyValueBuffer[i].GetValue<T>());
		}
		else
		{
			while (CurrentFrame <= KeyFrameBuffer[i])
			{
				while (!BoneCurve.IsValidIndex(CurrentFrame)) BoneCurve.AddZeroed();
				BoneCurve[CurrentFrame] = LerpValue(LastKeyFrame,
				                                    CurrentFrame,
				                                    KeyFrameBuffer[i],
				                                    LastKeyValue,
				                                    KeyValueBuffer[i].GetValue<T>());
				++CurrentFrame;
			}
		}
		LastKeyFrame = KeyFrameBuffer[i];
		LastKeyValue = KeyValueBuffer[i].GetValue<T>();
	}
}

template <typename T>
FORCEINLINE T FCastImporter::LerpValue(const uint32& LastKey, const uint32& CurrentKey, const uint32& EndKey,
                                       const T& LastValue,
                                       const T& EndValue)
{
	return FMath::Lerp(LastValue, EndValue, (float)(CurrentKey - LastKey) / (float)(EndKey - LastKey));
}
