#include "GameInfo/ModernWarfare6AssetDiscoverer.h"

#include "SeLogChannels.h"
#include "CDN/CoDCDNDownloaderV2.h"
#include "Database/CoDDatabaseService.h"
#include "Interface/IMemoryReader.h"
#include "Structures/MW6GameStructures.h"

FModernWarfare6AssetDiscoverer::FModernWarfare6AssetDiscoverer()
{
}

bool FModernWarfare6AssetDiscoverer::Initialize(IMemoryReader* InReader,
                                                const CoDAssets::FCoDGameProcess& InProcessInfo,
                                                TSharedPtr<LocateGameInfo::FParasyteBaseState> InParasyteState)
{
	Reader = InReader;
	ParasyteState = InParasyteState;

	if (!Reader || !Reader->IsValid() || !ParasyteState.IsValid())
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("MW6 Discoverer Init Failed: Invalid reader or Parasyte state."));
		return false;
	}

	if (ParasyteState->GameID == 0x4B4F4D41594D4159)
	{
		GameType = CoDAssets::ESupportedGames::ModernWarfare6;
		GameFlag = ParasyteState->Flags.Contains("sp")
			           ? CoDAssets::ESupportedGameFlags::SP
			           : CoDAssets::ESupportedGameFlags::MP;

		XSubDecrypt = MakeShared<FXSub>(ParasyteState->GameID, ParasyteState->GameDirectory);
		FCoDDatabaseService::Get().SetCurrentGameContext(ParasyteState->GameID, ParasyteState->GameDirectory);
		CDNDownloader = MakeUnique<FCoDCDNDownloaderV2>();
		if (CDNDownloader)
		{
			CDNDownloader->Initialize(ParasyteState->GameDirectory);
		}
		UE_LOG(LogITUAssetImporter, Log, TEXT("MW6 Discoverer Initialized for game ID %llX"),
		       ParasyteState->GameID);
		return true;
	}
	UE_LOG(LogITUAssetImporter, Error, TEXT("Discoverer Init Failed: Unexpected game ID %llX"),
	       ParasyteState->GameID);
	GameType = CoDAssets::ESupportedGames::None;
	GameFlag = CoDAssets::ESupportedGameFlags::None;
	return false;
}

TArray<FAssetPoolDefinition> FModernWarfare6AssetDiscoverer::GetAssetPools() const
{
	return {
		{9, EWraithAssetType::Model, TEXT("XModel")},
		{21, EWraithAssetType::Image, TEXT("XImage")},
		{7, EWraithAssetType::Animation, TEXT("XAnim")},
		{11, EWraithAssetType::Material, TEXT("XMaterial")},
		{193, EWraithAssetType::Sound, TEXT("XSound")}
	};
}

int32 FModernWarfare6AssetDiscoverer::DiscoverAssetsInPool(const FAssetPoolDefinition& PoolDefinition,
                                                           FAssetDiscoveredDelegate OnAssetDiscovered)
{
	if (!Reader || !Reader->IsValid() || !ParasyteState.IsValid()) return 0;

	FXAssetPool64 PoolHeader;
	if (!ReadAssetPoolHeader(PoolDefinition.PoolIdentifier, PoolHeader))
	{
		UE_LOG(LogTemp, Error, TEXT("Failed to read pool header for %s (ID: %d)"), *PoolDefinition.PoolName,
		       PoolDefinition.PoolIdentifier);
		return 0;
	}

	UE_LOG(LogTemp, Log, TEXT("Starting discovery for pool %s (Root: 0x%llX)"), *PoolDefinition.PoolName,
	       PoolHeader.Root);

	uint64 CurrentNodePtr = PoolHeader.Root;
	int DiscoveredCount = 0;
	while (CurrentNodePtr != 0)
	{
		FXAsset64 CurrentNode;
		if (!ReadAssetNode(CurrentNodePtr, CurrentNode))
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to read asset node at 0x%llX in pool %s."), CurrentNodePtr,
			       *PoolDefinition.PoolName);
			break;
		}

		if (CurrentNode.Header != 0)
		{
			switch (PoolDefinition.AssetType)
			{
			case EWraithAssetType::Model:
				DiscoverModelAssets(CurrentNode, OnAssetDiscovered);
				break;
			case EWraithAssetType::Image:
				DiscoverImageAssets(CurrentNode, OnAssetDiscovered);
				break;
			case EWraithAssetType::Animation:
				DiscoverAnimAssets(CurrentNode, OnAssetDiscovered);
				break;
			case EWraithAssetType::Material:
				DiscoverMaterialAssets(CurrentNode, OnAssetDiscovered);
				break;
			case EWraithAssetType::Sound:
				DiscoverSoundAssets(CurrentNode, OnAssetDiscovered);
				break;
			default:
				UE_LOG(LogTemp, Warning, TEXT("No discovery logic for asset type %d in pool %s"),
				       (int)PoolDefinition.AssetType, *PoolDefinition.PoolName);
				break;
			}
			DiscoveredCount++;
		}
		CurrentNodePtr = CurrentNode.Next;
	}
	UE_LOG(LogTemp, Log, TEXT("Finished discovery for pool %s. Discovered %d assets."), *PoolDefinition.PoolName,
	       DiscoveredCount);
	return DiscoveredCount;
}

