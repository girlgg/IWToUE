#pragma once

#include "MapImporter/CordycepProcess.h"
#include "Structures/SharedStructures.h"

class FGameInstance
{
public:
	virtual TArray<TSharedPtr<FCastMapInfo>> GetMapsInfo() = 0;

	virtual void DumpMap(FString MapName) = 0;
};

template <typename TGfxWorld, typename TGfxWorldTransientZone, typename TWorldSurfaces, typename TGfxSurface,
          typename TGfxUgbSurfData, typename TMaterial, typename TGfxWorldStaticModels,
          typename TGfxStaticModelCollection, typename TGfxStaticModel, typename TXModel,
          typename TGfxSModelInstanceData, typename TGfxWorldDrawOffset, typename TGfxWorldDrawVerts,
          typename TXModelLodInfo, typename TXModelSurfs, typename TXSurfaceShared, typename TXSurface>
class TBaseGame : public FGameInstance
{
public:
	TBaseGame(FCordycepProcess* InProcess)
		: GFXMAP_POOL_IDX(0), Process(InProcess)
	{
	}

	virtual ~TBaseGame()
	{
	}

	TArray<TSharedPtr<FCastMapInfo>> GetMapsInfo()
	{
		TArray<TSharedPtr<FCastMapInfo>> MapList;
		Process->EnumerableAssetPool(GFXMAP_POOL_IDX, [this, &MapList](const Cordycep::XAsset64& Asset)
		{
			TGfxWorld CurrentGfxWorld = ReadGfxWorld(Asset.Header);
			if (CurrentGfxWorld.BaseName == 0) return;
			FString BaseName = Process->ReadFString(CurrentGfxWorld.BaseName).TrimStartAndEnd();
			TSharedPtr<FCastMapInfo> MapInfo = MakeShareable(new FCastMapInfo());
			MapInfo->DisplayName = BaseName;
			MapInfo->DetailInfo = "[Todo] Map details";
			MapList.Add(MapInfo);
		});
		return MapList;
	}

	void DumpMap(FString MapName)
	{
		Process->EnumerableAssetPool(GFXMAP_POOL_IDX, [this, &MapName](const Cordycep::XAsset64& Asset)
		{
			TGfxWorld CurrentGfxWorld = ReadGfxWorld(Asset.Header);
			if (GetGfxWorldBaseName(CurrentGfxWorld) != MapName) return;
			DumpMap(Asset.Header, CurrentGfxWorld, MapName);
		});
	}

	void DumpMap(uint64 Address, TGfxWorld InGfxWorld, FString MapName)
	{
		GfxWorld = InGfxWorld;
		// 读取暂存区
		ReadTransientZones();
		// 处理Surface
		ProcessSurfaces();
		// 处理模型
		ProcessStaticModels();
		// 导入到UE
	}

protected:
	virtual TArray<FCastTextureInfo> PopulateMaterial(const TMaterial& Material) = 0;
	virtual float GetSurfaceScale(const TXSurface& Surface) = 0;
	virtual FVector3f GetSurfaceOffset(const TXSurface& Surface) = 0;
	virtual uint64 GetXPakKey(const TXModelSurfs& XModelSurfs, const TXSurfaceShared& Shared) = 0;

	virtual uint64 GetXyzOffset(const TXSurface& Surface) = 0;
	virtual uint64 GetTangentFrameOffset(const TXSurface& Surface) = 0;
	virtual uint64 GetTexCoordOffset(const TXSurface& Surface) = 0;

	virtual uint64 GetColorOffset(const TXSurface& Surface) = 0;
	virtual uint64 GetIndexDataOffset(const TXSurface& Surface) = 0;
	virtual uint64 GetPackedIndicesOffset(const TXSurface& Surface) = 0;

protected:
	static uint64 ComputeHash(FString Data)
	{
		uint64 Hash = 0xCBD29CE484222325;

		FTCHARToUTF8 Convert(*Data);
		const char* AnsiData = Convert.Get();
		int32 Length = Convert.Length();

		for (int32 i = 0; i < Length; i++)
		{
			Hash ^= static_cast<uint8>(AnsiData[i]);
			Hash *= 0x100000001B3;
		}

		return Hash;
	}

