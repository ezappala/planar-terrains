#pragma once

#include "CoreMinimal.h"
#include "ShaderParameterMacros.h"

/**
* 2x4 matrix of floating point values.
*/

MS_ALIGN(16) struct FMatrix2x4 {
    float M[2][4];

    void SetMatrix(const FMatrix& Mat) {
        const FMatrix::FReal* RESTRICT Src = &Mat.M[0][0];
        float* RESTRICT Dest = &M[0][0];

        Dest[0] = static_cast<float>(Src[0]); // [0][0]
        Dest[1] = static_cast<float>(Src[1]); // [0][1]
        Dest[2] = static_cast<float>(Src[2]); // [0][2]
        Dest[3] = static_cast<float>(Src[3]); // [0][3]

        Dest[4] = static_cast<float>(Src[4]); // [1][0]
        Dest[5] = static_cast<float>(Src[5]); // [1][1]
        Dest[6] = static_cast<float>(Src[6]); // [1][2]
        Dest[7] = static_cast<float>(Src[7]); // [1][3]
    }

    void SetMatrixTranspose(const FMatrix& Mat) {
        const FMatrix::FReal* RESTRICT Src = &Mat.M[0][0];
        float* RESTRICT Dest = &M[0][0];

        Dest[0] = static_cast<float>(Src[0]); // [0][0]
        Dest[1] = static_cast<float>(Src[4]); // [1][0]
        Dest[2] = static_cast<float>(Src[8]); // [2][0]
        Dest[3] = static_cast<float>(Src[12]); // [3][0]

        Dest[4] = static_cast<float>(Src[1]); // [0][1]
        Dest[5] = static_cast<float>(Src[5]); // [1][1]
        Dest[6] = static_cast<float>(Src[9]); // [2][1]
        Dest[7] = static_cast<float>(Src[13]); // [3][1]
    }

    void SetIdentity() {
        float* RESTRICT Dest = &M[0][0];

        Dest[0] = 1.0f; // [0][0]
        Dest[1] = 0.0f; // [0][1]
        Dest[2] = 0.0f; // [0][2]
        Dest[3] = 0.0f; // [0][3]

        Dest[4] = 0.0f; // [1][0]
        Dest[5] = 1.0f; // [1][1]
        Dest[6] = 0.0f; // [1][2]
        Dest[7] = 0.0f; // [1][3]
    }
} GCC_ALIGN(16);

/** Serializes the Matrix. */
inline FArchive& operator<<(FArchive& Ar, FMatrix2x4& M) {
    Ar << M.M[0][0] << M.M[0][1] << M.M[0][2] << M.M[0][3];
    Ar << M.M[1][0] << M.M[1][1] << M.M[1][2] << M.M[1][3];
    return Ar;
}

template <>
struct TShaderParameterTypeInfo<FMatrix2x4> {
    static constexpr EUniformBufferBaseType BaseType = UBMT_FLOAT32;
    static constexpr int32 NumRows = 2;
    static constexpr int32 NumColumns = 4;
    static constexpr int32 NumElements = 0;
    static constexpr int32 Alignment = 16;
    static constexpr bool bIsStoredInConstantBuffer = true;

    using TAlignedType = TAlignedTypedef<FMatrix2x4, Alignment>::Type;

    static const FShaderParametersMetadata* GetStructMetadata() { return nullptr; }
};

FORCEINLINE void TransposeTransform(FMatrix2x4& DstTransform, const FMatrix44f& SrcTransform) {
#if PLATFORM_ENABLE_VECTORINTRINSICS
    const VectorRegister4Float InRow0 = VectorLoadAligned(&SrcTransform.M[0][0]);
    const VectorRegister4Float InRow1 = VectorLoadAligned(&SrcTransform.M[1][0]);
    const VectorRegister4Float InRow2 = VectorLoadAligned(&SrcTransform.M[2][0]);
    const VectorRegister4Float InRow3 = VectorLoadAligned(&SrcTransform.M[3][0]);

    const VectorRegister4Float Temp0 = VectorShuffle(InRow0, InRow1, 0, 1, 0, 1);
    const VectorRegister4Float Temp1 = VectorShuffle(InRow2, InRow3, 0, 1, 0, 1);

    VectorStoreAligned(VectorShuffle(Temp0, Temp1, 0, 2, 0, 2), &DstTransform.M[0][0]);
    VectorStoreAligned(VectorShuffle(Temp0, Temp1, 1, 3, 1, 3), &DstTransform.M[1][0]);
#else
    const float* RESTRICT Src = &SrcTransform.M[0][0];
    float* RESTRICT Dest = &DstTransform.M[0][0];

    // Row0 = Src column 0
    Dest[0] = Src[0];
    Dest[1] = Src[4];
    Dest[2] = Src[8];
    Dest[3] = Src[12];

    // Row1 = Src column 1
    Dest[4] = Src[1];
    Dest[5] = Src[5];
    Dest[6] = Src[9];
    Dest[7] = Src[13];
#endif
}

void TransposeTransforms(
    FMatrix2x4* DstTransforms,
    const FMatrix44f* SrcTransforms,
    int64 Count
);
