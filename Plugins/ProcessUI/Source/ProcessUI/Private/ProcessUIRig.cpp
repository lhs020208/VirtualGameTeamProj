#include "ProcessUIRig.h"
#include "ProcessDEMObserver.h"
#include "Camera/CameraActor.h"
#include "Camera/CameraComponent.h"
#include "Components/StaticMeshComponent.h"
#include "Components/TextRenderComponent.h"
#include "Engine/Engine.h"
#include "Engine/GameViewportClient.h"
#include "Engine/World.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "Components/DirectionalLightComponent.h"
#include "DrawDebugHelpers.h"
#include "Misc/Paths.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/FileHelper.h"
#include "UnrealClient.h"
#include "Widgets/SOverlay.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SEditableTextBox.h"

using namespace ProcessUI;
namespace
{
TSharedRef<SWidget> Label(const FString& Text) { return SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).Text(FText::FromString(Text)).AutoWrapText(true); }
FString Flag(const FReading& R,const TCHAR* Yes,const TCHAR* No) {return R.Valid()?(R.Value==1?Yes:No):Display(R);}
FColor MaterialColor(const FString& Material){const uint32 H=GetTypeHash(Material);return FColor(100+H%155,120+(H/155)%135,120+(H/20925)%135);}
TSharedRef<SWidget> Button(const FString& Text,TFunction<void()> Action)
{ return SNew(SButton).ContentPadding(FMargin(8,5)).OnClicked_Lambda([Action](){Action();return FReply::Handled();})[Label(Text)]; }
TSharedRef<SWidget> Field(const FString& Title,const FString& Value,TFunction<void(const FString&)> Set)
{
    return SNew(SHorizontalBox)
        +SHorizontalBox::Slot().FillWidth(0.43f).VAlign(VAlign_Center)[Label(Title)]
        +SHorizontalBox::Slot().FillWidth(0.57f)[SNew(SEditableTextBox).Text(FText::FromString(Value)).OnTextCommitted_Lambda([Set](const FText& T,ETextCommit::Type){Set(T.ToString());})];
}
FString Number(double V) { return FString::SanitizeFloat(V); }
double Parse(const FString& V) { if(!V.IsNumeric()) return -1; return FCString::Atod(*V); }
}
AProcessUIRig::AProcessUIRig()
{
    PrimaryActorTick.bCanEverTick=true;
    RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("ProcessRoot"));
}
AProcessUIRig::~AProcessUIRig() = default;
void AProcessUIRig::SetRecipe(const FRecipe& R)
{
    TestProvider.Reset();
    if(bObserveDEM) Controller=MakeShared<FController>(MakeShared<FDEMObserver>(DEMSource));
    else { TestProvider=MakeShared<FTestProvider>(R.Hoppers); Controller=MakeShared<FController>(TestProvider.ToSharedRef()); }
    Controller->Recipe=R;
    if(HasActorBegunPlay()) { BuildVisuals(); if(Body.IsValid()) RebuildBody(); }
}
void AProcessUIRig::BeginPlay()
{
    Super::BeginPlay();
    FRecipe R; R.Name=TEXT("새 배치");
    for(int32 I=1;I<=3;++I) { FHopper H; H.Id=FString::Printf(TEXT("supply-%d"),I); R.Hoppers.Add(H); }
    SetRecipe(R);
    bSmoke=FParse::Param(FCommandLine::Get(),TEXT("ProcessUISmoke"));
    if(bSmoke&&!bObserveDEM) {SetRecipe(DemoRecipe());Controller->Start();}
    if(!bObserveDEM)
    {
        Camera=GetWorld()->SpawnActor<ACameraActor>();
        Camera->GetCameraComponent()->SetFieldOfView(65);
        Focus(TEXT("all"));
        if(auto* PC=GetWorld()->GetFirstPlayerController()){PC->SetViewTarget(Camera);if(PC->GetPawn())PC->GetPawn()->SetActorHiddenInGame(true);}
    }
    if(bShowPanel&&GEngine&&GEngine->GameViewport)
    {
        Panel=BuildPanel(); GEngine->GameViewport->AddViewportWidgetContent(Panel.ToSharedRef(),20);
        if(auto* PC=GetWorld()->GetFirstPlayerController()) { PC->bShowMouseCursor=true; FInputModeGameAndUI Mode; Mode.SetHideCursorDuringCapture(false); PC->SetInputMode(Mode); }
    }
}
void AProcessUIRig::EndPlay(const EEndPlayReason::Type Reason)
{
    if(Panel.IsValid()&&GEngine&&GEngine->GameViewport) GEngine->GameViewport->RemoveViewportWidgetContent(Panel.ToSharedRef());
    Panel.Reset(); Body.Reset();Legend.Reset(); Super::EndPlay(Reason);
}
FString AProcessUIRig::Summary() const
{
    const auto& C=*Controller;
    FString Stage=C.Recipe.Steps.IsValidIndex(C.StepIndex)?C.Recipe.Steps[C.StepIndex].Name:TEXT("혼합/대기");
    const FStep* ReceiverStep=C.Recipe.Steps.FindByPredicate([&](const FStep& S){return S.Id==C.ReceiverOwner;});
    const FString Clock=C.Snapshot().CanAutomate?FString::Printf(TEXT("%.2f / %.2f s"),C.MixingSeconds,C.Recipe.MixSeconds):TEXT("미지원 (자동 공정 미연결)");
    return FString::Printf(TEXT("%s  |  단계 %d: %s\n대기 조건: %s\n혼합 경과 %s\nRPM 목표 %.3g / 적용 %s / 실제 %s\n수취호퍼 점유: %s\n%s\n%s\n%s"),*C.StatusText(),C.StepIndex+1,*Stage,*C.WaitingText(),*Clock,C.Recipe.RPM,*Display(C.Snapshot().AppliedRPM),*Display(C.Snapshot().ActualRPM),ReceiverStep?*ReceiverStep->Name:TEXT("없음"),*C.ExceptionReason,*C.Message,*(C.Snapshot().Error+TEXT("\n")+Notice));
}
FString AProcessUIRig::DeviceText() const
{
    FString Out;
    for(const auto& D:Controller->Snapshot().Devices)
    {
        Out+=FString::Printf(TEXT("%s\n질량 %s | 누적 입 %s / 출 %s\n밸브 %s | 정지 %s | 온도 %s\n입자 수 %s  %s %s\n"),*D.Label,*Display(D.Mass,bGrams?1000:1,bGrams?TEXT("g"):TEXT("kg")),*Display(D.TotalIn,bGrams?1000:1,bGrams?TEXT("g"):TEXT("kg")),*Display(D.TotalOut,bGrams?1000:1,bGrams?TEXT("g"):TEXT("kg")),*Flag(D.ValveClosed,TEXT("닫힘 확인"),TEXT("열림/닫힘 미충족")),*Flag(D.Stopped,TEXT("확인"),TEXT("미충족")),*Display(D.Temperature),*Display(D.ParticleCount),*D.Warning,*D.Error);
        Out+=TEXT("장비 내 원료: ");if(!D.MaterialsValid)Out+=TEXT("미지원");else if(D.MaterialKg.IsEmpty())Out+=TEXT("없음 (테스트 확인)");else for(const auto& M:D.MaterialKg)if(M.Value>1e-8)Out+=FString::Printf(TEXT("%s %.4g %s  "),*M.Key,M.Value*(bGrams?1000:1),bGrams?TEXT("g"):TEXT("kg"));
        if(const FHopper* H=Controller->Recipe.Hoppers.FindByPredicate([&](const FHopper& X){return X.Id==D.Id;}))
        { Out+=FString::Printf(TEXT("\n공급속도 목표 %.4g / 적용 %s / 실제 %s\n입자 지름 목표 %.4g / 적용 %s / 실제 미지원"),H->RateKgS*(bGrams?1000:1),*Display(D.AppliedRate,bGrams?1000:1,bGrams?TEXT("g/s"):TEXT("kg/s")),*Display(D.ActualRate,bGrams?1000:1,bGrams?TEXT("g/s"):TEXT("kg/s")),H->DiameterM*(bMicrometers?1e6:1e3),*Display(D.AppliedDiameter,bMicrometers?1e6:1e3,bMicrometers?TEXT("µm"):TEXT("mm"))); }
        Out+=TEXT("\n\n");
    }
    return Out;
}
TSharedRef<SWidget> AProcessUIRig::BuildPanel()
{
    auto Tabs=SNew(SHorizontalBox);
    Tabs->AddSlot()[Button(TEXT("운전·상태"),[this](){Tab=0;RebuildBody();})];
    Tabs->AddSlot()[Button(TEXT("설정·시퀀스"),[this](){Tab=1;RebuildBody();})];
    Tabs->AddSlot()[Button(TEXT("장비·수동"),[this](){Tab=2;if(Controller->Editable()){const auto R=Controller->Recipe;SetRecipe(R);}else RebuildBody();})];
    TSharedRef<SWidget> Result=SNew(SOverlay)
    +SOverlay::Slot().HAlign(HAlign_Left).VAlign(VAlign_Fill).Padding(12)
    [SNew(SBox).WidthOverride(600)
      [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.015f,0.025f,0.04f,1.0f)).Padding(14)
        [SNew(SVerticalBox)
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(STextBlock).Text(FText::FromString(TEXT("공정 디지털트윈 | PC 제어"))).Font(FCoreStyle::GetDefaultFontStyle("Bold",20))]
          +SVerticalBox::Slot().AutoHeight().Padding(0,0,0,8)[SNew(STextBlock).Text_Lambda([this](){return FText::FromString(Controller->Snapshot().Source);}).ColorAndOpacity(FLinearColor(1,0.7f,0.2f)).AutoWrapText(true)]
          +SVerticalBox::Slot().AutoHeight()[Tabs]
          +SVerticalBox::Slot().FillHeight(1).Padding(0,10)[SNew(SScrollBox)+SScrollBox::Slot()[SAssignNew(Body,SVerticalBox)]]
        ]]]
    +SOverlay::Slot().HAlign(HAlign_Right).VAlign(VAlign_Top).Padding(620,12,12,12)
      [SNew(SBorder).BorderImage(FCoreStyle::Get().GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(0.015f,0.025f,0.04f,1.0f)).Padding(10)[SAssignNew(Legend,SVerticalBox)]];
    RebuildBody(); return Result;
}
void AProcessUIRig::RebuildBody()
{
    if(!Body.IsValid()) return;
    if(Legend.IsValid())
    {
        Legend->ClearChildren();Legend->AddSlot().AutoHeight()[Label(bObserveDEM?TEXT("DEM 관찰 · 읽기 전용"):TEXT("3D 공정 관찰 · 테스트 모드\n상징적 입자 이동 / DEM·CFD 아님"))];
        Legend->AddSlot().AutoHeight()[Label(TEXT("공급 → 칭량 → 수취 → 혼합\n장비·수동 탭에서 카메라 이동"))];
        if(!bObserveDEM){TSet<FString> Shown;for(const auto& H:Controller->Recipe.Hoppers)if(!H.Material.IsEmpty()&&!Shown.Contains(H.Material)){Shown.Add(H.Material);Legend->AddSlot().AutoHeight().Padding(0,3)[SNew(STextBlock).Text(FText::FromString(TEXT("● ")+H.Material)).ColorAndOpacity(FLinearColor(MaterialColor(H.Material)))];}}
    }
    Body->ClearChildren();
    auto Add=[this](TSharedRef<SWidget> W){Body->AddSlot().AutoHeight().Padding(0,3)[W];};
    auto& C=*Controller;
    if(Tab==0)
    {
        Add(SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",14)).Text_Lambda([this](){return FText::FromString(Summary());}).AutoWrapText(true));
        auto Row=SNew(SHorizontalBox);
        Row->AddSlot()[Button(TEXT("시작"),[this](){if(Controller->Editable()){const auto R=Controller->Recipe;SetRecipe(R);Controller->Start();}})];
        Row->AddSlot()[Button(TEXT("일시정지"),[this](){if(Controller->Run==ERun::Running)Controller->Pause(TEXT("사용자 일시정지"));})];
        Row->AddSlot()[Button(TEXT("재개"),[this](){Controller->Resume();})]; Add(Row);
        auto Stop=SNew(SHorizontalBox);
        Stop->AddSlot()[Button(TEXT("정지 · 배치 중단"),[this](){Controller->Abort();})];
        Stop->AddSlot()[Button(TEXT("초기화"),[this](){Controller->Reset();})]; Add(Stop);
        Add(Label(TEXT("정지는 배치를 중단합니다. 초기화는 실제 정지·닫힘 확인 후 가능합니다. 목표값은 실제값과 구분됩니다.")));
        Add(SNew(STextBlock).Text_Lambda([this](){return FText::FromString(Controller->ConfirmedSafe()?TEXT("실제 전체 정지·닫힘: 확인됨"):TEXT("실제 전체 정지·닫힘: 미확인 또는 미충족 — 장비 탭 확인"));}).AutoWrapText(true));
        Add(Label(TEXT("단계별 목표 / 적용 목표 / 공급원 칭량 누적량")));
        for(const auto& S:C.Recipe.Steps) if(S.Kind==EStepKind::WeighTransfer)
        {
            Add(SNew(STextBlock).Text_Lambda([this,S](){const auto& Snap=Controller->Snapshot();const double* V=Snap.Weighed.Find(S.Id);FReading Applied;Applied.Validity=EValidity::Missing;for(const auto& R:Snap.Receipts)if(R.Step==S.Id&&R.AppliedTarget.Valid())Applied=R.AppliedTarget;return FText::FromString(FString::Printf(TEXT("%s: 목표 %.4g %s / 적용 %s / 칭량 %s"),*S.Name,S.TargetKg*(bGrams?1000:1),bGrams?TEXT("g"):TEXT("kg"),*Display(Applied,bGrams?1000:1,bGrams?TEXT("g"):TEXT("kg")),Snap.LedgerValid?*FString::Printf(TEXT("%.4g %s"),(V?*V:0)*(bGrams?1000:1),bGrams?TEXT("g"):TEXT("kg")):TEXT("데이터 없음")));}).AutoWrapText(true));
        }
        Add(Label(TEXT("최근 명령: 접수/적용은 실제 정지·닫힘 완료와 다릅니다.")));
        Add(SNew(STextBlock).Text_Lambda([this](){FString S;const auto& Rs=Controller->Snapshot().Receipts;for(int32 I=FMath::Max(0,Rs.Num()-6);I<Rs.Num();++I){const auto& R=Rs[I];S+=FString::Printf(TEXT("#%llu %s: %s\n"),R.Request,*R.Device,*R.Detail);}return FText::FromString(S);}).AutoWrapText(true));
        if(TestProvider.IsValid())
        {
            Add(Label(TEXT("테스트 전용 예외 주입 — 실제 장비 기능 아님")));
            Add(Button(TEXT("준비 지연 0.02배 / 정상 복원"),[this](){TestProvider->PreparationSpeed=TestProvider->PreparationSpeed==1?0.02:1;Notice=FString::Printf(TEXT("테스트 준비 속도 %.2f배"),TestProvider->PreparationSpeed);}));
            Add(Button(TEXT("잘못된 원료 유입 확인 주입"),[this](){TestProvider->InjectWrongMaterial();}));
        }
    }
    else if(Tab==1)
    {
        if(!C.Editable()) { Add(Label(TEXT("운전 중 설정 편집은 잠겨 있습니다. 완료량 보호를 위해 초기화 후 편집하세요."))); return; }
        if(!bObserveDEM)Add(Button(TEXT("테스트 예제 채우기 (공정 사양 아님)"),[this](){SetRecipe(DemoRecipe());}));
        Add(Field(TEXT("배치 이름"),C.Recipe.Name,[this](const FString& V){Controller->Recipe.Name=V;}));
        Add(Field(TEXT("목표 RPM"),Number(C.Recipe.RPM),[this](const FString& V){Controller->Recipe.RPM=Parse(V);}));
        Add(Field(TEXT("혼합 시간 [s]"),Number(C.Recipe.MixSeconds),[this](const FString& V){Controller->Recipe.MixSeconds=Parse(V);}));
        Add(Button(bGrams?TEXT("질량 표시 g → kg"):TEXT("질량 표시 kg → g"),[this](){bGrams=!bGrams;RebuildBody();}));
        Add(Button(bMicrometers?TEXT("입자 지름 µm → mm"):TEXT("입자 지름 mm → µm"),[this](){bMicrometers=!bMicrometers;RebuildBody();}));
        Add(Label(TEXT("입자 크기는 지름입니다. 0.1 mm = 100 µm. DEM 반지름 변환은 어댑터에서만 수행합니다.")));
        for(int32 I=0;I<C.Recipe.Hoppers.Num();++I)
        {
            const auto& H=C.Recipe.Hoppers[I];
            Add(Label(TEXT("── 공급 호퍼 ")+H.Id));
            Add(Field(TEXT("물질"),H.Material,[this,I](const FString& V){Controller->Recipe.Hoppers[I].Material=V;}));
            Add(Button(H.Additive?TEXT("역할: 첨가제 (전환)"):TEXT("역할: 원료 (전환)"),[this,I](){auto& X=Controller->Recipe.Hoppers[I];X.Additive=!X.Additive;RebuildBody();}));
            Add(Field(bMicrometers?TEXT("입자 지름 [µm]"):TEXT("입자 지름 [mm]"),Number(H.DiameterM*(bMicrometers?1e6:1e3)),[this,I](const FString& V){Controller->Recipe.Hoppers[I].DiameterM=Parse(V)/(bMicrometers?1e6:1e3);}));
            Add(Field(bGrams?TEXT("공급 속도 [g/s]"):TEXT("공급 속도 [kg/s]"),Number(H.RateKgS*(bGrams?1000:1)),[this,I](const FString& V){Controller->Recipe.Hoppers[I].RateKgS=Parse(V)/(bGrams?1000:1);}));
            Add(Button(TEXT("이 호퍼 삭제"),[this,I](){Controller->Recipe.Hoppers.RemoveAt(I);RebuildBody();}));
        }
        Add(Button(TEXT("+ 공급 호퍼 추가"),[this](){FHopper H;H.Id=TEXT("supply-")+FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);Controller->Recipe.Hoppers.Add(H);RebuildBody();}));
        Add(Label(TEXT("── 시퀀스: 칭량·수취 이송을 쌓고 ‘믹서 투입’ 단계로 구분")));
        for(int32 I=0;I<C.Recipe.Steps.Num();++I)
        {
            const auto& S=C.Recipe.Steps[I];
            Add(Field(FString::Printf(TEXT("%d 단계 이름"),I+1),S.Name,[this,I](const FString& V){Controller->Recipe.Steps[I].Name=V;}));
            if(S.Kind==EStepKind::WeighTransfer)
            {
                Add(Field(TEXT("공급 호퍼 ID"),S.HopperId,[this,I](const FString& V){Controller->Recipe.Steps[I].HopperId=V;}));
                Add(Field(bGrams?TEXT("목표 투입량 [g]"):TEXT("목표 투입량 [kg]"),Number(S.TargetKg*(bGrams?1000:1)),[this,I](const FString& V){Controller->Recipe.Steps[I].TargetKg=Parse(V)/(bGrams?1000:1);}));
            }
            else
            {
                Add(Button(S.Timing==ETiming::Initial?TEXT("예약 OFF / 초기 투입 (전환)"):TEXT("예약 ON / 혼합 후 N초 (전환)"),[this,I](){auto& X=Controller->Recipe.Steps[I];X.Timing=X.Timing==ETiming::Initial?ETiming::AfterMixing:ETiming::Initial;RebuildBody();}));
                if(S.Timing==ETiming::AfterMixing) Add(Field(TEXT("믹서 투입 시작 [s]"),Number(S.AtSeconds),[this,I](const FString& V){Controller->Recipe.Steps[I].AtSeconds=Parse(V);}));
            }
            auto Row=SNew(SHorizontalBox);
            Row->AddSlot()[Button(TEXT("↑"),[this,I](){if(I>0)Controller->Recipe.Steps.Swap(I,I-1);RebuildBody();})];
            Row->AddSlot()[Button(TEXT("↓"),[this,I](){if(I+1<Controller->Recipe.Steps.Num())Controller->Recipe.Steps.Swap(I,I+1);RebuildBody();})];
            Row->AddSlot()[Button(TEXT("삭제"),[this,I](){Controller->Recipe.Steps.RemoveAt(I);RebuildBody();})]; Add(Row);
        }
        Add(Button(TEXT("+ 개별 칭량 → 수취 이송"),[this](){FStep S;S.Name=TEXT("개별 칭량·수취 이송");if(!Controller->Recipe.Hoppers.IsEmpty())S.HopperId=Controller->Recipe.Hoppers[0].Id;Controller->Recipe.Steps.Add(S);RebuildBody();}));
        Add(Button(TEXT("+ 믹서 투입"),[this](){FStep S;S.Kind=EStepKind::MixerCharge;S.Name=TEXT("믹서 투입");Controller->Recipe.Steps.Add(S);RebuildBody();}));
        Add(Button(TEXT("실행 전 검증"),[this](){const auto E=Controller->Validate(Controller->Recipe);Notice=E.IsEmpty()?TEXT("테스트 공급원 조건으로 검증 통과"):FString::Join(E,TEXT("\n"));RebuildBody();}));
        Add(Label(Notice));
        Add(Field(TEXT("저장/불러올 이름"),SaveName,[this](const FString& V){SaveName=V;}));
        Add(Button(TEXT("이름으로 저장 (동일 이름 덮어쓰기)"),[this](){auto R=Controller->Recipe;R.Name=SaveName;const auto E=Controller->Validate(R);if(E.IsEmpty())SaveRecipe(R,FPaths::ProjectSavedDir()/TEXT("ProcessUI/Recipes"),Notice);else Notice=FString::Join(E,TEXT("\n"));RebuildBody();}));
        Add(Button(TEXT("이름으로 불러오기 · 자동 실행 안 함"),[this](){FRecipe R;if(LoadRecipe(SaveName,FPaths::ProjectSavedDir()/TEXT("ProcessUI/Recipes"),R,Notice)){const auto E=Controller->Validate(R);if(E.IsEmpty()){SetRecipe(R);Notice=TEXT("불러오기·검증 완료. 편집 상태입니다.");}else Notice=FString::Join(E,TEXT("\n"));}RebuildBody();}));
    }
    else
    {
        Add(Button(TEXT("전체 카메라"),[this](){Focus(TEXT("all"));}));
        Add(SNew(STextBlock).Font(FCoreStyle::GetDefaultFontStyle("Regular",13)).Text_Lambda([this](){return FText::FromString(DeviceText());}).AutoWrapText(true));
        Add(SNew(STextBlock).Text_Lambda([this](){return FText::FromString(Notice);}).AutoWrapText(true));
        for(const auto& D:C.Snapshot().Devices)
        {
            Add(Button(D.Label+TEXT(" · 카메라 이동"),[this,Id=D.Id](){Focus(Id);}));
            if(!D.ValveId.IsEmpty())
            {
                auto Row=SNew(SHorizontalBox);
                Row->AddSlot()[Button(D.ValveId+TEXT(" 열기"),[this,Id=D.ValveId](){Controller->ManualValve(Id,true);})];
                Row->AddSlot()[Button(TEXT("닫기 · 정지 확인 전 가능"),[this,Id=D.ValveId](){Controller->ManualValve(Id,false);})]; Add(Row);
            }
        }
        if(TestProvider.IsValid()) Add(Button(TEXT("수취 밸브 닫힘 실패 주입 / 해제"),[this](){TestProvider->FailCloseValve=TestProvider->FailCloseValve.IsEmpty()?TEXT("receiver.outlet"):TEXT("");}));
    }
}
void AProcessUIRig::BuildVisuals()
{
    for(auto C:Visuals) if(C)C->DestroyComponent(); Visuals.Empty(); Positions.Empty();
    if(bObserveDEM){for(const auto& P:EquipmentFocusTargets)if(IsValid(P.Value))Positions.Add(P.Key.ToString(),P.Value->GetActorLocation());return;}
    auto* Fill=NewObject<UDirectionalLightComponent>(this);Fill->SetupAttachment(RootComponent);Fill->SetRelativeRotation(FRotator(-30,130,0));Fill->SetIntensity(2);Fill->SetForwardShadingPriority(1);Fill->RegisterComponent();Visuals.Add(Fill);
    UStaticMesh* Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Cylinder.Cylinder"));
    int32 N=Controller->Recipe.Hoppers.Num();
    for(int32 I=0;I<N;++I) Positions.Add(Controller->Recipe.Hoppers[I].Id,FVector(-260,I*150-(N-1)*75,260));
    Positions.Add(TEXT("weigher"),FVector(0,0,180)); Positions.Add(TEXT("receiver"),FVector(260,0,240)); Positions.Add(TEXT("mixer"),FVector(480,0,90));
    for(const auto& P:Positions)
    {
        auto* M=NewObject<UStaticMeshComponent>(this);M->SetupAttachment(RootComponent);M->SetStaticMesh(Mesh);M->SetCollisionEnabled(ECollisionEnabled::NoCollision);M->SetRelativeLocation(P.Value);M->SetRelativeScale3D(FVector(.8,.8,.9));M->RegisterComponent();Visuals.Add(M);
        // Built-in distance-field world font supports ASCII IDs. Korean labels use Slate's composite font.
        const FString Name=P.Key;
        auto* T=NewObject<UTextRenderComponent>(this);T->SetupAttachment(RootComponent);T->SetText(FText::FromString(Name));T->SetRelativeLocation(P.Value+FVector(0,0,70));T->SetRelativeRotation(FRotator(0,-90,0));T->SetWorldSize(18);T->SetTextRenderColor(FColor::Cyan);T->RegisterComponent();Visuals.Add(T);
    }
}
void AProcessUIRig::Focus(const FString& Id)
{
    if(bObserveDEM){Positions.Empty();for(const auto& P:EquipmentFocusTargets)if(IsValid(P.Value))Positions.Add(P.Key.ToString(),P.Value->GetActorLocation());}
    if(Positions.IsEmpty()||(Id!=TEXT("all")&&!Positions.Contains(Id))){Notice=TEXT("해당 장비의 카메라 대상 연결이 없습니다.");return;}
    if(!Camera){Camera=GetWorld()->SpawnActor<ACameraActor>();if(auto* PC=GetWorld()->GetFirstPlayerController())PC->SetViewTarget(Camera);}
    FVector Target=Id==TEXT("all")?FVector(80,0,170):Positions.FindRef(Id);
    if(bObserveDEM&&Id==TEXT("all")){Target=FVector::ZeroVector;for(const auto& P:Positions)Target+=P.Value;Target/=Positions.Num();}
    FVector Offset=Id==TEXT("all")?FVector(800,-1100,650):FVector(220,-350,180);
    Camera->SetActorLocation(Target+Offset);
    const FRotator View=(Target-Camera->GetActorLocation()).Rotation();
    const FVector Right=FRotationMatrix(View).GetUnitAxis(EAxis::Y);
    Camera->SetActorRotation((Target-Right*(Id==TEXT("all")?350:110)-Camera->GetActorLocation()).Rotation());
}
void AProcessUIRig::Tick(float Dt)
{
    Super::Tick(Dt); if(!Controller.IsValid())return;Controller->Tick(Dt);VisualTime+=Dt;
    if(bSmoke)
    {
        if(VisualTime>5&&!bSmokeShot){bSmokeShot=true;FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProcessUI/Preview.png"),true,false);}
        if(VisualTime>6&&SmokeStage==0){Controller->Pause(TEXT("화면 검증: 일시정지"));SmokeStage=1;}
        if(VisualTime>7&&SmokeStage==1){if(!Controller->Resume()){FFileHelper::SaveStringToFile(TEXT("FAIL smoke resume"),*(FPaths::ProjectSavedDir()/TEXT("ProcessUI/PreviewResult.txt")));FPlatformMisc::RequestExitWithStatus(false,1);}SmokeStage=2;}
        if(VisualTime>8&&SmokeStage==2){Tab=2;RebuildBody();Focus(TEXT("weigher"));SmokeStage=3;}
        if(VisualTime>9&&SmokeStage==3){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProcessUI/PreviewDevices.png"),true,false);SmokeStage=4;}
        if(VisualTime>10&&SmokeStage==4){Controller->Abort();SmokeStage=5;}
        if(VisualTime>10.5&&SmokeStage==5){Controller->Reset();SmokeStage=6;}
        if(VisualTime>11&&SmokeStage==6){Tab=1;RebuildBody();Focus(TEXT("all"));SmokeStage=7;}
        if(VisualTime>12&&SmokeStage==7){FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("ProcessUI/PreviewEditor.png"),true,false);SmokeStage=8;}
        if(VisualTime>13){const bool Ok=Controller->Editable()&&Panel.IsValid();FFileHelper::SaveStringToFile(Ok?TEXT("PASS UI render, pause/resume, device camera, reset, editor panel"):TEXT("FAIL UI smoke"),*(FPaths::ProjectSavedDir()/TEXT("ProcessUI/PreviewResult.txt")));FPlatformMisc::RequestExitWithStatus(false,Ok?0:1);}
    }
    if(bObserveDEM) return;
    for(const auto& H:Controller->Recipe.Hoppers) if(Positions.Contains(H.Id))DrawDebugLine(GetWorld(),Positions[H.Id],Positions[TEXT("weigher")],FColor(60,120,160),false,-1,0,3);
    DrawDebugLine(GetWorld(),Positions.FindRef(TEXT("weigher")),Positions.FindRef(TEXT("receiver")),FColor(60,120,160),false,-1,0,3);
    DrawDebugLine(GetWorld(),Positions.FindRef(TEXT("receiver")),Positions.FindRef(TEXT("mixer")),FColor(60,120,160),false,-1,0,3);
    const auto& S=Controller->Snapshot();
    for(const auto& D:S.Devices) if(D.MaterialsValid&&Positions.Contains(D.Id))
        for(const auto& Material:D.MaterialKg)for(int32 I=0;I<FMath::Min(100,int32(Material.Value*1600));++I)
        { const uint32 Hash=GetTypeHash(Material.Key);const double A=I*2.399+(Hash%360)+(D.Id==TEXT("mixer")&&S.ActualRPM.Valid()?VisualTime*S.ActualRPM.Value/60:0);const FVector V=Positions[D.Id]+FVector(FMath::Cos(A)*55,FMath::Sin(A)*55,(I%8)*5-20);DrawDebugPoint(GetWorld(),V,4,MaterialColor(Material.Key),false,-1,1); }
    if(Controller->Recipe.Steps.IsValidIndex(Controller->StepIndex))
    {
        FString From,To; const auto Op=Controller->Operation;
        if(Op==EOperation::Feed){From=Controller->Recipe.Steps[Controller->StepIndex].HopperId;To=TEXT("weigher");}
        if(Op==EOperation::Transfer){From=TEXT("weigher");To=TEXT("receiver");}
        if(Op==EOperation::Charge){From=TEXT("receiver");To=TEXT("mixer");}
        const FDevice* Moving=S.Devices.FindByPredicate([&](const FDevice& D){return D.Id==From;});
        if(Moving&&Moving->ActualRate.Valid()&&Moving->ActualRate.Value>0&&Positions.Contains(From)&&Positions.Contains(To))for(int32 I=0;I<18;++I)DrawDebugPoint(GetWorld(),FMath::Lerp(Positions[From],Positions[To],FMath::Frac(VisualTime*.7+I/18.0)),7,FColor::Orange,false,-1,1);
    }
}
