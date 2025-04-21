#include "Interface/ICastAssetImporter.h"

void ICastAssetImporter::ReportProgress(float Percent, const FText& Message)
{
	if (ProgressDelegate.IsBound())
	{
		ProgressDelegate.Execute(Percent, Message);
	}
	UE_LOG(LogCast, Log, TEXT("Progress: %.2f%% - %s"), Percent * 100.0f, *Message.ToString());
}

FString ICastAssetImporter::NoIllegalSigns(const FString& InString)
{
	FString CleanString;
	for (TCHAR Char : InString)
	{
		if (FChar::IsAlnum(Char) || Char == TEXT('_'))
		{
			CleanString.AppendChar(Char);
		}
		else
		{
			CleanString.AppendChar(TEXT('_'));
		}
	}
	return CleanString;
}
