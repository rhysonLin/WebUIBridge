#pragma once

#include "CoreMinimal.h"
#include "Engine/DeveloperSettings.h"
#include "InputCoreTypes.h"
#include "Host/WebUIInputModeSubsystem.h"
#include "WebUIBridgeSettings.generated.h"

class UWebUIHostWidget;

/**
 * WebUIBridge 项目级配置。
 *
 * 这些设置只需要在 Project Settings -> Plugins -> Web UI Bridge 配置一次，
 * 新建关卡不再手工放置 WebUI Host 或 Foundation Bridge Actor。
 */
UCLASS(Config=Game, DefaultConfig, meta=(DisplayName="Web UI Bridge"))
class WEBUIBRIDGE_API UWebUIBridgeSettings : public UDeveloperSettings
{
    GENERATED_BODY()

public:
    UWebUIBridgeSettings();

    virtual FName GetCategoryName() const override;

    /** 自动在 GameInstance 生命周期中创建全屏内嵌 WebUI。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bAutoCreateWebUI;

    /** PIE 中自动创建。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bCreateInPIE;

    /** Standalone / 打包程序中自动创建。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bCreateInStandalone;

    /** 每个 World 自动创建场景能力桥接 Actor。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bAutoCreateWorldBridge;

    /** 地图切换时尽量保留同一个 Browser Widget 与 React 状态。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    bool bKeepBrowserAcrossLevelTravel;

    /** 自动创建的宿主 Widget 类。为空时使用原生 UWebUIHostWidget。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime")
    TSubclassOf<UWebUIHostWidget> HostWidgetClass;

    /** 全屏 Widget 的 Viewport ZOrder。 */
    UPROPERTY(Config, EditAnywhere, Category="Runtime", meta=(ClampMin="0"))
    int32 ViewportZOrder;

    /** React / HTML 页面地址。 */
    UPROPERTY(Config, EditAnywhere, Category="Page")
    FString StartupURL;

    /** URL 未包含 runtime 参数时自动追加 runtime=webui。 */
    UPROPERTY(Config, EditAnywhere, Category="Page")
    bool bAppendWebUIRuntimeQuery;

    /** 初始输入状态。UI 模式操作网页；Game 模式操作三维场景。 */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    EWebUIInputMode InitialInputMode;

    /**
     * 自动输入路由。
     * EngineJS 把 DOM 上 data-engine-ui 的归一化矩形同步到 UE。
     * UE 全局 InputProcessor 直接根据鼠标位置切换：
     * - UI 区域：UIOnly + Browser 可命中；
     * - 场景区域：GameOnly + Browser HitTestInvisible。
     */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    bool bEnableAutomaticInputRouting;

    /** 允许通过快捷键在 UI / 三维场景输入间切换，作为自动识别失效时的备用手段。 */
    UPROPERTY(Config, EditAnywhere, Category="Input")
    bool bEnableInputModeHotkey;

    /** 默认 F1。进入场景操作后，用该键重新获得网页 UI 输入。 */
    UPROPERTY(Config, EditAnywhere, Category="Input", meta=(EditCondition="bEnableInputModeHotkey"))
    FKey InputModeToggleKey;

    /** resetView 优先使用带此 Tag 的 Actor / CameraActor。 */
    UPROPERTY(Config, EditAnywhere, Category="Camera")
    FName DefaultViewActorTag;

    /** 是否优先查找 DefaultViewActorTag。 */
    UPROPERTY(Config, EditAnywhere, Category="Camera")
    bool bPreferTaggedDefaultView;

    /** 输出自动运行时日志。 */
    UPROPERTY(Config, EditAnywhere, Category="Debug")
    bool bVerboseRuntimeLog;

    UFUNCTION(BlueprintPure, Category="Web UI Bridge")
    FString GetResolvedStartupURL() const;
};
