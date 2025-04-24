#include "Database/SqliteFileMetaRepository.h"

#include "Interface/IDatabaseConnection.h"

bool FSqliteFileMetaRepository::InsertOrUpdateFile(uint64 GameHash, const FString& RelativePath,
                                                               uint64 ContentHash, int64 LastModifiedTime,
                                                               int64 KnownFileId)
{
	if (KnownFileId != -1)
	{
		return Connection->ExecuteStatement(
			TEXT(
				"INSERT OR REPLACE INTO FileMeta (FileId, GameHash, Path, ContentHash, LastModifiedTime) VALUES (?, ?, ?, ?, ?);"),
			[&](FSQLitePreparedStatement& Stmt)
			{
				Stmt.SetBindingValueByIndex(1, KnownFileId);
				Stmt.SetBindingValueByIndex(2, GameHash);
				Stmt.SetBindingValueByIndex(3, RelativePath);
				Stmt.SetBindingValueByIndex(4, static_cast<int64>(ContentHash));
				Stmt.SetBindingValueByIndex(5, LastModifiedTime);
				return Stmt.Execute();
			}
		);
	}
	return Connection->ExecuteStatement(
		TEXT(
			"INSERT OR REPLACE INTO FileMeta (GameHash, Path, ContentHash, LastModifiedTime) VALUES (?, ?, ?, ?);"),
		[&](FSQLitePreparedStatement& Stmt)
		{
			Stmt.SetBindingValueByIndex(1, GameHash);
			Stmt.SetBindingValueByIndex(2, RelativePath);
			Stmt.SetBindingValueByIndex(3, static_cast<int64>(ContentHash));
			Stmt.SetBindingValueByIndex(4, LastModifiedTime);
			return Stmt.Execute();
		}
	);
}

TOptional<IFileMetaRepository::FExistingFileInfo> FSqliteFileMetaRepository::QueryFileInfo(uint64 GameHash,
	const FString& RelativePath)
{
	TOptional<FExistingFileInfo> Result;
	Connection->ExecuteStatement(
		TEXT("SELECT ContentHash, LastModifiedTime, FileId FROM FileMeta WHERE GameHash = ? AND Path = ?;"),
		[GameHash, RelativePath, &Result](FSQLitePreparedStatement& Stmt)
		{
			Stmt.SetBindingValueByIndex(1, static_cast<int64>(GameHash));
			Stmt.SetBindingValueByIndex(2, RelativePath);
			if (Stmt.Step() == ESQLitePreparedStatementStepResult::Row)
			{
				FExistingFileInfo Value;
				Stmt.GetColumnValueByIndex(0, Value.ContentHash);
				Stmt.GetColumnValueByIndex(1, Value.LastModifiedTime);
				Stmt.GetColumnValueByIndex(2, Value.FileId);
				Result.Emplace(MoveTemp(Value));
				return true;
			}
			return false;
		}
	);
	return Result;
}
