#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "UDLODExt.h"
#include "Modules/ModuleManager.h"

using namespace udlodext::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FUDLODExtModuleStartupLoadsRuntimeDependenciesTest,
    "UDLODExt.Module.StartupLoadsRuntimeDependencies",
    TestFlags)

bool FUDLODExtModuleStartupLoadsRuntimeDependenciesTest::RunTest(const FString& Parameters) {
    TestTrue(
        TEXT("UDLODExt runtime module is loaded"),
        FModuleManager::Get().IsModuleLoaded(TEXT("UDLODExt")));
    TestTrue(
        TEXT("UDLODExt startup initialized UnrealGDAL"),
        FModuleManager::Get().IsModuleLoaded(TEXT("UnrealGDAL")));

    FUDLODExtModule& module = FModuleManager::LoadModuleChecked<FUDLODExtModule>(TEXT("UDLODExt"));
    (void)module;

    TestTrue(
        TEXT("Runtime module remains loaded after lookup"),
        FModuleManager::Get().IsModuleLoaded(TEXT("UDLODExt")));
    return true;
}

#endif
