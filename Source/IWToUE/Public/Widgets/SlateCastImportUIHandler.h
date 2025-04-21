#pragma once
#include "Interface/ICastImportUIHandler.h"

class UCastImportUI;

class FSlateCastImportUIHandler : public ICastImportUIHandler
{
public:
	virtual bool GetImportOptions(FCastImportOptions& OutOptions, bool bShowDialog, bool bIsAutomated,
	                              const FString& FullPath, bool& bOutImportAll) override;
};
