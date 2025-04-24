#include "WraithX/WraithX.h"

#include "CastManager/CastAnimation.h"
#include "CastManager/CastRoot.h"
#include "GameInfo/ModernWarfare6.h"
#include "Utils/CoDAssetHelper.h"
#include "WraithX/GameProcess.h"

void FWraithX::InitializeGame()
{
	if (ProcessInstance.IsValid())
	{
		ProcessInstance.Reset();
	}
	ProcessInstance = MakeShared<FGameProcess>();
	// ProcessInstance->OnOnAssetLoadingDelegate.AddRaw(this, &FWraithX::OnAssetLoadingCall);
	// FCoDAssetDatabase::Get().GetTaskCompletedHandle().AddRaw(this, &FWraithX::OnAssetInitCompletedCall);
}

void FWraithX::RefreshGame()
{
	if (ProcessInstance)
	{
		// ProcessInstance->LoadGameFromParasyte();
		FCoDAssetDatabase::Get().GetTaskCompletedHandle().AddRaw(this, &FWraithX::OnAssetInitCompletedCall);
	}
}

void FWraithX::ImportAnim(FString ImportPath, TSharedPtr<FCoDAsset> Asset)
{
	TSharedPtr<FCoDAnim> ModelInfo = StaticCastSharedPtr<FCoDAnim>(Asset);

	FWraithXAnim GenericAnim;
	LoadGenericAnimAsset(GenericAnim, ModelInfo);

	FCastAnimationInfo AnimResult;
	TranslateXAnim(AnimResult, GenericAnim);
}

void FWraithX::ImportModel(FString ImportPath, TSharedPtr<FCoDAsset> Asset)
{
	TSharedPtr<FCoDModel> ModelInfo = StaticCastSharedPtr<FCoDModel>(Asset);
	FWraithXModel GenericModel;
	LoadGenericModelAsset(GenericModel, ModelInfo);

	// 准备材质
	for (FWraithXModelLod& LodModel : GenericModel.ModelLods)
	{
		for (FWraithXMaterial& Material : LodModel.Materials)
		{
			// 导出材质图像名
			// 导出材质图像
			// 注意重复性校验
		}
	}
	// 准备模型
	const int32 LodCount = GenericModel.ModelLods.Num();

	FCastRoot SceneRoot;
	SceneRoot.Models.Reserve(LodCount);
	SceneRoot.ModelLodInfo.Reserve(LodCount);

	for (int32 LodIdx = 0; LodIdx < LodCount; ++LodIdx)
	{
		FCastModelInfo ModelResult;

		TranslateXModel(ModelResult, GenericModel, LodIdx);

		FCastModelLod ModelLod;
		ModelLod.Distance = GenericModel.ModelLods[LodIdx].LodDistance;
		ModelLod.MaxDistance = GenericModel.ModelLods[LodIdx].LodMaxDistance;

		ModelLod.ModelIndex = SceneRoot.Models.Add(ModelResult);
		SceneRoot.ModelLodInfo.Add(ModelLod);
	}
	// 导出模型
}

void FWraithX::ImportImage(FString ImportPath, TSharedPtr<FCoDAsset> Asset)
{
	TSharedPtr<FCoDImage> Image = StaticCastSharedPtr<FCoDImage>(Asset);
	TArray<uint8> ImageData;
	uint8 ImageFormat = 0;

	if (Image->bIsFileEntry)
	{
		switch (ProcessInstance->GetCurrentGameType())
		{
		case CoDAssets::ESupportedGames::WorldAtWar:
		case CoDAssets::ESupportedGames::ModernWarfare:
		case CoDAssets::ESupportedGames::ModernWarfare2:
		case CoDAssets::ESupportedGames::ModernWarfare3:
		case CoDAssets::ESupportedGames::BlackOps:
			break;
		case CoDAssets::ESupportedGames::BlackOps2:
			break;
		case CoDAssets::ESupportedGames::BlackOps3:
		case CoDAssets::ESupportedGames::BlackOps4:
		case CoDAssets::ESupportedGames::BlackOpsCW:
		case CoDAssets::ESupportedGames::ModernWarfare4:
		case CoDAssets::ESupportedGames::ModernWarfare5:
			break;
		default:
			break;
		}
	}
	else
	{
		switch (ProcessInstance->GetCurrentGameType())
		{
		case CoDAssets::ESupportedGames::BlackOps3:
			break;
		case CoDAssets::ESupportedGames::BlackOps4:
			break;
		case CoDAssets::ESupportedGames::BlackOpsCW:
			break;
		case CoDAssets::ESupportedGames::Ghosts:
			break;
		case CoDAssets::ESupportedGames::AdvancedWarfare:
			break;
		case CoDAssets::ESupportedGames::ModernWarfareRemastered:
			break;
		case CoDAssets::ESupportedGames::ModernWarfare2Remastered:
			break;
		case CoDAssets::ESupportedGames::ModernWarfare4:
			break;
		case CoDAssets::ESupportedGames::ModernWarfare5:
			break;
		case CoDAssets::ESupportedGames::ModernWarfare6:
			ImageFormat = FModernWarfare6::ReadXImage(ProcessInstance, Image, ImageData);
		// FModernWarfare6
			break;
		case CoDAssets::ESupportedGames::WorldWar2:
			break;
		case CoDAssets::ESupportedGames::Vanguard:
			break;
		default:
			break;
		}
	}
	if (!ImageData.IsEmpty())
	{
		// UTexture2D* Texture = FCoDAssetHelper::CreateTextureFromDDSData(ImageData,
		//                                                                 Image->Width,
		//                                                                 Image->Height,
		//                                                                 ImageFormat,
		//                                                                 Image->AssetName);
		// FCoDAssetHelper::SaveObjectToPackage(Texture, Image->AssetName);
	}
}

