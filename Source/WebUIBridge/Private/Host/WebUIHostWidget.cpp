#include "Host/WebUIHostWidget.h"

#include "Blueprint/WidgetTree.h"
#include "Browser/WebUIBrowserWidget.h"
#include "Camera/WebUICameraSubsystem.h"
#include "Components/CanvasPanel.h"
#include "Components/CanvasPanelSlot.h"
#include "Dom/JsonObject.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "GameFramework/PlayerController.h"
#include "Foundation/WebUIFoundationBridgeActor.h"
#include "Kismet/GameplayStatics.h"
#include "Foundation/WebUIFoundationBridgeComponent.h"
#include "Runtime/WebUIWorldSubsystem.h"
#include "Runtime/WebUIRuntimeSubsystem.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"

namespace
{
	bool TryParseJsonObject(const FString& PayloadJson, TSharedPtr<FJsonObject>& OutObject)
	{
		if (PayloadJson.TrimStartAndEnd().IsEmpty())
		{
			OutObject = MakeShared<FJsonObject>();
			return true;
		}

		const TSharedRef<TJsonReader<>> Reader = TJsonReaderFactory<>::Create(PayloadJson);
		return FJsonSerializer::Deserialize(Reader, OutObject) && OutObject.IsValid();
	}

	FString SerializeJsonObject(const TSharedPtr<FJsonObject>& Object)
	{
		if (!Object.IsValid())
		{
			return TEXT("{}");
		}

		FString Output;
		const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Output);
		FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
		return Output;
	}

	double GetJsonNumberOrDefault(
		const TSharedPtr<FJsonObject>& Object,
		const TCHAR* FieldName,
		double DefaultValue = 0.0
	)
	{
		if (!Object.IsValid())
		{
			return DefaultValue;
		}

		double Value = DefaultValue;
		Object->TryGetNumberField(FieldName, Value);
		return Value;
	}

	FString EscapeWebUIJsonString(const FString& Value)
	{
		return Value
			.Replace(TEXT("\\"), TEXT("\\\\"))
			.Replace(TEXT("\""), TEXT("\\\""))
			.Replace(TEXT("\n"), TEXT("\\n"))
			.Replace(TEXT("\r"), TEXT("\\r"));
	}

	FString MakeWebUIRequestId()
	{
		return FString::Printf(
			TEXT("webui_%lld_%llu"),
			FDateTime::UtcNow().ToUnixTimestamp(),
			static_cast<unsigned long long>(FPlatformTime::Cycles64())
		);
	}
}

UWebUIHostWidget::UWebUIHostWidget(const FObjectInitializer& ObjectInitializer)
	: Super(ObjectInitializer)
{
	FoundationBridgeActorClass = AWebUIFoundationBridgeActor::StaticClass();
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
	ShutdownFoundationBridge();
	bInitialized = false;
	Super::NativeDestruct();
}

FReply UWebUIHostWidget::NativeOnMouseWheel(
	const FGeometry& InGeometry,
	const FPointerEvent& InMouseEvent
)
{
	// SWebBrowser 如果页面当前区域不可滚动，Wheel 可能以 Unhandled 继续冒泡，
	// 进而触发项目 PlayerController 的三维滚轮移动。UI 模式下在宿主层兜底消费。
	if (UGameInstance* GI = GetGameInstance())
	{
		if (UWebUIRuntimeSubsystem* Runtime =
			GI->GetSubsystem<UWebUIRuntimeSubsystem>())
		{
			if (Runtime->GetWebUIInputMode() == EWebUIInputMode::UI)
			{
				return FReply::Handled();
			}
		}
	}

	return Super::NativeOnMouseWheel(InGeometry, InMouseEvent);
}

