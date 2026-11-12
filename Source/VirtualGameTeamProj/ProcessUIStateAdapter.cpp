#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "ProcessControlUI.h"
#include "ProcessPlantBinding.h"
#include "PowderPlantTest.h"
#include "PowderCameraController.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetTree.h"
#include "Components/TextBlock.h"
#include "Components/Button.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Brushes/SlateRoundedBoxBrush.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "Framework/Application/SlateApplication.h"
#include "HAL/PlatformTime.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

namespace
{
const TCHAR* CommandWidgets[] = {TEXT("ValveOpen"), TEXT("ValveClose"), TEXT("TransferStart"), TEXT("TransferStop"),
    TEXT("MixerStart"), TEXT("MixerStop"), TEXT("SetRPM"), TEXT("ManualRequest")};
const TCHAR* EquipmentIds[] = {TEXT("supply-a"), TEXT("supply-b"), TEXT("supply-c"), TEXT("supply-d"), TEXT("supply-e"), TEXT("supply-f"), TEXT("weigher"), TEXT("receiver"), TEXT("mixer")};
const FLinearColor Ink(.025f,.055f,.09f), Muted(.16f,.22f,.28f), Accent(.02f,.25f,.40f);

bool IsImplementedId(const FString& Id, const FPlantSimulationSnapshot* State = nullptr)
{
    const int32 Index = ProcessPlantBinding::SupplierIndex(Id);
    const int32 Count = State ? State->RequestedSuppliers.Num() : 3;
    // A failed/uninitialized data load has no gate array yet; show that state, not "unimplemented".
    if(State && Count==0 && State->Validity!=EPlantValueValidity::Valid && Index!=INDEX_NONE) return true;
    return (Index != INDEX_NONE && Index < Count && ProcessPlantBinding::SupportedCount(Count)) ||
        Id == TEXT("weigher") || Id == TEXT("receiver") || Id == TEXT("mixer");
}

FString ReadString(UObject* Object, const TCHAR* Name)
{
    const auto* Property = FindFProperty<FStrProperty>(Object->GetClass(), Name);
    return Property ? Property->GetPropertyValue_InContainer(Object) : FString();
}

void SetLabel(UUserWidget* Widget, const TCHAR* Name, const TCHAR* Text)
{
    if (auto* Label = Cast<UTextBlock>(Widget->GetWidgetFromName(Name)))
        Label->SetText(FText::FromString(Text));
}

void StyleText(UUserWidget* W, const TCHAR* Name, int32 Size, FLinearColor Color)
{
    if (auto* Label = Cast<UTextBlock>(W->GetWidgetFromName(Name)))
    {
        auto Font = Label->GetFont(); Font.Size = Size; Label->SetFont(Font);
        Label->SetColorAndOpacity(FSlateColor(Color));
    }
}

void StyleButton(UUserWidget* W, const TCHAR* Name, FLinearColor Fill, bool Selected = false)
{
    if (auto* Button = Cast<UButton>(W->GetWidgetFromName(Name)))
    {
        auto Style = Button->GetStyle();
        const FLinearColor Outline = Selected ? Accent : FLinearColor(.55f,.65f,.72f);
        Style.Normal = FSlateRoundedBoxBrush(Fill, 8.f, Outline, Selected ? 3.f : 1.f);
        Style.Hovered = FSlateRoundedBoxBrush(Fill * .92f, 8.f, Accent, 2.f);
        Style.Pressed = FSlateRoundedBoxBrush(Fill * .84f, 8.f, Accent, 3.f);
        Style.NormalPadding = FMargin(4,6);
        Style.PressedPadding = FMargin(4,6);
        Button->SetStyle(Style); Button->SetBackgroundColor(FLinearColor::White);
    }
}

void HideUnavailableControls(UUserWidget* W)
{
    for (const TCHAR* Name : CommandWidgets)
        if (auto* Control = W->GetWidgetFromName(Name))
        { Control->SetIsEnabled(false); Control->SetVisibility(ESlateVisibility::Collapsed); }
    for (const TCHAR* Name : {TEXT("AmountLabel"), TEXT("AmountInput"), TEXT("RPMLabel"), TEXT("RPMInput"), TEXT("DetailsButton"), TEXT("DetailsText")})
        if (auto* Control = W->GetWidgetFromName(Name)) Control->SetVisibility(ESlateVisibility::Collapsed);
}

void PrepareReadOnlyPanel(UUserWidget* W)
{
    if(auto* Panel=Cast<UVerticalBox>(W->GetWidgetFromName(TEXT("PanelContents"))))
        if(!W->GetWidgetFromName(TEXT("TransferTelemetry")))
        {
            auto* Detail=W->WidgetTree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(),TEXT("TransferTelemetry"));
            Detail->SetAutoWrapText(true);Detail->SetVisibility(ESlateVisibility::Collapsed);
            Panel->AddChildToVerticalBox(Detail)->SetPadding(FMargin(6,8));StyleText(W,TEXT("TransferTelemetry"),15,Ink);
        }

