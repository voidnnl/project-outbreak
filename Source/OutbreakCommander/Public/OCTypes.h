// Blueprint-facing snapshots of simulation state. The sim core is pure C++;
// these structs are how the UI/HUD reads it each frame without coupling to it.
#pragma once

#include "CoreMinimal.h"
#include "OCTypes.generated.h"

UENUM(BlueprintType)
enum class EOCGameResult : uint8 { Ongoing, Won, Lost };

USTRUCT(BlueprintType)
struct FOCStatsSnapshot {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 CitizensAlive = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 CitizensHealthy = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 CitizensInfected = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 CitizensPanicked = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 CitizensDead = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 CitizensTurned = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 ZombiesAlive = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 ZombiesKilled = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 InitialPopulation = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") float InfectedFraction = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") float Morale = 70.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") float Day = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 HomelessCitizens = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") bool bAnyPowerOutage = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 ActiveSquads = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 Vehicles = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 TechsResearched = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") int32 Evacuated = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") bool bVaccineDeployed = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") bool bNuked = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") FString Weather = TEXT("clear");
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Stats") bool bNight = false;
};

USTRUCT(BlueprintType)
struct FOCResourcesSnapshot {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Money = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Food = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Water = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Fuel = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Materials = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Medicine = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Ammo = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double Research = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double PowerGeneration = 0.0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Resources") double PowerConsumption = 0.0;
};

// Squad orders and emergency policies (mirror the sim enums for Blueprints).
UENUM(BlueprintType)
enum class EOCSquadOrder : uint8 { Guard, Patrol, Attack, Retreat };

UENUM(BlueprintType)
enum class EOCPolicy : uint8 { Curfew, Quarantine, MartialLaw };

USTRUCT(BlueprintType)
struct FOCSquadInfo {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") int32 Id = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") FString UnitTypeId;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") FString Name;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") int32 Size = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") float X = 0.f, Y = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") EOCSquadOrder Order = EOCSquadOrder::Guard;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") bool bActive = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") float Hp = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") int32 Kills = 0;
};

USTRUCT(BlueprintType)
struct FOCVehicleInfo {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") int32 Id = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") FString TypeId;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") float X = 0.f, Y = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") float Hp = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") bool bDestroyed = false;
};

USTRUCT(BlueprintType)
struct FOCEnvironmentInfo {
    GENERATED_BODY()

    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") FString Weather;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") bool bNight = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") float Day = 0.f;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") bool bVaccineDeployed = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") bool bNuked = false;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") int32 Evacuated = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") int32 TechsResearched = 0;
    UPROPERTY(BlueprintReadOnly, Category = "Outbreak|Sim") FString CityName;
};
