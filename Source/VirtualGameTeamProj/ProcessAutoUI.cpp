#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessUIStateAdapter.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "HAL/PlatformTime.h"

void UProcessControlUI::StartAutomaticA()
{
 if(Is3D())return;if(auto* S=Service.Get()){S->StartAutomaticA();Refresh();}
}
void UProcessControlUI::CheckBatchRecovery(){if(!Is3D())if(auto* S=Service.Get()){S->CheckAutomaticBatch();RememberRecoveryCheck();Refresh();}}
void UProcessControlUI::ResumeBatchRecovery(){if(!Is3D())if(auto* S=Service.Get()){S->ResumeAutomaticBatch();Refresh();}}
void UProcessControlUI::AbandonBatchRecovery(){if(!Is3D())if(auto* S=Service.Get()){S->AbandonAutomaticBatch();Refresh();}}
void UProcessControlUI::RefreshAutomatic()
{
 auto* W=Widget.Get();auto* S=Service.Get();if(!W||!S)return;
 const auto& A=S->AutoRun.State;const auto& Snapshot=S->State.Snapshot;
 for(const TCHAR* Name:{TEXT("CheckBatch"),TEXT("ResumeBatch"),TEXT("AbandonBatch")})if(auto* B=Cast<UButton>(W->GetWidgetFromName(Name))){
  B->SetVisibility(A.RecoveryPending&&!Is3D()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
  const auto Why=S->AutoRun.CanRecover(S->State,FPlatformTime::Seconds());
  B->SetIsEnabled(!Is3D()&&!A.Active&&(FString(Name)!=TEXT("ResumeBatch")||Why.IsEmpty()));B->SetToolTipText(FText::FromString(Why));
 }

 if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("AutoStartA")))){
  const FString Why=S->AutoRun.CanStart(S->State,FPlatformTime::Seconds());
  B->SetVisibility(Snapshot.SupplyABatch.Supported&&!Is3D()?ESlateVisibility::Visible:ESlateVisibility::Collapsed);
  B->SetIsEnabled(!Is3D()&&Why.IsEmpty());B->SetToolTipText(FText::FromString(Why));
 }
 if(!A.RunId.IsValid()){
  if(Snapshot.SupplyABatch.Supported)if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("Help"))))
   T->SetText(FText::FromString(S->AutoSettings.MixingDurationSeconds>0?
    FString::Printf(TEXT("A 자동 배치 · 적용 혼합 시간 %.3f초 · 목표 RPM은 혼합기 패널에서 설정\n시작 시 RPM·혼합 시간을 이번 배치 값으로 확정합니다."),S->AutoSettings.MixingDurationSeconds):
    FString(TEXT("자동 시작 전 혼합기 패널에서 목표 RPM과 혼합 시간(초)을 적용하세요.\n혼합 시간은 작업자의 RPM 실제 적용 해석 시각부터 계산합니다."))));
  return;
 }
 const bool MixingNow=A.Epoch==Snapshot.Epoch&&A.Connection==S->State.Connection&&A.Stage==EProcessAutoStage::Mixing&&Snapshot.Validity==EPlantValueValidity::Valid&&
  Snapshot.RequestedMixerEnabled&&Snapshot.AppliedRPMValid&&Snapshot.AppliedRPM>0&&
  FPlatformTime::Seconds()-Snapshot.PublishedWallSeconds<=2.;
 const bool SameRun=A.Epoch==Snapshot.Epoch&&A.Connection==S->State.Connection;
 const FString Label=!SameRun?TEXT("이전 배치 기록 · 초기화/연결 변경"):
  A.Stage==EProcessAutoStage::Mixing&&!MixingNow?TEXT("혼합 상태 확인 중"):FProcessAutoRun::StageText(A.Stage);
 const FString MaterialLabel=!A.Entries.IsEmpty()?FString::Printf(TEXT("[%c] "),TCHAR('A'+A.ActiveSupplier)):A.AdditiveEnabled&&A.ActiveSupplier==2?(A.Stage==EProcessAutoStage::Complete?FString(TEXT("[A+B+C] ")):FString(TEXT("[첨가제 C] "))):A.BatchMode==EProcessBatchMode::SequentialAB?(Snapshot.SequentialBatch.Combined?TEXT("[A+B] "):(A.ActiveSupplier==1?TEXT("[B] "):TEXT("[A] "))):TEXT("");
 const FString Status=MaterialLabel+(A.BatchMode!=EProcessBatchMode::Full&&A.Stage==EProcessAutoStage::Feeding?FString(TEXT("목표량 공급 중")):Label)+TEXT(" · ")+(A.RecoveryPending&&!A.RecoveryReason.IsEmpty()?A.RecoveryReason:A.Reason);
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("Help")))){
  T->SetText(FText::FromString(Status+TEXT("\n")+(A.Active?TEXT("상충 수동 조작 잠금 · 전체 정지 가능"):S->State.StopDescription())));
  T->SetToolTipText(FText::FromString(FString::Printf(TEXT("자동 실행 %s · 초기화 %lld · 연결 %lld\n%s\n%s"),*A.RunId.ToString(),A.Epoch,A.Connection,*A.StopResult,*S->State.UnconfirmedEquipment())));
 }
 if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("TransferTelemetry")))){
  // Completion owns this panel on every refresh, independent of the last route flag.
  // Keep historical worker diagnostics intact; they no longer describe the current batch phase.
  if(SameRun&&A.Stage==EProcessAutoStage::Complete&&!A.Active){
   const double AKg=A.BatchMode==EProcessBatchMode::SequentialAB?A.FinalAKg:A.BatchKg;
   const double BKg=A.BatchMode==EProcessBatchMode::SequentialAB?A.FinalBKg:0;
   const double CKg=A.AdditiveEnabled?A.FinalAdditiveKg:0;
   FString Body=TEXT("배치 완료\n최종 혼합기 도착량 · 시뮬레이션 계산값\n");
   if(!A.Entries.IsEmpty())Body+=A.FinalSummary()+TEXT("\n");else{
   Body+=FString::Printf(TEXT("A %.6f g\n"),AKg*1000);
   if(A.BatchMode==EProcessBatchMode::SequentialAB)Body+=FString::Printf(TEXT("B %.6f g\n"),BKg*1000);
   if(A.AdditiveEnabled)Body+=FString::Printf(TEXT("C %.6f g\n"),CKg*1000);
   Body+=FString::Printf(TEXT("합계 %.6f g\n"),(AKg+BKg+CKg)*1000);
   }
   const double Age=FPlatformTime::Seconds()-Snapshot.PublishedWallSeconds;
   if(S->State.Connected&&Snapshot.Validity==EPlantValueValidity::Valid&&Age>=0&&Age<=2.&&Snapshot.AppliedRPMValid)
    Body+=FString::Printf(TEXT("계산 적용 RPM %.3f\n"),Snapshot.AppliedRPM);
   else Body+=TEXT("계산 적용 RPM 미확인 · 연결/최신 데이터 확인 필요\n");
   Body+=FString::Printf(TEXT("혼합 목표 %.3f초 · 실제 경과 %.6f초\nRPM 0 정지 적용 %.9f 해석초\n"),A.MixingDurationSeconds,A.MixingElapsedSeconds,A.MixerStoppedSimulationSeconds);
   if(!A.RecipeName.IsEmpty())Body+=TEXT("완료 레시피: ")+A.RecipeName+TEXT("\n");
   // Live stop/worker faults must remain visible even after a successfully completed batch.
   Body+=S->State.StopDescription();
   if(S->State.StopStage==EProcessStopStage::Failed){
    if(const auto* Stop=S->State.History.Find(S->State.StopRequest))Body+=TEXT("\n")+Stop->Message;
    Body+=TEXT("\n미확인: ")+S->State.StopFailureEquipment;
   }
   else if(S->State.IsStopping())Body+=TEXT("\n미확인: ")+S->State.UnconfirmedEquipment();
   if(!Snapshot.Error.IsEmpty())Body+=TEXT("\n현재 작업자 오류: ")+Snapshot.Error;
   T->SetVisibility(Is3D()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);
   T->SetText(FText::FromString(Body));
   T->SetToolTipText(FText::FromString(FString::Printf(TEXT("완료 배치 %s · 초기화 %lld · 연결 %lld\n최종 도착량은 이번 배치의 확정 결과입니다. 계산 적용 RPM은 최신 상태입니다."),*A.RunId.ToString(),A.Epoch,A.Connection)));
   return;
  }
  const auto& B=Snapshot.SupplyABatch;
  const bool Same=A.Epoch==Snapshot.Epoch&&A.Connection==S->State.Connection;
  FString Body=Status+FString::Printf(TEXT("\n%s %d개 / %.6f g · 시뮬레이션 계산값\n"),A.BatchMode!=EProcessBatchMode::Full&&!A.DoseFinalized?TEXT("현재 원료 재고 · 계량분 미확정"):TEXT("현재 계량분"),A.BatchCount,A.BatchKg*1000.);
  if(A.RecoveryPending||A.RecoveryCount>0||A.RecoveryAbandoned){
   Body+=TEXT("배치 기록 보존 · 계산 재개와 배치 재개는 별도\n")+A.RecoveryReason+TEXT("\n");
   for(const auto& R:Snapshot.Recovery.Materials){
    if(!A.UsesSupplier(R.Supplier))continue;const double Target=A.DoseFor(R.Supplier,false);
    const bool Final=!R.SelectedIds.IsEmpty();const double Unit=Snapshot.ModelParticleMassKg*1000;
    const double Passed=(R.SupplyConnector+R.Weighing+R.Pipe+R.Receiver+R.MixerConnector+R.Mixer)*Unit;
    Body+=FString::Printf(TEXT("%c: 이미 공급 %.6f g · 이동 중 %.6f g · 수취 %.6f g · 혼합기 %.6f g · 남은 %s %.6f g\n"),
     TCHAR('A'+R.Supplier),Passed,(R.SupplyConnector+R.Pipe+R.MixerConnector)*Unit,R.Receiver*Unit,R.Mixer*Unit,
     Final?TEXT("이송"):TEXT("공급"),Final?(R.SelectedIds.Num()-R.Mixer)*Unit:FMath::Max(0.,Target*1000-Passed));
   }
   Body+=TEXT("이어갈 단계: ")+FProcessAutoRun::StageText(A.RecoveryNext)+TEXT("\n");
  }
  if(!A.Entries.IsEmpty()){
   for(const auto& E:A.Entries)if(E.Enabled){
    const bool Known=Same&&Snapshot.Recovery.Valid&&Snapshot.SequentialBatch.Materials.IsValidIndex(E.Supplier);
    const auto* M=Known?&Snapshot.SequentialBatch.Materials[E.Supplier]:nullptr;
    Body+=FString::Printf(TEXT("%c %s · 목표 %g ± %g g · 확정 %s · 수취 %s\n"),'A'+E.Supplier,*E.DisplayName,E.TargetKg*1000,E.ToleranceKg*1000,
     M&&M->FinalSimulationSeconds>=0?*FString::Printf(TEXT("%g g"),M->FinalKg*1000):TEXT("미확인"),M?*FString::Printf(TEXT("%g g"),M->ReceiverCount*Snapshot.ModelParticleMassKg*1000):TEXT("미확인"));
   }
  }else if(!A.RecipeName.IsEmpty()){
   Body+=FString::Printf(TEXT("이번 실행 레시피: %s\n원료 표시명 A [%s]"),*A.RecipeName,*A.RecipeMaterialA);
   if(A.BatchMode==EProcessBatchMode::SequentialAB)Body+=FString::Printf(TEXT(" / B [%s]"),*A.RecipeMaterialB);
   if(A.AdditiveEnabled)Body+=FString::Printf(TEXT(" / C [%s]"),*A.RecipeMaterialC);
   Body+=TEXT("\n");
  }
  if(A.Entries.IsEmpty()&&A.BatchMode==EProcessBatchMode::SequentialAB){
   const auto FinalMass=[](double Kg){return Kg>0?FString::Printf(TEXT("확정 %.6f g"),Kg*1000):FString(TEXT("미확정"));};
   Body+=FString::Printf(TEXT("A 목표 %.6f g ± %.6f g · %s\nB 목표 %.6f g ± %.6f g · %s\n"),A.LockedTargetAKg*1000,A.LockedToleranceAKg*1000,*FinalMass(A.FinalAKg),A.LockedTargetBKg*1000,A.LockedToleranceBKg*1000,*FinalMass(A.FinalBKg));
   const auto& AB=Snapshot.SequentialBatch;
   Body+=FString::Printf(TEXT("수취 보관 A %.6f g / B %.6f g\n혼합기 도착 A %.6f g / B %.6f g\n"),AB.A.ReceiverCount*Snapshot.ModelParticleMassKg*1000,AB.B.ReceiverCount*Snapshot.ModelParticleMassKg*1000,AB.A.MixerCount*Snapshot.ModelParticleMassKg*1000,AB.B.MixerCount*Snapshot.ModelParticleMassKg*1000);
  }
  if(A.AdditiveEnabled){
   const auto& C=Snapshot.SequentialBatch.Additive;
   const FString FinalAdditive=A.FinalAdditiveKg>0?FString::Printf(TEXT("확정 %.6f g"),A.FinalAdditiveKg*1000):FString(TEXT("미확정"));
   const auto AppliedTime=[](double Seconds){return Seconds>=0?FString::Printf(TEXT("%.9f"),Seconds):FString(TEXT("미확인"));};
   Body+=FString::Printf(TEXT("첨가제 %c · %s · 목표 %.6f g ± %.6f g · %s\n수취 %.6f g · 연결부 %.6f g · 혼합기 %.6f g\n예약 N %.6f 해석초 · %s\n"),
    TCHAR('A'+A.AdditiveSupplier),A.AdditiveScheduled?TEXT("예약 ON"):TEXT("예약 OFF · 혼합 전 초기 투입"),A.AdditiveTargetKg*1000,A.AdditiveToleranceKg*1000,*FinalAdditive,
    C.ReceiverCount*Snapshot.ModelParticleMassKg*1000,C.ConnectorCount*Snapshot.ModelParticleMassKg*1000,C.MixerCount*Snapshot.ModelParticleMassKg*1000,
    A.AdditiveDelaySeconds,*A.AdditiveStatus);
   if(A.AdditiveDueSeconds>=0)Body+=FString::Printf(TEXT("예약 해석시각 %.9f · 준비 %s\n"),A.AdditiveDueSeconds,*AppliedTime(A.AdditivePreparedSeconds));
   if(A.AdditiveOpenRequestedSeconds>=0)Body+=FString::Printf(TEXT("개방 요청 %.9f · 실제 열림 %s\n"),A.AdditiveOpenRequestedSeconds,*AppliedTime(A.AdditiveOpenAppliedSeconds));
   if(A.AdditiveOpenAppliedSeconds>=0&&A.AdditiveScheduled)Body+=FString::Printf(TEXT("예약 대비 실제 개방 지연 %.9f초\n"),A.AdditiveOpenAppliedSeconds-A.AdditiveDueSeconds);
   Body+=FString::Printf(TEXT("계산 적용 RPM %.3f · 최초 혼합 적용 %s (해석초)\n"),Snapshot.AppliedRPM,*AppliedTime(A.MixerAppliedSimulationSeconds));
  }
  Body+=FString::Printf(TEXT("이번 배치 혼합 목표 %.3f초 · %.3f RPM\n경과 %.3f초 · 남은 시간 %.3f초 (해석 시간)\n"),A.MixingDurationSeconds,A.TargetMixerRPM,A.MixingElapsedSeconds,A.MixingRemainingSeconds);
  if(A.MixerStoppedSimulationSeconds>=0)Body+=FString::Printf(TEXT("정지 적용 %.9f초 · 시간 도달 이후 정지까지 %.6f초\n"),A.MixerStoppedSimulationSeconds,A.MixingStopOverrunSeconds);
  if(A.BatchMode!=EProcessBatchMode::Full){
   Body+=FString::Printf(TEXT("목표 %.6f g ± %.6f g (계량 허용 오차)\n차단 추정 %.6f g · 최근 증가분 %.6f g 포함\n"),A.TargetMassKg*1000,A.WeighingToleranceKg*1000,A.CutoffEstimatedKg*1000,A.CutoffLeadKg*1000);
   Body+=FString::Printf(TEXT("제동 후 배출 예상 %.6f g (현재 입자 위치·속도 기반)\n"),A.CutoffDrainableKg*1000.);
   if(Same&&Snapshot.SupplyABrake.Supported){const auto& Brake=PlantBrakeFor(Snapshot,A.ActiveSupplier);
    Body+=FString::Printf(TEXT("해석용 제동 피더: %s · 게이트 점유 %d개 · 보호 보류 %d회\n"),!Brake.Valid?TEXT("미확인"):(Brake.Applied?TEXT("제동 적용"):TEXT("해제")),Brake.GateOccupancyCount,Brake.GuardDeferrals);
   }
   if(A.FinalDoseSimulationSeconds>=0)Body+=FString::Printf(TEXT("작업자 최종 계량 %.6f g · 차이 %+.6f g · %s\n"),A.FinalDoseKg*1000,(A.FinalDoseKg-A.TargetMassKg)*1000,A.DoseFinalized?TEXT("허용 범위 내 확정"):TEXT("계량 범위 벗어남 · 출구 개방 금지"));
   else Body+=TEXT("최종 계량 미확정 · 차단 추정은 확정 계량값이 아닙니다.\n");
   if(Same&&B.SupplyCloseRequestedSeconds>=0&&B.SupplyClosedSeconds<0)Body+=FString::Printf(TEXT("공급 실제 닫힘 미확인 · 작업자 요청 %.6f초 이후 추가 통과 %+.6f g (관찰 중)\n"),B.SupplyCloseRequestedSeconds,B.ClosingExtraKg*1000);
   if(Same&&B.SupplyClosedSeconds>=0)Body+=FString::Printf(TEXT("공급 닫기 작업자 접수 %.6f초 → 실제 닫힘 %.6f초\n그 사이 추가 통과 %+.6f g · 상류 남은 양 %.6f g\n"),B.SupplyCloseRequestedSeconds,B.SupplyClosedSeconds,B.ClosingExtraKg*1000,B.InventorySourceCount*Snapshot.ModelParticleMassKg*1000);
  }
  if(Same&&B.Valid)Body+=FString::Printf(TEXT("현재 공급호퍼 %d개 · 연결부 %d개 · 칭량 %d개\n"),B.InventorySourceCount,B.ConnectorCount,B.WeighingCount);
  else Body+=TEXT("초기화·연결 변경 · 이전 배치 결과 무효\n");
  if(Same&&A.DownstreamStarted){
   const auto& F=Snapshot.MixerFeed;
   Body+=FString::Printf(TEXT("수취 전량 도착 확인 기록 %.6f g (%.6f 해석초)\n"),A.ArrivedKg*1000.,A.UICompleteSimulationSeconds);
   if(F.Valid&&Snapshot.Validity==EPlantValueValidity::Valid&&FPlatformTime::Seconds()-Snapshot.PublishedWallSeconds<=2.)
    Body+=FString::Printf(TEXT("이번 배치: 수취 잔량 %.6f g · 혼합 연결부 %.6f g\n혼합기 도착 %.6f g / %.6f g\n기존 혼합 원료 %.6f g 제외 · 추가 유입 %d개\n안정 %.3f / %.3f 해석초 · 수취 출구 %s\n혼합 목표 %.3f RPM · 계산 적용 %.3f RPM\n%s"),
     F.ReceiverKg*1000.,F.ConnectorKg*1000.,F.MixerKg*1000.,A.BatchKg*1000.,F.InitialMixerKg*1000.,F.AdditionalInflowCount,
     F.StableSeconds,Snapshot.Transfer.Criteria.StableSimulationSeconds,Snapshot.AppliedReceiverOpen?TEXT("열림"):TEXT("닫힘"),A.TargetMixerRPM,Snapshot.AppliedRPM,*F.Reason);
   else Body+=TEXT("혼합 투입 데이터 미확인 또는 갱신 중단");
  }
  else if(Same){
   // A's completed opening is historical once B owns the cleared transfer record.
   const bool PreviousOpening=A.BatchMode==EProcessBatchMode::SequentialAB&&A.ActiveSupplier!=A.NextRegular(INDEX_NONE)&&
    Snapshot.Transfer.Id==0&&Snapshot.FeedPreparation.State==EPlantFeedPreparationState::Opened;
   Body+=(PreviousOpening?FString(TEXT("현재 원료 공급 완료 확인 전 · 칭량호퍼 출구 닫힘 유지\n")):UProcessUIStateAdapter::FormatPreparation(Snapshot.FeedPreparation))+
    UProcessUIStateAdapter::FormatTransfer(Snapshot.Transfer).Replace(TEXT("수동 이송"),TEXT("이송"));
  }
  T->SetVisibility(Is3D()?ESlateVisibility::Collapsed:ESlateVisibility::Visible);T->SetText(FText::FromString(Body));
  if(Same&&!A.DownstreamStarted&&!Is3D()&&Snapshot.Transfer.State==EPlantTransferState::Complete&&
     Body.Contains(FString::Printf(TEXT("이송 #%lld · 완료"),Snapshot.Transfer.Id)))
    S->ConfirmAutomaticUI(Snapshot.Epoch,Snapshot.Transfer.Id,Snapshot.PhysicalSeconds);
 }
}
