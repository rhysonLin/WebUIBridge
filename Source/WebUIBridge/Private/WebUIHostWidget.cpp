#include "WebUIHostWidget.h"

#include "Components/Button.h"
#include "Components/MultiLineEditableTextBox.h"
#include "Components/TextBlock.h"
#include "Components/Widget.h"
#include "WebUIBrowserWidget.h"

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

void UWebUIHostWidget::InitializeHostWidget()
{
	if (bInitialized)
	{
		return;
	}

	if (!MainBrowser)
	{
		UE_LOG(LogTemp, Warning, TEXT("[WebUIHostWidget] MainBrowser is null. Please bind a UWebUIBrowserWidget named 'MainBrowser'."));
		AddLog(TEXT("Error"), TEXT("MainBrowser is null."));
		UpdateStatusText();
		return;
	}

	BindBrowserEvents();

	if (ReloadButton)
	{
		ReloadButton->OnClicked.RemoveDynamic(this, &UWebUIHostWidget::HandleReloadClicked);
		ReloadButton->OnClicked.AddDynamic(this, &UWebUIHostWidget::HandleReloadClicked);
	}

	if (ClearLogsButton)
	{
		ClearLogsButton->OnClicked.RemoveDynamic(this, &UWebUIHostWidget::HandleClearLogsClicked);
		ClearLogsButton->OnClicked.AddDynamic(this, &UWebUIHostWidget::HandleClearLogsClicked);
	}

	if (ToggleDebugButton)
	{
		ToggleDebugButton->OnClicked.RemoveDynamic(this, &UWebUIHostWidget::HandleToggleDebugClicked);
		ToggleDebugButton->OnClicked.AddDynamic(this, &UWebUIHostWidget::HandleToggleDebugClicked);
	}

	bDebugPanelVisible = bShowDebugPanelOnStart;
	SetDebugPanelVisible(bDebugPanelVisible);

	AddLog(TEXT("Host"), TEXT("Host widget initialized."));

	ApplyStartupConfig();
	UpdateStatusText();

	bInitialized = true;
}

void UWebUIHostWidget::ReloadBrowser()
{
	if (!MainBrowser)
	{
		AddLog(TEXT("Warning"), TEXT("ReloadBrowser failed: MainBrowser is null."));
		return;
	}

	const FString TargetUrl = !StartupURL.IsEmpty() ? StartupURL : CurrentUrl;
	if (TargetUrl.IsEmpty())
	{
		AddLog(TEXT("Warning"), TEXT("ReloadBrowser failed: no URL available."));
		return;
	}

	AddLog(TEXT("Host"), FString::Printf(TEXT("Reload browser: %s"), *TargetUrl));
	MainBrowser->LoadURLWithBridge(TargetUrl);
}

void UWebUIHostWidget::ClearLogs()
{
	LogEntries.Empty();
	RefreshLogText();
	AddLog(TEXT("Host"), TEXT("Logs cleared."));
}

void UWebUIHostWidget::SetDebugPanelVisible(bool bVisible)
{
	bDebugPanelVisible = bVisible;

	if (DebugPanel)
	{
		DebugPanel->SetVisibility(bVisible ? ESlateVisibility::SelfHitTestInvisible : ESlateVisibility::Collapsed);
	}

	UpdateStatusText();
}

void UWebUIHostWidget::HandleBrowserEvent(const FString& EventName, const FString& PayloadJson)
{
	LastBridgeEvent = EventName;

	AddLog(
		TEXT("Bridge"),
		FString::Printf(TEXT("Event=%s Payload=%s"), *EventName, *PayloadJson)
	);

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

	UpdateStatusText();
}

void UWebUIHostWidget::HandleBrowserUrlChanged(const FString& Url)
{
	CurrentUrl = Url;
	AddLog(TEXT("Browser"), FString::Printf(TEXT("URL changed: %s"), *Url));
	UpdateStatusText();
}

void UWebUIHostWidget::HandleReloadClicked()
{
	ReloadBrowser();
}

void UWebUIHostWidget::HandleClearLogsClicked()
{
	ClearLogs();
}