void FWraithX::ImportSound(FString ImportPath, TSharedPtr<FCoDAsset> Asset)
{
	TSharedPtr<FCoDSound> Sound = StaticCastSharedPtr<FCoDSound>(Asset);

	FWraithXSound OutSound;

	switch (ProcessInstance->GetCurrentGameType())
	{
	case CoDAssets::ESupportedGames::ModernWarfare6:
		FModernWarfare6::ReadXSound(OutSound, ProcessInstance, Sound);
		break;
	default:
		break;
	}
}

void FWraithX::ImportMaterial(FString ImportPath, TSharedPtr<FCoDAsset> Asset)
{
	TSharedPtr<FCoDMaterial> Material = StaticCastSharedPtr<FCoDMaterial>(Asset);
	FWraithXMaterial XMaterial;

	switch (ProcessInstance->GetCurrentGameType())
	{
	case CoDAssets::ESupportedGames::ModernWarfare6:
		FModernWarfare6::ReadXMaterial(XMaterial, ProcessInstance, Material->AssetPointer);
		break;
	default:
		break;
	}
}

void FWraithX::ImportSelection(FString ImportPath, TArray<TSharedPtr<FCoDAsset>> Selection)
{
	for (auto& Asset : Selection)
	{
		switch (Asset->AssetType)
		{
		case EWraithAssetType::Animation:
			ImportAnim(ImportPath, Asset);
			break;
		case EWraithAssetType::Image:
			ImportImage(ImportPath, Asset);
			break;
		case EWraithAssetType::Model:
			ImportModel(ImportPath, Asset);
			break;
		case EWraithAssetType::Sound:
			ImportSound(ImportPath, Asset);
			break;
		case EWraithAssetType::RawFile:
			break;
		case EWraithAssetType::Material:
			ImportMaterial(ImportPath, Asset);
			break;
		default:
			break;
		}
	}
}

void FWraithX::LoadGenericAnimAsset(FWraithXAnim& OutAnim, TSharedPtr<FCoDAnim> AnimInfo)
{
	switch (ProcessInstance->GetCurrentGameType())
	{
	case CoDAssets::ESupportedGames::ModernWarfare6:
		FModernWarfare6::ReadXAnim(OutAnim, ProcessInstance, AnimInfo);
		break;
	default:
		break;
	}
}

void FWraithX::LoadGenericModelAsset(FWraithXModel& OutModel, TSharedPtr<FCoDModel> ModelInfo)
{
	switch (ProcessInstance->GetCurrentGameType())
	{
	case CoDAssets::ESupportedGames::ModernWarfare6:
		FModernWarfare6::ReadXModel(OutModel, ProcessInstance, ModelInfo);
		break;
	default:
		break;
	}
}

