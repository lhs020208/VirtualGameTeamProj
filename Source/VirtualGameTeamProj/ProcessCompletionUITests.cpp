#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessUIStateAdapter.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Blueprint/UserWidget.h"
#include "Components/TextBlock.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "HAL/PlatformTime.h"
#include "UObject/StructOnScope.h"
#include "UObject/UnrealType.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FProcessCompletionUI,
 "ProcessUI.Display.CompletedBatchOwnsCurrentPanel",
 EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FProcessCompletionUI::RunTest(const FString&)
{
 UWorld::InitializationValues IV;
 IV.AllowAudioPlayback(false).CreatePhysicsScene(false).CreateNavigation(false).CreateAISystem(false);
 auto* World=UWorld::CreateWorld(EWorldType::Game,false,NAME_None,nullptr,true,ERHIFeatureLevel::Num,&IV);
 if(!TestNotNull(TEXT("test world"),World))return false;
 GEngine->CreateNewWorldContext(EWorldType::Game).SetCurrentWorld(World);
 World->InitializeActorsForPlay(FURL());
 auto* Player=NewObject<ULocalPlayer>(GEngine);
 auto* PC=World->SpawnActor<APlayerController>();PC->SetPlayer(Player);
 auto* Class=LoadClass<UUserWidget>(nullptr,TEXT("/Game/ProcessUIV2/Widgets/WBP_ProcessUIV2.WBP_ProcessUIV2_C"));
 auto* W=Class?CreateWidget<UUserWidget>(PC,Class):nullptr;
 if(!TestNotNull(TEXT("existing real 2D widget"),W)){World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return false;}
 auto* Adapter=NewObject<UProcessUIStateAdapter>(World);
 auto* Service=NewObject<UProcessControlService>(World);
 auto* UI=NewObject<UProcessControlUI>(World);
 TestTrue(TEXT("state adapter attached"),Adapter->Attach(W,nullptr));
 TestTrue(TEXT("control UI attached"),UI->Attach(W,Service));Adapter->CommandUI=UI;
 auto& M=Service->State;auto& S=M.Snapshot;auto& A=Service->AutoRun.State;
 M.Connected=true;M.Connection=4;M.ManualGranted=true;M.StopStage=EProcessStopStage::Complete;
 S.Epoch=8;S.Sequence=20;S.Validity=EPlantValueValidity::Valid;S.PublishedWallSeconds=FPlatformTime::Seconds();
 S.AppliedRPMValid=true;S.AppliedRPM=0;S.RequestedSuppliers.Init(false,6);
 S.Transfer.Id=3;S.Transfer.Validity=EPlantValueValidity::Valid;S.Transfer.State=EPlantTransferState::Indeterminate;
 S.Transfer.Reason=TEXT("수취호퍼 출구 개방·배출 요청으로 이번 도착량의 보존을 확인할 수 없습니다.");
 S.FeedPreparation.Enabled=true;S.FeedPreparation.Reason=TEXT("과거 공급 잔류 대기");
 A.RunId=FGuid::NewGuid();A.Epoch=S.Epoch;A.Connection=M.Connection;A.Stage=EProcessAutoStage::Complete;
 A.Active=false;A.DownstreamStarted=false;A.BatchMode=EProcessBatchMode::SequentialAB;A.AdditiveEnabled=true;
 A.FinalAKg=.040051542;A.FinalBKg=.039930905;A.FinalAdditiveKg=.019905134;
 A.RecoveryCount=1;A.RecoveryNext=EProcessAutoStage::TransferPressure;A.RecoveryReason=TEXT("과거 재개 준비");
 A.AdditiveStatus=TEXT("첨가제 수취 준비 완료 · 출구 닫힘 대기");
 A.Reason=TEXT("설정 혼합 시간 경과·이번 정지 요청의 계산 적용 RPM0 확인");
 A.MixingDurationSeconds=6;A.MixingElapsedSeconds=6.00384;A.MixerStoppedSimulationSeconds=12.54464;
 auto Text=[&](){return CastChecked<UTextBlock>(W->GetWidgetFromName(TEXT("TransferTelemetry")))->GetText().ToString();};
 auto CheckComplete=[&](){
  const auto T=Text();
  for(const TCHAR* Required:{TEXT("배치 완료"),TEXT("최종 혼합기 도착량"),TEXT("A 40.051542 g"),TEXT("B 39.930905 g"),TEXT("C 19.905134 g"),TEXT("계산 적용 RPM 0.000")})TestTrue(Required,T.Contains(Required));
  for(const TCHAR* Old:{TEXT("이송 #3"),TEXT("판정 불가"),TEXT("수취호퍼 출구 개방·배출"),TEXT("과거 공급 잔류 대기"),TEXT("과거 재개 준비"),TEXT("이어갈 단계"),TEXT("출구 닫힘 대기")})TestFalse(Old,T.Contains(Old));
 };
 UI->Refresh();CheckComplete();
 // Exercise the same adapter overwrite -> command UI refresh order used by selection/view changes.
 FindFProperty<FStrProperty>(W->GetClass(),TEXT("SelectedEquipmentId"))->SetPropertyValue_InContainer(W,TEXT("weigher"));
 for(int I=0;I<3;++I){++S.Sequence;S.PublishedWallSeconds=FPlatformTime::Seconds();Adapter->Refresh();CheckComplete();}
 for(const TCHAR* Name:{TEXT("Show3D"),TEXT("Show2D")}){
  auto* F=W->FindFunction(Name);if(TestNotNull(Name,F)){FStructOnScope Args(F);W->ProcessEvent(F,Args.GetStructMemory());Adapter->Refresh();}
 }
 CheckComplete();
 TestEqual(TEXT("diagnostic evidence is retained"),S.Transfer.Reason,FString(TEXT("수취호퍼 출구 개방·배출 요청으로 이번 도착량의 보존을 확인할 수 없습니다.")));
 TestEqual(TEXT("historical preparation retained"),S.FeedPreparation.Reason,FString(TEXT("과거 공급 잔류 대기")));
 TestEqual(TEXT("view and refresh issue no commands"),M.History.Num(),0);
 TestTrue(TEXT("completion state unchanged"),A.Stage==EProcessAutoStage::Complete);
 // Live failures must not be concealed by a previously completed batch.
 M.StopStage=EProcessStopStage::Failed;M.StopRequest=FGuid::NewGuid();M.StopFailureEquipment=TEXT("수취호퍼 압력");
 FProcessCommandRecord Stop;Stop.RequestId=M.StopRequest;Stop.Status=EProcessCommandStatus::Failed;Stop.Message=TEXT("현재 압력 복귀 실패");M.History.Add(Stop.RequestId,Stop);
 S.Error=TEXT("현재 작업자 오류 시험");UI->Refresh();
 for(const TCHAR* Required:{TEXT("정지 실패"),TEXT("현재 압력 복귀 실패"),TEXT("수취호퍼 압력"),TEXT("현재 작업자 오류 시험")})TestTrue(Required,Text().Contains(Required));
 S.Error.Empty();M.StopStage=EProcessStopStage::Complete;S.PublishedWallSeconds=FPlatformTime::Seconds()-3;
 UI->Refresh();TestTrue(TEXT("stale applied RPM never displayed as current zero"),Text().Contains(TEXT("계산 적용 RPM 미확인")));
 S.PublishedWallSeconds=FPlatformTime::Seconds();
 A.Stage=EProcessAutoStage::Transferring;A.Active=true;A.Reason=TEXT("현재 배관 잔류 대기");UI->Refresh();
 TestTrue(TEXT("active wait remains"),Text().Contains(A.Reason));TestTrue(TEXT("active transfer diagnosis remains"),Text().Contains(S.Transfer.Reason));
 A.Stage=EProcessAutoStage::Cancelled;A.Active=false;A.RecoveryPending=true;A.RecoveryReason=TEXT("배치 재개 차단 · ID 2163 / 공급원 1 / 공급 연결부 1");UI->Refresh();
 TestTrue(TEXT("wrong-source recovery blocker remains"),Text().Contains(A.RecoveryReason));
 A.RecoveryPending=false;A.Stage=EProcessAutoStage::Complete;A.BatchMode=EProcessBatchMode::Full;A.AdditiveEnabled=false;A.BatchKg=.402;
 UI->Refresh();TestTrue(TEXT("single A full batch final mass"),Text().Contains(TEXT("A 402.000000 g")));TestFalse(TEXT("unused B omitted"),Text().Contains(TEXT("B 39.")));
 A.BatchMode=EProcessBatchMode::TargetMass;A.BatchKg=.2;UI->Refresh();TestTrue(TEXT("single A target final mass"),Text().Contains(TEXT("A 200.000000 g")));
 UI->Detach();Adapter->Detach();World->DestroyWorld(false);GEngine->DestroyWorldContext(World);return true;
}
#endif
