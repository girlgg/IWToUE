#pragma once

template <typename T>
bool IMemoryReader::ReadMemory(uint64 Address, T& OutResult, bool bIsLocal)
{
	if (bIsLocal)
	{
		if (IsBadReadPtr(reinterpret_cast<const void*>(Address), sizeof(T))) return false;
		OutResult = *reinterpret_cast<T*>(Address);
		return true;
	}
	return ReadMemoryImpl(Address, &OutResult, sizeof(T));
}

template <typename T>
bool IMemoryReader::ReadArray(uint64 Address, TArray<T>& OutArray, uint64 Length)
{
	if (Length == 0)
	{
		OutArray.Empty();
		return true;
	}
	OutArray.SetNum(Length);
	return ReadArrayImpl(Address, OutArray.GetData(), Length, sizeof(T));
}