void UWebUIHostWidget::BuildUI()
{
	if (!WidgetTree || RootCanvas)
	{
		return;
	}

	RootCanvas = WidgetTree->ConstructWidget<UCanvasPanel>(
		UCanvasPanel::StaticClass(),
		TEXT("RootCanvas")
	);
	WidgetTree->RootWidget = RootCanvas;

	MainBrowser = WidgetTree->ConstructWidget<UWebUIBrowserWidget>(
		UWebUIBrowserWidget::StaticClass(),
		TEXT("MainBrowser")
	);
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
		BrowserCanvasSlot->SetOffsets(FMargin(0.f));
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

	if (bEnableFoundationProtocol && !InitializeFoundationBridge())
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[WebUIHostWidget] Foundation protocol enabled, but bridge initialization failed.")
		);
	}

	ApplyStartupConfig();

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[WebUIHostWidget] Host initialized. FoundationReady=%s CommandEvent=%s ResponseEvent=%s"),
		IsFoundationBridgeReady() ? TEXT("true") : TEXT("false"),
		*FoundationCommandEventName,
		*FoundationResponseEventName
	);

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

bool UWebUIHostWidget::InitializeFoundationBridge()
{
	if (IsFoundationBridgeReady())
	{
		return true;
	}

	UWorld* World = BoundFoundationWorld.IsValid()
		? BoundFoundationWorld.Get()
		: GetWorld();
	if (!World)
	{
		UE_LOG(LogTemp, Error, TEXT("[WebUIHostWidget] Cannot initialize Foundation bridge: World is null."));
		return false;
	}

	FoundationBridgeActor = nullptr;
	FoundationBridgeComponent = nullptr;
	bOwnsFoundationBridgeActor = false;
	BoundFoundationWorld = World;

	// 正式架构：由当前 WorldSubsystem 自动持有能力 Actor。
	if (UWebUIWorldSubsystem* WorldSubsystem = World->GetSubsystem<UWebUIWorldSubsystem>())
	{
		FoundationBridgeActor = WorldSubsystem->GetOrCreateBridgeActor();
		FoundationBridgeComponent = WorldSubsystem->GetBridgeComponent();
	}

	// 兼容旧工程：没有 WorldSubsystem 时仍允许查找或自动创建。
	if (!FoundationBridgeActor)
	{
		for (TActorIterator<AWebUIFoundationBridgeActor> It(World); It; ++It)
		{
			FoundationBridgeActor = *It;
			break;
		}
	}

	if (!FoundationBridgeActor && bAutoSpawnFoundationBridgeActor)
	{
		UClass* SpawnClass = FoundationBridgeActorClass
			? FoundationBridgeActorClass.Get()
			: AWebUIFoundationBridgeActor::StaticClass();

		FActorSpawnParameters SpawnParameters;
		SpawnParameters.Name = MakeUniqueObjectName(
			World,
			SpawnClass,
			TEXT("WebUIFoundationBridge_Legacy")
		);
		SpawnParameters.SpawnCollisionHandlingOverride =
			ESpawnActorCollisionHandlingMethod::AlwaysSpawn;
		SpawnParameters.ObjectFlags |= RF_Transient;

		FoundationBridgeActor = World->SpawnActor<AWebUIFoundationBridgeActor>(
			SpawnClass,
			FTransform::Identity,
			SpawnParameters
		);
		bOwnsFoundationBridgeActor = FoundationBridgeActor != nullptr;
	}

	if (!FoundationBridgeActor)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[WebUIHostWidget] Foundation Bridge Actor is unavailable in World=%s."),
			*GetNameSafe(World)
		);
		return false;
	}

	if (!FoundationBridgeComponent)
	{
		FoundationBridgeComponent = FoundationBridgeActor->BridgeComponent;
	}

	if (!FoundationBridgeComponent)
	{
		UE_LOG(LogTemp, Error, TEXT("[WebUIHostWidget] Foundation BridgeComponent is null."));
		return false;
	}

	FoundationBridgeComponent->OnResponseReady.RemoveDynamic(
		this,
		&UWebUIHostWidget::HandleFoundationResponseReady
	);
	FoundationBridgeComponent->OnResponseReady.AddDynamic(
		this,
		&UWebUIHostWidget::HandleFoundationResponseReady
	);

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[WebUIHostWidget] Foundation bridge bound. World=%s Actor=%s LegacyOwned=%s"),
		*GetNameSafe(World),
		*GetNameSafe(FoundationBridgeActor),
		bOwnsFoundationBridgeActor ? TEXT("true") : TEXT("false")
	);

	return true;
}

