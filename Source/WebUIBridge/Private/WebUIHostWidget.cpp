#include "WebUIHostWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "WebUIBrowserWidget.h"

UWebUIHostWidget::UWebUIHostWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

TSharedRef<SWidget> UWebUIHostWidget::RebuildWidget()
{
	BuildUI();
	return Super::RebuildWidget();
}

void UWebUIHostWidget::NativeConstruct()
{
	Super::NativeConstruct();
	InitializeHostWidget();
}

void UWebUIHostWidget::NativeDestruct()
{
	UnbindBrowserEvents();
	Super::NativeDestruct();
}

void UWebUIHostWidget::BuildUI()
{
	if (!WidgetTree)
	{
		return;
	}

	if (RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(UCanvasPanel::StaticClass(), TEXT("RootCanvas"));
	WidgetTree->RootWidget = RootCanvas;

	MainBrowser = WidgetTree->ConstructWidget<UWebUIBrowserWidget>(UWebUIBrowserWidget::StaticClass(), TEXT("MainBrowser"));
	MainBrowser->BridgeObjectName = BrowserBridgeObjectName;
	MainBrowser->bAutoSetupBridge = true;
	MainBrowser->bLoadStartupURLOnSynchronize = false;
	MainBrowser->bPreferStartupHTML = false;
	MainBrowser->bSendUEReadyAfterSetup = bBrowserSendUEReadyAfterSetup;
	MainBrowser->StartupURL = StartupURL;
	MainBrowser->SetBrowserSupportsTransparency(true);
	UCanvasPanelSlot* BrowserCanvasSlot = RootCanvas->AddChildToCanvas(MainBrowser);
	if (BrowserCanvasSlot)
	{
		BrowserCanvasSlot->SetAnchors(FAnchors(0.f, 0.f, 1.f, 1.f));
		BrowserCanvasSlot->SetOffsets(FMargin(0.f, 0.f, 0.f, 0.f));
		BrowserCanvasSlot->SetZOrder(0);
	}
}

void UWebUIHostWidget::InitializeHostWidget()
{
	if (bInitialized)
	{
		return;
	}

	if (!MainBrowser)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIHostWidget] MainBrowser is null."));
		return;
	}

	BindBrowserEvents();
	ApplyStartupConfig();

	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Host widget initialized."));

	bInitialized = true;
}

void UWebUIHostWidget::BindBrowserEvents()
{
	if (!MainBrowser)
	{
		return;
	}

	MainBrowser->OnBrowserEvent.RemoveDynamic(this, &UWebUIHostWidget::HandleBrowserEvent);
	MainBrowser->OnBrowserEvent.AddDynamic(this, &UWebUIHostWidget::HandleBrowserEvent);

	MainBrowser->OnPageUrlChanged.RemoveDynamic(this, &UWebUIHostWidget::HandleBrowserUrlChanged);
	MainBrowser->OnPageUrlChanged.AddDynamic(this, &UWebUIHostWidget::HandleBrowserUrlChanged);
}

void UWebUIHostWidget::UnbindBrowserEvents()
{
	if (!MainBrowser)
	{
		return;
	}

	MainBrowser->OnBrowserEvent.RemoveDynamic(this, &UWebUIHostWidget::HandleBrowserEvent);
	MainBrowser->OnPageUrlChanged.RemoveDynamic(this, &UWebUIHostWidget::HandleBrowserUrlChanged);
}

void UWebUIHostWidget::ApplyStartupConfig()
{
	if (!MainBrowser)
	{
		return;
	}

	MainBrowser->BridgeObjectName = BrowserBridgeObjectName;
	MainBrowser->bSendUEReadyAfterSetup = bBrowserSendUEReadyAfterSetup;
	MainBrowser->StartupURL = StartupURL;

	if (bAutoLoadStartupURL && !StartupURL.IsEmpty())
	{
		UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Load startup URL: %s"), *StartupURL);
		MainBrowser->LoadURLWithBridge(StartupURL);
		CurrentUrl = StartupURL;
	}
	else
	{
		CurrentUrl = MainBrowser->StartupURL;
	}
}

void UWebUIHostWidget::ReloadBrowser()
{
	if (!MainBrowser)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIHostWidget] ReloadBrowser failed: MainBrowser is null."));
		return;
	}

	const FString TargetUrl = !StartupURL.IsEmpty() ? StartupURL : CurrentUrl;
	if (TargetUrl.IsEmpty())
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIHostWidget] ReloadBrowser failed: no URL available."));
		return;
	}

	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Reload browser: %s"), *TargetUrl);
	MainBrowser->LoadURLWithBridge(TargetUrl);
}

void UWebUIHostWidget::HandleBrowserEvent(const FString& EventName, const FString& PayloadJson)
{
	LastBridgeEvent = EventName;

	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Bridge Event=%s Payload=%s"), *EventName, *PayloadJson);

	if (bAutoHandleBaseEvents && MainBrowser)
	{
		if (EventName == TEXT("BridgeReady"))
		{
			SendUEReady();
			SendSceneState();
		}
		else if (EventName == TEXT("Ping"))
		{
			SendPong();
		}
		else if (EventName == TEXT("RequestSceneState"))
		{
			SendSceneState();
		}
	}
}

void UWebUIHostWidget::HandleBrowserUrlChanged(const FString& Url)
{
	CurrentUrl = Url;
	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] URL changed: %s"), *Url);
}

void UWebUIHostWidget::SendUEReady()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = TEXT(R"({"ok":true,"source":"WebUIHostWidget"})");
	MainBrowser->SendEventToPage(TEXT("UEReady"), Payload);
	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Send UEReady: %s"), *Payload);
}

void UWebUIHostWidget::SendPong()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = TEXT(R"({"ok":true,"source":"WebUIHostWidget"})");
	MainBrowser->SendEventToPage(TEXT("Pong"), Payload);
	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Send Pong: %s"), *Payload);
}

void UWebUIHostWidget::SendSceneState()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = BuildSceneStateJson();
	MainBrowser->SendEventToPage(TEXT("SceneState"), Payload);
	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget] Send SceneState: %s"), *Payload);
}

FString UWebUIHostWidget::BuildSceneStateJson() const
{
	return FString::Printf(
		TEXT(R"({"host":"WebUIHostWidget","status":"running","url":"%s","interactionMode":"%s","lastEvent":"%s"})"),
		CurrentUrl.IsEmpty() ? TEXT("") : *CurrentUrl,
		*GetInteractionModeString(),
		*LastBridgeEvent
	);
}

FString UWebUIHostWidget::GetInteractionModeString() const
{
	switch (InteractionMode)
	{
	case EWebUIHostInteractionMode::FullIntercept:
		return TEXT("FullIntercept");
	case EWebUIHostInteractionMode::PassiveOverlay:
		return TEXT("PassiveOverlay");
	case EWebUIHostInteractionMode::Reserved:
		return TEXT("Reserved");
	default:
		return TEXT("Unknown");
	}
}