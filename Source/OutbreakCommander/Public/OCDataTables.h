// UE DataTable row structs. These mirror the CSV data tables in Content/Data
// (the sim core parses the same CSVs directly; UE imports them as DataTables).
// Designers edit the CSVs; both the sim and the engine read the same values.
#pragma once

#include "CoreMinimal.h"
#include "Engine/DataTable.h"
#include "OCDataTables.generated.h"

USTRUCT(BlueprintType)
struct FOCResourceDef : public FTableRowBase {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Resource")
    FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Resource")
    FString Category;
};

USTRUCT(BlueprintType)
struct FOCBuildingDef : public FTableRowBase {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 CostMoney = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 CostMaterials = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float PowerGeneration = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float PowerConsumption = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float WaterProduction = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float WaterConsumption = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float FoodProduction = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float FuelConsumption = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    FString Function;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 Housing = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 Jobs = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    float BuildTimeSeconds = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 MaxHp = 0;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 Width = 1;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Building")
    int32 Height = 1;
};

USTRUCT(BlueprintType)
struct FOCZombieTypeDef : public FTableRowBase {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    FString Name;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    float Hp = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    float SpeedTilesPerSec = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    float DamagePerSec = 1.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    float BiteChancePerSec = 0.1f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    float SightRadiusTiles = 6.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    float SmellRadiusTiles = 12.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    int32 BountyMoney = 0;
    // AI behavior: chase | hitandrun | pushahead | snipe
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    FString Behavior = TEXT("chase");
    // Visual model under Content/Assets (e.g. Monsters/monster_demon.gltf)
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Zombie")
    FString Model;
};

USTRUCT(BlueprintType)
struct FOCBalanceRow : public FTableRowBase {
    GENERATED_BODY()

    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Balance")
    float Value = 0.f;
    UPROPERTY(EditAnywhere, BlueprintReadWrite, Category = "Outbreak|Balance")
    FString Description;
};
