#include "Foundation/WebUIFoundationBridgeActor.h"

#include "Components/SceneComponent.h"
#include "Foundation/WebUIFoundationActorControlComponent.h"
#include "Foundation/WebUIFoundationBridgeComponent.h"
#include "Foundation/WebUIFoundationCameraComponent.h"
#include "Foundation/WebUIFoundationMarkerManagerComponent.h"

AWebUIFoundationBridgeActor::AWebUIFoundationBridgeActor()
{
    PrimaryActorTick.bCanEverTick = false;

    Root = CreateDefaultSubobject<USceneComponent>(TEXT("Root"));
    SetRootComponent(Root);

    BridgeComponent = CreateDefaultSubobject<UWebUIFoundationBridgeComponent>(
        TEXT("WebUIFoundationBridgeComponent")
    );
    CameraComponent = CreateDefaultSubobject<UWebUIFoundationCameraComponent>(
        TEXT("WebUIFoundationCameraComponent")
    );
    MarkerManagerComponent = CreateDefaultSubobject<UWebUIFoundationMarkerManagerComponent>(
        TEXT("WebUIFoundationMarkerManagerComponent")
    );
    ActorControlComponent = CreateDefaultSubobject<UWebUIFoundationActorControlComponent>(
        TEXT("WebUIFoundationActorControlComponent")
    );
}