bool UWebUIHostWidget::IsFoundationBridgeReady() const
{
	return IsValid(FoundationBridgeActor) && IsValid(FoundationBridgeComponent);
}

void UWebUIHostWidget::ShutdownFoundationBridge()
{
	if (FoundationBridgeComponent)
	{
		FoundationBridgeComponent->OnResponseReady.RemoveDynamic(
			this,
			&UWebUIHostWidget::HandleFoundationResponseReady
		);
	}

	if (
		bOwnsFoundationBridgeActor &&
		bDestroyAutoSpawnedFoundationBridgeOnDestruct &&
		IsValid(FoundationBridgeActor)
	)
	{
		FoundationBridgeActor->Destroy();
	}

	FoundationBridgeComponent = nullptr;
	FoundationBridgeActor = nullptr;
	bOwnsFoundationBridgeActor = false;
	BoundFoundationWorld.Reset();
}

void UWebUIHostWidget::HandleWorldChanged(UWorld* NewWorld)
{
	if (!NewWorld)
	{
		return;
	}

	if (BoundFoundationWorld.Get() == NewWorld && IsFoundationBridgeReady())
	{
		return;
	}

	ShutdownFoundationBridge();
	BoundFoundationWorld = NewWorld;

	if (bEnableFoundationProtocol)
	{
		InitializeFoundationBridge();
	}

	if (MainBrowser)
	{
		TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
		Payload->SetStringField(TEXT("world"), NewWorld->GetName());
		Payload->SetBoolField(TEXT("foundationReady"), IsFoundationBridgeReady());

		TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
		Response->SetStringField(TEXT("source"), TEXT("ue"));
		Response->SetStringField(TEXT("type"), TEXT("webUIWorldChanged"));
		Response->SetStringField(TEXT("requestId"), TEXT("runtime_event"));
		Response->SetBoolField(TEXT("success"), true);
		Response->SetObjectField(TEXT("payload"), Payload);

		MainBrowser->SendEventToPage(FoundationResponseEventName, SerializeJsonObject(Response));
	}
}

void UWebUIHostWidget::PrepareForWorldCleanup(UWorld* CleaningWorld)
{
	if (!CleaningWorld || BoundFoundationWorld.Get() != CleaningWorld)
	{
		return;
	}

	ShutdownFoundationBridge();
}

void UWebUIHostWidget::SetBrowserInteractionEnabled(bool bEnabled)
{
	if (!MainBrowser)
	{
		return;
	}

	const ESlateVisibility TargetVisibility =
		bEnabled ? ESlateVisibility::Visible : ESlateVisibility::HitTestInvisible;

	SetVisibility(TargetVisibility);
	MainBrowser->SetVisibility(TargetVisibility);
}

void UWebUIHostWidget::NotifyWebUIInputModeChanged(
	EWebUIInputMode NewMode,
	const FString& ToggleKeyName
)
{
	if (!MainBrowser)
	{
		return;
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(
		TEXT("mode"),
		NewMode == EWebUIInputMode::UI ? TEXT("ui") : TEXT("scene")
	);
	Payload->SetStringField(TEXT("toggleKey"), ToggleKeyName);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("source"), TEXT("ue"));
	Response->SetStringField(TEXT("type"), TEXT("webUIInputModeChanged"));
	Response->SetStringField(TEXT("requestId"), TEXT("runtime_event"));
	Response->SetBoolField(TEXT("success"), true);
	Response->SetObjectField(TEXT("payload"), Payload);

	MainBrowser->SendEventToPage(FoundationResponseEventName, SerializeJsonObject(Response));
}

