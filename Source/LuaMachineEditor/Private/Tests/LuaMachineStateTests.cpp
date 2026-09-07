// Copyright 2025 - Roberto De Ioris

#if WITH_DEV_AUTOMATION_TESTS
#include "Tests/LuaUnitTestState.h"
#include "Misc/AutomationTest.h"
#include "Async/Async.h"
#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTLS.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_Integer, "LuaMachine.UnitTests.State.Integer", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_Integer::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	FLuaValue LuaValue = UnitTestState->RunString("return 1 + 1", "");

	TestTrue(TEXT("LuaValue.Integer == 2"), LuaValue.Integer == 2);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_String, "LuaMachine.UnitTests.State.String", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_String::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	FLuaValue LuaValue = UnitTestState->RunString("return \"lua\"", "");

	TestTrue(TEXT("LuaValue.String == \"lua\""), LuaValue.String == "lua");

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_Call, "LuaMachine.UnitTests.State.Call", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_Call::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	UnitTestState->RunString("testtable = { testfunction = function() return \"lua\" end }", "");

	FLuaValue LuaTestFunction = UnitTestState->GetLuaValueFromGlobalName("testtable.testfunction");

	TestTrue(TEXT("LuaValue.String == \"lua\""), UnitTestState->LuaValueCall(LuaTestFunction, {}).String == "lua");

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_UObject, "LuaMachine.UnitTests.State.UObject", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_UObject::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	FLuaValue ComparisonFunction = UnitTestState->RunString("return function(a, b) return a == b; end", "");

	TestTrue(TEXT("LuaValue.Bool == true"), UnitTestState->LuaValueCall(ComparisonFunction, { FLuaValue(TestWorld), FLuaValue(TestWorld) }).Bool);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_OwnerLifecycle, "LuaMachine.UnitTests.State.OwnerLifecycle", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

// a: Exercise initialized-state adoption, deferred destruction before drain, destruction racing registry closure, and late destruction after closure without entering Lua from GameThread.
bool FLuaMachineStateTest_OwnerLifecycle::RunTest(const FString& Parameters)
{
	TStrongObjectPtr<UWorld> TestWorld(UWorld::CreateWorld(EWorldType::Inactive, false));
	TStrongObjectPtr<ULuaUnitTestState> UnitTestState(NewObject<ULuaUnitTestState>());
	UnitTestState->MaxMemoryUsage = 0; // a: This test isolates reference ownership and must not trigger the separate Luau memory-limit fixture while creating its stress batch.
	UnitTestState->GetLuaState(TestWorld.Get());
	TWeakObjectPtr<ULuaUnitTestState> WeakTestState = UnitTestState.Get();
	// a: The same state first exercises ordinary GameThread reference destruction before the quiescent handoff adopts it.
	FLuaValue* LegacyReference = new FLuaValue(UnitTestState->RunString(TEXT("return {}"), TEXT("OwnerLifecycleLegacyReference")));
	const int32 LegacyRefId = LegacyReference->LuaRef;
	delete LegacyReference;
	UnitTestState->GetRef(LegacyRefId);
	TestFalse(TEXT("Unconfigured GameThread release remains immediate"), lua_istable(UnitTestState->GetInternalLuaState(), -1) != 0);
	UnitTestState->Pop();
	UnitTestState->RunString(TEXT("startupValue = 41"), TEXT("OwnerLifecycleStartup"));
	FEvent* FirstReferenceReady = FPlatformProcess::GetSynchEventFromPool();
	FEvent* FirstReferenceReleased = FPlatformProcess::GetSynchEventFromPool();
	FEvent* RacingReferencesReady = FPlatformProcess::GetSynchEventFromPool();
	FEvent* BeginClosingRace = FPlatformProcess::GetSynchEventFromPool();
	TAtomic<FLuaValue*> FirstReference{nullptr};
	TArray<FLuaValue*> RacingReferences;
	FLuaValue* LateReference = nullptr;
	// a: These booleans return copied test observations; the receiving thread never reads Lua stack or table contents.
	struct FOwnerLifecycleResult
	{
		bool bAdoptedInitializedVm = false;
		bool bReferenceRetainedBeforeDrain = false;
		bool bReferenceReleasedAfterDrain = false;
	};
	ULuaUnitTestState* StatePtr = UnitTestState.Get();
	TFuture<FOwnerLifecycleResult> OwnerResult = Async(EAsyncExecution::Thread, [StatePtr, FirstReferenceReady, FirstReferenceReleased, RacingReferencesReady, BeginClosingRace, &FirstReference, &RacingReferences, &LateReference]()
	{
		FOwnerLifecycleResult Result;
		int32 FirstRefId;
		{
			FGCScopeGuard GcGuard;
			StatePtr->ConfigureLuaOwnerThread(FPlatformTLS::GetCurrentThreadId());
			Result.bAdoptedInitializedVm = StatePtr->RunString(TEXT("return startupValue + 1"), TEXT("OwnerLifecycleAdopted")).ToInteger() == 42;
			FLuaValue* ReferencePtr = new FLuaValue(StatePtr->RunString(TEXT("return {}"), TEXT("OwnerLifecycleDeferredReference")));
			FirstRefId = ReferencePtr->LuaRef;
			FirstReference.Store(ReferencePtr);
		}
		// a: The other thread only destroys these test-owned handles; the server releases its GC scope before every rendezvous.
		FirstReferenceReady->Trigger();
		FirstReferenceReleased->Wait();
		{
			FGCScopeGuard GcGuard;
			StatePtr->GetRef(FirstRefId);
			Result.bReferenceRetainedBeforeDrain = lua_istable(StatePtr->GetInternalLuaState(), -1);
			StatePtr->Pop();
			StatePtr->DrainDeferredLuaReferences();
			StatePtr->GetRef(FirstRefId);
			Result.bReferenceReleasedAfterDrain = !lua_istable(StatePtr->GetInternalLuaState(), -1);
			StatePtr->Pop();
			for (int32 ReferenceIndex = 0; ReferenceIndex < 64; ++ReferenceIndex) // a: A small batch gives deferred destructors opportunities to overlap the close transition; this is test workload, not runtime tuning.
			{
				RacingReferences.Add(new FLuaValue(StatePtr->RunString(TEXT("return {}"), TEXT("OwnerLifecycleRacingReference"))));
			}
			LateReference = new FLuaValue(StatePtr->RunString(TEXT("return {}"), TEXT("OwnerLifecycleLateReference")));
		}
		RacingReferencesReady->Trigger();
		BeginClosingRace->Wait();
		{
			FGCScopeGuard GcGuard;
			StatePtr->CloseOwnedLuaState();
		}
		return Result;
	});
	FirstReferenceReady->Wait();
	delete FirstReference.Exchange(nullptr);
	FirstReferenceReleased->Trigger();
	RacingReferencesReady->Wait();
	BeginClosingRace->Trigger();
	for (FLuaValue* ReferencePtr : RacingReferences)
	{
		delete ReferencePtr;
	}
	const FOwnerLifecycleResult Result = OwnerResult.Get();
	delete LateReference;
	FPlatformProcess::ReturnSynchEventToPool(FirstReferenceReady);
	FPlatformProcess::ReturnSynchEventToPool(FirstReferenceReleased);
	FPlatformProcess::ReturnSynchEventToPool(RacingReferencesReady);
	FPlatformProcess::ReturnSynchEventToPool(BeginClosingRace);
	TestTrue(TEXT("Initialized VM preserves globals after owner adoption"), Result.bAdoptedInitializedVm);
	TestTrue(TEXT("Off-owner destruction does not release the registry before owner drain"), Result.bReferenceRetainedBeforeDrain);
	TestTrue(TEXT("Owner drain releases the queued registry reference"), Result.bReferenceReleasedAfterDrain);
	UnitTestState.Reset();
	CollectGarbage(RF_NoFlags, true);
	TestFalse(TEXT("Explicitly closed VM shell is collectible after racing and late handle destruction"), WeakTestState.IsValid());
	TestWorld->DestroyWorld(false);
	return true;
}

