#include "Runtime/WebUIRuntimeSubsystem.h"

#include "Host/WebUIHostWidget.h"
#include "Settings/WebUIBridgeSettings.h"

#include "Blueprint/UserWidget.h"
#include "Containers/Ticker.h"
#include "Engine/Engine.h"
#include "Engine/GameInstance.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Kismet/GameplayStatics.h"
#include "UObject/UObjectGlobals.h"
#include "Framework/Application/IInputProcessor.h"
#include "Framework/Application/SlateApplication.h"


class FWebUIRuntimeInputProcessor : public IInputProcessor
{
public:
    explicit FWebUIRuntimeInputProcessor(UWebUIRuntimeSubsystem* InOwner)
        : Owner(InOwner)
    {
    }

    virtual void Tick(
        const float DeltaTime,
        FSlateApplication& SlateApp,
        TSharedRef<ICursor> Cursor
    ) override
    {
    }

    virtual bool HandleMouseMoveEvent(
        FSlateApplication& SlateApp,
        const FPointerEvent& MouseEvent
    ) override
    {
        if (UWebUIRuntimeSubsystem* OwnerPtr = Owner.Get())
        {
            OwnerPtr->UpdateAutomaticInputRouteFromScreenPosition(
                MouseEvent.GetScreenSpacePosition(),
                MouseEvent.GetPressedButtons().Num() > 0
            );
        }
        return false;
    }

    virtual bool HandleMouseButtonDownEvent(
        FSlateApplication& SlateApp,
        const FPointerEvent& MouseEvent
    ) override
    {
        // 按键发生前先根据当前位置完成 UI/Scene 路由。
        if (UWebUIRuntimeSubsystem* OwnerPtr = Owner.Get())
        {
            OwnerPtr->UpdateAutomaticInputRouteFromScreenPosition(
                MouseEvent.GetScreenSpacePosition(),
                false
            );
        }
        return false;
    }

    virtual bool HandleMouseButtonUpEvent(
        FSlateApplication& SlateApp,
        const FPointerEvent& MouseEvent
    ) override
    {
        if (UWebUIRuntimeSubsystem* OwnerPtr = Owner.Get())
        {
            OwnerPtr->UpdateAutomaticInputRouteFromScreenPosition(
                MouseEvent.GetScreenSpacePosition(),
                MouseEvent.GetPressedButtons().Num() > 0
            );
        }
        return false;
    }

    virtual bool HandleMouseWheelOrGestureEvent(
        FSlateApplication& SlateApp,
        const FPointerEvent& InWheelEvent,
        const FPointerEvent* InGestureEvent
    ) override
    {
        if (UWebUIRuntimeSubsystem* OwnerPtr = Owner.Get())
        {
            OwnerPtr->UpdateAutomaticInputRouteFromScreenPosition(
                InWheelEvent.GetScreenSpacePosition(),
                InWheelEvent.GetPressedButtons().Num() > 0
            );
        }
        return false;
    }

    virtual bool HandleKeyDownEvent(
        FSlateApplication& SlateApp,
        const FKeyEvent& InKeyEvent
    ) override
    {
        UWebUIRuntimeSubsystem* OwnerPtr = Owner.Get();
        const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
        if (
            !OwnerPtr ||
            !Settings ||
            !Settings->bEnableInputModeHotkey ||
            !Settings->InputModeToggleKey.IsValid() ||
            InKeyEvent.GetKey() != Settings->InputModeToggleKey
        )
        {
            return false;
        }

        OwnerPtr->ToggleWebUIInputMode(true);
        return true;
    }

private:
    TWeakObjectPtr<UWebUIRuntimeSubsystem> Owner;
};

void UWebUIRuntimeSubsystem::Initialize(FSubsystemCollectionBase& Collection)
{
    Super::Initialize(Collection);

    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    DesiredInputMode = Settings ? Settings->InitialInputMode : EWebUIInputMode::UI;

    PostLoadMapHandle = FCoreUObjectDelegates::PostLoadMapWithWorld.AddUObject(
        this,
        &UWebUIRuntimeSubsystem::HandlePostLoadMap
    );
    WorldCleanupHandle = FWorldDelegates::OnWorldCleanup.AddUObject(
        this,
        &UWebUIRuntimeSubsystem::HandleWorldCleanup
    );
    TickHandle = FTSTicker::GetCoreTicker().AddTicker(
        FTickerDelegate::CreateUObject(this, &UWebUIRuntimeSubsystem::TickRuntime),
        0.0f
    );

    EnsureInputPreProcessor();
    EnsureWebUI();
}