	TGfxWorld ReadGfxWorld(uint64 Address)
	{
		return Process->ReadMemory<TGfxWorld>(Address);
	}

	FString GetGfxWorldBaseName(TGfxWorld InGfxWorld)
	{
		return InGfxWorld.BaseName == 0 ? "" : Process->ReadFString(InGfxWorld.BaseName).TrimStartAndEnd();
	}

	void ReadTransientZones()
	{
		TransientZones.Reserve(GfxWorld.TransientZoneCount);
		for (uint32 i = 0; i < GfxWorld.TransientZoneCount; i++)
		{
			TransientZones.Add(Process->ReadMemory<TGfxWorldTransientZone>(GfxWorld.TransientZones[i]));
		}
	}

	FCastMeshData ReadMesh(TGfxSurface GfxSurface, TGfxUgbSurfData UgbSurfData, TMaterial Material,
	                       TGfxWorldTransientZone Zone)
	{
		TGfxWorldDrawOffset WorldDrawOffset = UgbSurfData.WorldDrawOffset;

		FCastMeshData Mesh;

		Mesh.Material.Name = FString::Printf(TEXT("xmaterial_%llx"), (Material.Hash & 0x0FFFFFFFFFFFFFFF));
		Mesh.Material.MaterialHash = ComputeHash(Mesh.Material.Name);
		Mesh.Mesh.UVLayer = UgbSurfData.LayerCount;

		uint16 VertexCount = GfxSurface.VertexCount; // 顶点数

		Mesh.Mesh.VertexUV.Reserve(VertexCount);
		Mesh.Mesh.VertexPositions.Reserve(VertexCount);
		Mesh.Mesh.VertexNormals.Reserve(VertexCount);
		Mesh.Mesh.VertexTangents.Reserve(VertexCount);

		uint64 XyzPtr = Zone.DrawVerts.PosData + UgbSurfData.XyzOffset;
		uint64 TangentFramePtr = Zone.DrawVerts.PosData + UgbSurfData.TangentFrameOffset;
		uint64 TexCoordPtr = Zone.DrawVerts.PosData + UgbSurfData.TexCoordOffset;

		for (uint16 VertexIdx = 0; VertexIdx < VertexCount; ++VertexIdx)
		{
			uint64 PackedPosition = Process->ReadMemory<uint64>(XyzPtr + VertexIdx * 8);
			FVector3f Position{
				((PackedPosition >> 0) & 0x1FFFFF) * WorldDrawOffset.Scale + WorldDrawOffset.X,
				((PackedPosition >> 21) & 0x1FFFFF) * WorldDrawOffset.Scale + WorldDrawOffset.Y,
				((PackedPosition >> 42) & 0x1FFFFF) * WorldDrawOffset.Scale + WorldDrawOffset.Z
			};
			Mesh.Mesh.VertexPositions.Add(Position);

			uint32 PackedTangentFrame = Process->ReadMemory<uint32>(TangentFramePtr + VertexIdx * 4);
			FVector3f Tangent, Normal;
			UnpackCoDQTangent(PackedTangentFrame, Tangent, Normal);
			Mesh.Mesh.VertexTangents.Add(Tangent);
			Mesh.Mesh.VertexNormals.Add(Normal);

			FVector2f UV = Process->ReadMemory<FVector2f>(TexCoordPtr);
			Mesh.Mesh.VertexUV.Add(UV);
			TexCoordPtr += 8;

			for (uint32 LayerIdx = 1; LayerIdx < UgbSurfData.LayerCount; LayerIdx++)
			{
				// Todo 多层UV，读取剩下的层次UV
				TexCoordPtr += 8;
			}
		}

		if (UgbSurfData.ColorOffset != 0)
		{
			Mesh.Mesh.VertexColor.Reserve(VertexCount);
			uint64 ColorPtr = Zone.DrawVerts.PosSize + UgbSurfData.ColorOffset;
			for (uint32 VertexIdx = 0; VertexIdx < UgbSurfData.VertexCount; VertexIdx++)
			{
				// TODO Cannot to read!
				// uint32 Color = Process->ReadMemory<uint32>(ColorPtr + VertexIdx * 4);
				// Mesh.Mesh.VertexColor.Add(Color);
				Mesh.Mesh.VertexColor.Add(0);
			}
		}

		uint64 TableOffsetPtr = Zone.DrawVerts.TableData + GfxSurface.TableIndex * 40;
		uint64 IndicesPtr = Zone.DrawVerts.Indices + GfxSurface.BaseIndex * 2;
		uint64 PackedIndices = Zone.DrawVerts.PackedIndices + GfxSurface.PackedIndicesOffset;

		Mesh.Mesh.Faces.SetNum(GfxSurface.TriCount * 3);

		for (int TriIndex = 0; TriIndex < GfxSurface.TriCount; ++TriIndex)
		{
			TArray<uint16> Faces;
			UnpackFaceIndices(Faces, TableOffsetPtr, GfxSurface.PackedIndicesTableCount, PackedIndices, IndicesPtr,
			                  TriIndex);
			int Index = TriIndex * 3;
			Mesh.Mesh.Faces[Index] = Faces[2];
			Mesh.Mesh.Faces[Index + 1] = Faces[1];
			Mesh.Mesh.Faces[Index + 2] = Faces[0];
		}

		Mesh.Textures = PopulateMaterial(Material);

		return Mesh;
	}

