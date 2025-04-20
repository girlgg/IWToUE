#include "GameInfo/ModernWarfare6AssetHandler.h"

#include "Database/CoDDatabaseService.h"
#include "GameInfo/ModernWarfare6.h"
#include "Interface/IMemoryReader.h"
#include "Structures/MW6SPGameStructures.h"
#include "Utils/CoDAssetHelper.h"
#include "Utils/CoDBonesHelper.h"
#include "WraithX/CoDAssetDatabase.h"

bool FModernWarfare6AssetHandler::ReadModelData(TSharedPtr<FCoDModel> ModelInfo, FWraithXModel& OutModel)
{
	FMW6XModel ModelData;
	MemoryReader->ReadMemory<FMW6XModel>(ModelInfo->AssetPointer, ModelData);
	FMW6BoneInfo BoneInfo;
	MemoryReader->ReadMemory<FMW6BoneInfo>(ModelData.BoneInfoPtr, BoneInfo);

	OutModel.ModelName = ModelInfo->AssetName;

	if (BoneInfo.BoneParentsPtr)
	{
		OutModel.BoneCount = BoneInfo.NumBones;
		OutModel.RootBoneCount = BoneInfo.NumRootBones;
	}

	OutModel.BoneRotationData = EBoneDataTypes::DivideBySize;
	OutModel.IsModelStreamed = true;
	OutModel.BoneIDsPtr = BoneInfo.BoneIDsPtr;
	OutModel.BoneIndexSize = sizeof(uint32);
	OutModel.BoneParentsPtr = BoneInfo.BoneParentsPtr;
	OutModel.BoneParentSize = sizeof(uint16);
	OutModel.RotationsPtr = BoneInfo.RotationsPtr;
	OutModel.TranslationsPtr = BoneInfo.TranslationsPtr;
	OutModel.BaseTransformPtr = BoneInfo.BaseMatriciesPtr;

	for (uint32 LodIdx = 0; LodIdx < ModelData.NumLods; ++LodIdx)
	{
		FMW6XModelLod ModelLod;
		MemoryReader->ReadMemory<FMW6XModelLod>(ModelData.LodInfo, ModelLod);

		FWraithXModelLod& LodRef = OutModel.ModelLods.AddDefaulted_GetRef();

		LodRef.LodDistance = ModelLod.LodDistance;
		LodRef.LODStreamInfoPtr = ModelLod.MeshPtr;
		uint64 XSurfacePtr = ModelLod.SurfsPtr;

		LodRef.Submeshes.Reserve(ModelLod.NumSurfs);
		for (uint32 SurfaceIdx = 0; SurfaceIdx < ModelLod.NumSurfs; ++SurfaceIdx)
		{
			FWraithXModelSubmesh& SubMeshRef = LodRef.Submeshes.AddDefaulted_GetRef();

			FMW6XSurface Surface;
			MemoryReader->ReadMemory<FMW6XSurface>(XSurfacePtr + SurfaceIdx * sizeof(FMW6XSurface), Surface);

			SubMeshRef.VertexCount = Surface.VertCount;
			SubMeshRef.FaceCount = Surface.TriCount;
			SubMeshRef.PackedIndexTableCount = Surface.PackedIndicesTableCount;
			SubMeshRef.VertexOffset = Surface.XyzOffset;
			SubMeshRef.VertexUVsOffset = Surface.TexCoordOffset;
			SubMeshRef.VertexTangentOffset = Surface.TangentFrameOffset;
			SubMeshRef.FacesOffset = Surface.IndexDataOffset;
			SubMeshRef.PackedIndexTableOffset = Surface.PackedIndiciesTableOffset;
			SubMeshRef.PackedIndexBufferOffset = Surface.PackedIndicesOffset;
			SubMeshRef.VertexColorOffset = Surface.VertexColorOffset;
			SubMeshRef.WeightsOffset = Surface.WeightsOffset;

			if (Surface.OverrideScale != -1)
			{
				SubMeshRef.Scale = Surface.OverrideScale;
				SubMeshRef.XOffset = 0;
				SubMeshRef.YOffset = 0;
				SubMeshRef.ZOffset = 0;
			}
			else
			{
				SubMeshRef.Scale = FMath::Max3(Surface.Min, Surface.Max, Surface.Scale);
				SubMeshRef.XOffset = Surface.OffsetsX;
				SubMeshRef.YOffset = Surface.OffsetsY;
				SubMeshRef.ZOffset = Surface.OffsetsZ;
			}
			for (int32 WeightIdx = 0; WeightIdx < 8; ++WeightIdx)
			{
				SubMeshRef.WeightCounts[WeightIdx] = Surface.WeightCounts[WeightIdx];
			}

			uint64 MaterialHandle;
			MemoryReader->ReadMemory<uint64>(ModelData.MaterialHandles + SurfaceIdx * sizeof(uint64), MaterialHandle);

			FWraithXMaterial& Material = LodRef.Materials.AddDefaulted_GetRef();
			ReadMaterialDataFromPtr(MaterialHandle, Material);
		}
	}

	return true;
}