    SetLabel(W, TEXT("Title"), TEXT("원료 투입 · 혼합\n장비 상태 확인"));
    SetLabel(W, TEXT("View2DText"), TEXT("공정도"));
    SetLabel(W, TEXT("View3DText"), TEXT("3D 보기"));
    SetLabel(W, TEXT("Help"), TEXT("실시간 시뮬레이션 · 미보정\n장비를 선택해 상태를 확인하세요. 지금은 조회만 가능합니다."));
    SetLabel(W, TEXT("SelectHint"), TEXT("확인할 장비를 선택하세요\n선택한 장비의 상태가 여기에 표시됩니다."));
    SetLabel(W, TEXT("Disconnected"), TEXT("지금은 상태만 확인할 수 있습니다.\n밸브 열기·원료 이송·혼합 조작은 사용할 수 없습니다."));
    SetLabel(W, TEXT("QuantityLabel"), TEXT("입자 수"));
    SetLabel(W, TEXT("CloseButtonText"), TEXT("닫기 · Esc"));
    SetLabel(W, TEXT("DiagramNote"), TEXT("공급 → 칭량 → 수취 → 혼합 순서의 장비 배치입니다.\n‘계산 중’ 표시는 원료 이송·혼합이 실행 중이라는 뜻은 아닙니다."));
    StyleButton(W, TEXT("PanelBlocker"), FLinearColor(.97f,.98f,1.f));
    StyleButton(W, TEXT("CloseButton"), FLinearColor(.82f,.90f,.95f));
    for (const TCHAR* Name : {TEXT("PanelName"), TEXT("StatusValue"), TEXT("QuantityValue"), TEXT("ValveValue")})
        StyleText(W, Name, FCString::Strcmp(Name, TEXT("PanelName")) == 0 ? 23 : 19, Ink);
    for (const TCHAR* Name : {TEXT("PanelDescription"), TEXT("PanelState"), TEXT("QuantityLabel"), TEXT("ValveLabel"), TEXT("Disconnected"), TEXT("DataSource")})
    {
        StyleText(W, Name, 15, Muted);
        if (auto* Label = Cast<UTextBlock>(W->GetWidgetFromName(Name))) Label->SetAutoWrapText(true);
    }
    StyleText(W, TEXT("CloseButtonText"), 17, Ink);
    StyleText(W, TEXT("Help"), 16, FLinearColor::White);
    StyleText(W, TEXT("ThreeDNotice"), 18, FLinearColor::White);
    if (auto* Panel = Cast<UVerticalBox>(W->GetWidgetFromName(TEXT("PanelContents"))))
    {
        // Keep the source visible and the close action at the end of the compact read-only panel.
        if (auto* Close = W->GetWidgetFromName(TEXT("CloseButton")))
        { Panel->RemoveChild(Close); Panel->AddChildToVerticalBox(Close); }
        for (auto* Child : Panel->GetAllChildren())
            if (auto* Slot = Cast<UVerticalBoxSlot>(Child->Slot)) Slot->SetPadding(FMargin(6,3,6,7));
    }
    HideUnavailableControls(W);
}

FString TileText(const FProcessEquipmentSnapshot& S)
{
    if(S.EquipmentId==TEXT("weigher"))
        return TEXT("현재 담긴 양\n")+UProcessUIStateAdapter::FormatMass(S.CurrentMass)+
            (S.ValveAppliedOpen.Validity==EPlantValueValidity::Valid?(S.ValveAppliedOpen.Value!=0?TEXT("\n밸브 열림"):TEXT("\n밸브 닫힘")):TEXT(""));

    switch (S.Validity)
    {
    case EPlantValueValidity::Unsupported: return TEXT("시뮬레이션\n미구현");
    case EPlantValueValidity::SourceMissing: return TEXT("연결 없음");
    case EPlantValueValidity::Initializing: return TEXT("준비 중");
    case EPlantValueValidity::Stale: return TEXT("갱신 중단");
    case EPlantValueValidity::Error: return TEXT("오류 확인");
    case EPlantValueValidity::Unknown: return TEXT("상태 미확인");
    default: break;
    }
    if (S.EquipmentId == TEXT("mixer"))
        return S.AppliedRPM.Validity == EPlantValueValidity::Valid ? FString::Printf(TEXT("계산 적용\n%.1f rpm"), S.AppliedRPM.Value) : TEXT("회전수\n확인 대기");
    const TCHAR* Valve = S.ValveAppliedOpen.Value != 0 ? TEXT("열림") : TEXT("닫힘");
    return S.InitialParticleCount.Validity == EPlantValueValidity::Valid && S.InitialParticleCount.Value == 0 ?
        FString::Printf(TEXT("밸브 %s\n초기 0개"), Valve) : FString::Printf(TEXT("밸브\n%s"), Valve);
}