void FWraithX::TranslateXModel(FCastModelInfo& OutModel, FWraithXModel& InModel, int32 LodIdx)
{
	OutModel.Name = InModel.ModelName;

	FWraithXModelLod& LodRef = InModel.ModelLods[LodIdx];

	TMap<FString, uint32> MaterialMap;

	OutModel.Materials.Reserve(LodRef.Materials.Num());
	for (int32 MatIdx = 0; MatIdx < LodRef.Materials.Num(); ++MatIdx)
	{
		FWraithXMaterial& MatRef = LodRef.Materials[MatIdx];

		if (MaterialMap.Contains(MatRef.MaterialName))
		{
			LodRef.Submeshes[MatIdx].MaterialIndex = MaterialMap[MatRef.MaterialName];
		}
		else
		{
			FCastMaterialInfo& MaterialInfo = OutModel.Materials.AddDefaulted_GetRef();
			MaterialInfo.Name = MatRef.MaterialName;

			// TODO 加载材质
			for (const FWraithXImage& Image : MatRef.Images)
			{
				FCastTextureInfo& Texture = MaterialInfo.Textures.AddDefaulted_GetRef();
				Texture.TextureName = Image.ImageName;

				FString TextureSemantic = FString::Printf(TEXT("unk_semantic_0x%x"), Image.SemanticHash);
				Texture.TextureSemantic = TextureSemantic;
			}
			MaterialMap.Emplace(MatRef.MaterialName, OutModel.Materials.Num() - 1);
		}
	}
	if (InModel.IsModelStreamed)
	{
		switch (ProcessInstance->GetCurrentGameType())
		{
		case CoDAssets::ESupportedGames::ModernWarfare6:
			FModernWarfare6::LoadXModel(ProcessInstance, InModel, LodRef, OutModel);
			break;
		default:
			break;
		}
	}
	else
	{
	}
}

void FWraithX::TranslateXAnim(FCastAnimationInfo& OutAnim, FWraithXAnim& InAnim)
{
	OutAnim.Name = InAnim.AnimationName;
	OutAnim.Framerate = InAnim.FrameRate;
	OutAnim.Looping = InAnim.LoopingAnimation;

	OutAnim.Type = ECastAnimationType::Relative;
	if (InAnim.ViewModelAnimation)
	{
		OutAnim.Type = ECastAnimationType::Absolute;
	}
	if (InAnim.DeltaTranslationPtr || InAnim.Delta2DRotationsPtr || InAnim.Delta3DRotationsPtr)
	{
		OutAnim.Type = ECastAnimationType::Delta;
		OutAnim.DeltaTagName = TEXT("tag_origin");
	}
	if (InAnim.AdditiveAnimation)
	{
		OutAnim.Type = ECastAnimationType::Additive;
	}

	switch (ProcessInstance->GetCurrentGameType())
	{
	case CoDAssets::ESupportedGames::ModernWarfare6:
		FModernWarfare6::LoadXAnim(ProcessInstance, InAnim, OutAnim);
		break;
	default:
		break;
	}
	for (auto& NoteTrack : InAnim.Reader.Notetracks)
	{
		OutAnim.AddNotificationKey(NoteTrack.Key, (uint32)NoteTrack.Value);
	}

	if (InAnim.DeltaTranslationPtr)
	{
		switch (ProcessInstance->GetCurrentGameType())
		{
		case CoDAssets::ESupportedGames::ModernWarfare6:
			DeltaTranslation64(OutAnim, InAnim, InAnim.FrameCount > 255 ? 2 : 1);
			break;
		default:
			break;
		}
	}
	if (InAnim.Delta2DRotationsPtr)
	{
		switch (ProcessInstance->GetCurrentGameType())
		{
		case CoDAssets::ESupportedGames::ModernWarfare6:
			Delta2DRotation64(OutAnim, InAnim, InAnim.FrameCount > 255 ? 2 : 1);
			break;
		default:
			break;
		}
	}
	if (InAnim.Delta3DRotationsPtr)
	{
		switch (ProcessInstance->GetCurrentGameType())
		{
		case CoDAssets::ESupportedGames::ModernWarfare6:
			Delta3DRotation64(OutAnim, InAnim, InAnim.FrameCount > 255 ? 2 : 1);
			break;
		default:
			break;
		}
	}
}