bool FModernWarfare6AssetHandler::ReadAnimData(TSharedPtr<FCoDAnim> AnimInfo, FWraithXAnim& OutAnim)
{
	return false;
}

bool FModernWarfare6AssetHandler::ReadImageData(TSharedPtr<FCoDImage> ImageInfo, TArray<uint8>& OutImageData,
                                                uint8& OutFormat)
{
	return false;
}

bool FModernWarfare6AssetHandler::ReadSoundData(TSharedPtr<FCoDSound> SoundInfo, FWraithXSound& OutSound)
{
	return false;
}

bool FModernWarfare6AssetHandler::ReadMaterialData(TSharedPtr<FCoDMaterial> MaterialInfo, FWraithXMaterial& OutMaterial)
{
	return false;
}

bool FModernWarfare6AssetHandler::ReadMaterialDataFromPtr(uint64 MaterialHandle, FWraithXMaterial& OutMaterial)
{
	if (GameProcess->GetCurrentGameFlag() == CoDAssets::ESupportedGameFlags::SP)
	{
		FMW6SPMaterial MaterialData;
		MemoryReader->ReadMemory<FMW6SPMaterial>(MaterialHandle, MaterialData);
	}
	else
	{
		FMW6Material MaterialData;
		MemoryReader->ReadMemory<FMW6Material>(MaterialHandle, MaterialData);

		OutMaterial.MaterialName = FCoDDatabaseService::Get().GetPrintfAssetName(MaterialData.Hash, TEXT("xmaterial"));

		TArray<FMW6GfxImage> Images;
		Images.Reserve(MaterialData.ImageCount);
		for (uint32 i = 0; i < MaterialData.ImageCount; i++)
		{
			uint64 ImagePtr;
			MemoryReader->ReadMemory<uint64>(MaterialData.ImageTable + i * sizeof(uint64), ImagePtr);
			FMW6GfxImage Image;
			MemoryReader->ReadMemory<FMW6GfxImage>(ImagePtr, Image);
			Images.Add(Image);
		}

		OutMaterial.Images.Empty();

		for (int i = 0; i < MaterialData.TextureCount; i++)
		{
			FMW6MaterialTextureDef TextureDef;
			MemoryReader->ReadMemory<FMW6MaterialTextureDef>(
				MaterialData.TextureTable + i * sizeof(FMW6MaterialTextureDef), TextureDef);
			FMW6GfxImage Image = Images[TextureDef.ImageIdx];

			FString ImageName;

			ImageName = FCoDDatabaseService::Get().GetPrintfAssetName(Image.Hash, TEXT("ximage"));

			// 一些占位符之类的，没有用
			if (!ImageName.IsEmpty() && ImageName[0] != '$')
			{
				FWraithXImage& ImageOut = OutMaterial.Images.AddDefaulted_GetRef();
				ImageOut.ImageName = ImageName;
				ImageOut.SemanticHash = TextureDef.Index;
			}
		}
	}
	return true;
}

bool FModernWarfare6AssetHandler::TranslateModel(FWraithXModel& InModel, int32 LodIdx,
                                                 FCastModelInfo& OutModelInfo)
{
	OutModelInfo.Name = InModel.ModelName;

	FWraithXModelLod& LodRef = InModel.ModelLods[LodIdx];

	TMap<FString, uint32> MaterialMap;

	OutModelInfo.Materials.Reserve(LodRef.Materials.Num());
	for (int32 MatIdx = 0; MatIdx < LodRef.Materials.Num(); ++MatIdx)
	{
		FWraithXMaterial& MatRef = LodRef.Materials[MatIdx];

		if (MaterialMap.Contains(MatRef.MaterialName))
		{
			LodRef.Submeshes[MatIdx].MaterialIndex = MaterialMap[MatRef.MaterialName];
		}
		else
		{
			FCastMaterialInfo& MaterialInfo = OutModelInfo.Materials.AddDefaulted_GetRef();
			MaterialInfo.Name = MatRef.MaterialName;

			// TODO 加载材质
			for (const FWraithXImage& Image : MatRef.Images)
			{
				FCastTextureInfo& Texture = MaterialInfo.Textures.AddDefaulted_GetRef();
				Texture.TextureName = Image.ImageName;

				FString TextureSemantic = FString::Printf(TEXT("unk_semantic_0x%x"), Image.SemanticHash);
				Texture.TextureSemantic = TextureSemantic;
			}
			MaterialMap.Emplace(MatRef.MaterialName, OutModelInfo.Materials.Num() - 1);
		}
	}
	if (InModel.IsModelStreamed)
	{
		LoadXModel(InModel, LodRef, OutModelInfo);
	}
	else
	{
	}
	return true;
}

