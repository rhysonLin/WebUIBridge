#include "WebUIBrowserWidget.h"

UWebUIBrowserWidget::UWebUIBrowserWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
}

void UWebUIBrowserWidget::InitializeBrowserWidget()
{
	if (bInitialized)
	{
		return;
	}

	SetupBridge();

	if (bLoadStartupURLOnSynchronize)
	{
		LoadStartupContent();
	}

	if (bSendUEReadyAfterSetup)
	{
		NotifyFrontendReady();
	}

	bInitialized = true;
	OnBrowserWidgetInitialized.Broadcast();
}

void UWebUIBrowserWidget::LoadStartupContent()
{
	if (bPreferStartupHTML && !StartupHTML.IsEmpty())
	{
		LoadHTMLWithBridge(StartupHTML, TEXT("http://localhost/"));
		return;
	}

	if (!StartupURL.IsEmpty())
	{
		LoadURLWithBridge(StartupURL);
	}
}

void UWebUIBrowserWidget::NotifyFrontendReady()
{
	SendEventToPage(
		ReadyEventName.IsEmpty() ? TEXT("UEReady") : ReadyEventName,
		ReadyPayloadJson.IsEmpty() ? TEXT("{\"ok\":true}") : ReadyPayloadJson
	);
}

void UWebUIBrowserWidget::SynchronizeProperties()
{
	Super::SynchronizeProperties();
	InitializeBrowserWidget();
}