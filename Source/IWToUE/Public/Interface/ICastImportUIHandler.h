#pragma once

class UCastImportUI;
struct FCastImportOptions;

class IWTOUE_API ICastImportUIHandler
{
public:
	virtual ~ICastImportUIHandler() = default;
	virtual bool GetImportOptions(FCastImportOptions& OutOptions, bool bShowDialog, bool bIsAutomated,
	                              const FString& FullPath, bool& bOutImportAll) = 0;
};