bool FModernWarfare6AssetHandler::TranslateAnim(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo)
{
	return false;
}

void FModernWarfare6AssetHandler::ApplyDeltaTranslation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim)
{
}

void FModernWarfare6AssetHandler::ApplyDelta2DRotation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim)
{
}

void FModernWarfare6AssetHandler::ApplyDelta3DRotation(FCastAnimationInfo& OutAnim, const FWraithXAnim& InAnim)
{
}

bool FModernWarfare6AssetHandler::LoadStreamedModelData(const FWraithXModel& InModel, FWraithXModelLod& InOutLod,
                                                        FCastModelInfo& OutModelInfo)
{
	return false;
}

bool FModernWarfare6AssetHandler::LoadStreamedAnimData(const FWraithXAnim& InAnim, FCastAnimationInfo& OutAnimInfo)
{
	return false;
}

void FModernWarfare6AssetHandler::LoadXModel(FWraithXModel& InModel, FWraithXModelLod& ModelLod,
                                             FCastModelInfo& OutModel)
{
	FMW6XModelSurfs MeshInfo;
	MemoryReader->ReadMemory<FMW6XModelSurfs>(ModelLod.LODStreamInfoPtr, MeshInfo);
	FMW6XSurfaceShared BufferInfo;
	MemoryReader->ReadMemory<FMW6XSurfaceShared>(MeshInfo.Shared, BufferInfo);

	TArray<uint8> MeshDataBuffer;
	if (BufferInfo.Data)
	{
		MemoryReader->ReadArray(BufferInfo.Data, MeshDataBuffer, BufferInfo.DataSize);
	}
	else
	{
		MeshDataBuffer = GameProcess->GetDecrypt()->ExtractXSubPackage(MeshInfo.XPakKey, BufferInfo.DataSize);
	}

	CoDBonesHelper::ReadXModelBones(MemoryReader, InModel, ModelLod, OutModel);

	if (MeshDataBuffer.IsEmpty())
	{
		return;
	}

	for (FWraithXModelSubmesh& Submesh : ModelLod.Submeshes)
	{
		FCastMeshInfo& Mesh = OutModel.Meshes.AddDefaulted_GetRef();
		Mesh.MaterialIndex = Submesh.MaterialIndex;

		FBufferReader VertexPosReader(MeshDataBuffer.GetData() + Submesh.VertexOffset,
		                              Submesh.VertexOffset * sizeof(uint64), false);
		FBufferReader VertexTangentReader(MeshDataBuffer.GetData() + Submesh.VertexTangentOffset,
		                                  Submesh.VertexOffset * sizeof(uint32), false);
		FBufferReader VertexUVReader(MeshDataBuffer.GetData() + Submesh.VertexUVsOffset,
		                             Submesh.VertexOffset * sizeof(uint32), false);
		FBufferReader FaceIndiciesReader(MeshDataBuffer.GetData() + Submesh.FacesOffset,
		                                 Submesh.FacesOffset * sizeof(uint16) * 3, false);

		// 顶点权重
		Mesh.VertexWeights.SetNum(Submesh.VertexCount);
		int32 WeightDataLength = 0;
		for (int32 WeightIdx = 0; WeightIdx < 8; ++WeightIdx)
		{
			WeightDataLength += (WeightIdx + 1) * 4 * Submesh.WeightCounts[WeightIdx];
		}
		// TODO 多骨骼支持
		if (OutModel.Skeletons.Num() > 0)
		{
			FBufferReader VertexWeightReader(MeshDataBuffer.GetData() + Submesh.WeightsOffset, WeightDataLength, false);
			uint32 WeightDataIndex = 0;
			for (int32 i = 0; i < 8; ++i)
			{
				for (int32 WeightIdx = 0; WeightIdx < i + 1; ++WeightIdx)
				{
					for (uint32 LocalWeightDataIndex = WeightDataIndex;
					     LocalWeightDataIndex < WeightDataIndex + Submesh.WeightCounts[i]; ++LocalWeightDataIndex)
					{
						Mesh.VertexWeights[LocalWeightDataIndex].WeightCount = WeightIdx + 1;
						uint16 BoneValue;
						VertexWeightReader << BoneValue;
						Mesh.VertexWeights[LocalWeightDataIndex].BoneValues[WeightIdx] = BoneValue;

						if (WeightIdx > 0)
						{
							uint16 WeightValue;
							VertexWeightReader << WeightValue;
							Mesh.VertexWeights[LocalWeightDataIndex].WeightValues[WeightIdx] = WeightValue / 65536.f;
							Mesh.VertexWeights[LocalWeightDataIndex].WeightValues[0] -= Mesh.VertexWeights[
									LocalWeightDataIndex].
								WeightValues[WeightIdx];
						}
						else
						{
							VertexWeightReader.Seek(VertexWeightReader.Tell() + 2);
						}
					}
				}
				WeightDataIndex += Submesh.WeightCounts[i];
			}
		}

		Mesh.VertexPositions.Reserve(Submesh.VertexCount);
		Mesh.VertexTangents.Reserve(Submesh.VertexCount);
		Mesh.VertexNormals.Reserve(Submesh.VertexCount);
		Mesh.VertexUV.Reserve(Submesh.VertexCount);
		Mesh.VertexColor.Reserve(Submesh.VertexCount);
		// UV
		for (uint32 VertexIdx = 0; VertexIdx < Submesh.VertexCount; ++VertexIdx)
		{
			FVector3f& VertexPos = Mesh.VertexPositions.AddDefaulted_GetRef();
			uint64 PackedPos;
			VertexPosReader << PackedPos;
			VertexPos = FVector3f{
				((PackedPos >> 00 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Submesh.Scale + Submesh.XOffset,
				((PackedPos >> 21 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Submesh.Scale + Submesh.YOffset,
				((PackedPos >> 42 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Submesh.Scale + Submesh.ZOffset
			};

			uint32 PackedTangentFrame;
			VertexTangentReader << PackedTangentFrame;
			FVector3f Normal, Tangent;
			FCoDMeshHelper::UnpackCoDQTangent(PackedTangentFrame, Tangent, Normal);
			Mesh.VertexNormals.Add(Normal);
			Mesh.VertexTangents.Add(Tangent);

			uint16 HalfUvU, HalfUvV;
			VertexUVReader << HalfUvU << HalfUvV;
			FFloat16 HalfFloatU, HalfFloatV;
			HalfFloatU.Encoded = HalfUvU;
			HalfFloatV.Encoded = HalfUvV;
			Mesh.VertexUV.Emplace(HalfUvU, HalfFloatU);
		}
		// TODO Second UV
		// 顶点色
		if (Submesh.VertexColorOffset != 0xFFFFFFFF)
		{
			FBufferReader VertexColorReader(MeshDataBuffer.GetData() + Submesh.VertexColorOffset,
			                                Submesh.FacesOffset * sizeof(uint32), false);
			for (uint32 VertexIdx = 0; VertexIdx < Submesh.VertexCount; VertexIdx++)
			{
				uint32 VertexColor;
				VertexColorReader << VertexColor;
				Mesh.VertexColor.Add(VertexColor);
			}
		}
		// 面
		uint64 TableOffsetPtr = reinterpret_cast<uint64>(MeshDataBuffer.GetData()) + Submesh.PackedIndexTableOffset;
		uint64 PackedIndices = reinterpret_cast<uint64>(MeshDataBuffer.GetData()) + Submesh.PackedIndexBufferOffset;
		uint64 IndicesPtr = reinterpret_cast<uint64>(MeshDataBuffer.GetData()) + Submesh.FacesOffset;

		for (uint32 TriIdx = 0; TriIdx < Submesh.FaceCount; TriIdx++)
		{
			TArray<uint16> Faces;
			FCoDMeshHelper::UnpackFaceIndices(MemoryReader, Faces, TableOffsetPtr, Submesh.PackedIndexTableCount,
			                                  PackedIndices, IndicesPtr, TriIdx, true);

			Mesh.Faces.Add(Faces[2]);
			Mesh.Faces.Add(Faces[1]);
			Mesh.Faces.Add(Faces[0]);
		}
	}
}
