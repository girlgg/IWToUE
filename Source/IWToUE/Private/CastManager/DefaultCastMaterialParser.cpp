#include "CastManager/DefaultCastMaterialParser.h"

#include "SeLogChannels.h"
#include "Widgets/CastImportUI.h"

bool FDefaultCastMaterialParser::ParseMaterialFiles(FCastMaterialInfo& MaterialInfo, const FString& MaterialBasePath,
                                                    const FString& DefaultTexturePath, const FString& TextureFormat,
                                                    ECastTextureImportType TexturePathType,
                                                    const FString& GlobalMaterialPath)
{
	bool bSuccess = true;
	MaterialInfo.Textures.Empty();
	MaterialInfo.Settings.Empty();

	FString TexturesFileName = FPaths::Combine(MaterialBasePath, MaterialInfo.Name + TEXT("_images.txt"));
	TArray<FString> TextureContent;
	if (FFileHelper::LoadFileToStringArray(TextureContent, *TexturesFileName))
	{
		FString ResolvedTextureSourcePath = ResolveTextureBasePath(MaterialInfo.Name, DefaultTexturePath,
		                                                           TexturePathType, GlobalMaterialPath);

		for (int32 LineIndex = 1; LineIndex < TextureContent.Num(); ++LineIndex)
		{
			if (!TextureContent[LineIndex].IsEmpty())
			{
				FCastTextureInfo ParsedTexture;
				if (ParseTextureLine(ParsedTexture, TextureContent[LineIndex], ResolvedTextureSourcePath,
				                     TextureFormat))
				{
					if (ParsedTexture.TextureName[0] == '$') continue;
					MaterialInfo.Textures.Add(ParsedTexture);
				}
				else
				{
					UE_LOG(LogCast, Warning, TEXT("Failed to parse texture line %d in file: %s"), LineIndex + 1,
					       *TexturesFileName);
				}
			}
		}
	}
	else
	{
		UE_LOG(LogCast, Warning, TEXT("Could not load texture file: %s"), *TexturesFileName);
	}

	FString SettingsFileName = FPaths::Combine(MaterialBasePath, MaterialInfo.Name + TEXT("_settings.txt"));
	TArray<FString> SettingsContent;
	if (FFileHelper::LoadFileToStringArray(SettingsContent, *SettingsFileName))
	{
		if (SettingsContent.Num() > 1)
		{
			TArray<FString> TechSetLineParts;
			SettingsContent[1].ParseIntoArray(TechSetLineParts, TEXT(": "), false);
			if (TechSetLineParts.Num() > 1)
			{
				MaterialInfo.TechSet = TechSetLineParts[1].TrimStartAndEnd();
			}
			else
			{
				UE_LOG(LogCast, Warning, TEXT("Could not parse TechSet from line: %s in file: %s"), *SettingsContent[1],
				       *SettingsFileName);
			}

			for (int32 LineIndex = 3; LineIndex < SettingsContent.Num(); ++LineIndex)
			{
				if (!SettingsContent[LineIndex].IsEmpty())
				{
					FCastSettingInfo ParsedSetting;
					if (ParseSettingLine(ParsedSetting, SettingsContent[LineIndex]))
					{
						MaterialInfo.Settings.Add(ParsedSetting);
					}
					else
					{
						UE_LOG(LogCast, Warning, TEXT("Failed to parse setting line %d in file: %s"), LineIndex + 1,
						       *SettingsFileName);
					}
				}
			}
		}
		else
		{
			UE_LOG(LogCast, Warning, TEXT("Settings file has too few lines: %s"), *SettingsFileName);
		}
	}
	else
	{
		UE_LOG(LogCast, Warning, TEXT("Could not load settings file: %s"), *SettingsFileName);
	}

	return bSuccess;
}

bool FDefaultCastMaterialParser::ParseTextureLine(FCastTextureInfo& OutTexture, const FString& Line,
                                                  const FString& BaseTexturePath, const FString& ImageFormat)
{
	TArray<FString> LineParts;
	Line.ParseIntoArray(LineParts, TEXT(","), false); // Don't keep empty parts

	if (LineParts.Num() < 2)
	{
		UE_LOG(LogCast, Warning, TEXT("Invalid texture line format: %s"), *Line);
		return false;
	}

	OutTexture.TextureType = LineParts[0].TrimStartAndEnd();
	FString TextureName = LineParts[1].TrimStartAndEnd();
	OutTexture.TextureName = TextureName;
	OutTexture.TexturePath = FPaths::Combine(BaseTexturePath, TextureName + TEXT(".") + ImageFormat);
	FPaths::NormalizeFilename(OutTexture.TexturePath);

	OutTexture.TextureObject = nullptr;

	return true;
}

bool FDefaultCastMaterialParser::ParseSettingLine(FCastSettingInfo& OutSetting, const FString& Line)
{
	TArray<FString> LineParts;
	Line.ParseIntoArray(LineParts, TEXT(","), false);

	if (LineParts.Num() < 2)
	{
		UE_LOG(LogCast, Warning, TEXT("Invalid setting line format: %s"), *Line);
		return false;
	}

	FString TypeString = LineParts[0].TrimStartAndEnd();
	OutSetting.Name = LineParts[1].TrimStartAndEnd();

	const ESettingType* FoundType = SettingTypeMap.Find(TypeString);
	if (!FoundType)
	{
		UE_LOG(LogCast, Warning, TEXT("Unknown setting type '%s' in line: %s"), *TypeString, *Line);
		return false;
	}
	OutSetting.Type = *FoundType;

	int32 RequiredParts = 0;
	switch (OutSetting.Type)
	{
	case Color:
	case Float4: RequiredParts = 6;
		break;
	case Float3: RequiredParts = 5;
		break;
	case Float2: RequiredParts = 4;
		break;
	case Float1: RequiredParts = 3;
		break;
	default:
		return false;
	}

	if (LineParts.Num() < RequiredParts)
	{
		UE_LOG(LogCast, Warning,
		       TEXT("Insufficient data for setting type '%s' (%d parts required, %d found) in line: %s"), *TypeString,
		       RequiredParts, LineParts.Num(), *Line);
		return false;
	}

	float X = 0.0f, Y = 0.0f, Z = 0.0f, W = 0.0f;
	X = FCString::Atof(*LineParts[2]);
	if (RequiredParts >= 4) Y = FCString::Atof(*LineParts[3]);
	if (RequiredParts >= 5) Z = FCString::Atof(*LineParts[4]);
	if (RequiredParts >= 6) W = FCString::Atof(*LineParts[5]);

	OutSetting.Value = FVector4(X, Y, Z, W);
	return true;
}

FString FDefaultCastMaterialParser::ResolveTextureBasePath(const FString& MaterialName,
                                                           const FString& DefaultTexturePath,
                                                           ECastTextureImportType TexturePathType,
                                                           const FString& GlobalMaterialPath)
{
	switch (TexturePathType)
	{
	case ECastTextureImportType::CastTIT_GlobalMaterials:
		return FPaths::Combine(GlobalMaterialPath, MaterialName);
	case ECastTextureImportType::CastTIT_GlobalImages:
		return GlobalMaterialPath;
	case ECastTextureImportType::CastTIT_Default:
	default:
		return FPaths::Combine(DefaultTexturePath, MaterialName);
	}
}