FProcessStateValue Value(double Number, const TCHAR* Unit, EPlantValueValidity Validity)
{
    FProcessStateValue Result;
    Result.Value = Number; Result.Unit = Unit; Result.Validity = Validity;
    return Result;
}

const TCHAR* ExecutionText(EPlantExecutionState State)
{
    switch (State)
    {
    case EPlantExecutionState::Running: return TEXT("시뮬레이션 계산 중");
    case EPlantExecutionState::Paused: return TEXT("계산 일시정지");
    case EPlantExecutionState::PauseRequested: return TEXT("계산 일시정지 확인 중");
    case EPlantExecutionState::ResumeRequested: return TEXT("계산 재개 확인 중");
    default: return TEXT("계산 상태를 확인할 수 없습니다");
    }
}
}

FProcessEquipmentSnapshot UProcessUIStateAdapter::BuildEquipment(const FString& Id, const FPlantSimulationSnapshot* S, double Now, double MaxAge)
{
    FProcessEquipmentSnapshot R;
    R.EquipmentId = Id;
    if(Id==TEXT("weigher")||Id==TEXT("receiver"))R.Source=TEXT("시뮬레이션 계산값");
    if (!IsImplementedId(Id, S))
    {
        R.Validity = EPlantValueValidity::Unsupported;
        return R; // A visible model is not evidence that the bound solver has this gate.
    }
    if (!S)
    {
        R.Validity = EPlantValueValidity::SourceMissing;
        return R;
    }
    R.SourceActorPath = S->SourceActorPath; R.Epoch = S->Epoch; R.Sequence = S->Sequence;
    R.UpdatedUtc = S->UpdatedUtc; R.Execution = S->Execution; R.Error = S->Error;
    R.Validity = S->Validity;
    if (R.Validity == EPlantValueValidity::Valid &&
        (S->PublishedWallSeconds <= 0 || Now - S->PublishedWallSeconds > MaxAge)) R.Validity = EPlantValueValidity::Stale;
    const auto Validity = R.Validity;
    if(Id==TEXT("weigher")||Id==TEXT("receiver"))
    {
        R.CurrentMass=Id==TEXT("weigher")?S->WeighingMass:S->ReceiverMass;R.Transfer=S->Transfer;
        if(Validity!=EPlantValueValidity::Valid){R.CurrentMass.Validity=Validity;R.Transfer.Validity=Validity;}
        if(!S->MassDiagnostic.IsEmpty())R.Error+=TEXT("\n")+S->MassDiagnostic;
    }

    R.SimulationTime = Value(S->PhysicalSeconds, TEXT("s"), Validity);
    if (Id.StartsWith(TEXT("supply-")))
    {
        const int32 Index = Id[7] - TCHAR('a');
        if (!S->RequestedSuppliers.IsValidIndex(Index))
        {
            if(R.Validity==EPlantValueValidity::Valid) R.Validity = EPlantValueValidity::Unknown;
            R.SimulationTime.Validity = R.Validity;
            return R;
        }
        R.ValveRequestedOpen = Value(S->RequestedSuppliers[Index], TEXT("bool"), Validity);
        R.ValveAppliedOpen = Value((S->AppliedSupplierMask & (1 << Index)) != 0, TEXT("bool"), Validity);
        if (S->InitialSupplierCounts.IsValidIndex(Index))
            R.InitialParticleCount = Value(S->InitialSupplierCounts[Index], TEXT("particles (initial)"), Validity);
        // No supplier region count exists. Initial charge is not current inventory.
    }
    else if (Id == TEXT("weigher"))
    {
        R.RegionParticleCount = Value(S->InWeighingHopper, TEXT("particles"), Validity);
        R.ValveRequestedOpen = Value(S->RequestedHopperOpen, TEXT("bool"), Validity);
        R.ValveAppliedOpen = Value(S->AppliedHopperOpen, TEXT("bool"), Validity);
    }
    else if (Id == TEXT("receiver"))
    {
        R.RegionParticleCount = Value(S->InReceiver, TEXT("particles"), Validity);
        R.ValveRequestedOpen = Value(S->RequestedReceiverOpen, TEXT("bool"), Validity);
        R.ValveAppliedOpen = Value(S->AppliedReceiverOpen, TEXT("bool"), Validity);
    }
    else if (Id == TEXT("mixer"))
    {
        R.RegionParticleCount = Value(S->InMixer, TEXT("particles"), Validity);
        R.RequestedRPM = Value(S->RequestedRPM, TEXT("rpm (requested setpoint)"), Validity);
        R.AppliedRPM = Value(S->AppliedRPM, TEXT("rpm (last committed batch)"), S->AppliedRPMValid ? Validity : EPlantValueValidity::Unknown);
    }
    return R;
}

