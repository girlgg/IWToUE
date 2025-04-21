#pragma once
#include "CastManager.h"

struct FCastSceneInfo;
struct FCastScene;
class FCastManager;

class FCastReader
{
public:
	FCastReader()
	{
	}

	bool LoadFile(const FString& Filename);

	bool ReadSceneInfo(FCastSceneInfo& OutSceneInfo);

	bool ImportSceneData();

	FCastScene* GetCastScene() const;

	FMD5Hash GetFileHash() const { return FileHash; }
	FString GetLoadedFilename() const { return LoadedFilename; }

	void Cleanup();

private:
	enum class EImportPhase { NOTSTARTED, FILEOPENED, IMPORTED, FAILED };

	TUniquePtr<FCastManager> CastManager = MakeUnique<FCastManager>();
	EImportPhase CurrentPhase = EImportPhase::NOTSTARTED;
	FMD5Hash FileHash;
	FString LoadedFilename;
};
