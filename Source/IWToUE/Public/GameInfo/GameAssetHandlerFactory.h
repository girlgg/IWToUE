#pragma once

class IGameAssetHandler;
class FGameProcess;

class FGameAssetHandlerFactory
{
public:
	static TSharedPtr<IGameAssetHandler> CreateHandler(TSharedPtr<FGameProcess> ProcessInstance);
};
