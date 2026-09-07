// Copyright 2018-2023 - Roberto De Ioris


#include "LuaCommandExecutor.h"
#include "LuaBlueprintFunctionLibrary.h"

FLuaCommandExecutor::FLuaCommandExecutor()
{
	LuaState = nullptr;
}

FLuaCommandExecutor::~FLuaCommandExecutor()
{
}

FName FLuaCommandExecutor::GetName() const
{
	return FName(*FString::Printf(TEXT("LuaMachine:%s"), *LuaState->GetName()));
}

FText FLuaCommandExecutor::GetDisplayName() const
{
	return FText::FromName(GetName());
}

FText FLuaCommandExecutor::GetDescription() const
{
	return GetDisplayName();
}

FText FLuaCommandExecutor::GetHintText() const
{
	return FText::GetEmpty();
}

#if ENGINE_MAJOR_VERSION < 5 || (ENGINE_MAJOR_VERSION == 5 && ENGINE_MINOR_VERSION < 8)
void FLuaCommandExecutor::GetAutoCompleteSuggestions(const TCHAR* Input, TArray<FString>& Out)
{
}
#endif

void FLuaCommandExecutor::GetExecHistory(TArray<FString>& Out)
{
	IConsoleManager::Get().GetConsoleHistory(*(GetName().ToString()), Out);
}

bool FLuaCommandExecutor::Exec(const TCHAR* Input)
{
	// a: TODO SERVER_THREAD_REVIEW: The stock console calls owned server Lua on GameThread and stays registered until shell destruction; route execution and handle explicit VM closure before claiming support. See BlockGame AUTOMATION_CONTEXT/ServerThreadMigration.md.
	IConsoleManager::Get().AddConsoleHistoryEntry(*(GetName().ToString()), Input);

	LuaState->RunString(Input, "");

	return true;
}

bool FLuaCommandExecutor::AllowHotKeyClose() const
{
	return false;
}

bool FLuaCommandExecutor::AllowMultiLine() const
{
	return true;
}

FInputChord FLuaCommandExecutor::GetHotKey() const
{
	return FInputChord();
}