void UWebUIRuntimeSubsystem::Deinitialize()
{
    if (PostLoadMapHandle.IsValid())
    {
        FCoreUObjectDelegates::PostLoadMapWithWorld.Remove(PostLoadMapHandle);
        PostLoadMapHandle.Reset();
    }

    if (WorldCleanupHandle.IsValid())
    {
        FWorldDelegates::OnWorldCleanup.Remove(WorldCleanupHandle);
        WorldCleanupHandle.Reset();
    }

    if (TickHandle.IsValid())
    {
        FTSTicker::GetCoreTicker().RemoveTicker(TickHandle);
        TickHandle.Reset();
    }

    RemoveInputPreProcessor();
    DestroyWebUI();
    BoundWorld.Reset();
    AppliedPlayerController.Reset();
    bHasAppliedInputMode = false;
    WebUIInputRegions.Reset();
    Super::Deinitialize();
}

bool UWebUIRuntimeSubsystem::EnsureWebUI()
{
    UGameInstance* GI = GetGameInstance();
    return GI ? EnsureWebUIForWorld(GI->GetWorld()) : false;
}

bool UWebUIRuntimeSubsystem::ShouldCreateForWorld(const UWorld* World) const
{
    if (!World || World->GetNetMode() == NM_DedicatedServer)
    {
        return false;
    }

    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    if (!Settings || !Settings->bAutoCreateWebUI)
    {
        return false;
    }

    if (World->WorldType == EWorldType::PIE)
    {
        return Settings->bCreateInPIE;
    }

    if (World->WorldType == EWorldType::Game || World->WorldType == EWorldType::GamePreview)
    {
        return Settings->bCreateInStandalone;
    }

    return false;
}

bool UWebUIRuntimeSubsystem::EnsureWebUIForWorld(UWorld* World)
{
    if (!ShouldCreateForWorld(World))
    {
        return false;
    }

    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    if (!Settings)
    {
        return false;
    }

    if (!IsValid(HostWidget))
    {
        TSubclassOf<UWebUIHostWidget> WidgetClass = Settings->HostWidgetClass;
        if (!WidgetClass)
        {
            WidgetClass = UWebUIHostWidget::StaticClass();
        }

        HostWidget = CreateWidget<UWebUIHostWidget>(GetGameInstance(), WidgetClass);
        if (!HostWidget)
        {
            UE_LOG(LogTemp, Error, TEXT("[WebUIRuntimeSubsystem] CreateWidget failed."));
            return false;
        }

        HostWidget->StartupURL = ResolveStartupURL();
        HostWidget->bAutoLoadStartupURL = true;
        HostWidget->bEnableFoundationProtocol = true;
        HostWidget->bAutoSpawnFoundationBridgeActor = false;
        HostWidget->bDestroyAutoSpawnedFoundationBridgeOnDestruct = false;
        HostWidget->AddToViewport(Settings->ViewportZOrder);
    }
    else if (!HostWidget->IsInViewport())
    {
        HostWidget->AddToViewport(Settings->ViewportZOrder);
    }

    if (BoundWorld.Get() != World)
    {
        AppliedPlayerController.Reset();
        bHasAppliedInputMode = false;
    }

    BoundWorld = World;
    HostWidget->HandleWorldChanged(World);
    ApplyDesiredInputMode(false);

    if (Settings->bVerboseRuntimeLog)
    {
        UE_LOG(
            LogTemp,
            Log,
            TEXT("[WebUIRuntimeSubsystem] WebUI ready. Widget=%s World=%s URL=%s"),
            *GetNameSafe(HostWidget),
            *GetNameSafe(World),
            *HostWidget->StartupURL
        );
    }

    return true;
}

void UWebUIRuntimeSubsystem::DestroyWebUI()
{
    if (IsValid(HostWidget))
    {
        HostWidget->RemoveFromParent();
    }

    HostWidget = nullptr;
}

UWebUIHostWidget* UWebUIRuntimeSubsystem::GetHostWidget() const
{
    return IsValid(HostWidget) ? HostWidget.Get() : nullptr;
}

bool UWebUIRuntimeSubsystem::SetWebUIInputMode(EWebUIInputMode NewMode, bool bNotifyFrontend)
{
    DesiredInputMode = NewMode;
    ApplyDesiredInputMode(bNotifyFrontend);

    UGameInstance* GI = GetGameInstance();
    UWebUIInputModeSubsystem* InputSubsystem = GI
        ? GI->GetSubsystem<UWebUIInputModeSubsystem>()
        : nullptr;

    return InputSubsystem && InputSubsystem->GetTargetPlayerController() != nullptr;
}