void FWraithX::DeltaTranslation64(FCastAnimationInfo& OutAnim, FWraithXAnim& InAnim, uint32 FrameSize)
{
	/*uint32 FrameCount = ProcessInstance->ReadMemory<uint16>(InAnim.DeltaTranslationPtr);
	InAnim.DeltaTranslationPtr += 2;
	uint8 DataSize = ProcessInstance->ReadMemory<uint8>(InAnim.DeltaTranslationPtr);
	InAnim.DeltaTranslationPtr += 6;
	FVector3f MinVec = ProcessInstance->ReadMemory<FVector3f>(InAnim.DeltaTranslationPtr);
	InAnim.DeltaTranslationPtr += 12;
	FVector3f SizeVec = ProcessInstance->ReadMemory<FVector3f>(InAnim.DeltaTranslationPtr);
	InAnim.DeltaTranslationPtr += 12;
	if (!FrameCount)
	{
		OutAnim.AddTranslationKey(TEXT("tag_origin"), 0, FVector(MinVec));
		return;
	}
	uint64 DeltaDataPtr = ProcessInstance->ReadMemory<uint64>(InAnim.DeltaTranslationPtr);
	InAnim.DeltaTranslationPtr += 8;
	if (FrameCount)
	{
		for (uint32 i = 0; i <= FrameCount; ++i)
		{
			uint32 FrameIndex = 0;
			if (FrameSize == 1)
			{
				FrameIndex = ProcessInstance->ReadMemory<uint8>(InAnim.DeltaTranslationPtr);
				InAnim.DeltaTranslationPtr += 1;
			}
			else if (FrameSize == 2)
			{
				FrameIndex = ProcessInstance->ReadMemory<uint16>(InAnim.DeltaTranslationPtr);
				InAnim.DeltaTranslationPtr += 2;
			}

			float XCoord, YCoord, ZCoord;
			if (DataSize == 1)
			{
				XCoord = ProcessInstance->ReadMemory<uint8>(DeltaDataPtr);
				YCoord = ProcessInstance->ReadMemory<uint8>(DeltaDataPtr + 1);
				ZCoord = ProcessInstance->ReadMemory<uint8>(DeltaDataPtr + 2);
				DeltaDataPtr += 3;
			}
			else
			{
				XCoord = ProcessInstance->ReadMemory<uint16>(DeltaDataPtr);
				YCoord = ProcessInstance->ReadMemory<uint16>(DeltaDataPtr + 2);
				ZCoord = ProcessInstance->ReadMemory<uint16>(DeltaDataPtr + 4);
				DeltaDataPtr += 6;
			}
			float TranslationX = (SizeVec.X * XCoord) + MinVec.X;
			float TranslationY = (SizeVec.Y * YCoord) + MinVec.Y;
			float TranslationZ = (SizeVec.Z * ZCoord) + MinVec.Z;

			OutAnim.AddTranslationKey(TEXT("tag_origin"), FrameIndex,
			                          FVector(TranslationX, TranslationY, TranslationZ));
		}
	}*/
}

void FWraithX::Delta2DRotation64(FCastAnimationInfo& OutAnim, FWraithXAnim& InAnim, uint32 FrameSize)
{
	/*const uint32 FrameCount = ProcessInstance->ReadMemory<uint16>(InAnim.Delta2DRotationsPtr);
	InAnim.Delta2DRotationsPtr += 8;

	if (FrameCount)
	{
		const auto RotationData = ProcessInstance->ReadMemory<FQuat2Data>(InAnim.Delta2DRotationsPtr);
		InAnim.Delta2DRotationsPtr += 4;

		if (InAnim.RotationType == EAnimationKeyTypes::DivideBySize)
		{
			const FVector4 Rot{
				0, 0, ((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f)
			};
			OutAnim.AddRotationKey("tag_origin", 0, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::HalfFloat)
		{
			FFloat16 RZ, RW;
			RZ.Encoded = (uint16)RotationData.RotationZ;
			RW.Encoded = (uint16)RotationData.RotationW;
			const FVector4 Rot{0, 0, RZ, RW};

			OutAnim.AddRotationKey("tag_origin", 0, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::QuatPackingA)
		{
			auto Rotation = VectorPacking::QuatPacking2DA(*(uint32*)&RotationData);
			OutAnim.AddRotationKey("tag_origin", 0, Rotation);
		}
		return;
	}

	uint64 DeltaDataPtr = ProcessInstance->ReadMemory<uint64>(InAnim.Delta2DRotationsPtr);
	InAnim.Delta2DRotationsPtr += 8;

	if (!FrameCount)
		return;

	for (uint32 i = 0; i <= FrameCount; i++)
	{
		uint32_t FrameIndex = 0;
		if (FrameSize == 1)
		{
			FrameIndex = ProcessInstance->ReadMemory<uint8>(InAnim.Delta2DRotationsPtr);
			InAnim.Delta2DRotationsPtr += 1;
		}
		else if (FrameSize == 2)
		{
			FrameIndex = ProcessInstance->ReadMemory<uint16>(InAnim.Delta2DRotationsPtr);
			InAnim.Delta2DRotationsPtr += 2;
		}

		FQuat2Data RotationData = ProcessInstance->ReadMemory<FQuat2Data>(DeltaDataPtr);
		DeltaDataPtr += 4;

		if (InAnim.RotationType == EAnimationKeyTypes::DivideBySize)
		{
			FVector4 Rot{
				0, 0,
				((float)RotationData.RotationZ / 32768.f),
				((float)RotationData.RotationW / 32768.f)
			};
			OutAnim.AddRotationKey("tag_origin", FrameIndex, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::HalfFloat)
		{
			FFloat16 RZ, RW;
			RZ.Encoded = (uint16)RotationData.RotationZ;
			RW.Encoded = (uint16)RotationData.RotationW;
			const FVector4 Rot{0, 0, RZ, RW};

			OutAnim.AddRotationKey("tag_origin", FrameIndex, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::QuatPackingA)
		{
			auto Rotation = VectorPacking::QuatPacking2DA(*(uint32*)&RotationData);
			OutAnim.AddRotationKey("tag_origin", FrameIndex, Rotation);
		}
	}*/
}

