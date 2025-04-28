#include "WraithX/WraithSettings.h"

UWraithSettings::FOnSettingsChanged UWraithSettings::OnSettingsChanged;

void UWraithSettings::Save()
{
	SaveConfig();
	OnSettingsChanged.Broadcast();
}

FText UWraithSettings::GetGameTypeDisplayName(EGameType Value)
{
	UEnum* Enum = StaticEnum<EGameType>();
	return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)) : FText::GetEmpty();
}

FText UWraithSettings::GetTextureExportPathModeDisplayName(ETextureExportPathMode Value)
{
	UEnum* Enum = StaticEnum<ETextureExportPathMode>();
	return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)) : FText::GetEmpty();
}

FText UWraithSettings::GetMaterialExportPathModeDisplayName(EMaterialExportPathMode Value)
{
	UEnum* Enum = StaticEnum<EMaterialExportPathMode>();
	return Enum ? Enum->GetDisplayNameTextByValue(static_cast<int64>(Value)) : FText::GetEmpty();
}
