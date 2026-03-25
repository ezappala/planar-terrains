#if WITH_DEV_AUTOMATION_TESTS

#include "tests_shared.h"

#include "preprocess_attachment_config.h"

using namespace preprocess::tests;

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessAttachmentConfigFormatMappingsTest,
    "UDLODPreprocessor.Preprocess.AttachmentConfig.FormatMappings",
    TestFlags)

bool FPreprocessAttachmentConfigFormatMappingsTest::RunTest(const FString& Parameters) {
    TestEqual(TEXT("Rgb8U bytes"), bytes_per_pixel(EAttachmentFormat::Rgb8U), 4u);
    TestEqual(TEXT("Rgba8U bytes"), bytes_per_pixel(EAttachmentFormat::Rgba8U), 4u);
    TestEqual(TEXT("R16U bytes"), bytes_per_pixel(EAttachmentFormat::R16U), 2u);
    TestEqual(TEXT("R16I bytes"), bytes_per_pixel(EAttachmentFormat::R16I), 2u);
    TestEqual(TEXT("Rg16U bytes"), bytes_per_pixel(EAttachmentFormat::Rg16U), 4u);
    TestEqual(TEXT("R32F bytes"), bytes_per_pixel(EAttachmentFormat::R32F), 4u);

    TestEqual(
        TEXT("Rgb8U pixel format"),
        to_pixel_format(EAttachmentFormat::Rgb8U),
        PF_R8G8B8A8_UINT);
    TestEqual(
        TEXT("R16U pixel format"),
        to_pixel_format(EAttachmentFormat::R16U),
        PF_R16_UINT);
    TestEqual(
        TEXT("R16I pixel format"),
        to_pixel_format(EAttachmentFormat::R16I),
        PF_R16_SINT);
    TestEqual(
        TEXT("Rg16U pixel format"),
        to_pixel_format(EAttachmentFormat::Rg16U),
        PF_R16G16_UINT);
    TestEqual(
        TEXT("R32F pixel format"),
        to_pixel_format(EAttachmentFormat::R32F),
        PF_R32_FLOAT);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessAttachmentConfigDerivedSizesTest,
    "UDLODPreprocessor.Preprocess.AttachmentConfig.DerivedSizes",
    TestFlags)

bool FPreprocessAttachmentConfigDerivedSizesTest::RunTest(const FString& Parameters) {
    FAttachmentConfig config;
    config.texture_size = 128;
    config.border_size = 6;
    config.mip_level_count = 4;
    config.mask = true;
    config.format = EAttachmentFormat::R16U;

    TestEqual(TEXT("Center size"), config.center_size(), 116u);
    TestEqual(TEXT("Offset size"), config.offset_size(), 122u);

    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(
    FPreprocessAttachmentConfigAttachmentConstructionTest,
    "UDLODPreprocessor.Preprocess.AttachmentConfig.AttachmentConstruction",
    TestFlags)

bool FPreprocessAttachmentConfigAttachmentConstructionTest::RunTest(const FString& Parameters) {
    FAttachmentConfig config;
    config.texture_size = 256;
    config.border_size = 4;
    config.mip_level_count = 5;
    config.mask = true;
    config.format = EAttachmentFormat::Rgba8U;

    const FAttachment attachment{config, TEXT("/Game/Terrain/height")};
    const FAttachment same_attachment{config, TEXT("/Game/Terrain/height")};

    TestEqual(TEXT("Attachment path"), attachment.path, FString(TEXT("/Game/Terrain/height")));
    TestEqual(TEXT("Attachment texture size"), attachment.texture_size, 256u);
    TestEqual(TEXT("Attachment center size"), attachment.center_size, 248u);
    TestEqual(TEXT("Attachment border size"), attachment.border_size, 4u);
    TestEqual(TEXT("Attachment mip count"), attachment.mip_level_count, 5u);
    TestTrue(TEXT("Attachment mask"), attachment.mask);
    TestTrue(TEXT("Attachment equality"), attachment == same_attachment);
    TestEqual(TEXT("Attachment hash"), GetTypeHash(attachment), GetTypeHash(same_attachment));

    return true;
}

#endif