FString UProcessUIStateAdapter::FormatMass(const FPlantMassValue& Mass)
{
    if(Mass.Validity!=EPlantValueValidity::Valid||!FMath::IsFinite(Mass.Kg)||Mass.Kg<0)return TEXT("데이터 없음");
    if(Mass.Kg==0)return TEXT("0 g");
    return Mass.Kg<1?FString::Printf(TEXT("%.3f g"),Mass.Kg*1000):FString::Printf(TEXT("%.4f kg"),Mass.Kg);
}

FString UProcessUIStateAdapter::FormatTransfer(const FPlantTransferSnapshot& T)
{
    if(T.Validity!=EPlantValueValidity::Valid)return TEXT("수동 이송 · 판정 불가\n데이터 없음\n")+T.Reason;
    if(T.State==EPlantTransferState::Idle)return TEXT("수동 이송 · 시작 전\n칭량호퍼 출구의 실제 개방 직전 기준량을 기록합니다.\n시험 기준 · 운전 기준 미확정");
    const TCHAR* State=T.State==EPlantTransferState::Complete?TEXT("완료"):T.State==EPlantTransferState::Indeterminate?TEXT("판정 불가"):
        T.State==EPlantTransferState::Empty?TEXT("이송할 원료 없음"):TEXT("이송 중");
    auto Mass=[](double Kg){FPlantMassValue V;V.Kg=Kg;V.Validity=EPlantValueValidity::Valid;return FormatMass(V);};
    FString Text=FString::Printf(TEXT("수동 이송 #%lld · %s\n기준량 %s\n남은 양 %s / 이동 중 %s\n이번 도착 %s\n기존 수취량 %s (도착량에서 제외)\n안정 관찰 %.3f / %.3f초 (해석 시간)\n출구 밸브 %s · 완료와 별도 확인\n%s\n시험 기준: 잔량 WH %s / 배관 %s\n질량 오차 %s · 운전 기준 미확정"),
        T.Id,State,*Mass(T.BaselineKg),*Mass(T.RemainingKg),*Mass(T.InTransitKg),*Mass(T.ArrivedKg),*Mass(T.InitialReceiverKg),T.StableSeconds,T.Criteria.StableSimulationSeconds,
        T.OutletAppliedOpen?TEXT("열림"):TEXT("닫힘"),*T.Reason,*Mass(T.Criteria.WeighingResidualKg),*Mass(T.Criteria.PipeResidualKg),*Mass(T.Criteria.MassErrorKg));
    if(T.UnclassifiedKg>0)Text+=TEXT("\n미분류 ")+Mass(T.UnclassifiedKg);
    if(T.OutletAppliedOpen&&T.CarrierFlowValid&&T.CalculatedCarrierFlowM3s==0&&T.State!=EPlantTransferState::Complete)
        Text+=TEXT("\n계산된 이송 공기 유량 0 m³/s · 현재 공기 흐름이 없습니다.\n현재 모델은 공급 밸브를 모두 닫으면 이송 공기도 흐르지 않습니다. 독립 이송 공기 공급은 미지원입니다.");
    return Text;
}

void UProcessUIStateAdapter::Format(const FProcessEquipmentSnapshot& S, FText& Status, FText& Quantity, FText& Valve)
{
    // Always replace every field, including unsupported/missing/error cases.
    Quantity = FText::FromString(S.EquipmentId==TEXT("weigher")||S.EquipmentId==TEXT("receiver")?TEXT("데이터 없음"):TEXT("미확인")); Valve = FText::FromString(TEXT("미확인"));
    switch (S.Validity)
    {
    case EPlantValueValidity::Unsupported: Status = FText::FromString(TEXT("시뮬레이션 미구현")); return;
    case EPlantValueValidity::SourceMissing: Status = FText::FromString(TEXT("시뮬레이션에 연결되지 않았습니다")); return;
    case EPlantValueValidity::Initializing: Status = FText::FromString(TEXT("준비 중 · 첫 계산 결과를 기다립니다")); return;
    case EPlantValueValidity::Stale: Status = FText::FromString(TEXT("상태 갱신이 멈췄습니다\n이전 수치는 표시하지 않습니다")); return;
    case EPlantValueValidity::Error: Status = FText::FromString(TEXT("계산 중 오류가 발생했습니다")); return;
    case EPlantValueValidity::Unknown: Status = FText::FromString(TEXT("장비 상태를 확인할 수 없습니다")); return;
    default: break;
    }
    Status = FText::FromString(FString::Printf(TEXT("%s\n시뮬레이션 시간 %.4f초"), ExecutionText(S.Execution), S.SimulationTime.Value));
    if(S.EquipmentId==TEXT("weigher")||S.EquipmentId==TEXT("receiver"))
        Quantity=FText::FromString(FormatMass(S.CurrentMass)+TEXT("\n모델 기준 · 배치 환산 없음"));
    else if (S.RegionParticleCount.Validity == EPlantValueValidity::Valid)
        Quantity = FText::FromString(FText::AsNumber(int64(S.RegionParticleCount.Value)).ToString() + TEXT("개\n무게(kg)는 제공하지 않습니다"));
    else if (S.InitialParticleCount.Validity == EPlantValueValidity::Valid)
        Quantity = FText::FromString(FText::AsNumber(int64(S.InitialParticleCount.Value)).ToString() + TEXT("개\n현재 남은 양은 집계하지 않습니다"));
    if (S.EquipmentId == TEXT("mixer"))
    {
        const FString Applied = S.AppliedRPM.Validity == EPlantValueValidity::Valid ?
            FString::Printf(TEXT("%.1f rpm"), S.AppliedRPM.Value) : TEXT("미확인");
        Valve = FText::FromString(FString::Printf(TEXT("설정값  %.1f rpm\n최근 계산에 사용  %s"), S.RequestedRPM.Value, *Applied));
    }
    else if (S.ValveRequestedOpen.Validity == EPlantValueValidity::Valid)
    {
        const bool Requested = S.ValveRequestedOpen.Value != 0, Applied = S.ValveAppliedOpen.Value != 0;
        Valve = FText::FromString(FString::Printf(TEXT("요청한 상태  %s\n계산에 반영된 상태  %s%s"),
            Requested ? TEXT("열림") : TEXT("닫힘"), Applied ? TEXT("열림") : TEXT("닫힘"),
            Requested != Applied ? TEXT("\n변경 반영을 기다리고 있습니다") : TEXT("")));
    }
}

