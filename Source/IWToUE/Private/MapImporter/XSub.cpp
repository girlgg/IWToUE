#include "MapImporter/XSub.h"

#include "Serialization/LargeMemoryReader.h"
#include "Utils/BinaryReader.h"
#include "HAL/PlatformFilemanager.h"
#include "Misc/FileHelper.h"
#include "oodle2.h"
#include "Database/CoDDatabaseService.h"

FXSub::FXSub(uint64 GameID, const FString& GamePath)
{
	// SharedGamePath = GamePath;
	// FPaths::NormalizeFilename(SharedGamePath);
}

void FXSub::LoadFiles()
{
	{
		FRWScopeLock WriteLock(CacheLock, SLT_Write);
		LoadAssetCache();
	}

	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
	TArray<FString> FoundFiles;
	PlatformFile.FindFilesRecursively(FoundFiles, *SharedGamePath, TEXT(".xsub"));

	// 清理已移除文件的缓存
	TSet<FString> CurrentFileSet;
	for (FString& FilePath : FoundFiles)
	{
		FPaths::NormalizeFilename(FilePath);
		if (FilePath.StartsWith(SharedGamePath, ESearchCase::IgnoreCase))
		{
			int32 PrefixLength = SharedGamePath.Len();
			FString RelativePath = FilePath.RightChop(PrefixLength);

			RelativePath.RemoveFromStart(TEXT("/"));

			FilePath = RelativePath;
			CurrentFileSet.Add(FilePath);
		}
	}
	{
		FRWScopeLock WriteLock(CacheLock, SLT_Write);
		TArray<FString> FilesToRemove;
		for (const auto& FileMetaPair : FileMetas)
		{
			if (!CurrentFileSet.Contains(FileMetaPair.Key))
				FilesToRemove.Add(FileMetaPair.Key);
		}
		for (const FString& File : FilesToRemove)
		{
			RemoveInvalidEntries(File);
			FileMetas.Remove(File);
		}
	}

	ParallelFor(FoundFiles.Num(), [&](int32 Index)
	{
		const FString& FileRelativePath = FoundFiles[Index];
		const FString& FileAbsolutePath = SharedGamePath / FileRelativePath;
		if (!NeedsReload(FileRelativePath)) return;

		TMap<uint64, FXSubPackageCacheObject> LocalCache;

		TUniquePtr<IMappedFileHandle> MappedHandle(
			FPlatformFileManager::Get().GetPlatformFile().OpenMapped(*FileAbsolutePath));
		if (MappedHandle)
		{
			TUniquePtr<IMappedFileRegion> MappedRegion(MappedHandle->MapRegion(0, MappedHandle->GetFileSize()));
			if (MappedRegion)
			{
				FLargeMemoryReader Reader((MappedRegion->GetMappedPtr()), MappedRegion->GetMappedSize());
				ReadXSub(Reader, FileRelativePath, Index, LocalCache);
			}
		}

		// 线程安全合并缓存
		{
			FRWScopeLock WriteLock(CacheLock, SLT_Write);
			for (const auto& Pair : LocalCache)
			{
				CacheObjects.Add(Pair.Key, Pair.Value);
			}

			// 更新文件元数据
			FXSubFileMeta NewMeta;
			NewMeta.LastModified = IFileManager::Get().GetTimeStamp(*FileAbsolutePath);
			NewMeta.FileHash = ComputeOptimizedHash(FileAbsolutePath);
			FileMetas.Add(FileRelativePath, NewMeta);
		}
	}, true);

	{
		FRWScopeLock ReadLock(CacheLock, SLT_ReadOnly);
		SaveAssetCache();
	}
}

