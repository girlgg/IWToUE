#include "Database/CoDFileTracker.h"

#include "SeLogChannels.h"
#include "Interface/DatabaseRepository.h"
#include "Interfaces/IPluginManager.h"
#include "MapImporter/XSub.h"
#include "Serialization/LargeMemoryReader.h"
#include "Utils/BinaryReader.h"


FCoDFileTracker::FCoDFileTracker(const TSharedPtr<IFileMetaRepository>& InFileMetaRepo,
                                 const TSharedPtr<IGameRepository>& InGameRepo,
                                 const TSharedPtr<IAssetNameRepository>& InAssetNameRepo,
                                 const TSharedPtr<IXSubInfoRepository>& InXSubInfoRepo)
	: FileMetaRepo(InFileMetaRepo), GameRepo(InGameRepo), AssetNameRepo(InAssetNameRepo), XSubInfoRepo(InXSubInfoRepo)
{
}

FCoDFileTracker::~FCoDFileTracker()
{
	Stop();
	if (Thread)
	{
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
}

void FCoDFileTracker::Initialize(uint64 GameHash, const FString& GamePath)
{
	CurrentGameHash = GameHash;
	CurrentGamePath = GamePath;
	FPaths::NormalizeDirectoryName(CurrentGamePath);
	PluginBasePath = IPluginManager::Get().FindPlugin(TEXT("IWToUE"))->GetBaseDir();
	if (CurrentGameHash && !CurrentGamePath.IsEmpty())
	{
		GameRepo->InsertOrUpdateGame(CurrentGameHash, CurrentGamePath);
	}
}

void FCoDFileTracker::StartTrackingAsync()
{
	if (Thread || !bIsComplete)
	{
		UE_LOG(LogITUDatabase, Warning, TEXT("File tracking already in progress."));
		return;
	}
	bIsComplete = false;
	bShouldRun = true;
	Thread = FRunnableThread::Create(this, TEXT("CoDFileTrackerThread"));
	UE_LOG(LogITUDatabase, Log, TEXT("Started File Tracking Thread."));
}

uint32 FCoDFileTracker::Run()
{
	UE_LOG(LogITUDatabase, Log, TEXT("File Tracking Thread Started Execution."));
	ProcessFiles();
	UE_LOG(LogITUDatabase, Log, TEXT("File Tracking Thread Finished Execution."));
	bIsComplete = true;
	OnComplete.ExecuteIfBound();
	return 0;
}

void FCoDFileTracker::Stop()
{
	bShouldRun = false;
}

void FCoDFileTracker::ProcessFiles()
{
	// --- WNI Processing ---
	TArray<FString> WniFiles = FindWniFilesToTrack();
	TMap<uint64, FString> CombinedAssetNameMap;
	int ProcessedWni = 0;
	for (const FString& FilePath : WniFiles)
	{
		if (!bShouldRun) break;

		FString RelativePath = GetRelativePath(FilePath, PluginBasePath);
		uint64 ContentHash = ComputeFileContentHash(FilePath);
		int64 LastModifiedTime = GetFileLastModifiedTime(FilePath);
		int64 FileId = -1;
		bool bNeedsUpdate = CheckIfFileNeedsUpdate(1, RelativePath, ContentHash, LastModifiedTime, FileId);

		if (bNeedsUpdate && FileId != -1)
		{
			TMap<uint64, FString> Items;
			ParseWniFile(FilePath, Items);
			CombinedAssetNameMap.Append(Items);
			ProcessedWni++;
		}
	}
	if (CombinedAssetNameMap.Num() > 0)
	{
		UE_LOG(LogTemp, Log, TEXT("Updating AssetName cache from %d WNI files..."), ProcessedWni);
		bool bWniBatchOk = AssetNameRepo->BatchInsertOrUpdate(CombinedAssetNameMap);
		UE_LOG(LogTemp, Log, TEXT("WNI AssetName batch update %s."), bWniBatchOk ? TEXT("succeeded") : TEXT("FAILED"));
	}
	// --- XSub Processing ---
	if (CurrentGameHash != 0 && !CurrentGamePath.IsEmpty())
	{
		TArray<FString> XSubFiles = FindXSubFilesToTrack();
		TMap<uint64, FXSubPackageCacheObject> CombinedXSubMap;
		int ProcessedXSub = 0;
		for (const FString& FilePath : XSubFiles)
		{
			if (!bShouldRun) break;

			FString RelativePath = GetRelativePath(FilePath, CurrentGamePath);
			uint64 ContentHash = ComputeFileContentHash(FilePath);
			int64 LastModifiedTime = GetFileLastModifiedTime(FilePath);
			int64 FileId = -1;

			bool bNeedsUpdate = CheckIfFileNeedsUpdate(CurrentGameHash, RelativePath, ContentHash, LastModifiedTime,
			                                           FileId);

			if (bNeedsUpdate && FileId != -1)
			{
				TMap<uint64, FXSubPackageCacheObject> Items;
				ParseXSubFile(FilePath, Items, FileId);
				CombinedXSubMap.Append(Items);
				ProcessedXSub++;
			}
		}
		if (CombinedXSubMap.Num() > 0)
		{
			UE_LOG(LogTemp, Log, TEXT("Updating XSub cache from %d XSub files..."), ProcessedXSub);
			bool bXsubBatchOk = XSubInfoRepo->BatchInsertOrUpdate(CombinedXSubMap);
			UE_LOG(LogTemp, Log, TEXT("XSub batch update %s."), bXsubBatchOk ? TEXT("succeeded") : TEXT("FAILED"));
		}
	}
}

TArray<FString> FCoDFileTracker::FindWniFilesToTrack() const
{
	TArray<FString> WniFiles;
	FString TrackedDir = FPaths::ProjectPluginsDir() / TEXT("IWToUE");
	IFileManager::Get().FindFilesRecursive(WniFiles, *TrackedDir, TEXT("*.wni"), true, false);

	return WniFiles;
}

TArray<FString> FCoDFileTracker::FindXSubFilesToTrack() const
{
	TArray<FString> XSubFiles;
	if (!CurrentGamePath.IsEmpty())
	{
		IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();
		PlatformFile.FindFilesRecursively(XSubFiles, *CurrentGamePath, TEXT(".xsub"));
	}
	return XSubFiles;
}

FString FCoDFileTracker::GetRelativePath(const FString& FullPath, const FString& BaseDir) const
{
	FString NormalizedFullPath = FPaths::ConvertRelativePathToFull(FullPath);
	FString NormalizedBaseDir = FPaths::ConvertRelativePathToFull(BaseDir);

	FPaths::NormalizeDirectoryName(NormalizedFullPath);
	FPaths::NormalizeDirectoryName(NormalizedBaseDir);

	NormalizedBaseDir += TEXT("/");

	if (NormalizedFullPath.StartsWith(NormalizedBaseDir))
	{
		return NormalizedFullPath.Mid(NormalizedBaseDir.Len());
	}

	return FullPath;
}

uint64 FCoDFileTracker::ComputeFileContentHash(const FString& FilePath, int64 ComputeSize) const
{
	IPlatformFile& PlatformFile = FPlatformFileManager::Get().GetPlatformFile();

	TUniquePtr<IFileHandle> FileHandle(PlatformFile.OpenRead(*FilePath));
	if (!FileHandle)
	{
		return 0;
	}

	int64 FileToReadSize = FMath::Min(ComputeSize, FileHandle->Size());
	TArray<uint8> Buffer;
	Buffer.SetNumUninitialized(FileToReadSize);
	bool bSuccess = FileHandle->Read(Buffer.GetData(), FileToReadSize);

	if (!bSuccess)
	{
		return 0;
	}

	uint64 Hash = CityHash64((const char*)Buffer.GetData(), FileToReadSize);
	return Hash;
}

int64 FCoDFileTracker::GetFileLastModifiedTime(const FString& FilePath) const
{
	FDateTime TimeStamp = IFileManager::Get().GetTimeStamp(*FilePath);
	return TimeStamp.ToUnixTimestamp();
}

void FCoDFileTracker::ParseWniFile(const FString& FilePath, TMap<uint64, FString>& Items)
{
	TArray<uint8> FileData;
	if (!FFileHelper::LoadFileToArray(FileData, *FilePath)) return;

	FMemoryReader Ar(FileData, true);
	uint32 Magic;
	Ar << Magic;
	if (Magic != 0x20494E57) // 'WNI '
	{
		UE_LOG(LogTemp, Warning, TEXT("Invalid file format: %s"), *FilePath);
		return;
	}

	// 版本号
	Ar.Seek(Ar.Tell() + 2);

	// 条目数
	uint32 EntryCount;
	Ar << EntryCount;

	// 压缩大小
	uint32 PackedSize, UnpackedSize;
	Ar << PackedSize;
	Ar << UnpackedSize;

	// 校验剩余数据大小
	const int64 RemainingSize = Ar.TotalSize() - Ar.Tell();
	if (RemainingSize < PackedSize)
	{
		UE_LOG(LogTemp, Warning, TEXT("Corrupted file: %s"), *FilePath);
		return;
	}

	// 读取压缩数据
	TArray<uint8> CompressedData;
	CompressedData.SetNumUninitialized(PackedSize);
	Ar.Serialize(CompressedData.GetData(), PackedSize);

	// 准备解压缓冲区
	TArray<uint8> DecompressedData;
	const int32 PaddedSize = (UnpackedSize + 3) & ~3;
	DecompressedData.SetNumUninitialized(PaddedSize);

	// LZ4解压
	bool bDecompressSuccess = FCompression::UncompressMemory(
		NAME_LZ4,
		DecompressedData.GetData(),
		UnpackedSize,
		CompressedData.GetData(),
		CompressedData.Num(),
		COMPRESS_NoFlags
	);

	if (!bDecompressSuccess)
	{
		UE_LOG(LogTemp, Warning, TEXT("Decompression failed: %s"), *FilePath);
		return;
	}

	const uint8* StreamPtr = DecompressedData.GetData();
	const int64 StreamLength = DecompressedData.Num();
	int64 CurrentPos = 0;

	// 验证解压后数据足够存放EntryCount
	if (StreamLength < static_cast<int64>(EntryCount * sizeof(uint64_t)))
	{
		UE_LOG(LogTemp, Error, TEXT("Corrupted decompressed data"));
		return;
	}
	auto CheckBounds = [](int64 Current, int64 Needed, int64 Total)
	{
		if (Current + Needed > Total)
		{
			UE_LOG(LogTemp, Error, TEXT("Data overflow: need %lld bytes at offset %lld"), Needed, Current);
			return false;
		}
		return true;
	};
	// 解析条目
	for (uint32 i = 0; i < EntryCount; ++i)
	{
		// 1. 读取Key（小端序处理）
		uint64 Key;
		if (!CheckBounds(CurrentPos, sizeof(uint64), StreamLength)) break;

		FMemory::Memcpy(&Key, StreamPtr + CurrentPos, sizeof(uint64));
		// Key = FGenericPlatformMisc::NetToHost64(Key); // 正确的字节序转换
		Key &= 0x0FFFFFFFFFFFFFFF; // 应用掩码
		CurrentPos += sizeof(uint64);

		// 2. 读取C风格字符串
		if (!CheckBounds(CurrentPos, 1, StreamLength)) break;

		FString Value;
		const ANSICHAR* StrStart = reinterpret_cast<const ANSICHAR*>(StreamPtr + CurrentPos);
		int32 MaxLen = StreamLength - CurrentPos;
		int32 StrLen = 0;

		// 手动查找null终止符
		while (StrLen < MaxLen && StrStart[StrLen] != '\0')
		{
			StrLen++;
		}

		if (StrLen == MaxLen)
		{
			UE_LOG(LogTemp, Warning, TEXT("Unterminated string at entry %d"), i);
			break;
		}

		Value = FString(StrLen, StrStart); // UTF-8转换
		CurrentPos += StrLen + 1; // 跳过null终止符

		Items.Add(Key, Value);
	}
}

void FCoDFileTracker::ParseXSubFile(const FString& FilePath, TMap<uint64, FXSubPackageCacheObject>& Items, int64 FileId)
{
	TUniquePtr<IMappedFileHandle> MappedHandle(
		FPlatformFileManager::Get().GetPlatformFile().OpenMapped(*FilePath));
	if (!MappedHandle)
	{
		return;
	}
	TUniquePtr<IMappedFileRegion> MappedRegion(MappedHandle->MapRegion(0, MappedHandle->GetFileSize()));
	if (!MappedRegion)
	{
		return;
	}
	FLargeMemoryReader Reader((MappedRegion->GetMappedPtr()), MappedRegion->GetMappedSize());

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

		CacheObject.FileId = FileId;

		Reader.Seek(OriginalPos);
		Items.Emplace(Key, CacheObject);
	}
}

