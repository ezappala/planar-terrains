#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_util.h"
#include "Misc/FileHelper.h"
#include "Misc/ScopeExit.h"

using namespace preprocess;
using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessUtilDirectoryHelpersTest,
    "UDLODPreprocessor.Preprocess.Util.DirectoryHelpers",
    TestFlags)

bool FPreprocessUtilDirectoryHelpersTest::RunTest(const FString& Parameters) {
    const FString root = make_unique_test_root(TEXT("UtilDirectories"));
    ON_SCOPE_EXIT { delete_tree(root); };

    IFileManager::Get().MakeDirectory(*root, true);
    const FString file_path = FPaths::Combine(root, TEXT("marker.txt"));
    TestTrue(
        TEXT("Marker file written"),
        FFileHelper::SaveStringToFile(TEXT("marker"), *file_path));

    util::clear_directory(root);
    TestTrue(TEXT("Directory recreated"), IFileManager::Get().DirectoryExists(*root));
    TestFalse(TEXT("Directory contents removed"), FPaths::FileExists(file_path));

    util::delete_directory(root);
    TestFalse(TEXT("Directory deleted"), IFileManager::Get().DirectoryExists(*root));

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessUtilNoDataAndDatatypeHelpersTest,
    "UDLODPreprocessor.Preprocess.Util.NoDataAndDatatypeHelpers",
    TestFlags)

bool FPreprocessUtilNoDataAndDatatypeHelpersTest::RunTest(const FString& Parameters) {
    const GDALDatasetRef with_no_data = create_memory_dataset<float>(4, 4);
    TestEqual(
        TEXT("Setting no-data succeeds"),
        with_no_data->GetRasterBand(1)->SetNoDataValue(-11.0),
        CE_None);

    const TOptional<double> no_data_value = util::get_no_data_value(with_no_data);
    TestTrue(TEXT("No-data value is present"), no_data_value.IsSet());
    if (!no_data_value.IsSet()) { return false; }
    TestEqual(TEXT("No-data value"), no_data_value.GetValue(), -11.0);

    const GDALDatasetRef without_no_data = create_memory_dataset<float>(4, 4);
    TestFalse(
        TEXT("Missing no-data returns NullOpt"),
        util::get_no_data_value(without_no_data).IsSet());

    FPreprocessContext context = make_context(make_unique_test_root(TEXT("UtilDatatype")));
    context.data_type = static_cast<EGDALDataType>(GDT_UInt16);
    TestEqual(
        TEXT("Datatype transpose"),
        util::transpose_gdal_datatype(context),
        ext::GDALDataType::GDT_UInt16);

    return true;
}

#endif