	void ProcessSurfaces()
	{
		// TODO: 变为Model而不是Mesh
		const uint64 StartCycles = FPlatformTime::Cycles64();

		TWorldSurfaces GfxWorldSurfaces = GfxWorld.Surfaces;
		Meshes.Reserve(GfxWorldSurfaces.Count);

		FCriticalSection MeshCriticalSection;
		FCriticalSection ModelCriticalSection;

		ParallelFor(GfxWorldSurfaces.Count, [&](const uint32 Index)
		{
			TGfxSurface GfxSurface = Process->ReadMemory<TGfxSurface>(
				GfxWorldSurfaces.Surfaces + Index * sizeof(TGfxSurface));
			TGfxUgbSurfData UgbSurfData = Process->ReadMemory<TGfxUgbSurfData>(
				GfxWorldSurfaces.UgbSurfData + GfxSurface.UgbSurfDataIndex * sizeof(TGfxUgbSurfData));
			const TGfxWorldTransientZone& Zone = TransientZones[UgbSurfData.TransientZoneIndex];
			if (Zone.Hash == 0) return;

			uint64 MaterialPtr = Process->ReadMemory<uint64>(GfxWorldSurfaces.Materials + GfxSurface.MaterialIndex * 8);
			TMaterial Material = Process->ReadMemory<TMaterial>(MaterialPtr);
			const FCastMeshData Mesh = ReadMesh(GfxSurface, UgbSurfData, Material, Zone);
			{
				FScopeLock MeshLock(&MeshCriticalSection);
				Meshes.Add(Mesh);
			}
		});

		const double DurationMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);
		UE_LOG(LogTemp, Verbose, TEXT("Processed %d surfaces in %.2fms"), GfxWorldSurfaces.Count, DurationMs);
	}

	static void UnpackCoDQTangent(const uint32 Packed, FVector3f& Tangent, FVector3f& Normal)
	{
		uint32 Idx = Packed >> 30;

		float TX = ((Packed >> 00 & 0x3FF) / 511.5f - 1.0f) / 1.4142135f;
		float TY = ((Packed >> 10 & 0x3FF) / 511.5f - 1.0f) / 1.4142135f;
		float TZ = ((Packed >> 20 & 0x1FF) / 255.5f - 1.0f) / 1.4142135f;
		float TW = 0.0f;
		float Sum = TX * TX + TY * TY + TZ * TZ;

		if (Sum <= 1.0f) TW = FMath::Sqrt(1.0f - Sum);

		float QX = 0.0f;
		float QY = 0.0f;
		float QZ = 0.0f;
		float QW = 0.0f;

		auto SetVal = [&QX,&QY,&QW,&QZ](float& B1, float& B2, float& B3, float& B4)
		{
			QX = B1;
			QY = B2;
			QZ = B3;
			QW = B4;
		};

		switch (Idx)
		{
		case 0:
			SetVal(TW, TX, TY, TZ);
			break;
		case 1:
			SetVal(TX, TW, TY, TZ);
			break;
		case 2:
			SetVal(TX, TY, TW, TZ);
			break;
		case 3:
			SetVal(TX, TY, TZ, TW);
			break;
		default:
			break;
		}

		Tangent = FVector3f(1 - 2 * (QY * QY + QZ * QZ), 2 * (QX * QY + QW * QZ), 2 * (QX * QZ - QW * QY));

		FVector3f Bitangent(2 * (QX * QY - QW * QZ), 1 - 2 * (QX * QX + QZ * QZ), 2 * (QY * QZ + QW * QX));

		Normal = Tangent.Cross(Bitangent);
	}

	uint8 FindFaceIndex(uint64 PackedIndices, uint32 Index, uint8 Bits, bool IsLocal = false)
	{
		unsigned long BitIndex;

		if (!_BitScanReverse64(&BitIndex, Bits)) BitIndex = 64;
		else BitIndex ^= 0x3F;

		const uint16 Offset = Index * (64 - BitIndex);
		const uint8 BitCount = 64 - BitIndex;
		const uint64 PackedIndicesPtr = PackedIndices + (Offset >> 3);
		const uint8 BitOffset = Offset & 7;

		const uint8 PackedIndice = Process->ReadMemory<uint8>(PackedIndicesPtr, IsLocal);

		if (BitOffset == 0)
			return PackedIndice & ((1 << BitCount) - 1);

		if (8 - BitOffset < BitCount)
		{
			const uint8 nextPackedIndice = Process->ReadMemory<uint8>(PackedIndicesPtr + 1, IsLocal);
			return (PackedIndice >> BitOffset) & ((1 << (8 - BitOffset)) - 1) | ((nextPackedIndice & ((1 << (64 -
				BitIndex - (8 - BitOffset))) - 1)) << (8 - BitOffset));
		}

		return (PackedIndice >> BitOffset) & ((1 << BitCount) - 1);
	}

	bool UnpackFaceIndices(TArray<uint16>& InFacesArr, uint64 Tables, uint64 TableCount, uint64 PackedIndices,
	                       uint64 Indices, uint64 FaceIndex, const bool IsLocal = false)
	{
		InFacesArr.SetNum(3);
		uint64 CurrentFaceIndex = FaceIndex;
		for (int i = 0; i < TableCount; i++)
		{
			uint64 TablePtr = Tables + (i * 40);
			uint64 IndicesPtr = PackedIndices + Process->ReadMemory<uint32>(TablePtr + 36, IsLocal);
			uint8 Count = Process->ReadMemory<uint8>(TablePtr + 35, IsLocal);
			if (CurrentFaceIndex < Count)
			{
				uint8 Bits = (uint8)(Process->ReadMemory<uint8>(TablePtr + 34, IsLocal) - 1);
				FaceIndex = Process->ReadMemory<uint32>(TablePtr + 28, IsLocal);

				uint32 Face1Offset = FindFaceIndex(IndicesPtr, CurrentFaceIndex * 3 + 0, Bits, IsLocal) + FaceIndex;
				uint32 Face2Offset = FindFaceIndex(IndicesPtr, CurrentFaceIndex * 3 + 1, Bits, IsLocal) + FaceIndex;
				uint32 Face3Offset = FindFaceIndex(IndicesPtr, CurrentFaceIndex * 3 + 2, Bits, IsLocal) + FaceIndex;

				InFacesArr[0] = Process->ReadMemory<uint16>(Indices + Face1Offset * 2, IsLocal);
				InFacesArr[1] = Process->ReadMemory<uint16>(Indices + Face2Offset * 2, IsLocal);
				InFacesArr[2] = Process->ReadMemory<uint16>(Indices + Face3Offset * 2, IsLocal);
				return true;
			}
			CurrentFaceIndex -= Count;
		}
		return false;
	}

	void ProcessStaticModels()
	{
		TGfxWorldStaticModels SModels = GfxWorld.SModels;

		const uint64 StartCycles = FPlatformTime::Cycles64();
		UE_LOG(LogTemp, Verbose, TEXT("Reading %d static models..."), SModels.CollectionsCount);

		for (uint32 ModelIdx = 0; ModelIdx < SModels.CollectionsCount; ModelIdx++)
		{
			ProcessStaticModelInfo(SModels, ModelIdx);
		}

		const double DurationMs = FPlatformTime::ToMilliseconds64(FPlatformTime::Cycles64() - StartCycles);
		UE_LOG(LogTemp, Verbose, TEXT("Read %d static models in %.2f ms."), SModels.CollectionsCount, DurationMs);
	}

	void ProcessStaticModelInfo(TGfxWorldStaticModels SModels, int ModelIndex)
	{
		TGfxStaticModelCollection Collection = Process->ReadMemory<TGfxStaticModelCollection>(
			SModels.Collections + ModelIndex * sizeof(TGfxStaticModelCollection));
		TGfxStaticModel StaticModel = Process->ReadMemory<TGfxStaticModel>(
			SModels.SModels + Collection.SModelIndex * sizeof(TGfxStaticModel));
		TGfxWorldTransientZone Zone = TransientZones[Collection.TransientGfxWorldPlaced];

		if (Zone.Hash == 0) return;

		TXModel XModel = Process->ReadMemory<TXModel>(StaticModel.XModel);

		uint64 XModelHash = XModel.Hash & 0x0FFFFFFFFFFFFFFF;

		TXModelLodInfo LodInfo = Process->ReadMemory<TXModelLodInfo>(XModel.LodInfo);
		TXModelSurfs XModelSurfs = Process->ReadMemory<TXModelSurfs>(LodInfo.MeshPtr);
		TXSurfaceShared Shared = Process->ReadMemory<TXSurfaceShared>(XModelSurfs.Shared);

		TSharedPtr<FCastModelInfo> XModelInfo;

		if (Models.Contains(XModelHash))
		{
			XModelInfo = *Models.Find(XModelHash);
		}
		else
		{
			uint64 PakKey = GetXPakKey(XModelSurfs, Shared);

			if (Shared.Data != 0)
			{
				XModelInfo = ReadXModelMeshes(XModel, Shared.Data, false);
			}
			else
			{
				/*Process->XSubDecrypt->AccessCache([&](const TMap<uint64, FXSubPackageCacheObject>& Cache)
				{
					if (Cache.Contains(PakKey))
					{
						TArray<uint8> Buffer = Process->XSubDecrypt->ExtractXSubPackage(PakKey, Shared.DataSize);
						XModelInfo = ReadXModelMeshes(XModel, reinterpret_cast<uint64>(Buffer.GetData()), true);
					}
				});*/
			}
			/*else if (Shared.data == 0 && CASCPackage.Assets.ContainsKey(pakKey))
			{
				byte[]
				buffer = CASCPackage.ExtractXSubPackage(pakKey, Shared.dataSize);
				nint sharedPtr = Marshal.AllocHGlobal((int)Shared.dataSize);
				Marshal.Copy(buffer, 0, sharedPtr, (int)Shared.dataSize);
				XModelInfo = ReadXModelMeshes(XModel, (nint)SharedPtr, true);
				Marshal.FreeHGlobal(SharedPtr);
			}*/
		}

		XModelInfo->Meshes[0].UVLayer = 1;
		Models.Add(XModelHash, XModelInfo);

		const FString XModelName = Process->ReadFString(XModel.NamePtr);
		FString CleanedName = XModelName.TrimStartAndEnd();
		if (int32 LastSlashIndex; CleanedName.FindLastChar(TEXT('/'), LastSlashIndex))
		{
			CleanedName = CleanedName.RightChop(LastSlashIndex + 1);
		}
		if (int32 LastColonIndex; CleanedName.FindLastChar(TEXT(':'), LastColonIndex) && LastColonIndex > 0 &&
			CleanedName[LastColonIndex - 1] == ':')
		{
			CleanedName = CleanedName.RightChop(LastColonIndex + 1);
		}

		uint32 InstanceId = Collection.FirstInstance;
		while (InstanceId < Collection.FirstInstance + Collection.InstanceCount)
		{
			TGfxSModelInstanceData InstanceData = Process->ReadMemory<TGfxSModelInstanceData>(
				SModels.InstanceData + InstanceId * sizeof(TGfxSModelInstanceData));

			const FVector3f Translation{
				InstanceData.Translation[0] * 0.000244140625f,
				InstanceData.Translation[1] * 0.000244140625f,
				InstanceData.Translation[2] * 0.000244140625f
			};

			FVector3f TranslationForComparison{Translation.X, Translation.Y, 0};

			FQuat Rotation(
				FMath::Clamp(InstanceData.Orientation[0] * 0.000030518044f - 1.0f, -1.0f, 1.0f),
				FMath::Clamp(InstanceData.Orientation[1] * 0.000030518044f - 1.0f, -1.0f, 1.0f),
				FMath::Clamp(InstanceData.Orientation[2] * 0.000030518044f - 1.0f, -1.0f, 1.0f),
				FMath::Clamp(InstanceData.Orientation[3] * 0.000030518044f - 1.0f, -1.0f, 1.0f)
			);
			Rotation.Normalize();

			FFloat16 HalfFloatScale;
			HalfFloatScale.Encoded = InstanceData.HalfFloatScale;
			float Scale = HalfFloatScale;

			++InstanceId;
		}
	}

	TSharedPtr<FCastModelInfo> ReadXModelMeshes(TXModel XModel, uint64 Shared, bool IsLocal = false)
	{
		TXModelLodInfo LodInfo = Process->ReadMemory<TXModelLodInfo>(XModel.LodInfo);
		TXModelSurfs XModelSurfs = Process->ReadMemory<TXModelSurfs>(LodInfo.MeshPtr);

		TXSurface Surface = Process->ReadMemory<TXSurface>(XModelSurfs.Surfs);
		TMaterial Material = Process->ReadMemory<TMaterial>(Process->ReadMemory<uint64>(XModel.MaterialHandlesPtr));

		TSharedPtr<FCastModelInfo> ModelInfo = MakeShared<FCastModelInfo>();
		FCastMeshInfo Mesh;

		FCastMaterialInfo MaterialInfo;
		MaterialInfo.Name = FString::Printf(TEXT("xmaterial_%llx"), Material.Hash & 0x0FFFFFFFFFFFFFFF);
		MaterialInfo.MaterialHash = ComputeHash(MaterialInfo.Name);
		MaterialInfo.Textures = PopulateMaterial(Material);

		// ModelInfo->Materials.Add(MaterialInfo);

		uint64 XyzPtr = Shared + GetXyzOffset(Surface);
		uint64 TangentFramePtr = Shared + GetTangentFrameOffset(Surface);
		uint64 TexCoordPtr = Shared + GetTexCoordOffset(Surface);

		float Scale = GetSurfaceScale(Surface);
		FVector3f Offset = GetSurfaceOffset(Surface);
		for (uint32 VertexIdx = 0; VertexIdx < Surface.VertCount; VertexIdx++)
		{
			uint64 PackedPos = Process->ReadMemory<uint64>(XyzPtr + VertexIdx * 8, IsLocal);
			FVector3f Position{
				((PackedPos >> 00 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Scale + Offset.X,
				((PackedPos >> 21 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Scale + Offset.Y,
				((PackedPos >> 42 & 0x1FFFFF) * (1.0f / 0x1FFFFF * 2.0f) - 1.0f) * Scale + Offset.Z
			};

			Mesh.VertexPositions.Add(Position * MeshPositionScale);

			uint32 PackedTangentFrame = Process->ReadMemory<uint32>(TangentFramePtr + VertexIdx * 4, IsLocal);
			FVector3f Normal, Tangent;
			UnpackCoDQTangent(PackedTangentFrame, Tangent, Normal);
			Mesh.VertexNormals.Add(Normal);
			Mesh.VertexTangents.Add(Tangent);

			uint16 HalfUvU = Process->ReadMemory<uint16>(TexCoordPtr + VertexIdx * 4, IsLocal);
			FFloat16 HalfFloatU;
			HalfFloatU.Encoded = HalfUvU;
			float UvU = HalfFloatU;
			uint16 HalfUvV = Process->ReadMemory<uint16>(TexCoordPtr + VertexIdx * 4 + 2, IsLocal);
			FFloat16 HalfFloatV;
			HalfFloatV.Encoded = HalfUvV;
			float UvV = HalfFloatV;
			Mesh.VertexUV.Add(FVector2f(UvU, UvV));
		}

		if (GetColorOffset(Surface) != 0xFFFFFFFF)
		{
			uint64 ColorPtr = Shared + GetColorOffset(Surface);
			for (uint32 VertexIdx = 0; VertexIdx < Surface.VertCount; VertexIdx++)
			{
				uint32 Color = Process->ReadMemory<uint32>(ColorPtr + VertexIdx * 4, IsLocal);
				Mesh.VertexColor.Add(Color);
			}
		}

		if (Surface.SecondUVOffset != 0xFFFFFFFF)
		{
			uint64 TexCoord2Ptr = Shared + Surface.SecondUVOffset;
			for (uint32 VertexIdx = 0; VertexIdx < Surface.VertCount; VertexIdx++)
			{
				uint16 HalfUvU = Process->ReadMemory<uint16>(TexCoord2Ptr + VertexIdx * 4, IsLocal);
				FFloat16 HalfFloatU;
				HalfFloatU.Encoded = HalfUvU;
				float UvU = HalfFloatU;
				uint16 HalfUvV = Process->ReadMemory<uint16>(TexCoord2Ptr + VertexIdx * 4 + 2, IsLocal);
				FFloat16 HalfFloatV;
				HalfFloatV.Encoded = HalfUvV;
				float UvV = HalfFloatV;
				Mesh.VertexUV.Add(FVector2f(UvU, UvV));
				// TODO Second UV
			}
		}

		uint64 TableOffsetPtr = Shared + Surface.PackedIndiciesTableOffset;
		uint64 IndicesPtr = Shared + GetIndexDataOffset(Surface);
		uint64 PackedIndices = Shared + GetPackedIndicesOffset(Surface);

		for (uint32 TriIdx = 0; TriIdx < Surface.TriCount; TriIdx++)
		{
			TArray<uint16> Faces;
			UnpackFaceIndices(Faces, TableOffsetPtr, Surface.PackedIndicesTableCount, PackedIndices, IndicesPtr,
			                  TriIdx, IsLocal);
			Mesh.Faces.Add(Faces[2]);
			Mesh.Faces.Add(Faces[1]);
			Mesh.Faces.Add(Faces[0]);
		}
		ModelInfo->Meshes.Add(Mesh);
		return ModelInfo;
	}

protected:
	uint32 GFXMAP_POOL_IDX;

	float MeshPositionScale = 1.f;

public:
	FCordycepProcess* Process{nullptr};

private:
	TGfxWorld GfxWorld;

	TArray<TGfxWorldTransientZone> TransientZones;

	TArray<FCastMeshData> Meshes;

	TMap<uint64, TSharedPtr<FCastModelInfo>> Models;
};