void UWebUIHostWidget::HandleBrowserEvent(
	const FString& EventName,
	const FString& PayloadJson
)
{
	LastBridgeEvent = EventName;

	UE_LOG(
		LogTemp,
		Log,
		TEXT("[WebUIHostWidget] Bridge Event=%s Payload=%s"),
		*EventName,
		*PayloadJson
	);

	// 正式统一协议：完整 EngineJS message 直接交给 WebUIFoundation 分发器。
	if (bEnableFoundationProtocol && EventName == FoundationCommandEventName)
	{
		HandleFoundationDescriptor(PayloadJson);
		return;
	}

	// 兼容直接以命令名作为事件名的旧/简化页面。
	if (
		bEnableFoundationProtocol &&
		bAcceptDirectFoundationCommandEvents &&
		IsFoundationCommandType(EventName)
	)
	{
		HandleDirectFoundationCommand(EventName, PayloadJson);
		return;
	}

	// 旧版 WebUIBridge 相机接口继续保留。
	if (EventName == TEXT("Camera.MoveToWorld"))
	{
		HandleMoveToWorld(PayloadJson);
		return;
	}

	if (EventName == TEXT("Camera.MoveToGeo"))
	{
		HandleMoveToGeo(PayloadJson);
		return;
	}

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

void UWebUIHostWidget::HandleFoundationDescriptor(const FString& DescriptorJson)
{
	if (!IsFoundationBridgeReady() && !InitializeFoundationBridge())
	{
		SendFoundationTransportError(
			DescriptorJson,
			TEXT("unknownResult"),
			TEXT("WebUI Foundation bridge is not available.")
		);
		return;
	}

	TSharedPtr<FJsonObject> Descriptor;
	if (!TryParseJsonObject(DescriptorJson, Descriptor))
	{
		SendFoundationTransportError(
			DescriptorJson,
			TEXT("invalidJson"),
			TEXT("Invalid Foundation descriptor JSON from embedded HTML.")
		);
		return;
	}

	FoundationBridgeComponent->HandleWebUIFoundationMessage(DescriptorJson);
}

void UWebUIHostWidget::HandleDirectFoundationCommand(
	const FString& CommandType,
	const FString& PayloadJson
)
{
	HandleFoundationDescriptor(BuildFoundationDescriptor(CommandType, PayloadJson));
}

void UWebUIHostWidget::HandleFoundationResponseReady(const FString& ResponseJson)
{
	if (!MainBrowser)
	{
		UE_LOG(
			LogTemp,
			Error,
			TEXT("[WebUIHostWidget] Foundation response generated, but MainBrowser is null. Response=%s"),
			*ResponseJson
		);
		return;
	}

	MainBrowser->SendEventToPage(FoundationResponseEventName, ResponseJson);

	UE_LOG(
		LogTemp,
		Warning,
		TEXT("[WebUIHostWidget] Foundation response sent to embedded HTML. Event=%s Response=%s"),
		*FoundationResponseEventName,
		*ResponseJson
	);
}

void UWebUIHostWidget::SendFoundationTransportError(
	const FString& DescriptorJson,
	const FString& FallbackType,
	const FString& Message
)
{
	if (!MainBrowser)
	{
		return;
	}

	FString RequestId = TEXT("unknown");
	FString ResultType = FallbackType;

	TSharedPtr<FJsonObject> Descriptor;
	if (TryParseJsonObject(DescriptorJson, Descriptor) && Descriptor.IsValid())
	{
		Descriptor->TryGetStringField(TEXT("requestId"), RequestId);

		FString CommandType;
		if (Descriptor->TryGetStringField(TEXT("type"), CommandType) && !CommandType.IsEmpty())
		{
			ResultType = CommandType + TEXT("Result");
		}
	}

	TSharedPtr<FJsonObject> Payload = MakeShared<FJsonObject>();
	Payload->SetStringField(TEXT("message"), Message);

	TSharedPtr<FJsonObject> Response = MakeShared<FJsonObject>();
	Response->SetStringField(TEXT("source"), TEXT("ue"));
	Response->SetStringField(TEXT("type"), ResultType);
	Response->SetStringField(TEXT("requestId"), RequestId);
	Response->SetBoolField(TEXT("success"), false);
	Response->SetObjectField(TEXT("payload"), Payload);

	MainBrowser->SendEventToPage(
		FoundationResponseEventName,
		SerializeJsonObject(Response)
	);
}

FString UWebUIHostWidget::BuildFoundationDescriptor(
	const FString& CommandType,
	const FString& PayloadJson
) const
{
	TSharedPtr<FJsonObject> Payload;
	if (!TryParseJsonObject(PayloadJson, Payload))
	{
		Payload = MakeShared<FJsonObject>();
	}

	TSharedPtr<FJsonObject> Descriptor = MakeShared<FJsonObject>();
	Descriptor->SetStringField(TEXT("source"), TEXT("enginejs"));
	Descriptor->SetStringField(TEXT("type"), CommandType);
	Descriptor->SetStringField(TEXT("requestId"), MakeWebUIRequestId());
	Descriptor->SetNumberField(
		TEXT("timestamp"),
		static_cast<double>(FDateTime::UtcNow().ToUnixTimestamp()) * 1000.0
	);
	Descriptor->SetObjectField(TEXT("payload"), Payload);
	return SerializeJsonObject(Descriptor);
}

bool UWebUIHostWidget::IsFoundationCommandType(const FString& EventName) const
{
	static const TSet<FString> CommandTypes =
	{
		TEXT("engineInit"),
		TEXT("requestSceneState"),
		TEXT("info"),
		TEXT("flyTo"),
		TEXT("resetView"),
		TEXT("setInputEnabled"),
		TEXT("setMouseWheelMoveConfig"),
		TEXT("getMouseWheelMoveConfig"),
		TEXT("getViewPosition"),
		TEXT("addMarker"),
		TEXT("updateMarker"),
		TEXT("removeMarker"),
		TEXT("clearMarkers"),
		TEXT("moveActor"),
		TEXT("restoreActors"),
		TEXT("getActorInfo"),
		TEXT("setWebUIInputMode"),
		TEXT("setWebUIInputRegions"),
		TEXT("getWebUIInputMode"),
		TEXT("toggleWebUIInputMode")
	};

	return CommandTypes.Contains(EventName);
}

void UWebUIHostWidget::SendUEReady()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = FString::Printf(
		TEXT(R"({"ok":true,"source":"WebUIHostWidget","foundationReady":%s})"),
		IsFoundationBridgeReady() ? TEXT("true") : TEXT("false")
	);
	MainBrowser->SendEventToPage(TEXT("UEReady"), Payload);
}

