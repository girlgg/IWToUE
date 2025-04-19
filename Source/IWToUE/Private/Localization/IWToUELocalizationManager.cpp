#include "Localization/IWToUELocalizationManager.h"

#include "Interfaces/IPluginManager.h"
#include "Internationalization/Culture.h"

#define LOCTEXT_NAMESPACE "IWToUELocalization"

FIWToUELocalizationManager& FIWToUELocalizationManager::Get()
{
	static FIWToUELocalizationManager Instance;
	return Instance;
}

const FString* FIWToUELocalizationManager::GetStringPtr(const FString& Key) const
{
	return LocalizationMap.Find(Key);
}

FString FIWToUELocalizationManager::GetString(const FString& Key, const FString& NotFoundText) const
{
	const FString* FoundText = LocalizationMap.Find(Key);
	return (FoundText != nullptr) ? *FoundText : NotFoundText;
}

FText FIWToUELocalizationManager::GetText(const FString& Key, const FString& NotFoundText) const
{
	FString FoundText = GetString(Key, NotFoundText);
	return FText::FromString(FoundText);
}

FIWToUELocalizationManager::FIWToUELocalizationManager()
{
	LoadLocalizationData();
}

void FIWToUELocalizationManager::LoadLocalizationData()
{
	LocalizationMap.Empty();

	const FString LanguageCode = GetCurrentLanguageCode();
	const FString FilePath = GetLocalizationFilePath(LanguageCode);

	// 检查文件是否存在
	if (!FPaths::FileExists(FilePath))
	{
		UE_LOG(LogTemp, Error, TEXT("Localization file not found: %s"), *FilePath);
		return;
	}

	// 读取INI文件
	FConfigFile ConfigFile;
	ConfigFile.Read(FilePath);

	// 解析Strings节
	const FConfigSection* StringsSection = ConfigFile.FindSection(TEXT("Strings"));
	if (!StringsSection)
	{
		UE_LOG(LogTemp, Error, TEXT("Missing 'Strings' section in localization file: %s"), *FilePath);
		return;
	}

	// 填充键值对
	for (const auto& Entry : *StringsSection)
	{
		const FString Key = Entry.Key.ToString();
		const FString Value = Entry.Value.GetValue();
		LocalizationMap.Add(Key, Value);
	}
}

FString FIWToUELocalizationManager::GetCurrentLanguageCode() const
{
	return FInternationalization::Get().GetCurrentLanguage()->GetName();
}

FString FIWToUELocalizationManager::GetLocalizationFilePath(const FString& LanguageCode) const
{
	TArray<FString> FallbackChain;
	GenerateLanguageFallbackChain(LanguageCode, FallbackChain);

	for (const FString& Variant : FallbackChain)
	{
		const FString FullPath = FPaths::Combine(
			FPaths::ProjectPluginsDir() / TEXT("IWToUE"),
			TEXT("Resources"),
			TEXT("Localization"),
			FString::Printf(TEXT("%s.ini"), *Variant)
		);

		if (FPaths::FileExists(FullPath))
		{
			return FullPath;
		}
	}

	return FPaths::Combine(
		FPaths::ProjectPluginsDir() / TEXT("IWToUE"),
		TEXT("Resources"),
		TEXT("Localization"),
		FString::Printf(TEXT("%s.ini"), *FallbackChain[0])
	);
}

void FIWToUELocalizationManager::GenerateLanguageFallbackChain(const FString& LanguageCode,
                                                               TArray<FString>& OutChain) const
{
	TArray<FString> Parts;
	LanguageCode.ParseIntoArray(Parts, TEXT("-"), true);

	OutChain.Reset();
	OutChain.Add(LanguageCode);

	if (Parts.Num() > 1)
	{
		if (Parts.Num() >= 3)
		{
			TArray<FString> RootAndLastPart;
			RootAndLastPart.Add(Parts[0]);
			RootAndLastPart.Add(Parts.Last());
			OutChain.Add(FString::Join(RootAndLastPart, TEXT("-")));
		}

		for (int32 i = Parts.Num() - 1; i > 0; --i)
		{
			TArray<FString> SubParts;
			for (int32 j = 0; j < i; ++j)
			{
				SubParts.Add(Parts[j]);
			}
			OutChain.Add(FString::Join(SubParts, TEXT("-")));
		}
	}
}

#undef LOCTEXT_NAMESPACE
