#include "GameInfo/ModernWarfare5AssetDiscoverer.h"

#include "SeLogChannels.h"
#include "CDN/CoDCDNDownloaderV2.h"
#include "Database/CoDDatabaseService.h"
#include "Interface/IMemoryReader.h"
#include "MapImporter/XSub.h"
#include "WraithX/CoDAssetType.h"
#include "WraithX/GameProcess.h"
#include "WraithX/LocateGameInfo.h"
#include "WraithX/WraithSettings.h"
#include "WraithX/WraithSettingsManager.h"
#include "Structures/MW5GameStructures.h"
#include "Utils/AssetDiscovererHelper.h"

FModernWarfare5AssetDiscoverer::FModernWarfare5AssetDiscoverer()
{
}

bool FModernWarfare5AssetDiscoverer::Initialize(IMemoryReader* InReader,
                                                const CoDAssets::FCoDGameProcess& InProcessInfo,
                                                TSharedPtr<LocateGameInfo::FParasyteBaseState> InParasyteState)
{
	Reader = InReader;
	ParasyteState = InParasyteState;

	if (!Reader || !Reader->IsValid() || !ParasyteState.IsValid())
	{
		UE_LOG(LogITUAssetImporter, Error, TEXT("MW5 Discoverer Init Failed: Invalid reader or Parasyte state."));
		return false;
	}
	if (ParasyteState->GameID == 0x3232524157444F4D)
	{
		GameType = CoDAssets::ESupportedGames::ModernWarfare5;
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
		UE_LOG(LogITUAssetImporter, Log, TEXT("MW5 Discoverer Initialized for game ID %llX"),
		       ParasyteState->GameID);
		return true;
	}
	UE_LOG(LogITUAssetImporter, Error, TEXT("Discoverer Init Failed: Unexpected game ID %llX"),
	       ParasyteState->GameID);
	GameType = CoDAssets::ESupportedGames::None;
	GameFlag = CoDAssets::ESupportedGameFlags::None;
	return false;
}

TArray<FAssetPoolDefinition> FModernWarfare5AssetDiscoverer::GetAssetPools() const
{
	return {
		{9, EWraithAssetType::Model, TEXT("XModel")},
		{19, EWraithAssetType::Image, TEXT("XImage")},
		{7, EWraithAssetType::Animation, TEXT("XAnim")},
		{11, EWraithAssetType::Material, TEXT("XMaterial")},
		{197, EWraithAssetType::Sound, TEXT("XSound")},
		{50, EWraithAssetType::Map, TEXT("XMap")}
	};
}

int32 FModernWarfare5AssetDiscoverer::DiscoverAssetsInPool(const FAssetPoolDefinition& PoolDefinition,
                                                           FAssetDiscoveredDelegate OnAssetDiscovered)
{
	if (!Reader || !Reader->IsValid() || !ParasyteState.IsValid()) return 0;

	const UWraithSettings* Settings = GEditor->GetEditorSubsystem<UWraithSettingsManager>()->GetSettings();

	if (Settings && !Settings->General.bLoadModels && PoolDefinition.AssetType == EWraithAssetType::Model)
	{
		return 0;
	}
	if (Settings && !Settings->General.bLoadAnimations && PoolDefinition.AssetType == EWraithAssetType::Animation)
	{
		return 0;
	}
	if (Settings && !Settings->General.bLoadAudio && PoolDefinition.AssetType == EWraithAssetType::Sound)
	{
		return 0;
	}
	if (Settings && !Settings->General.bLoadMaterials && PoolDefinition.AssetType == EWraithAssetType::Material)
	{
		return 0;
	}
	if (Settings && !Settings->General.bLoadTextures && PoolDefinition.AssetType == EWraithAssetType::Image)
	{
		return 0;
	}
	if (Settings && !Settings->General.bLoadMaps && PoolDefinition.AssetType == EWraithAssetType::Map)
	{
		return 0;
	}
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
				{
					DiscoverModelAssets(CurrentNode, OnAssetDiscovered);
					DiscoveredCount++;
					break;
				}
			case EWraithAssetType::Image:
				{
					DiscoverImageAssets(CurrentNode, OnAssetDiscovered);
					DiscoveredCount++;
					break;
				}
			case EWraithAssetType::Animation:
				{
					DiscoverAnimAssets(CurrentNode, OnAssetDiscovered);
					DiscoveredCount++;
					break;
				}
			case EWraithAssetType::Material:
				{
					DiscoverMaterialAssets(CurrentNode, OnAssetDiscovered);
					DiscoveredCount++;
					break;
				}
			case EWraithAssetType::Sound:
				{
					DiscoverSoundAssets(CurrentNode, OnAssetDiscovered);
					DiscoveredCount++;
					break;
				}
			case EWraithAssetType::Map:
				{
					DiscoverMapAssets(CurrentNode, OnAssetDiscovered);
					DiscoveredCount++;
					break;
				}
			default:
				UE_LOG(LogTemp, Warning, TEXT("No discovery logic for asset type %d in pool %s"),
				       (int)PoolDefinition.AssetType, *PoolDefinition.PoolName);
				break;
			}
		}
		CurrentNodePtr = CurrentNode.Next;
	}
	UE_LOG(LogTemp, Log, TEXT("Finished discovery for pool %s. Discovered %d assets."), *PoolDefinition.PoolName,
	       DiscoveredCount);
	return DiscoveredCount;
}

