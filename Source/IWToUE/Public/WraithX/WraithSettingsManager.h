#pragma once

#include "CoreMinimal.h"
#include "UObject/Object.h"
#include "WraithSettingsManager.generated.h"

class UWraithSettings;

enum class ESettingsCategory
{
	General,
	Advanced,
	Appearance
};

struct FAppearanceSettings
{
	FColor PrimaryColor = FColor(0.2f, 0.2f, 0.8f);
	float UIScale = 1.0f;
};

struct FGeneralSettings
{
	bool bAutoRefresh = true;
	int32 MaxHistoryItems = 100;
};

struct FAdvancedSettings
{
	bool bUseParallelProcessing = true;
	int32 CacheSizeMB = 512;
};

UCLASS()
class IWTOUE_API UWraithSettingsManager : public UEditorSubsystem
{
	GENERATED_BODY()

public:
	const UWraithSettings* GetSettings() const;
	UWraithSettings* GetSettingsMutable();

	virtual void Initialize(FSubsystemCollectionBase& Collection) override;
	virtual void Deinitialize() override;

private:
	UPROPERTY()
	TObjectPtr<UWraithSettings> SettingsObject;
};
