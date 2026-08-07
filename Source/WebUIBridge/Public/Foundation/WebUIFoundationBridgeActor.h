#pragma once

#include "CoreMinimal.h"
#include "GameFramework/Actor.h"
#include "WebUIFoundationBridgeActor.generated.h"

class USceneComponent;
class UWebUIFoundationBridgeComponent;
class UWebUIFoundationCameraComponent;
class UWebUIFoundationMarkerManagerComponent;
class UWebUIFoundationActorControlComponent;

/**
 * Embedded HTML route business host.
 *
 * This actor deliberately contains no Pixel Streaming dependency. It hosts the
 * WebUI-local copy of the same generic digital-twin feature components used by
 * PixelFoundation: camera/input, markers, actor movement/restore and scene state.
 */
UCLASS(BlueprintType, Blueprintable)
class WEBUIBRIDGE_API AWebUIFoundationBridgeActor : public AActor
{
    GENERATED_BODY()

public:
    AWebUIFoundationBridgeActor();

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Web UI|Foundation")
    TObjectPtr<USceneComponent> Root;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Web UI|Foundation")
    TObjectPtr<UWebUIFoundationBridgeComponent> BridgeComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Web UI|Foundation")
    TObjectPtr<UWebUIFoundationCameraComponent> CameraComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Web UI|Foundation")
    TObjectPtr<UWebUIFoundationMarkerManagerComponent> MarkerManagerComponent;

    UPROPERTY(VisibleAnywhere, BlueprintReadOnly, Category="Web UI|Foundation")
    TObjectPtr<UWebUIFoundationActorControlComponent> ActorControlComponent;
};
