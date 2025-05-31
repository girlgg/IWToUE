#pragma once
#include "Database/CoDDatabaseService.h"
#include "Interface/IMemoryReader.h"
#include "WraithX/GameProcess.h"

template <typename TGameAssetStruct, typename TCoDAssetType>
void ProcessGenericAssetNode(IMemoryReader* Reader, const FXAsset64& AssetNode,
                             EWraithAssetType WraithType,
                             const TCHAR* DefaultPrefix,
                             TFunction<void(const TGameAssetStruct& AssetHeader,
                                            TSharedPtr<TCoDAssetType> CoDAsset)> Customizer,
                             FAssetDiscoveredDelegate OnAssetDiscovered)
{
	TGameAssetStruct AssetHeader;
	if (!Reader->ReadMemory<TGameAssetStruct>(AssetNode.Header, AssetHeader))
	{
		UE_LOG(LogTemp, Warning, TEXT("Failed to read asset header struct at 0x%llX"), AssetNode.Header);
		return;
	}

	uint64 AssetHash = AssetHeader.Hash & 0xFFFFFFFFFFFFFFF;

	FCoDDatabaseService::Get().GetAssetNameAsync(
		AssetHash, [AssetHeader, NodeHeader = AssetNode.Header, NodeTemp = AssetNode.Temp, WraithType,
			DefaultPrefix = FString(DefaultPrefix), Customizer = MoveTemp(Customizer),
			OnAssetDiscovered, AssetHash](TOptional<FString> QueryName)
		{
			TSharedPtr<TCoDAssetType> LoadedAsset = MakeShared<TCoDAssetType>();
			LoadedAsset->AssetType = WraithType;
			LoadedAsset->AssetName = QueryName.IsSet()
				                         ? QueryName.GetValue()
				                         : FString::Printf(TEXT("%s_%llx"), *DefaultPrefix, AssetHash);
			LoadedAsset->AssetPointer = NodeHeader;
			LoadedAsset->AssetStatus = EWraithAssetStatus::Loaded;

			if constexpr (std::is_same_v<TCoDAssetType, FCoDAnim>)
			{
				LoadedAsset->AssetStatus = NodeTemp == 1
					                           ? EWraithAssetStatus::Placeholder
					                           : EWraithAssetStatus::Loaded;
			}
			Customizer(AssetHeader, LoadedAsset);

			OnAssetDiscovered.ExecuteIfBound(StaticCastSharedPtr<FCoDAsset>(LoadedAsset));
		});
}

FORCEINLINE FString ProcessAssetName(const FString& Name)
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