#if LUAMACHINE_LUAU
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_CallTyped, "LuaMachine.UnitTests.State.CallTyped", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_CallTyped::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	UnitTestState->RunString("--!strict\ntesttable = { testfunction = function(a: number, b: number) : number return a + b end }", "");

	FLuaValue LuaTestFunction = UnitTestState->GetLuaValueFromGlobalName("testtable.testfunction");

	TestTrue(TEXT("LuaValue.String == \"lua\""), UnitTestState->LuaValueCall(LuaTestFunction, { 1, 2 }).Integer == 3);

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_MaxMemoryUsage, "LuaMachine.UnitTests.State.MaxMemoryUsage", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_MaxMemoryUsage::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	UnitTestState->bLogError = false;

	UnitTestState->MaxMemoryUsage = 1;

	UnitTestState->RunString("return \"xyz\"", "");

	TestTrue(TEXT("LuaState Error"), UnitTestState->LastError.Contains("MaxMemoryUsage reached"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_Readonly, "LuaMachine.UnitTests.State.Readonly", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_Readonly::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	UnitTestState->bLogError = false;

	UnitTestState->SetLuaValueFromGlobalName("testvalue", UnitTestState->CreateLuaTable());

	UnitTestState->SetLuaTableReadonly(UnitTestState->GetLuaValueFromGlobalName("testvalue"), true);

	UnitTestState->RunString("testvalue.x = 22", "");

	TestTrue(TEXT("LuaState Error"), UnitTestState->LastError.Contains("attempt to modify a readonly table"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_Sandbox, "LuaMachine.UnitTests.State.Sandbox", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_Sandbox::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	UnitTestState->bLogError = false;

	UnitTestState->SetLuaValueFromGlobalName("testvalue", UnitTestState->CreateLuaTable());

	UnitTestState->Sandbox();

	UnitTestState->RunString("testvalue.x = 22", "");

	TestTrue(TEXT("LuaState Error"), UnitTestState->LastError.Contains("attempt to modify a readonly table"));

	return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FLuaMachineStateTest_SingleStep, "LuaMachine.UnitTests.State.SingleStep", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FLuaMachineStateTest_SingleStep::RunTest(const FString& Parameters)
{
	UWorld* TestWorld = UWorld::CreateWorld(EWorldType::Inactive, false);

	ULuaUnitTestState* UnitTestState = ULuaState::CreateDynamicLuaState<ULuaUnitTestState>(TestWorld);

	UnitTestState->SetSingleStep(true);

	UnitTestState->RunString("function test() x = 100; y = 200; z = 300; end; test()", "");

	TestTrue(TEXT("LuaState->StepCount > 0"), UnitTestState->StepCount > 0);

	UnitTestState->StepCount = 0;

	UnitTestState->SetSingleStep(false);

	UnitTestState->RunString("function test2() x = 100; y = 200; z = 300; end; test2()", "");

	TestTrue(TEXT("LuaState->StepCount == 0"), UnitTestState->StepCount == 0);

	return true;
}

#endif

#endif