void UWebUIHostWidget::SendPong()
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = FString::Printf(
		TEXT(R"({"ok":true,"source":"WebUIHostWidget","foundationReady":%s})"),
		IsFoundationBridgeReady() ? TEXT("true") : TEXT("false")
	);
	MainBrowser->SendEventToPage(TEXT("Pong"), Payload);
}

void UWebUIHostWidget::SendSceneState()
{
	if (!MainBrowser)
	{
		return;
	}

	MainBrowser->SendEventToPage(TEXT("SceneState"), BuildSceneStateJson());
}

void UWebUIHostWidget::HandleMoveToWorld(const FString& PayloadJson)
{
	if (!MainBrowser)
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	if (!TryParseJsonObject(PayloadJson, Root))
	{
		SendCameraMoveResult(TEXT("Camera.MoveToWorld.Result"), false, TEXT("Invalid JSON payload."));
		return;
	}

	const double X = GetJsonNumberOrDefault(Root, TEXT("x"));
	const double Y = GetJsonNumberOrDefault(Root, TEXT("y"));
	const double Z = GetJsonNumberOrDefault(Root, TEXT("z"));
	const double Pitch = GetJsonNumberOrDefault(Root, TEXT("pitch"));
	const double Yaw = GetJsonNumberOrDefault(Root, TEXT("yaw"));
	const double Roll = GetJsonNumberOrDefault(Root, TEXT("roll"));

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		SendCameraMoveResult(TEXT("Camera.MoveToWorld.Result"), false, TEXT("GameInstance is null."));
		return;
	}

	UWebUICameraSubsystem* CameraSubsystem = GameInstance->GetSubsystem<UWebUICameraSubsystem>();
	if (!CameraSubsystem)
	{
		SendCameraMoveResult(TEXT("Camera.MoveToWorld.Result"), false, TEXT("WebUICameraSubsystem is null."));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		SendCameraMoveResult(TEXT("Camera.MoveToWorld.Result"), false, TEXT("PlayerController is null."));
		return;
	}

	CameraSubsystem->SetTargetPlayerController(PlayerController);

	const bool bOk = CameraSubsystem->MoveControlledPawnToWorldLocation(
		FVector(static_cast<float>(X), static_cast<float>(Y), static_cast<float>(Z)),
		FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll))
	);

	SendCameraMoveResult(
		TEXT("Camera.MoveToWorld.Result"),
		bOk,
		bOk ? TEXT("") : TEXT("MoveControlledPawnToWorldLocation failed. Check PlayerController and Pawn.")
	);

	if (bOk)
	{
		SendSceneState();
	}
}

