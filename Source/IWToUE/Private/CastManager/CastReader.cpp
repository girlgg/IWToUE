#include "CastManager/CastReader.h"

#include "SeLogChannels.h"
#include "CastManager/CastManager.h"

bool FCastReader::LoadFile(const FString& Filename)
{
	if (CurrentPhase != EImportPhase::NOTSTARTED && CurrentPhase != EImportPhase::FAILED)
	{
		UE_LOG(LogCast, Warning, TEXT("LoadFile called while already processing file: %s"), *LoadedFilename);
		if (Filename == LoadedFilename) return CurrentPhase != EImportPhase::FAILED;
		Cleanup();
	}

	if (!FPaths::FileExists(Filename))
	{
		UE_LOG(LogCast, Error, TEXT("File does not exist: %s"), *Filename);
		CurrentPhase = EImportPhase::FAILED;
		return false;
	}

	if (!CastManager->Initialize(Filename))
	{
		UE_LOG(LogCast, Error, TEXT("FCastManager failed to initialize with file: %s"), *Filename);
		CurrentPhase = EImportPhase::FAILED;
		return false;
	}

	FileHash = FMD5Hash::HashFile(*Filename);
	LoadedFilename = Filename;
	CurrentPhase = EImportPhase::FILEOPENED;
	return true;
}

bool FCastReader::ReadSceneInfo(FCastSceneInfo& OutSceneInfo)
{
	return true;
}

bool FCastReader::ImportSceneData()
{
	if (CurrentPhase != EImportPhase::FILEOPENED)
	{
		UE_LOG(LogCast, Error, TEXT("ImportSceneData called in incorrect phase (%d) for file %s"),
		       static_cast<int>(CurrentPhase), *LoadedFilename);
		return false;
	}

	if (CastManager->Import())
	{
		CurrentPhase = EImportPhase::IMPORTED;
		return true;
	}
	UE_LOG(LogCast, Error, TEXT("FCastManager failed to import scene data."));
	CurrentPhase = EImportPhase::FAILED;
	return false;
}

FCastScene* FCastReader::GetCastScene() const
{
	if (CurrentPhase != EImportPhase::IMPORTED)
	{
		UE_LOG(LogCast, Warning, TEXT("GetCastScene called in incorrect phase (%d)"), static_cast<int>(CurrentPhase));
		return nullptr;
	}
	return CastManager->Scene.Get();
}

void FCastReader::Cleanup()
{
	UE_LOG(LogCast, Log, TEXT("Cleaning up FCastReader state for: %s"), *LoadedFilename);
	if (CastManager)
	{
		CastManager->ReleaseScene();
	}
	CurrentPhase = EImportPhase::NOTSTARTED;
	FileHash = FMD5Hash();
	LoadedFilename.Empty();
}