void FWraithX::Delta3DRotation64(FCastAnimationInfo& OutAnim, FWraithXAnim& InAnim, uint32 FrameSize)
{
	/*uint32 FrameCount = ProcessInstance->ReadMemory<uint16>(InAnim.Delta3DRotationsPtr);
	InAnim.Delta3DRotationsPtr += 8;

	if (FrameCount == 0)
	{
		auto RotationData = ProcessInstance->ReadMemory<FQuatData>(InAnim.Delta3DRotationsPtr);
		InAnim.Delta3DRotationsPtr += 8;

		if (InAnim.RotationType == EAnimationKeyTypes::DivideBySize)
		{
			FVector4 Rot{
				((float)RotationData.RotationX / 32768.f), ((float)RotationData.RotationY / 32768.f),
				((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f)
			};
			OutAnim.AddRotationKey("tag_origin", 0, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::HalfFloat)
		{
			FFloat16 RX, RY, RZ, RW;
			RX.Encoded = (uint16)RotationData.RotationX;
			RY.Encoded = (uint16)RotationData.RotationY;
			RZ.Encoded = (uint16)RotationData.RotationZ;
			RW.Encoded = (uint16)RotationData.RotationW;
			const FVector4 Rot{RX, RY, RZ, RW};

			OutAnim.AddRotationKey("tag_origin", 0, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::QuatPackingA)
		{
			auto Rotation = VectorPacking::QuatPackingA(*(uint64_t*)&RotationData);
			OutAnim.AddRotationKey("tag_origin", 0, Rotation);
		}
		return;
	}

	uint64 DeltaDataPtr = ProcessInstance->ReadMemory<uint64>(InAnim.Delta3DRotationsPtr);
	InAnim.Delta3DRotationsPtr += 8;

	if (!FrameCount)
		return;

	for (uint32 i = 0; i <= FrameCount; i++)
	{
		uint32 FrameIndex = 0;
		if (FrameSize == 1)
		{
			FrameIndex = ProcessInstance->ReadMemory<uint8>(InAnim.Delta3DRotationsPtr);
			InAnim.Delta3DRotationsPtr += 1;
		}
		else if (FrameSize == 2)
		{
			FrameIndex = ProcessInstance->ReadMemory<uint16>(InAnim.Delta3DRotationsPtr);
			InAnim.Delta3DRotationsPtr += 2;
		}

		auto RotationData = ProcessInstance->ReadMemory<FQuatData>(DeltaDataPtr);
		DeltaDataPtr += 8;

		if (InAnim.RotationType == EAnimationKeyTypes::DivideBySize)
		{
			FVector4 Rot{
				((float)RotationData.RotationX / 32768.f), ((float)RotationData.RotationY / 32768.f),
				((float)RotationData.RotationZ / 32768.f), ((float)RotationData.RotationW / 32768.f)
			};
			OutAnim.AddRotationKey("tag_origin", FrameIndex, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::HalfFloat)
		{
			FFloat16 RX, RY, RZ, RW;
			RX.Encoded = (uint16)RotationData.RotationX;
			RY.Encoded = (uint16)RotationData.RotationY;
			RZ.Encoded = (uint16)RotationData.RotationZ;
			RW.Encoded = (uint16)RotationData.RotationW;
			const FVector4 Rot{RX, RY, RZ, RW};

			OutAnim.AddRotationKey("tag_origin", 0, Rot);
		}
		else if (InAnim.RotationType == EAnimationKeyTypes::QuatPackingA)
		{
			auto Rotation = VectorPacking::QuatPackingA(*(uint64_t*)&RotationData);
			OutAnim.AddRotationKey("tag_origin", 0, Rotation);
		}
	}*/
}

void FWraithX::OnAssetInitCompletedCall()
{
	OnOnAssetInitCompletedDelegate.Broadcast();
	OnOnAssetInitCompletedDelegate.Clear();
}

void FWraithX::OnAssetLoadingCall(float InProgress)
{
	// OnOnAssetLoadingDelegate.Broadcast(InProgress);
}
