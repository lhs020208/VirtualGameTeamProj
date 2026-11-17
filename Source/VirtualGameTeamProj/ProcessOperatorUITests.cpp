#include "ProcessDisplay.h"
#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessUIStateAdapter.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/EditableTextBox.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "Widgets/SWidget.h"
#include <limits>

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessDisplayNumbers,"ProcessUI.Usability.DisplayAndExactNumbers",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessDisplayNumbers::RunTest(const FString&)
{
 TestEqual(TEXT("integer zeros removed"),ProcessDisplay::Number(2),FString(TEXT("2")));
 TestEqual(TEXT("pressure trailing zeros removed"),ProcessDisplay::Number(.5),FString(TEXT("0.5")));
 TestEqual(TEXT("two display decimals"),ProcessDisplay::Mass(.039971117),FString(TEXT("39.97 g")));
 TestEqual(TEXT("small nonzero is not zero"),ProcessDisplay::Mass(.0000001),FString(TEXT("<0.01 g")));
 TestEqual(TEXT("valid zero"),ProcessDisplay::Mass(0),FString(TEXT("0 g")));
 TestEqual(TEXT("unknown never zero"),ProcessDisplay::Mass(0,false),FString(TEXT("미확인")));
 TestTrue(TEXT("one residual particle retained"),ProcessDisplay::Mass(.0000001,true,1).Contains(TEXT("1개 남음")));
 for(double V:{.03997111712345679,.000000000000123456789,15.123456789012345,.5,0.,-1.}){double Parsed=0;TestTrue(TEXT("exact editor parses"),ProcessDisplay::Parse(ProcessDisplay::Exact(V),Parsed));TestEqual(TEXT("round-trip without display rounding"),Parsed,V);}
 double Value=42;for(const TCHAR* Bad:{TEXT("nan"),TEXT("inf"),TEXT("3 g"),TEXT(""),TEXT("2.4.5")}){TestFalse(TEXT("strict numeric input"),ProcessDisplay::Parse(Bad,Value));TestEqual(TEXT("failure preserves previous value"),Value,42.);}
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessOperatorEditing,"ProcessUI.Usability.RealWidgetDraftAndCompletion",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessOperatorEditing::RunTest(const FString&)
{
 UWorld::InitializationValues IV;IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);if(!TestNotNull(TEXT("world"),World))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);World->InitializeActorsForPlay(FURL());
 auto* Player=NewObject<ULocalPlayer>(GEngine);auto* PC=World->SpawnActor<APlayerController>();PC->SetPlayer(Player);
 auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/ProcessUIV2/Widgets/WBP_ProcessUIV2.WBP_ProcessUIV2_C"));auto* W=Class?CreateWidget<UUserWidget>(PC,Class):nullptr;
 if(!TestNotNull(TEXT("existing widget"),W)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* Adapter=NewObject<UProcessUIStateAdapter>(World);auto* Service=NewObject<UProcessControlService>(World);auto* UI=NewObject<UProcessControlUI>(World);
 auto& M=Service->State;auto& S=M.Snapshot;M.Connected=true;M.Connection=1;M.ManualGranted=true;S.Epoch=2;S.Validity=EPlantValueValidity::Valid;S.PublishedWallSeconds=FPlatformTime::Seconds();
 S.SourceActorPath=TEXT("/Game/ProcessPlant_20261111_LowPoly/L_ProcessPlant_LowPoly_1111.L_ProcessPlant_LowPoly_1111:PersistentLevel.PowderPlantTest_0");S.SupplyABatch.Supported=true;S.AppliedRPMValid=true;S.AppliedRPM=0;
 FProcessRecipe R;R.Name=TEXT("정밀도 시험");R.Mode=EProcessBatchMode::SequentialAB;R.TargetAKg=.03997111712345679;R.ToleranceAKg=.0012345678901234567;R.TargetBKg=.040123456789012345;R.ToleranceBKg=.0019876543210987654;R.AdditiveEnabled=true;R.TargetCKg=.019123456789012345;R.ToleranceCKg=.001111111111111111;R.Scheduled=true;R.DelaySeconds=.12345678901234567;R.TargetRPM=15.123456789012345;R.MixingSeconds=6.987654321098765;
 FString Why;TestTrue(TEXT("configuration only"),Service->ApplyRecipe(R,Why));
 TestTrue(TEXT("adapter attach"),Adapter->Attach(W,nullptr));TestTrue(TEXT("control attach"),UI->Attach(W,Service));Adapter->CommandUI=UI;
 auto Input=[&](const TCHAR* N){return CastChecked<UEditableTextBox>(W->GetWidgetFromName(N));};
 auto Text=[&](const TCHAR* N){return CastChecked<UTextBlock>(W->GetWidgetFromName(N))->GetText().ToString();};
 TestNotNull(TEXT("operator layout created"),W->GetWidgetFromName(TEXT("OperatorSummary")));
 auto Slate=W->TakeWidget();UI->ToggleRecipes();W->ForceLayoutPrepass();
 UI->CopyAppliedRecipe();const FString Initial=Input(TEXT("RecipeTargetA"))->GetText().ToString();
 Input(TEXT("RecipeMaterialA"))->SetText(FText::FromString(TEXT("이름만 수정")));UI->ApplyRecipeDraft();
 const auto Applied=Service->GetAppliedRecipe();auto Expected=R;Expected.MaterialA=TEXT("이름만 수정");TestTrue(TEXT("unmodified native recipe numbers remain exact"),ProcessRecipe::Same(Applied,Expected));
 const int32 Commands=M.History.Num();
 Input(TEXT("RecipeTargetA"))->SetText(FText::FromString(TEXT("39.97111712345678")));
 for(int I=0;I<20;++I){++S.Sequence;S.PublishedWallSeconds=FPlatformTime::Seconds();UI->Refresh();}
 TestEqual(TEXT("snapshot refresh never replaces typed text"),Input(TEXT("RecipeTargetA"))->GetText().ToString(),FString(TEXT("39.97111712345678")));
 auto* Plus=CastChecked<UButton>(W->GetWidgetFromName(TEXT("RecipeTargetAPlus")));Plus->OnClicked.Broadcast();double Value=0;ProcessDisplay::Parse(Input(TEXT("RecipeTargetA"))->GetText().ToString(),Value);TestEqual(TEXT("gram step preserves fractional draft"),Value,39.97111712345678+1.);
 Input(TEXT("RecipeTargetA"))->OnTextCommitted.Broadcast(Input(TEXT("RecipeTargetA"))->GetText(),ETextCommit::OnEnter);
 TestEqual(TEXT("step/Enter/refresh issue no process command"),M.History.Num(),Commands);TestTrue(TEXT("draft has not applied"),ProcessRecipe::Same(Service->GetAppliedRecipe(),Applied));
 TestTrue(TEXT("dirty state beside input"),Text(TEXT("RecipeTargetAFeedback")).Contains(TEXT("미적용")));
 Input(TEXT("RecipeTargetA"))->SetText(FText::FromString(TEXT("nan")));UI->Refresh();TestTrue(TEXT("invalid reason beside input"),Text(TEXT("RecipeTargetAFeedback")).Contains(TEXT("유한한 숫자")));UI->ApplyRecipeDraft();TestTrue(TEXT("invalid apply preserves complete recipe"),ProcessRecipe::Same(Service->GetAppliedRecipe(),Applied));
 Input(TEXT("RecipeDelay"))->SetText(FText::FromString(TEXT("0.2")));CastChecked<UButton>(W->GetWidgetFromName(TEXT("RecipeDelayPlus")))->OnClicked.Broadcast();TestEqual(TEXT("decimal step does not expose binary rounding tail"),Input(TEXT("RecipeDelay"))->GetText().ToString(),FString(TEXT("0.3")));
 Input(TEXT("RecipeTargetA"))->SetText(FText::FromString(TEXT("125000000000000000000")));Plus->OnClicked.Broadcast();double Large=0;ProcessDisplay::Parse(Input(TEXT("RecipeTargetA"))->GetText().ToString(),Large);TestEqual(TEXT("large exponent zero is never trimmed by decimal step"),Large,1.25e20);
 TestFalse(TEXT("Start cannot steal Enter focus"),CastChecked<UButton>(W->GetWidgetFromName(TEXT("AutoStartA")))->GetIsFocusable());
 S.PipeMass.Validity=EPlantValueValidity::Valid;S.PipeMass.Kg=.0000001;UI->Refresh();TestTrue(TEXT("tiny residue visible in main screen"),Text(TEXT("OperatorSummary")).Contains(TEXT("<0.01 g")));
 auto& A=Service->AutoRun.State;A.RunId=FGuid::NewGuid();A.Epoch=S.Epoch;A.Connection=M.Connection;A.Stage=EProcessAutoStage::Complete;A.Active=false;A.BatchMode=EProcessBatchMode::SequentialAB;A.AdditiveEnabled=true;A.FinalAKg=.039971117;A.FinalBKg=.040051542;A.FinalAdditiveKg=.019864921;
 UI->Refresh();TestTrue(TEXT("compact final arrival"),Text(TEXT("OperatorSummary")).Contains(TEXT("39.97 g")));TestTrue(TEXT("raw final values available in details"),Text(TEXT("TransferTelemetry")).Contains(TEXT("39.971117 g")));TestFalse(TEXT("main screen omits old transfer diagnosis"),Text(TEXT("OperatorSummary")).Contains(TEXT("판정 불가")));
 TestTrue(TEXT("completed batch retains actual disabled new-start reason beside button"),Text(TEXT("OperatorActionHint")).Contains(Service->AutoRun.CanStart(M,FPlatformTime::Seconds())));

 // Operator guidance consumes the same live admission predicates; no device writes on query/navigation.
 A=FProcessAutoSnapshot();Service->RecipeRevision=0;S.GPUDevice=TEXT("test GPU");S.SupplyAPurge.Supported=S.SupplyAPurge.Valid=true;S.FeedPreparation.Enabled=true;S.MixerFeed.Valid=S.MixerFeed.Supported=true;
 Service->AutoRun.Settings=Service->AutoSettings=FProcessAutoSettings();UI->Refresh();
 TestFalse(TEXT("unconfigured start disabled"),CastChecked<UButton>(W->GetWidgetFromName(TEXT("AutoStartA")))->GetIsEnabled());
 TestTrue(TEXT("actual RPM blocker beside disabled start"),Text(TEXT("OperatorActionHint")).Contains(TEXT("RPM")));
 TestTrue(TEXT("recipe not applied explicitly stated"),Text(TEXT("OperatorActionHint")).Contains(TEXT("레시피 미적용")));
 UI->OpenRecipeSettings();TestTrue(TEXT("applied values visible outside scrolling editor"),Text(TEXT("OperatorAppliedRecipe")).Contains(TEXT("현재 개별 적용값")));
 Service->ApplyRecipe(R,Why);UI->Refresh();TestTrue(TEXT("applied recipe separate from remaining blocker"),Text(TEXT("OperatorActionHint")).Contains(TEXT("레시피 적용됨")));
 TestFalse(TEXT("recipe apply is not process readiness"),CastChecked<UButton>(W->GetWidgetFromName(TEXT("AutoStartA")))->GetIsEnabled());
 UI->CloseRecipes();
 auto& B=S.SupplyABatch;B.Valid=true;B.CaptureRevision=0;B.TotalCount=B.SourceCount=2000;B.TotalKg=.08;S.ModelParticleMassKg=.00004;
 S.SequentialBatch.B.SourceCount=S.SequentialBatch.B.InitialCount=2000;S.SequentialBatch.Additive.SourceCount=S.SequentialBatch.Additive.InitialCount=2000;
 S.SupplyBBrake.Supported=S.SupplyBBrake.Valid=S.SupplyCBrake.Supported=S.SupplyCBrake.Valid=true;
 S.SupplyBPurge.Supported=S.SupplyBPurge.Valid=S.SupplyCPurge.Supported=S.SupplyCPurge.Valid=true;
 S.WeighingMass.Validity=S.PipeMass.Validity=S.ReceiverMass.Validity=EPlantValueValidity::Valid;S.WeighingMass.Kg=S.PipeMass.Kg=S.ReceiverMass.Kg=0;
 S.RequestedSuppliers.Init(false,6);UI->Refresh();
 TestTrue(TEXT("resolved predicate enables start"),CastChecked<UButton>(W->GetWidgetFromName(TEXT("AutoStartA")))->GetIsEnabled());
 TestTrue(TEXT("ready guidance refreshes with button"),Text(TEXT("OperatorActionHint")).Contains(TEXT("시작 가능")));
 const int32 BeforeGuidance=M.History.Num();UI->OpenRecipeSettings();UI->CloseRecipes();UI->Refresh();TestEqual(TEXT("guidance and navigation never dispatch controls"),M.History.Num(),BeforeGuidance);
 A.RunId=FGuid::NewGuid();A.Epoch=S.Epoch;A.Connection=M.Connection;A.Stage=EProcessAutoStage::Cancelled;A.RecoveryPending=true;A.BatchRevision=8;A.BatchMode=EProcessBatchMode::TargetMass;A.LockedTargetAKg=.006;A.LockedToleranceAKg=.001;
 B.CaptureRevision=8;B.TotalCount=10;B.TotalKg=.01;B.SourceCount=5;B.ConnectorCount=3;B.WeighingCount=2;S.ModelParticleMassKg=.001;
 S.Execution=EPlantExecutionState::Paused;S.RequestedRunning=false;S.AppliedPressureValid=true;M.StopStage=EProcessStopStage::Complete;
 S.Recovery.Valid=S.Recovery.Supported=true;S.Recovery.Materials.SetNum(3);for(int I=0;I<3;++I)S.Recovery.Materials[I].Supplier=I;
 S.Recovery.Materials[0].Source=5;S.Recovery.Materials[0].SupplyConnector=3;S.Recovery.Materials[0].Weighing=2;
 UI->Refresh();TestTrue(TEXT("unreviewed quantities marked checking"),Text(TEXT("OperatorSummary")).Contains(TEXT("확인 중")));
 TestTrue(TEXT("fixture passes real recovery validator"),Service->AutoRun.CanRecover(M,FPlatformTime::Seconds()).IsEmpty());
 UI->RememberRecoveryCheck();UI->Refresh();TestTrue(TEXT("successful check removes contradictory needs-check title"),Text(TEXT("OperatorStage")).Contains(TEXT("상태 확인 완료")));
 TestFalse(TEXT("checked title no longer asks for check"),Text(TEXT("OperatorStage")).Contains(TEXT("상태 확인 필요")));
 TestTrue(TEXT("remaining supply includes connector material"),Text(TEXT("OperatorSummary")).Contains(TEXT("남은 공급 1 g")));
 ++S.Sequence;S.PublishedWallSeconds=FPlatformTime::Seconds();UI->Refresh();TestTrue(TEXT("new publication of same paused state keeps review"),Text(TEXT("OperatorStage")).Contains(TEXT("확인 완료")));
 M.LastRequest=FGuid::NewGuid();UI->Refresh();TestTrue(TEXT("manual command invalidates UI review"),Text(TEXT("OperatorSummary")).Contains(TEXT("확인 중")));
 UI->RememberRecoveryCheck();S.PhysicalSeconds+=.01;UI->Refresh();TestFalse(TEXT("physical movement invalidates review"),UI->RecoveryCheckCurrent());
 S.Recovery.ForeignIds={2163};S.Recovery.ForeignSuppliers={1};S.Recovery.ForeignRegions={TEXT("공급 연결부 1")};UI->Refresh();
 TestTrue(TEXT("long resume label stays inside button using explicit two lines"),CastChecked<UTextBlock>(CastChecked<UButton>(W->GetWidgetFromName(TEXT("ResumeBatch")))->GetContent())->GetText().ToString().Contains(TEXT("\n")));
 TestFalse(TEXT("foreign history blocks actual recovery button"),CastChecked<UButton>(W->GetWidgetFromName(TEXT("ResumeBatch")))->GetIsEnabled());
 TestTrue(TEXT("foreign source and ID directly beside button"),Text(TEXT("OperatorActionHint")).Contains(TEXT("2163")));
 TestTrue(TEXT("blocked recovery still requires explicit remedy"),Text(TEXT("OperatorActionHint")).Contains(TEXT("초기화")));
 TestEqual(TEXT("recovery guidance never sends commands"),M.History.Num(),BeforeGuidance);
 TestFalse(TEXT("real recovery admission rejects recorded foreign material"),Service->AutoRun.Recover(M,FPlatformTime::Seconds()));TestFalse(TEXT("no automatic stage starts on rejected recovery"),A.Active);
 A.RecoveryPending=false;M.StopStage=EProcessStopStage::Failed;M.StopFailureEquipment=TEXT("수취 밸브");UI->Refresh();TestTrue(TEXT("stop failure also visible without recoverable batch"),Text(TEXT("OperatorActionHint")).Contains(TEXT("수취 밸브")));M.StopStage=EProcessStopStage::Complete;

 A.Stage=EProcessAutoStage::Cancelled;A.RecoveryPending=true;A.RecoveryReason=TEXT("배치 재개 차단 · ID 2163 / 공급원 1 / 공급 연결부 1");UI->Refresh();TestTrue(TEXT("block reason immediately visible"),Text(TEXT("OperatorReason")).Contains(TEXT("ID 2163")));
 M.StopStage=EProcessStopStage::Failed;M.StopFailureEquipment=TEXT("B 밸브");UI->Refresh();TestTrue(TEXT("full stop failure immediately visible"),Text(TEXT("OperatorStage")).Contains(TEXT("정지 실패")));TestTrue(TEXT("unconfirmed equipment visible"),Text(TEXT("OperatorReason")).Contains(TEXT("B 밸브")));
 S.PublishedWallSeconds=FPlatformTime::Seconds()-3;UI->Refresh();TestTrue(TEXT("staleness retains stop failure equipment"),Text(TEXT("OperatorReason")).Contains(TEXT("B 밸브")));TestTrue(TEXT("staleness still marked unknown"),Text(TEXT("OperatorReason")).Contains(TEXT("미확인")));
 M.StopStage=EProcessStopStage::Complete;UI->Refresh();TestTrue(TEXT("staleness retains latched recovery blocker"),Text(TEXT("OperatorReason")).Contains(TEXT("ID 2163")));
 UI->Detach();Adapter->Detach();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