void UWebUIHostWidget::HandleMoveToGeo(const FString& PayloadJson)
{
	if (!MainBrowser)
	{
		return;
	}

	TSharedPtr<FJsonObject> Root;
	if (!TryParseJsonObject(PayloadJson, Root))
	{
		SendCameraMoveResult(TEXT("Camera.MoveToGeo.Result"), false, TEXT("Invalid JSON payload."));
		return;
	}

	const double Longitude = GetJsonNumberOrDefault(Root, TEXT("longitude"));
	const double Latitude = GetJsonNumberOrDefault(Root, TEXT("latitude"));
	const double Height = GetJsonNumberOrDefault(Root, TEXT("height"));
	const double Pitch = GetJsonNumberOrDefault(Root, TEXT("pitch"));
	const double Yaw = GetJsonNumberOrDefault(Root, TEXT("yaw"));
	const double Roll = GetJsonNumberOrDefault(Root, TEXT("roll"));

	UGameInstance* GameInstance = GetGameInstance();
	if (!GameInstance)
	{
		SendCameraMoveResult(TEXT("Camera.MoveToGeo.Result"), false, TEXT("GameInstance is null."));
		return;
	}

	UWebUICameraSubsystem* CameraSubsystem = GameInstance->GetSubsystem<UWebUICameraSubsystem>();
	if (!CameraSubsystem)
	{
		SendCameraMoveResult(TEXT("Camera.MoveToGeo.Result"), false, TEXT("WebUICameraSubsystem is null."));
		return;
	}

	APlayerController* PlayerController = UGameplayStatics::GetPlayerController(this, 0);
	if (!PlayerController)
	{
		SendCameraMoveResult(TEXT("Camera.MoveToGeo.Result"), false, TEXT("PlayerController is null."));
		return;
	}

	CameraSubsystem->SetTargetPlayerController(PlayerController);

	const bool bOk = CameraSubsystem->MoveControlledPawnToGeoLocation(
		Longitude,
		Latitude,
		Height,
		FRotator(static_cast<float>(Pitch), static_cast<float>(Yaw), static_cast<float>(Roll))
	);

	SendCameraMoveResult(
		TEXT("Camera.MoveToGeo.Result"),
		bOk,
		bOk ? TEXT("") : TEXT("MoveControlledPawnToGeoLocation failed. Check CesiumGeoreference, PlayerController and Pawn.")
	);

	if (bOk)
	{
		SendSceneState();
	}
}

void UWebUIHostWidget::SendCameraMoveResult(
	const FString& EventName,
	bool bOk,
	const FString& ErrorMessage
)
{
	if (!MainBrowser)
	{
		return;
	}

	const FString Payload = FString::Printf(
		TEXT(R"({"ok":%s,"error":"%s"})"),
		bOk ? TEXT("true") : TEXT("false"),
		*EscapeWebUIJsonString(ErrorMessage)
	);

	MainBrowser->SendEventToPage(EventName, Payload);
}

FString UWebUIHostWidget::BuildSceneStateJson() const
{
	return FString::Printf(
		TEXT(R"({"host":"WebUIHostWidget","status":"running","world":"%s","url":"%s","interactionMode":"%s","lastEvent":"%s","foundationReady":%s})"),
		*EscapeWebUIJsonString(GetNameSafe(BoundFoundationWorld.Get())),
		*EscapeWebUIJsonString(CurrentUrl),
		*GetInteractionModeString(),
		*EscapeWebUIJsonString(LastBridgeEvent),
		IsFoundationBridgeReady() ? TEXT("true") : TEXT("false")
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