bool UProcessUIStateAdapter::Attach(UUserWidget* InWidget, APowderPlantTest* InSource)
{
    Detach();
    if (!IsValid(InWidget) || (InSource && InSource->GetWorld() != InWidget->GetWorld())) return false;
    auto* Event = FindFProperty<FMulticastDelegateProperty>(InWidget->GetClass(), TEXT("OnStateRequested"));
    auto* Apply = InWidget->FindFunction(TEXT("ApplyEquipmentSnapshot"));
    if (!Event || !Apply) return false;
    const auto* Existing = Event->GetMulticastDelegate(Event->ContainerPtrToValuePtr<void>(InWidget));
    if (Existing && Existing->IsBound()) return false; // Refuse competing state suppliers.
    // Fail closed if the Blueprint interface is changed, rather than invoking a mismatched function.
    if (!FindFProperty<FStrProperty>(Apply, TEXT("EquipmentId")) || !FindFProperty<FBoolProperty>(Apply, TEXT("IsTestData"))) return false;
    for (const TCHAR* Name : {TEXT("StatusText"), TEXT("QuantityText"), TEXT("ValveText"), TEXT("SourceLabel")})
        if (!FindFProperty<FTextProperty>(Apply, Name)) return false;
    Widget = InWidget; SourceActor = InSource;
    PrepareReadOnlyPanel(InWidget);
    for (int32 Index = 0; Index < 9; ++Index)
    {
        const FString Name = FString::Printf(TEXT("Equipment%dText"), Index);
        SetLabel(InWidget, *Name, Index >= 3 && Index <= 5 ? TEXT("시뮬레이션\n미구현") : TEXT("상태\n조회"));
        StyleText(InWidget, *Name, 15, Ink);
        if (auto* Label = Cast<UTextBlock>(InWidget->GetWidgetFromName(*Name)))
        { Label->SetAutoWrapText(false); Label->SetWrapTextAt(0.f); }
    }
    for (const TCHAR* Name : {TEXT("AmountInput"), TEXT("RPMInput")})
        if (auto* Input = InWidget->GetWidgetFromName(Name)) Input->SetIsEnabled(false);
    FScriptDelegate Handler; Handler.BindUFunction(this, TEXT("HandleStateRequested"));
    Event->AddDelegate(Handler, InWidget);
    DestructHandle = InWidget->OnNativeDestruct.AddUObject(this, &UProcessUIStateAdapter::WidgetDestructed);
    Refresh();
    return true;
}

void UProcessUIStateAdapter::Detach()
{
    if (auto* W = Widget.Get())
    {
        if (auto* Event = FindFProperty<FMulticastDelegateProperty>(W->GetClass(), TEXT("OnStateRequested")))
        {
            FScriptDelegate Handler; Handler.BindUFunction(this, TEXT("HandleStateRequested"));
            Event->RemoveDelegate(Handler, W);
        }
        W->OnNativeDestruct.Remove(DestructHandle);
    }
    Widget.Reset(); SourceActor.Reset(); Cursor = {}; Latest.Empty(); DestructHandle.Reset();
}

void UProcessUIStateAdapter::WidgetDestructed(UUserWidget*) { Detach(); }
bool UProcessUIStateAdapter::IsAttached() const { return Widget.IsValid(); }