void FXSub::ReadXSub(FLargeMemoryReader& Reader, const FString& FilePath, int32 FileIndex,
                     TMap<uint64, FXSubPackageCacheObject>& LocalCache)
{
	uint32 Magic = FBinaryReader::ReadUInt32(Reader);
	uint16 Unknown1 = FBinaryReader::ReadUInt16(Reader);
	uint16 Version = FBinaryReader::ReadUInt16(Reader);
	uint64 Unknown = FBinaryReader::ReadUInt64(Reader);
	uint64 Type = FBinaryReader::ReadUInt64(Reader);
	uint64 Size = FBinaryReader::ReadUInt64(Reader);
	FString UnknownHashes = FBinaryReader::ReadFString(Reader, 1896);
	uint64 FileCount = FBinaryReader::ReadUInt64(Reader);
	uint64 DataOffset = FBinaryReader::ReadUInt64(Reader);
	uint64 DataSize = FBinaryReader::ReadUInt64(Reader);
	uint64 HashCount = FBinaryReader::ReadUInt64(Reader);
	uint64 HashOffset = FBinaryReader::ReadUInt64(Reader);
	uint64 HashSize = FBinaryReader::ReadUInt64(Reader);
	uint64 Unknown3 = FBinaryReader::ReadUInt64(Reader);
	uint64 UnknownOffset = FBinaryReader::ReadUInt64(Reader);
	uint64 Unknown4 = FBinaryReader::ReadUInt64(Reader);
	uint64 IndexCount = FBinaryReader::ReadUInt64(Reader);
	uint64 IndexOffset = FBinaryReader::ReadUInt64(Reader);
	uint64 IndexSize = FBinaryReader::ReadUInt64(Reader);

	if (Type != 3) return;
	if (Magic != 0x4950414b || HashOffset >= static_cast<uint64>(Reader.TotalSize()))
	{
		UE_LOG(LogTemp, Error, TEXT("Invalid XSub file %s"), *FilePath);
		return;
	}

	Reader.Seek(HashOffset);
	for (uint64 i = 0; i < HashCount; i++)
	{
		uint64 Key = FBinaryReader::ReadUInt64(Reader);
		uint64 PackedInfo = FBinaryReader::ReadUInt64(Reader);
		uint32 PackedInfoEx = FBinaryReader::ReadUInt32(Reader);

		FXSubPackageCacheObject CacheObject;
		CacheObject.Offset = ((PackedInfo >> 32) << 7);
		CacheObject.CompressedSize = ((PackedInfo >> 1) & 0x3FFFFFFF);
		CacheObject.Path = FilePath;

		// 解析块头信息计算解压后尺寸
		const int64 OriginalPos = Reader.Tell();
		Reader.Seek(CacheObject.Offset);

		// 读取块头验证Key
		uint16 BlockHeader;
		Reader << BlockHeader;
		uint64 KeyFromBlock;
		Reader << KeyFromBlock;

		if (KeyFromBlock == Key)
		{
			Reader.Seek(CacheObject.Offset + 22);
			uint8 BlockCount;
			Reader << BlockCount;

			uint32 TotalDecompressed = 0;
			for (uint8 j = 0; j < BlockCount; ++j)
			{
				uint8 CompressionType;
				uint32 CompressedSize, DecompressedSize;
				Reader << CompressionType;
				Reader << CompressedSize;
				Reader << DecompressedSize;
				Reader.Seek(12);

				TotalDecompressed += DecompressedSize;
			}
			CacheObject.UncompressedSize = TotalDecompressed;
		}
		else
		{
			CacheObject.UncompressedSize = CacheObject.CompressedSize;
		}

		Reader.Seek(OriginalPos);
		LocalCache.Emplace(Key, CacheObject);
	}
}

TArray<uint8> FXSub::ExtractXSubPackage(uint64 Key, uint32 Size)
{
	FRWScopeLock ReadLock(CacheLock, SLT_ReadOnly);

	TOptional<FXSubPackageCacheObject> ResObj = FCoDDatabaseService::Get().GetXSubInfoSync(Key);
	if (!ResObj.IsSet())
	{
		UE_LOG(LogTemp, Warning, TEXT("%lld does not exist in the current package objects database."), Key);
		return TArray<uint8>();
	}
	FXSubPackageCacheObject CacheObject = ResObj.GetValue();

	TUniquePtr<FArchive> Reader(IFileManager::Get().CreateFileReader(*CacheObject.Path, FILEREAD_Silent));
	if (!Reader)
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to open file: %s"), *CacheObject.Path);
		return TArray<uint8>();
	}

	const uint32 BufferSize = Size > 0 ? Size : 0x2400000;
	TArray<uint8> DecompressedData;
	DecompressedData.AddUninitialized(BufferSize);

	uint64 BlockPosition = CacheObject.Offset;
	const uint64 BlockEnd = CacheObject.Offset + CacheObject.CompressedSize;

	Reader->Seek(BlockPosition + 2);

	// 验证密钥
	uint64 FileKey;
	*Reader << FileKey;
	if (FileKey != Key)
	{
		Reader->Seek(BlockPosition);
		TArray<uint8> RawData;
		RawData.SetNumUninitialized(CacheObject.CompressedSize);
		Reader->Serialize(RawData.GetData(), CacheObject.CompressedSize);
		return RawData;
	}

	TArray<FXSubBlock> Blocks;
	TArray<uint8> CompressedData;
	uint64 BlockDataSize = 0;

	while (BlockPosition < BlockEnd)
	{
		// 读取块信息
		Reader->Seek(BlockPosition + 22);
		uint8 BlockCount;
		*Reader << BlockCount;

		Blocks.SetNum(BlockCount, EAllowShrinking::No);
		for (int32 i = 0; i < BlockCount; ++i)
		{
			auto& [CompressionType, CompressedSize, DecompressedSize, BlockOffset, DecompressedOffset, Unknown]
				= Blocks[i];
			*Reader << CompressionType;
			*Reader << CompressedSize;
			*Reader << DecompressedSize;
			*Reader << BlockOffset;
			*Reader << DecompressedOffset;
			*Reader << Unknown;
		}

		// 处理每个数据块
		for (int32 i = 0; i < BlockCount; ++i)
		{
			const FXSubBlock& Block = Blocks[i];

			// 定位到块数据位置
			Reader->Seek(BlockPosition + Block.BlockOffset);

			// 读取压缩数据
			CompressedData.SetNumUninitialized(Block.CompressedSize, EAllowShrinking::No);
			Reader->Serialize(CompressedData.GetData(), Block.CompressedSize);

			const uint8* CompressedBlockPtr = CompressedData.GetData();
			uint8* DecompressedPtr = DecompressedData.GetData() + Block.DecompressedOffset;

			switch (Block.CompressionType)
			{
			case 6:
				{
					BlockDataSize += OodleLZ_Decompress(CompressedBlockPtr,
					                                    Block.CompressedSize,
					                                    DecompressedPtr,
					                                    Block.DecompressedSize,
					                                    OodleLZ_FuzzSafe_No,
					                                    OodleLZ_CheckCRC_No,
					                                    OodleLZ_Verbosity_None,
					                                    nullptr,
					                                    0,
					                                    nullptr,
					                                    nullptr,
					                                    nullptr,
					                                    0,
					                                    OodleLZ_Decode_ThreadPhaseAll);
				}
				break;
			default:
				UE_LOG(LogTemp, Warning, TEXT("Unknown compression type %d"), Block.CompressionType);
				break;
			}
		}

		const int64 CurrentPos = Reader->Tell();
		BlockPosition = (CurrentPos + 0x7F) & ~0x7F;
		if (CurrentPos != BlockPosition)
		{
			Reader->Seek(BlockPosition);
		}
	}

	DecompressedData.SetNum(BlockDataSize);
	return DecompressedData;
}

