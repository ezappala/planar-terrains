#pragma once

#include "Commandlets/Commandlet.h"

#include "preprocessor_cmdlet.generated.h"

UCLASS()
class UDLODTERRAIN_API UUDLODPreprocessorCommandlet final : public UCommandlet {
    GENERATED_BODY()

public:
    UUDLODPreprocessorCommandlet();

    virtual int32 Main(const FString& params) override;
};
