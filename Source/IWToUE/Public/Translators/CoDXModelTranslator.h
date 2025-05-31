#pragma once

struct FCastRoot;
struct FCastModelInfo;
struct FWraithXModel;
class IGameAssetHandler;

class FCoDXModelTranslator
{
public:
	static bool TranslateModel(IGameAssetHandler* GameHandler,
		FWraithXModel& InModel,
		int32 LodIdx,
		FCastModelInfo& OutModelInfo,
		const FCastRoot& InSceneRoot);
};
