#pragma once

#include "CoreMinimal.h"

class FIWToUELocalizationManager
{
public:
	static FIWToUELocalizationManager& Get();
	/**
	 * @brief 获取原始的本地化字符串
	 * @param Key 要查找的键
	 * @return 如果找到则返回指向字符串的指针，否则返回 nullptr
	 */
	const FString* GetStringPtr(const FString& Key) const;
	/**
	 * @brief 获取原始的本地化字符串，如果找不到则返回默认文本
	 * @param Key 要查找的键
	 * @param NotFoundText 如果找不到Key时返回的字符串 (默认为空)
	 * @return 找到的字符串或NotFoundText
	 */
	FString GetString(const FString& Key, const FString& NotFoundText = TEXT("LOC_TEXT_NOT_FOUND")) const;
	/**
	 * @brief 获取本地化文本 (FText)，不进行格式化。
	 * @param Key 要查找的键
	 * @param NotFoundText 如果找不到Key时返回的文本 (默认为空)
	 * @return 找到的文本或基于NotFoundText创建的文本 (使用 FText::FromString)
	 */
	FText GetText(const FString& Key, const FString& NotFoundText = TEXT("LOC_TEXT_NOT_FOUND")) const;
	/**
	 * @brief 获取格式化后的本地化文本 (FText)，使用 FText::Format 风格。需要 .ini 文件中的值包含 {0}, {1} 等占位符。
	 * @tparam Args 参数类型包
	 * @param Key 要查找的格式化字符串的键
	 * @param NotFoundText 如果找不到Key时返回的文本 (不会被格式化)
	 * @param InArgs 用于填充占位符的参数 (应为 FFormatArgumentData 或可隐式转换的类型，如 FText, int, float, FString 等)
	 *             建议使用 FText::AsNumber, FText::AsPercent, FText::AsDate 等来包装数字/日期以获得正确的本地化格式。
	 * @return 格式化后的 FText 或基于 NotFoundText 创建的 FText
	 * 
	 * @code
	 * FText Formatted = FIWToUELocalizationManager::Get().GetFormattedText(
	 *     "AssetCountFormat",               // Key in .ini: "Total: {0} | Filtered: {1}"
	 *     "Counts unavailable",             // NotFoundText
	 *     FText::AsNumber(TotalCount),      // Argument for {0}
	 *     FText::AsNumber(FilteredCount)    // Argument for {1}
	 * );
	 * @endcode
	 */
	template <typename... Args>
	FText GetFormattedText(const FString& Key, const FString& NotFoundText = FString(), Args&&... InArgs) const;
	/**
	 * @brief 获取格式化后的字符串 (FString)，使用 FString::Printf 风格。
	 *        需要 .ini 文件中的值包含 %d, %s, %f 等占位符。
	 * @tparam Args 参数类型包
	 * @param Key 要查找的格式化字符串的键
	 * @param NotFoundText 如果找不到Key时返回的字符串 (不会被格式化)
	 * @param InArgs 用于填充占位符的参数 (应匹配 % 占位符的类型)
	 * @return 格式化后的 FString 或 NotFoundText
	 *
	 * @warning FString::Printf 不具备 FText::Format 的本地化感知能力 (如数字格式)。
	 * @warning 如果格式字符串与参数不匹配，可能导致运行时错误或未定义行为。
	 *
	 * @code
	 * FString Formatted = FIWToUELocalizationManager::Get().GetFormattedString_Printf(
	 *     "StatusFormatPrintf",             // Key in .ini: "Status: %s, Value: %d"
	 *     "Status unavailable",             // NotFoundText
	 *     TEXT("Ready"),                    // Argument for %s
	 *     42                                // Argument for %d
	 * );
	 * @endcode
	 */
	template <typename... Args>
	FString GetFormattedString_Printf(const FString& Key, const FString& NotFoundText = FString(),
	                                  Args&&... InArgs) const;
	/**
	 * @brief 获取格式化后的本地化文本 (FText)，内部使用 FString::Printf。
	 * @tparam Args 参数类型包
	 * @param Key 要查找的格式化字符串的键
	 * @param NotFoundText 如果找不到Key时返回的文本 (不会被格式化)
	 * @param InArgs 用于填充占位符的参数 (应匹配 % 占位符的类型)
	 * @return 基于 Printf 格式化结果创建的 FText 或基于 NotFoundText 创建的 FText
	 *
	 * @warning 这个函数返回的 FText 是通过 FText::FromString 创建的，它通常是“文化不变”的，
	 *          并且不适用于需要根据区域设置进行不同显示（如数字格式）的 UI 元素。
	 */
	template <typename... Args>
	FText GetFormattedText_Printf(const FString& Key, const FString& NotFoundText = FString(), Args&&... InArgs) const;

private:
	FIWToUELocalizationManager();

	/*!
	 * 加载本地化数据
	 */
	void LoadLocalizationData();

	/*!
	 * @return 获取当前语言代码 
	 */
	FString GetCurrentLanguageCode() const;

	/*!
	 * @return 本地化文件路径
	 */
	FString GetLocalizationFilePath(const FString& LanguageCode) const;
	void GenerateLanguageFallbackChain(const FString& LanguageCode, TArray<FString>& OutChain) const;

	TMap<FString, FString> LocalizationMap;
};

template <typename... Args>
FText FIWToUELocalizationManager::GetFormattedText(const FString& Key, const FString& NotFoundText,
                                                   Args&&... InArgs) const
{
	const FString* FoundFormatStringPtr = GetStringPtr(Key);
	if (!FoundFormatStringPtr)
	{
		return FText::FromString(NotFoundText);
	}

	FText FormatPattern = FText::FromString(*FoundFormatStringPtr);

	TArray<FFormatArgumentValue> FormatArgs;
	(FormatArgs.Emplace(FFormatArgumentValue(std::forward<Args>(InArgs))), ...);

	return FText::Format(FormatPattern, FFormatOrderedArguments(FormatArgs));
}

template <typename... Args>
FString FIWToUELocalizationManager::GetFormattedString_Printf(const FString& Key, const FString& NotFoundText,
                                                              Args&&... InArgs) const
{
	const FString* FoundFormatStringPtr = GetStringPtr(Key);
	if (!FoundFormatStringPtr)
	{
		return NotFoundText;
	}

	return FString::Printf(**FoundFormatStringPtr, std::forward<Args>(InArgs)...);
}

template <typename... Args>
FText FIWToUELocalizationManager::GetFormattedText_Printf(const FString& Key, const FString& NotFoundText,
                                                          Args&&... InArgs) const
{
	FString FormattedString = GetFormattedString_Printf(Key, NotFoundText, std::forward<Args>(InArgs)...);
	return FText::FromString(FormattedString);
}