bool FModernWarfare6AssetDiscoverer::LoadStringTableEntry(uint64 Index, FString& OutString)
{
	if (Reader && ParasyteState.IsValid() && ParasyteState->StringsAddress != 0)
	{
		return Reader->ReadString(ParasyteState->StringsAddress + Index, OutString);
	}
	OutString.Empty();
	return false;
}

bool FModernWarfare6AssetDiscoverer::ReadAssetPoolHeader(int32 PoolIdentifier, FXAssetPool64& OutPoolHeader)
{
	if (!Reader || !ParasyteState.IsValid() || ParasyteState->PoolsAddress == 0) return false;
	uint64 PoolAddress = ParasyteState->PoolsAddress + sizeof(FXAssetPool64) * PoolIdentifier;
	return Reader->ReadMemory<FXAssetPool64>(PoolAddress, OutPoolHeader);
}

bool FModernWarfare6AssetDiscoverer::ReadAssetNode(uint64 AssetNodePtr, FXAsset64& OutAssetNode)
{
	if (!Reader || AssetNodePtr == 0) return false;
	return Reader->ReadMemory<FXAsset64>(AssetNodePtr, OutAssetNode);
}

void FModernWarfare6AssetDiscoverer::DiscoverModelAssets(FXAsset64 AssetNode,
                                                         FAssetDiscoveredDelegate OnAssetDiscovered)
{
	FMW6XModel ModelHeader;
	if (!Reader->ReadMemory<FMW6XModel>(AssetNode.Header, ModelHeader))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read XModel header at 0x%llX"), AssetNode.Header);
		return;
	}
	uint64 ModelHash = ModelHeader.Hash & 0xFFFFFFFFFFFFFFF;

	auto CreateModel = [&](FString ModelName)
	{
		TSharedPtr<FCoDModel> LoadedModel = MakeShared<FCoDModel>();
		LoadedModel->AssetType = EWraithAssetType::Model;
		LoadedModel->AssetName = ProcessAssetName(ModelName);
		LoadedModel->AssetPointer = AssetNode.Header;
		LoadedModel->BoneCount =
			(ModelHeader.ParentListPtr > 0) ? (ModelHeader.NumBones + ModelHeader.UnkBoneCount) : 0;
		LoadedModel->LodCount = ModelHeader.NumLods;
		LoadedModel->AssetStatus = AssetNode.Temp == 1
			                           ? EWraithAssetStatus::Placeholder
			                           : EWraithAssetStatus::Loaded;

		OnAssetDiscovered.ExecuteIfBound(LoadedModel);
	};

	if (ModelHeader.NamePtr != 0)
	{
		FString RawModelName;
		if (Reader->ReadString(ModelHeader.NamePtr, RawModelName))
		{
			CreateModel(RawModelName);
		}
		else
		{
			UE_LOG(LogTemp, Warning, TEXT("Failed to read XModel name string at 0x%llX, using hash."),
			       ModelHeader.NamePtr);
			CreateModel(FString::Printf(TEXT("xmodel_%llx"), ModelHash));
		}
	}
	else
	{
		FCoDDatabaseService::Get().GetAssetNameAsync(
			ModelHash, [CreateModel = MoveTemp(CreateModel), ModelHash](TOptional<FString> QueryName)
			{
				CreateModel(QueryName.IsSet()
					            ? QueryName.GetValue()
					            : FString::Printf(TEXT("xmodel_%llx"), ModelHash));
			});
	}
}

