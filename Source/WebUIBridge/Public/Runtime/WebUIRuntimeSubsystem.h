#pragma once

#include "CoreMinimal.h"
#include "Subsystems/GameInstanceSubsystem.h"
#include "Containers/Ticker.h"
#include "Host/WebUIInputModeSubsystem.h"
#include "WebUIRuntimeSubsystem.generated.h"

class UWebUIHostWidget;
class UWorld;
class APlayerController;
class FWebUIRuntimeInputProcessor;

/**
 * 应用级 WebUI 运行时。
 *
 * 生命周期属于 GameInstance：
 * - 自动创建一次全屏 WebBrowser；
 * - 地图切换时复用页面并重新绑定新 World 的能力组件；
 * - 统一管理 UI / Scene 输入模式；
 * - 新建关卡不再手工放置任何 WebUI Actor。
 */
UCLASS()
class WEBUIBRIDGE_API UWebUIRuntimeSubsystem : public UGameInstanceSubsystem
{
    GENERATED_BODY()

public:
    virtual void Initialize(FSubsystemCollectionBase& Collection) override;
    virtual void Deinitialize() override;

    UFUNCTION(BlueprintCallable, Category="Web UI Bridge|Runtime")
    bool EnsureWebUI();

    UFUNCTION(BlueprintCallable, Category="Web UI Bridge|Runtime")
    void DestroyWebUI();

    UFUNCTION(BlueprintPure, Category="Web UI Bridge|Runtime")
    UWebUIHostWidget* GetHostWidget() const;

    UFUNCTION(BlueprintCallable, Category="Web UI Bridge|Input")
    bool SetWebUIInputMode(EWebUIInputMode NewMode, bool bNotifyFrontend = true);

    UFUNCTION(BlueprintCallable, Category="Web UI Bridge|Input")
    bool ToggleWebUIInputMode(bool bNotifyFrontend = true);

    UFUNCTION(BlueprintPure, Category="Web UI Bridge|Input")
    EWebUIInputMode GetWebUIInputMode() const;

    UFUNCTION(BlueprintPure, Category="Web UI Bridge|Input")
    FString GetInputModeToggleKeyName() const;

    /** EngineJS 将 data-engine-ui 的归一化矩形同步到 UE；Scene 模式无需 Browser 命中也能自动识别。 */
    void SetWebUIInputRegions(const TArray<FBox2D>& InNormalizedRegions);
    void ClearWebUIInputRegions();
    int32 GetWebUIInputRegionCount() const { return WebUIInputRegions.Num(); }

    /** ScreenPosition 使用 Slate 绝对坐标。 */
    bool IsScreenPositionInsideWebUIRegion(const FVector2D& ScreenPosition) const;

    /** 由全局 Slate InputProcessor 在鼠标移动/按键/滚轮时驱动自动路由。 */
    void UpdateAutomaticInputRouteFromScreenPosition(
        const FVector2D& ScreenPosition,
        bool bPointerButtonHeld
    );

    void RefreshAutomaticInputRouteFromCursor();

private:
    void HandlePostLoadMap(UWorld* LoadedWorld);
    void HandleWorldCleanup(UWorld* World, bool bSessionEnded, bool bCleanupResources);
    bool TickRuntime(float DeltaTime);
    bool ShouldCreateForWorld(const UWorld* World) const;
    bool EnsureWebUIForWorld(UWorld* World);
    void ApplyDesiredInputMode(bool bNotifyFrontend);
    void EnsureInputPreProcessor();
    void RemoveInputPreProcessor();
    FString ResolveStartupURL() const;

private:
    UPROPERTY(Transient)
    TObjectPtr<UWebUIHostWidget> HostWidget = nullptr;

    TWeakObjectPtr<UWorld> BoundWorld;

    UPROPERTY(Transient)
    EWebUIInputMode DesiredInputMode = EWebUIInputMode::UI;

    TWeakObjectPtr<APlayerController> AppliedPlayerController;
    EWebUIInputMode AppliedInputMode = EWebUIInputMode::UI;
    bool bHasAppliedInputMode = false;

    TArray<FBox2D> WebUIInputRegions;

    TSharedPtr<FWebUIRuntimeInputProcessor> InputPreProcessor;

    FDelegateHandle PostLoadMapHandle;
    FDelegateHandle WorldCleanupHandle;
    FTSTicker::FDelegateHandle TickHandle;
};
