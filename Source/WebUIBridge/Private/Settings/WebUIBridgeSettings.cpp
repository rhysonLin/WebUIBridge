#include "Settings/WebUIBridgeSettings.h"

#include "Host/WebUIHostWidget.h"
#include "InputCoreTypes.h"

UWebUIBridgeSettings::UWebUIBridgeSettings()
{
    bAutoCreateWebUI = true;
    bCreateInPIE = true;
    bCreateInStandalone = true;
    bAutoCreateWorldBridge = true;
    bKeepBrowserAcrossLevelTravel = true;

    HostWidgetClass = UWebUIHostWidget::StaticClass();
    ViewportZOrder = 10000;

    StartupURL = TEXT("http://127.0.0.1:5173/?runtime=webui");
    bAppendWebUIRuntimeQuery = true;

    InitialInputMode = EWebUIInputMode::UI;
    bEnableAutomaticInputRouting = true;
    bEnableInputModeHotkey = true;
    InputModeToggleKey = EKeys::F1;

    DefaultViewActorTag = TEXT("WebUI.DefaultView");
    bPreferTaggedDefaultView = true;
    bVerboseRuntimeLog = true;
}

FName UWebUIBridgeSettings::GetCategoryName() const
{
    return TEXT("Plugins");
}

FString UWebUIBridgeSettings::GetResolvedStartupURL() const
{
    FString Result = StartupURL.TrimStartAndEnd();
    if (!bAppendWebUIRuntimeQuery || Result.IsEmpty() || Result.Contains(TEXT("runtime=")))
    {
        return Result;
    }

    Result += Result.Contains(TEXT("?")) ? TEXT("&runtime=webui") : TEXT("?runtime=webui");
    return Result;
}