bool UWebUIRuntimeSubsystem::ToggleWebUIInputMode(bool bNotifyFrontend)
{
    return SetWebUIInputMode(
        DesiredInputMode == EWebUIInputMode::UI
            ? EWebUIInputMode::Game
            : EWebUIInputMode::UI,
        bNotifyFrontend
    );
}

EWebUIInputMode UWebUIRuntimeSubsystem::GetWebUIInputMode() const
{
    return DesiredInputMode;
}

FString UWebUIRuntimeSubsystem::GetInputModeToggleKeyName() const
{
    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    return Settings ? Settings->InputModeToggleKey.GetDisplayName().ToString() : TEXT("F1");
}

void UWebUIRuntimeSubsystem::HandlePostLoadMap(UWorld* LoadedWorld)
{
    if (!LoadedWorld || LoadedWorld->GetGameInstance() != GetGameInstance())
    {
        return;
    }

    EnsureWebUIForWorld(LoadedWorld);
}

void UWebUIRuntimeSubsystem::HandleWorldCleanup(
    UWorld* World,
    bool bSessionEnded,
    bool bCleanupResources
)
{
    if (!World || World != BoundWorld.Get())
    {
        return;
    }

    if (IsValid(HostWidget))
    {
        HostWidget->PrepareForWorldCleanup(World);
    }

    BoundWorld.Reset();

    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    if (Settings && !Settings->bKeepBrowserAcrossLevelTravel)
    {
        DestroyWebUI();
    }
}

bool UWebUIRuntimeSubsystem::TickRuntime(float DeltaTime)
{
    UGameInstance* GI = GetGameInstance();
    UWorld* World = GI ? GI->GetWorld() : nullptr;

    if (ShouldCreateForWorld(World))
    {
        if (!IsValid(HostWidget) || BoundWorld.Get() != World || !HostWidget->IsInViewport())
        {
            EnsureWebUIForWorld(World);
        }

        EnsureInputPreProcessor();

        // PlayerController 可能晚于 Widget 创建。每帧轻量检查，直到输入模式真正应用。
        ApplyDesiredInputMode(false);
    }

    return true;
}

void UWebUIRuntimeSubsystem::ApplyDesiredInputMode(bool bNotifyFrontend)
{
    UGameInstance* GI = GetGameInstance();
    UWorld* World = GI ? GI->GetWorld() : nullptr;
    APlayerController* PC = World ? UGameplayStatics::GetPlayerController(World, 0) : nullptr;

    UWebUIInputModeSubsystem* InputSubsystem = GI
        ? GI->GetSubsystem<UWebUIInputModeSubsystem>()
        : nullptr;

    const bool bNeedsApply =
        PC &&
        InputSubsystem &&
        (!bHasAppliedInputMode || AppliedPlayerController.Get() != PC || AppliedInputMode != DesiredInputMode);

    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    const bool bAutomaticRouting = Settings && Settings->bEnableAutomaticInputRouting;

    if (bNeedsApply)
    {
        InputSubsystem->SetTargetPlayerController(PC);
        if (DesiredInputMode == EWebUIInputMode::UI)
        {
            InputSubsystem->EnterUIMode();
        }
        else if (bAutomaticRouting)
        {
            InputSubsystem->EnterAutomaticSceneMode();
        }
        else
        {
            InputSubsystem->EnterGameMode();
        }

        AppliedPlayerController = PC;
        AppliedInputMode = DesiredInputMode;
        bHasAppliedInputMode = true;
    }

    if (IsValid(HostWidget))
    {
        // Scene 模式必须让全屏 Browser 真正退出 Slate 命中链，
        // 否则右键捕获 / MouseMove 仍可能被 SWebBrowser 抢走。
        // 自动重新进入 UI 不再依赖 Browser mouseover，而由 UE InputProcessor +
        // EngineJS 同步的归一化 UI 矩形完成。
        HostWidget->SetBrowserInteractionEnabled(
            DesiredInputMode == EWebUIInputMode::UI
        );
        if (bNotifyFrontend)
        {
            HostWidget->NotifyWebUIInputModeChanged(
                DesiredInputMode,
                GetInputModeToggleKeyName()
            );
        }
    }
}