void UWebUIHostWidget::HandleToggleDebugClicked()
{
	SetDebugPanelVisible(!bDebugPanelVisible);
	AddLog(TEXT("Host"), FString::Printf(TEXT("Debug panel visible: %s"), bDebugPanelVisible ? TEXT("true") : TEXT("false")));
}

void UWebUIHostWidget::ApplyStartupConfig()
{
	if (!MainBrowser)
	{
		return;
	}

	if (!StartupURL.IsEmpty())
	{
		MainBrowser->StartupURL = StartupURL;
	}

	if (bAutoLoadStartupURL && !StartupURL.IsEmpty())
	{
		AddLog(TEXT("Host"), FString::Printf(TEXT("Load startup URL: %s"), *StartupURL));
		MainBrowser->LoadURLWithBridge(StartupURL);
		CurrentUrl = StartupURL;
	}
	else
	{
		CurrentUrl = MainBrowser->StartupURL;
	}
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

	if (ReloadButton)
	{
		ReloadButton->OnClicked.RemoveDynamic(this, &UWebUIHostWidget::HandleReloadClicked);
	}

	if (ClearLogsButton)
	{
		ClearLogsButton->OnClicked.RemoveDynamic(this, &UWebUIHostWidget::HandleClearLogsClicked);
	}

	if (ToggleDebugButton)
	{
		ToggleDebugButton->OnClicked.RemoveDynamic(this, &UWebUIHostWidget::HandleToggleDebugClicked);
	}
}

void UWebUIHostWidget::UpdateStatusText()
{
	if (!StatusText)
	{
		return;
	}

	const FString Status = FString::Printf(
		TEXT("URL: %s\nBridgeName: ueBridge\nInteractionMode: %s\nDebugVisible: %s\nLastEvent: %s"),
		CurrentUrl.IsEmpty() ? TEXT("(empty)") : *CurrentUrl,
		*GetInteractionModeString(),
		bDebugPanelVisible ? TEXT("true") : TEXT("false"),
		*LastBridgeEvent
	);

	StatusText->SetText(FText::FromString(Status));
}

void UWebUIHostWidget::RefreshLogText()
{
	if (!LogTextBox)
	{
		return;
	}

	FString Combined;
	for (const FWebUIHostLogEntry& Entry : LogEntries)
	{
		Combined += FString::Printf(TEXT("[%s] [%s] %s\n"), *Entry.Timestamp, *Entry.Category, *Entry.Message);
	}

	LogTextBox->SetText(FText::FromString(Combined));
}

void UWebUIHostWidget::AddLog(const FString& Category, const FString& Message)
{
	FWebUIHostLogEntry Entry;
	Entry.Timestamp = GetNowTimeString();
	Entry.Category = Category;
	Entry.Message = Message;

	LogEntries.Add(Entry);

	while (LogEntries.Num() > MaxLogEntries)
	{
		LogEntries.RemoveAt(0);
	}

	UE_LOG(LogTemp, Log, TEXT("[WebUIHostWidget][%s] %s"), *Category, *Message);

	RefreshLogText();
}

void UWebUIHostWidget::SendUEReady()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = TEXT(R"({"ok":true,"source":"WebUIHostWidget"})");
	MainBrowser->SendEventToPage(TEXT("UEReady"), Payload);
	AddLog(TEXT("Host"), FString::Printf(TEXT("Send UEReady: %s"), *Payload));
}

void UWebUIHostWidget::SendPong()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = TEXT(R"({"ok":true,"source":"WebUIHostWidget"})");
	MainBrowser->SendEventToPage(TEXT("Pong"), Payload);
	AddLog(TEXT("Host"), FString::Printf(TEXT("Send Pong: %s"), *Payload));
}

void UWebUIHostWidget::SendSceneState()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = BuildSceneStateJson();
	MainBrowser->SendEventToPage(TEXT("SceneState"), Payload);
	AddLog(TEXT("Host"), FString::Printf(TEXT("Send SceneState: %s"), *Payload));
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

FString UWebUIHostWidget::GetNowTimeString() const
{
	return FDateTime::Now().ToString(TEXT("%H:%M:%S"));
}