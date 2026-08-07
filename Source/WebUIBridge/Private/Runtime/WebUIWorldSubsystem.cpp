#include "Runtime/WebUIWorldSubsystem.h"

#include "Foundation/WebUIFoundationBridgeActor.h"
#include "Foundation/WebUIFoundationBridgeComponent.h"
#include "Settings/WebUIBridgeSettings.h"

#include "Engine/World.h"
#include "EngineUtils.h"

bool UWebUIWorldSubsystem::ShouldCreateSubsystem(UObject* Outer) const
{
    const UWorld* World = Cast<UWorld>(Outer);
    if (!World)
    {
        return false;
    }

    const bool bSupportedType =
        World->WorldType == EWorldType::Game ||
        World->WorldType == EWorldType::PIE ||
        World->WorldType == EWorldType::GamePreview;

    return bSupportedType && Super::ShouldCreateSubsystem(Outer);
}

void UWebUIWorldSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    // 不在 WorldSubsystem 初始化阶段立即 SpawnActor。
    // Host / Runtime 在 World 已可用时首次请求，避免过早创建导致 BeginPlay 顺序问题。
}

void UWebUIWorldSubsystem::Deinitialize()
{
    DestroyOwnedBridgeActor();
    BridgeActor = nullptr;
    Super::Deinitialize();
}

AWebUIFoundationBridgeActor* UWebUIWorldSubsystem::GetOrCreateBridgeActor()
{
    if (IsValid(BridgeActor))
    {
        return BridgeActor;
    }

    UWorld* World = GetWorld();
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return nullptr;
    }

    for (TActorIterator<AWebUIFoundationBridgeActor> It(World); It; ++It)
    {
        BridgeActor = *It;
        bOwnsBridgeActor = false;
        return BridgeActor;
    }

    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    if (Settings && !Settings->bAutoCreateWorldBridge)
    {
        return nullptr;
    }

    FActorSpawnParameters SpawnParameters;
    SpawnParameters.Name = MakeUniqueObjectName(
        World,
        AWebUIFoundationBridgeActor::StaticClass(),
        TEXT("WebUIFoundationBridge_Auto")
    );
    SpawnParameters.SpawnCollisionHandlingOverride =
        ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
    SpawnParameters.ObjectFlags |= RF_Transient;

    BridgeActor = World->SpawnActor<AWebUIFoundationBridgeActor>(
        AWebUIFoundationBridgeActor::StaticClass(),
        FTransform::Identity,
        SpawnParameters
    );
    bOwnsBridgeActor = IsValid(BridgeActor);

    if (const UWebUIBridgeSettings* RuntimeSettings = GetDefault<UWebUIBridgeSettings>())
    {
        if (RuntimeSettings->bVerboseRuntimeLog)
        {
            UE_LOG(
                LogTemp,
                Log,
                TEXT("[WebUIWorldSubsystem] Bridge Actor=%s World=%s AutoSpawned=%s"),
                *GetNameSafe(BridgeActor),
                *GetNameSafe(World),
                bOwnsBridgeActor ? TEXT("true") : TEXT("false")
            );
        }
    }

    return BridgeActor;
}

AWebUIFoundationBridgeActor* UWebUIWorldSubsystem::GetBridgeActor() const
{
    return IsValid(BridgeActor) ? BridgeActor.Get() : nullptr;
}

UWebUIFoundationBridgeComponent* UWebUIWorldSubsystem::GetBridgeComponent() const
{
    AWebUIFoundationBridgeActor* Actor = GetBridgeActor();
    return Actor ? Actor->BridgeComponent : nullptr;
}

void UWebUIWorldSubsystem::DestroyOwnedBridgeActor()
{
    if (bOwnsBridgeActor && IsValid(BridgeActor))
    {
        BridgeActor->Destroy();
    }

    BridgeActor = nullptr;
    bOwnsBridgeActor = false;
}