bool FXSub::ExistsKey(uint64 CacheID)
{
	TOptional<FXSubPackageCacheObject> Result = FCoDDatabaseService::Get().GetXSubInfoSync(CacheID);
	return Result.IsSet();
}

void FXSub::RemoveInvalidEntries(const FString& RemovedFilePath)
{
	for (auto It = CacheObjects.CreateIterator(); It; ++It)
	{
		if (It.Value().Path == RemovedFilePath)
		{
			It.RemoveCurrent();
		}
	}
}

void FXSub::SaveAssetCache()
{
	FString SavePath = FPaths::ProjectSavedDir() / TEXT("IWToUE") / TEXT("XSubCache.bin");

	TArray<uint8> SaveData;
	FMemoryWriter Writer(SaveData);

	FXSubAssetCache CurrentCache{CacheObjects, FileMetas};
	Writer << CurrentCache;

	FFileHelper::SaveArrayToFile(SaveData, *SavePath);
}

void FXSub::LoadAssetCache()
{
	FString LoadPath = FPaths::ProjectSavedDir() / TEXT("IWToUE") / TEXT("XSubCache.bin");

	TArray<uint8> LoadData;
	if (!FFileHelper::LoadFileToArray(LoadData, *LoadPath)) return;

	FMemoryReader Reader(LoadData);
	FXSubAssetCache LoadedCache;
	Reader << LoadedCache;

	CacheObjects = MoveTemp(LoadedCache.CacheObjects);
	FileMetas = MoveTemp(LoadedCache.FileMetas);
}

bool FXSub::NeedsReload(const FString& FilePath)
{
	FRWScopeLock ReadLock(CacheLock, SLT_ReadOnly);

	const FString FileAbsolutePath = SharedGamePath / FilePath;
	const FDateTime CurrentTime = IFileManager::Get().GetTimeStamp(*FileAbsolutePath);
	const FXSubFileMeta* CachedMeta = FileMetas.Find(FilePath);

	// 首次发现文件
	if (!CachedMeta) return true;

	// 修改时间变化时再计算哈希
	if (CachedMeta->LastModified != CurrentTime)
	{
		const FString NewHash = ComputeOptimizedHash(FileAbsolutePath);
		return NewHash != CachedMeta->FileHash;
	}
	return false;
}

FString FXSub::ComputeOptimizedHash(const FString& FilePath)
{
	TUniquePtr<FArchive> Ar(IFileManager::Get().CreateFileReader(*FilePath));
	if (!Ar) return FString();

	const int64 SampleSize = FMath::Min(Ar->TotalSize(), 1024LL * 1024); // 采样1MB计算哈希
	TArray<uint8> HeadData;
	HeadData.SetNumUninitialized(SampleSize);
	Ar->Serialize(HeadData.GetData(), SampleSize);

	FSHAHash Hash;
	FSHA1::HashBuffer(HeadData.GetData(), SampleSize, Hash.Hash);
	return Hash.ToString();
}
