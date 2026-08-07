#pragma once

#include "CoreMinimal.h"
#include "Subsystems/WorldSubsystem.h"
#include "WebUIWorldSubsystem.generated.h"

class AWebUIFoundationBridgeActor;
class UWebUIFoundationBridgeComponent;

/**
 * 当前 World 的数字孪生能力容器。
 *
 * 每个关卡自动拥有一套 Camera / ActorControl / Marker / Scene 能力，
 * 但不要求设计人员在 World Outliner 中手工放置 Actor。
 */
UCLASS()
class WEBUIBRIDGE_API UWebUIWorldSubsystem : public UWorldSubsystem
{
    GENERATED_BODY()

public:
    virtual bool ShouldCreateSubsystem(UObject* Outer) const override;
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Web UI Bridge|World")
    AWebUIFoundationBridgeActor* GetOrCreateBridgeActor();

    UFUNCTION(BlueprintPure, Category="Web UI Bridge|World")
    AWebUIFoundationBridgeActor* GetBridgeActor() const;

    UFUNCTION(BlueprintPure, Category="Web UI Bridge|World")
    UWebUIFoundationBridgeComponent* GetBridgeComponent() const;

    UFUNCTION(BlueprintCallable, Category="Web UI Bridge|World")
    void DestroyOwnedBridgeActor();

private:
    UPROPERTY(Transient)
    TObjectPtr<AWebUIFoundationBridgeActor> BridgeActor = nullptr;

    UPROPERTY(Transient)
    bool bOwnsBridgeActor = false;
};
