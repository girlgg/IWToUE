#pragma once

struct FXSubPackageCacheObject;

class IAssetNameRepository
{
public:
	virtual ~IAssetNameRepository() = default;
	virtual bool InsertOrUpdate(uint64 Hash, const FString& Value) = 0;
	virtual bool BatchInsertOrUpdate(const TMap<uint64, FString>& Items) = 0;
	virtual TOptional<FString> QueryValue(uint64 Hash) = 0;
	virtual bool DeleteByHash(uint64 Hash) = 0;
};

class IXSubInfoRepository
{
public:
	virtual ~IXSubInfoRepository() = default;
	virtual bool BatchInsertOrUpdate(const TMap<uint64, FXSubPackageCacheObject>& Items) = 0;
	virtual TOptional<FXSubPackageCacheObject> QueryValue(uint64 DecryptionKey) = 0;
};

class IFileMetaRepository
{
public:
	virtual ~IFileMetaRepository() = default;
	virtual bool InsertOrUpdateFile(uint64 GameHash, const FString& RelativePath, uint64 ContentHash,
	                                            int64 LastModifiedTime, int64 KnownFileId = -1) = 0;

	struct FExistingFileInfo
	{
		uint64 ContentHash;
		int64 LastModifiedTime;
		int64 FileId;
	};

	virtual TOptional<FExistingFileInfo> QueryFileInfo(uint64 GameHash, const FString& RelativePath) = 0;
};

class IGameRepository
{
public:
	virtual ~IGameRepository() = default;
	virtual bool InsertOrUpdateGame(uint64 GameHash, const FString& GamePath) = 0;
	virtual TOptional<FString> QueryGamePath(uint64 GameHash) = 0;
};