FProcessEquipmentSnapshot UProcessUIStateAdapter::ReadEquipment(const FString& Id)
{
    check(IsInGameThread());
    auto* Source = SourceActor.Get();
    if (!IsValid(Source) || Source->IsActorBeingDestroyed() || !Source->HasActorBegunPlay())
    {
        Latest.Empty();
        return BuildEquipment(Id, nullptr, FPlatformTime::Seconds());
    }
    auto State = Source->GetSimulationSnapshot();
    if (!Cursor.Accept(State.Epoch, State.Sequence))
    {
        State.Validity = EPlantValueValidity::Stale;
        State.Error = TEXT("이전 초기화 세대 또는 순서의 결과");
    }
    return BuildEquipment(Id, &State, FPlatformTime::Seconds());
}

void UProcessUIStateAdapter::HandleStateRequested(FString EquipmentId) { Push(EquipmentId); }

void UProcessUIStateAdapter::Push(const FString& Id)
{
    auto* W = Widget.Get();
    if (!W || Id.IsEmpty()) return;
    const auto State = ReadEquipment(Id);
    // Never retain values from an earlier reset, including unselected equipment.
    for (auto It = Latest.CreateIterator(); It; ++It)
        if (It.Value().Epoch < State.Epoch) It.RemoveCurrent();
    Latest.Add(Id, State);
    UFunction* Apply = W->FindFunction(TEXT("ApplyEquipmentSnapshot"));
    if (!Apply) return;
    FStructOnScope Params(Apply);
    uint8* Data = Params.GetStructMemory();
    FindFProperty<FStrProperty>(Apply, TEXT("EquipmentId"))->SetPropertyValue_InContainer(Data, Id);
    FText Status, Quantity, Valve; Format(State, Status, Quantity, Valve);
    FindFProperty<FTextProperty>(Apply, TEXT("StatusText"))->SetPropertyValue_InContainer(Data, Status);
    FindFProperty<FTextProperty>(Apply, TEXT("QuantityText"))->SetPropertyValue_InContainer(Data, Quantity);
    FindFProperty<FTextProperty>(Apply, TEXT("ValveText"))->SetPropertyValue_InContainer(Data, Valve);
    FindFProperty<FTextProperty>(Apply, TEXT("SourceLabel"))->SetPropertyValue_InContainer(Data, FText::FromString(State.Source));
    FindFProperty<FBoolProperty>(Apply, TEXT("IsTestData"))->SetPropertyValue_InContainer(Data, false);
    W->ProcessEvent(Apply, Data);
    if (Id == ReadString(W, TEXT("SelectedEquipmentId")))
    {
        SetLabel(W, TEXT("ValveLabel"), Id == TEXT("mixer") ? TEXT("혼합기 회전 설정") : TEXT("밸브 상태"));
        SetLabel(W, TEXT("QuantityLabel"), Id.StartsWith(TEXT("supply-")) ? TEXT("처음 넣은 입자 수") : (Id==TEXT("weigher")||Id==TEXT("receiver"))?TEXT("현재 담긴 양"):TEXT("현재 영역의 입자 수"));
        const TCHAR* Description = Id.StartsWith(TEXT("supply-")) ? TEXT("칭량호퍼로 원료를 보내는 공급 장비입니다.") :
            Id == TEXT("weigher") ? TEXT("호퍼 내부 입자의 모델 질량입니다. 출구를 열면 수취호퍼까지 이번 이송분을 관찰합니다.") :
            Id == TEXT("receiver") ? TEXT("혼합기로 보내기 전 원료를 받아두는 호퍼입니다.") : TEXT("원료를 섞는 장비입니다. 아래 회전수는 계산에 사용된 값이며 실측값은 아닙니다.");
        if (State.Validity == EPlantValueValidity::Unsupported) Description = TEXT("공정도에는 표시되지만 이 장비의 시뮬레이션은 아직 구현되지 않았습니다.");
        SetLabel(W, TEXT("PanelDescription"), Description);
        if (auto* Label = Cast<UTextBlock>(W->GetWidgetFromName(TEXT("DataSource"))))
            Label->SetToolTipText(FText::FromString(State.UpdatedUtc.GetTicks() > 0 ? FString::Printf(TEXT("%s\n갱신 UTC %s\n초기화 세대 %lld · 스냅샷 %lld\n%s"),
                *State.SourceActorPath, *State.UpdatedUtc.ToIso8601(), State.Epoch, State.Sequence, *State.Error) : TEXT("아직 전달된 시뮬레이션 상태가 없습니다.")));
        if (auto* Label = W->GetWidgetFromName(TEXT("StatusValue"))) Label->SetToolTipText(FText::FromString(State.Error));
        if(auto* Detail=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("TransferTelemetry"))))
        {
            Detail->SetVisibility((Id==TEXT("weigher")||Id==TEXT("receiver"))?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
            Detail->SetText(FText::FromString(FormatTransfer(State.Transfer)));
            Detail->SetToolTipText(FText::FromString(FString::Printf(TEXT("시뮬레이션 계산값 · 모델 입자 기준 · kg 내부 단위\n이송 %lld · 연결 %lld · 개방 직전 작업자 제어 번호 %lld\n시작 해석 시간 %.6f초 · 관찰 %.6f초\n질량 갱신 UTC %s\n입자 중심은 한 구역에만 집계합니다. 도착량은 이번 ID의 현재 수취호퍼 체류량입니다.\n자동 차단·자동 이송·다음 투입은 실행하지 않습니다."),
                State.Transfer.Id,State.Transfer.Connection,State.Transfer.OpeningControlRevision,State.Transfer.StartedSimulationSeconds,State.Transfer.ObservedSimulationSeconds,*State.CurrentMass.UpdatedUtc.ToIso8601())));
        }
        HideUnavailableControls(W);
        if(CommandUI.IsValid())CommandUI->Refresh();
    }
}

