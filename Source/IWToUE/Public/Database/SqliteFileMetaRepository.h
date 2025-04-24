#pragma once
#include "Interface/DatabaseRepository.h"

class IDatabaseConnection;

class FSqliteFileMetaRepository : public IFileMetaRepository
{
public:
	FSqliteFileMetaRepository(TSharedRef<IDatabaseConnection> InConnection) : Connection(InConnection)
	{
	}

	virtual bool InsertOrUpdateFile(uint64 GameHash, const FString& RelativePath, uint64 ContentHash,
	                                            int64 LastModifiedTime, int64 KnownFileId = -1) override;
	virtual TOptional<FExistingFileInfo> QueryFileInfo(uint64 GameHash, const FString& RelativePath) override;

private:
	TSharedRef<IDatabaseConnection> Connection;
};