bool FCoDFileTracker::CheckIfFileNeedsUpdate(uint64 GameHash, const FString& RelativePath, uint64 NewContentHash,
                                             int64 NewLastModifiedTime, int64& OutFileId)
{
	TOptional<IFileMetaRepository::FExistingFileInfo> ExistingInfo =
		FileMetaRepo->QueryFileInfo(GameHash, RelativePath);

	if (ExistingInfo.IsSet())
	{
		OutFileId = ExistingInfo->FileId;
		if (NewContentHash == ExistingInfo->ContentHash && NewLastModifiedTime == ExistingInfo->LastModifiedTime)
		{
			return false;
		}
		FileMetaRepo->InsertOrUpdateFile(GameHash, RelativePath, NewContentHash, NewLastModifiedTime, OutFileId);
		return true;
	}
	if (FileMetaRepo->InsertOrUpdateFile(GameHash, RelativePath, NewContentHash,
	                                     NewLastModifiedTime))
	{
		TOptional<IFileMetaRepository::FExistingFileInfo> NewInfo =
			FileMetaRepo->QueryFileInfo(GameHash, RelativePath);
		if (NewInfo.IsSet())
		{
			OutFileId = NewInfo->FileId;
			return true;
		}
	}
	UE_LOG(LogTemp, Error, TEXT("Failed to insert FileMeta for %s"), *RelativePath);
	OutFileId = -1;
	return false;
}