void UProcessUIStateAdapter::Refresh()
{
    auto* W = Widget.Get(); if (!W) return;
    HideUnavailableControls(W);
    const FString Selected = ReadString(W, TEXT("SelectedEquipmentId"));
    const FString Hovered = ReadString(W, TEXT("HoverEquipmentId"));
    Push(Selected);
    if (Hovered != Selected) Push(Hovered);
    for (int32 Index = 0; Index < UE_ARRAY_COUNT(EquipmentIds); ++Index)
    {
        const auto State = ReadEquipment(EquipmentIds[Index]);
        const FString Button = FString::Printf(TEXT("Equipment%d"), Index), Label = Button + TEXT("Text");
        SetLabel(W, *Label, *TileText(State));
        FLinearColor Fill(.82f,.93f,.97f);
        if (State.Validity == EPlantValueValidity::Error) Fill = FLinearColor(1.f,.80f,.78f);
        else if (State.Validity == EPlantValueValidity::Stale || State.Validity == EPlantValueValidity::Initializing) Fill = FLinearColor(1.f,.92f,.70f);
        else if (State.Validity != EPlantValueValidity::Valid) Fill = FLinearColor(.86f,.89f,.91f);
        StyleButton(W, *Button, Fill, Selected == EquipmentIds[Index]);
    }
    const auto* ViewProperty = FindFProperty<FBoolProperty>(W->GetClass(), TEXT("Is3D"));
    const bool In3D = ViewProperty && ViewProperty->GetPropertyValue_InContainer(W);
    StyleButton(W, TEXT("View2D"), FLinearColor(.82f,.90f,.95f), !In3D);
    StyleButton(W, TEXT("View3D"), FLinearColor(.82f,.90f,.95f), In3D);
    StyleText(W, TEXT("View2DText"), 17, Ink); StyleText(W, TEXT("View3DText"), 17, Ink);
    if (auto* Is3D = FindFProperty<FBoolProperty>(W->GetClass(), TEXT("Is3D")); Is3D && Is3D->GetPropertyValue_InContainer(W))
    {
        FText Status, Quantity, Valve;
        Format(ReadEquipment(TEXT("weigher")), Status, Quantity, Valve);
        SetLabel(W, TEXT("ThreeDNotice"), *FString::Printf(TEXT("3D 관찰 · 조회 전용\n%s\n\n우클릭 드래그: 회전 · 우클릭 + WASD/QE: 이동\n가운데 버튼: 평행 이동 · 휠: 확대/축소\n밸브·혼합 명령은 아직 연결하지 않았습니다."), *Status.ToString()));
    }
    if(CommandUI.IsValid())CommandUI->Refresh();
}

bool UProcessUIStateSubsystem::DoesSupportWorldType(EWorldType::Type Type) const
{
    return Type == EWorldType::Game || Type == EWorldType::PIE;
}

void UProcessUIStateSubsystem::OnWorldBeginPlay(UWorld& World)
{
    Super::OnWorldBeginPlay(World);
    PendingSourcePath = ProcessPlantBinding::SourcePath(UWorld::RemovePIEPrefix(World.GetOutermost()->GetName()));
    PendingOpen = !PendingSourcePath.IsEmpty();
}

void UProcessUIStateSubsystem::Tick(float DeltaTime)
{
    Super::Tick(DeltaTime);
    if(ControlService)ControlService->Tick();
    if(ControlUI)ControlUI->Refresh();
    if (PendingOpen && GetWorld()->GetFirstPlayerController())
    {
        PendingOpen = false;
        // A specific saved object reference, never GetAllActorsOfClass/first-match selection.
        FSoftObjectPath Reference(PendingSourcePath);
        if (GetWorld()->WorldType == EWorldType::PIE) Reference.FixupForPIE(GetWorld()->GetOutermost()->GetPIEInstanceID());
        OpenForActor(Cast<APowderPlantTest>(Reference.ResolveObject()));
    }
    const double Now = FPlatformTime::Seconds();
    if (Adapter && Now - LastRefresh >= .1)
    {
        LastRefresh = Now;
        if (!StatusWidget || !StatusWidget->IsInViewport() || !Adapter->IsAttached()) Close();
        else Adapter->Refresh();
    }
}