bool FModernWarfare5AssetDiscoverer::LoadStringTableEntry(uint64 Index, FString& OutString)
{
	if (Reader && ParasyteState.IsValid() && ParasyteState->StringsAddress != 0)
	{
		return Reader->ReadString(ParasyteState->StringsAddress + Index, OutString);
	}
	OutString.Empty();
	return false;
}

bool FModernWarfare5AssetDiscoverer::ReadAssetPoolHeader(int32 PoolIdentifier, FXAssetPool64& OutPoolHeader)
{
	if (!Reader || !ParasyteState.IsValid() || ParasyteState->PoolsAddress == 0) return false;
	uint64 PoolAddress = ParasyteState->PoolsAddress + sizeof(FXAssetPool64) * PoolIdentifier;
	return Reader->ReadMemory<FXAssetPool64>(PoolAddress, OutPoolHeader);
}

bool FModernWarfare5AssetDiscoverer::ReadAssetNode(uint64 AssetNodePtr, FXAsset64& OutAssetNode)
{
	if (!Reader || AssetNodePtr == 0) return false;
	return Reader->ReadMemory<FXAsset64>(AssetNodePtr, OutAssetNode);
}

void FModernWarfare5AssetDiscoverer::DiscoverModelAssets(FXAsset64 AssetNode,
                                                         FAssetDiscoveredDelegate OnAssetDiscovered)
{
	FMW5XModel ModelHeader;
	if (!Reader->ReadMemory<FMW5XModel>(AssetNode.Header, ModelHeader))
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

		OnAssetDiscovered.ExecuteIfBound(StaticCastSharedPtr<FCoDAsset>(LoadedModel));
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

void FModernWarfare5AssetDiscoverer::DiscoverImageAssets(FXAsset64 AssetNode,
                                                         FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW5GfxImage, FCoDImage>(
	Reader, AssetNode, EWraithAssetType::Image, TEXT("ximage"),
	[](const FMW5GfxImage& ImageHeader, TSharedPtr<FCoDImage> LoadedImage)
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

void FModernWarfare5AssetDiscoverer::DiscoverAnimAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW5XAnim, FCoDAnim>(
	Reader, AssetNode, EWraithAssetType::Animation, TEXT("xanim"),
	[](const FMW5XAnim& Anim, TSharedPtr<FCoDAnim> LoadedAnim)
	{
		LoadedAnim->AssetType = EWraithAssetType::Animation;
		LoadedAnim->Framerate = Anim.Framerate;
		LoadedAnim->FrameCount = Anim.FrameCount;
		LoadedAnim->BoneCount = Anim.TotalBoneCount;
	},
	OnAssetDiscovered
);
}

void FModernWarfare5AssetDiscoverer::DiscoverMaterialAssets(FXAsset64 AssetNode,
                                                            FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW5XMaterial, FCoDMaterial>(
	Reader, AssetNode, EWraithAssetType::Material, TEXT("xmaterial"),
	[](const FMW5XMaterial& Material, TSharedPtr<FCoDMaterial> LoadedMaterial)
	{
		LoadedMaterial->AssetType = EWraithAssetType::Material;
		LoadedMaterial->ImageCount = Material.ImageCount;
	},
	OnAssetDiscovered
);
}

void FModernWarfare5AssetDiscoverer::DiscoverSoundAssets(FXAsset64 AssetNode,
                                                         FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW5SoundAsset, FCoDSound>(
		Reader, AssetNode, EWraithAssetType::Sound, TEXT("xsound"),
		[](const FMW5SoundAsset& Sound, TSharedPtr<FCoDSound> LoadedSound)
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

// 测试 mp_embassy
void FModernWarfare5AssetDiscoverer::DiscoverMapAssets(FXAsset64 AssetNode, FAssetDiscoveredDelegate OnAssetDiscovered)
{
	ProcessGenericAssetNode<FMW5GfxWorld, FCoDMap>(
		Reader, AssetNode, EWraithAssetType::Map, TEXT("xmap"),
		[this](const FMW5GfxWorld& WorldHeader, TSharedPtr<FCoDMap> LoadedMap)
		{
			Reader->ReadString(WorldHeader.BaseName, LoadedMap->AssetName);
		},
		OnAssetDiscovered
	);
}