void FModernWarfare6AssetDiscoverer::DiscoverImageAssets(FXAsset64 AssetNode,
                                                         FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW6GfxImage, FCoDImage>(
		AssetNode, EWraithAssetType::Image, TEXT("ximage"),
		[](const FMW6GfxImage& ImageHeader, TSharedPtr<FCoDImage> LoadedImage)
		{
			LoadedImage->Width = ImageHeader.Width;
			LoadedImage->Height = ImageHeader.Height;
			LoadedImage->Format = ImageHeader.ImageFormat;
			LoadedImage->bIsFileEntry = false;
			LoadedImage->Streamed = ImageHeader.LoadedImagePtr == 0;
		},
		OnAssetDiscovered
	);
}

void FModernWarfare6AssetDiscoverer::DiscoverAnimAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW6XAnim, FCoDAnim>(
		AssetNode, EWraithAssetType::Animation, TEXT("xanim"),
		[](const FMW6XAnim& Anim, TSharedPtr<FCoDAnim> LoadedAnim)
		{
			LoadedAnim->AssetType = EWraithAssetType::Animation;
			LoadedAnim->Framerate = Anim.Framerate;
			LoadedAnim->FrameCount = Anim.FrameCount;
			LoadedAnim->BoneCount = Anim.TotalBoneCount;
		},
		OnAssetDiscovered
	);
}

void FModernWarfare6AssetDiscoverer::DiscoverMaterialAssets(FXAsset64 AssetNode,
                                                            FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW6Material, FCoDMaterial>(
		AssetNode, EWraithAssetType::Material, TEXT("xmaterial"),
		[](const FMW6Material& Material, TSharedPtr<FCoDMaterial> LoadedMaterial)
		{
			LoadedMaterial->AssetType = EWraithAssetType::Material;
			LoadedMaterial->ImageCount = Material.ImageCount;
		},
		OnAssetDiscovered
	);
}

void FModernWarfare6AssetDiscoverer::DiscoverSoundAssets(FXAsset64 AssetNode,
                                                         FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW6SoundAsset, FCoDSound>(
		AssetNode, EWraithAssetType::Sound, TEXT("xsound"),
		[](const FMW6SoundAsset& Sound, TSharedPtr<FCoDSound> LoadedSound)
		{
			LoadedSound->FullName = LoadedSound->AssetName;
			LoadedSound->AssetType = EWraithAssetType::Sound;
			LoadedSound->AssetName = FPaths::GetBaseFilename(
				LoadedSound->AssetName);
			int32 DotIndex = LoadedSound->AssetName.Find(TEXT("."));
			if (DotIndex != INDEX_NONE)
			{
				LoadedSound->AssetName = LoadedSound->AssetName.Left(
					DotIndex);
			}
			LoadedSound->FullPath = FPaths::GetPath(LoadedSound->AssetName);

			LoadedSound->FrameRate = Sound.FrameRate;
			LoadedSound->FrameCount = Sound.FrameCount;
			LoadedSound->ChannelsCount = Sound.ChannelCount;
			LoadedSound->AssetSize = -1;
			LoadedSound->bIsFileEntry = false;
			LoadedSound->Length = 1000.0f * (LoadedSound->FrameCount /
				static_cast<float>(LoadedSound->FrameRate));
		},
		OnAssetDiscovered
	);
}

FString FModernWarfare6AssetDiscoverer::ProcessAssetName(const FString& Name)
{
	FString CleanedName = Name.Replace(TEXT("::"), TEXT("_")).TrimStartAndEnd();
	int32 Index;
	if (CleanedName.FindLastChar(TEXT('/'), Index))
	{
		CleanedName = CleanedName.RightChop(Index + 1);
	}
	if (CleanedName.FindLastChar(TEXT(':'), Index) && Index > 0 && CleanedName[Index - 1] == ':')
	{
		CleanedName = CleanedName.RightChop(Index + 1);
	}
	return CleanedName;
}