TStatId UProcessUIStateSubsystem::GetStatId() const
{
    RETURN_QUICK_DECLARE_CYCLE_STAT(UProcessUIStateSubsystem, STATGROUP_Tickables);
}

bool UProcessUIStateSubsystem::OpenForActor(APowderPlantTest* Actor)
{
    if (Actor && Actor->GetWorld() != GetWorld()) return false;
    auto* PC = GetWorld()->GetFirstPlayerController();
    if (!PC) return false;
    Close();
    auto* Class = LoadClass<UUserWidget>(nullptr, TEXT("/Game/ProcessUIV2/Widgets/WBP_ProcessUIV2.WBP_ProcessUIV2_C"));
    if (!Class) return false;
    StatusWidget = CreateWidget<UUserWidget>(PC, Class);
    if (!StatusWidget) return false;
    Adapter = NewObject<UProcessUIStateAdapter>(this);
    StatusWidget->AddToViewport(100);
    if (!Adapter->Attach(StatusWidget, Actor))
    {
        UE_LOG(LogTemp, Error, TEXT("Process UI read-only interface mismatch")); Close(); return false;
    }
    if(!ControlService)ControlService=NewObject<UProcessControlService>(this);
    ControlService->Attach(Actor);
    ControlUI=NewObject<UProcessControlUI>(this);
    if(!ControlUI->Attach(StatusWidget,ControlService)){Close();return false;}
    Adapter->CommandUI=ControlUI;
    auto* Observation = FindFProperty<FMulticastDelegateProperty>(StatusWidget->GetClass(), TEXT("OnObservationViewRequested"));
    if (!Observation) { Close(); return false; }
    FScriptDelegate Handler; Handler.BindUFunction(this, TEXT("HandleObservationViewRequested"));
    Observation->AddDelegate(Handler, StatusWidget);
    HandleObservationViewRequested(false);
    UE_LOG(LogTemp, Display, TEXT("Process UI explicit source: %s"), Actor ? *Actor->GetPathName() : TEXT("missing explicitly bound plant actor"));
    return true;
}

void UProcessUIStateSubsystem::Close()
{
    if(ControlUI)ControlUI->Detach();
    ControlUI=nullptr;
    if (Adapter) Adapter->Detach();
    if (StatusWidget)
    {
        if (auto* Event = FindFProperty<FMulticastDelegateProperty>(StatusWidget->GetClass(), TEXT("OnObservationViewRequested")))
        {
            FScriptDelegate Handler; Handler.BindUFunction(this, TEXT("HandleObservationViewRequested"));
            Event->RemoveDelegate(Handler, StatusWidget);
        }
        if (auto* PC = StatusWidget->GetOwningPlayer())
        {
            if (auto* Camera = Cast<APowderCameraController>(PC)) Camera->CancelNavigation();
            FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false);
            Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
            PC->SetInputMode(Input); PC->bShowMouseCursor = true;
        }
        StatusWidget->RemoveFromParent();
    }
    Adapter = nullptr; StatusWidget = nullptr;
}

bool UProcessUIStateSubsystem::IsReadOnlyScreenActive() const
{
    return StatusWidget && StatusWidget->IsInViewport();
}

bool UProcessUIStateSubsystem::IsObservationViewActive() const
{
    const auto* Is3D = StatusWidget ? FindFProperty<FBoolProperty>(StatusWidget->GetClass(), TEXT("Is3D")) : nullptr;
    return IsReadOnlyScreenActive() && Is3D && Is3D->GetPropertyValue_InContainer(StatusWidget);
}

void UProcessUIStateSubsystem::HandleObservationViewRequested(bool ShowObservation)
{
    auto* PC = StatusWidget ? StatusWidget->GetOwningPlayer() : nullptr;
    if (!PC) return;
    if (auto* Camera = Cast<APowderCameraController>(PC)) Camera->CancelNavigation();
    if (ShowObservation)
    {
        FInputModeGameAndUI Input; Input.SetHideCursorDuringCapture(false);
        Input.SetLockMouseToViewportBehavior(EMouseLockMode::DoNotLock);
        PC->SetInputMode(Input);
        if (FSlateApplication::IsInitialized()) FSlateApplication::Get().SetAllUserFocusToGameViewport();
    }
    else
    {
        FInputModeUIOnly Input; Input.SetWidgetToFocus(StatusWidget->TakeWidget());
        PC->SetInputMode(Input);
    }
    PC->bShowMouseCursor = true;
    if (Adapter) Adapter->Refresh();
}

void UProcessUIStateSubsystem::Deinitialize()
{
    PendingOpen = false; Close();
    if(ControlService)ControlService->Detach();
    ControlService=nullptr;
    Super::Deinitialize();
}