void UWebUIRuntimeSubsystem::SetWebUIInputRegions(const TArray<FBox2D>& InNormalizedRegions)
{
    WebUIInputRegions.Reset();
    WebUIInputRegions.Reserve(InNormalizedRegions.Num());

    for (const FBox2D& Region : InNormalizedRegions)
    {
        FVector2D Min(
            FMath::Clamp(Region.Min.X, 0.0, 1.0),
            FMath::Clamp(Region.Min.Y, 0.0, 1.0)
        );
        FVector2D Max(
            FMath::Clamp(Region.Max.X, 0.0, 1.0),
            FMath::Clamp(Region.Max.Y, 0.0, 1.0)
        );

        if (Max.X < Min.X)
        {
            Swap(Max.X, Min.X);
        }
        if (Max.Y < Min.Y)
        {
            Swap(Max.Y, Min.Y);
        }

        if ((Max.X - Min.X) > KINDA_SMALL_NUMBER && (Max.Y - Min.Y) > KINDA_SMALL_NUMBER)
        {
            WebUIInputRegions.Emplace(Min, Max);
        }
    }

    RefreshAutomaticInputRouteFromCursor();
}

void UWebUIRuntimeSubsystem::ClearWebUIInputRegions()
{
    WebUIInputRegions.Reset();
}

bool UWebUIRuntimeSubsystem::IsScreenPositionInsideWebUIRegion(
    const FVector2D& ScreenPosition
) const
{
    if (!IsValid(HostWidget) || WebUIInputRegions.Num() == 0)
    {
        return false;
    }

    const FGeometry Geometry = HostWidget->GetCachedGeometry();
    const FVector2D LocalSize = Geometry.GetLocalSize();
    if (LocalSize.X <= KINDA_SMALL_NUMBER || LocalSize.Y <= KINDA_SMALL_NUMBER)
    {
        return false;
    }

    const FVector2D LocalPosition = Geometry.AbsoluteToLocal(ScreenPosition);
    const FVector2D Normalized(
        LocalPosition.X / LocalSize.X,
        LocalPosition.Y / LocalSize.Y
    );

    if (Normalized.X < 0.0 || Normalized.X > 1.0 || Normalized.Y < 0.0 || Normalized.Y > 1.0)
    {
        return false;
    }

    for (const FBox2D& Region : WebUIInputRegions)
    {
        if (
            Normalized.X >= Region.Min.X &&
            Normalized.X <= Region.Max.X &&
            Normalized.Y >= Region.Min.Y &&
            Normalized.Y <= Region.Max.Y
        )
        {
            return true;
        }
    }

    return false;
}

void UWebUIRuntimeSubsystem::UpdateAutomaticInputRouteFromScreenPosition(
    const FVector2D& ScreenPosition,
    bool bPointerButtonHeld
)
{
    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    if (!Settings || !Settings->bEnableAutomaticInputRouting || WebUIInputRegions.Num() == 0)
    {
        return;
    }

    // 右键旋转、左键拖拽、滑块拖动期间锁定当前路由，避免跨过面板边界时中断捕获。
    if (bPointerButtonHeld)
    {
        return;
    }

    const EWebUIInputMode TargetMode = IsScreenPositionInsideWebUIRegion(ScreenPosition)
        ? EWebUIInputMode::UI
        : EWebUIInputMode::Game;

    if (TargetMode != DesiredInputMode)
    {
        SetWebUIInputMode(TargetMode, true);
    }
}

void UWebUIRuntimeSubsystem::RefreshAutomaticInputRouteFromCursor()
{
    if (!FSlateApplication::IsInitialized())
    {
        return;
    }

    UpdateAutomaticInputRouteFromScreenPosition(
        FSlateApplication::Get().GetCursorPos(),
        false
    );
}

void UWebUIRuntimeSubsystem::EnsureInputPreProcessor()
{
    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    if (
        !Settings ||
        (!Settings->bEnableInputModeHotkey && !Settings->bEnableAutomaticInputRouting)
    )
    {
        RemoveInputPreProcessor();
        return;
    }

    if (InputPreProcessor.IsValid() || !FSlateApplication::IsInitialized())
    {
        return;
    }

    InputPreProcessor = MakeShared<FWebUIRuntimeInputProcessor>(this);
    FSlateApplication::Get().RegisterInputPreProcessor(InputPreProcessor, 0);
}

void UWebUIRuntimeSubsystem::RemoveInputPreProcessor()
{
    if (!InputPreProcessor.IsValid())
    {
        return;
    }

    if (FSlateApplication::IsInitialized())
    {
        FSlateApplication::Get().UnregisterInputPreProcessor(InputPreProcessor);
    }

    InputPreProcessor.Reset();
}

FString UWebUIRuntimeSubsystem::ResolveStartupURL() const
{
    const UWebUIBridgeSettings* Settings = GetDefault<UWebUIBridgeSettings>();
    return Settings ? Settings->GetResolvedStartupURL() : TEXT("http://127.0.0.1:5173/?runtime=webui");
}
