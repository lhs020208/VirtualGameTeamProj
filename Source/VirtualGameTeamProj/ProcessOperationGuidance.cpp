#include "ProcessControlUI.h"
#include "ProcessControlService.h"
#include "ProcessDisplay.h"
#include "Blueprint/UserWidget.h"
#include "Components/Button.h"
#include "Components/TextBlock.h"
#include "Components/Border.h"
#include "HAL/PlatformTime.h"

// Presentation only. CanStart/CanRecover and the existing service own every admission decision.
void UProcessControlUI::OpenRecipeSettings(){if(Is3D())return;RecipeOpen=true;OperatorDetailsOpen=false;RefreshRecipeList();Refresh();}
void UProcessControlUI::RememberRecoveryCheck()
{
 auto* C=Service.Get();if(!C)return;
 if(!C->AutoRun.CanRecover(C->State,FPlatformTime::Seconds()).IsEmpty()){CheckedRun.Invalidate();return;}
 const auto& M=C->State;CheckedRun=C->AutoRun.State.RunId;CheckedStop=M.StopRequest;CheckedCommand=M.LastRequest;
 CheckedEpoch=M.Snapshot.Epoch;CheckedConnection=M.Connection;CheckedControl=M.Snapshot.RequestedControlRevision;CheckedSimulationSeconds=M.Snapshot.PhysicalSeconds;
}
bool UProcessControlUI::RecoveryCheckCurrent() const
{
 const auto* C=Service.Get();if(!C)return false;const auto& M=C->State;
 return CheckedRun.IsValid()&&CheckedRun==C->AutoRun.State.RunId&&CheckedStop==M.StopRequest&&CheckedCommand==M.LastRequest&&
  CheckedEpoch==M.Snapshot.Epoch&&CheckedConnection==M.Connection&&CheckedControl==M.Snapshot.RequestedControlRevision&&CheckedSimulationSeconds==M.Snapshot.PhysicalSeconds&&
  C->AutoRun.CanRecover(M,FPlatformTime::Seconds()).IsEmpty();
}
void UProcessControlUI::RefreshOperationGuidance()
{
 auto* W=Widget.Get();auto* C=Service.Get();if(!W||!C)return;const auto& M=C->State;const auto& S=M.Snapshot;const auto& A=C->AutoRun.State;
 const double Now=FPlatformTime::Seconds();const bool Fresh=M.Connected&&S.Validity==EPlantValueValidity::Valid&&S.PublishedWallSeconds>0&&Now>=S.PublishedWallSeconds&&Now-S.PublishedWallSeconds<=2.;
 const bool Same=A.Epoch==S.Epoch&&A.Connection==M.Connection,Recovery=Same&&A.RecoveryPending&&!A.Active;
 const FString StartWhy=C->AutoRun.CanStart(M,Now),RecoverWhy=C->AutoRun.CanRecover(M,Now);
 const bool Checked=Recovery&&RecoveryCheckCurrent(),Ready=Recovery&&RecoverWhy.IsEmpty();
 auto Put=[&](const TCHAR* N,const FString& V){if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(N)))T->SetText(FText::FromString(V));};
 auto Show=[&](const TCHAR* N,bool V){if(auto* X=W->GetWidgetFromName(N))X->SetVisibility(V?ESlateVisibility::Visible:ESlateVisibility::Collapsed);};
 auto Caption=[&](const TCHAR* N,const TCHAR* V){if(auto* B=Cast<UButton>(W->GetWidgetFromName(N)))if(auto* T=Cast<UTextBlock>(B->GetContent()))T->SetText(FText::FromString(V));};
 Caption(TEXT("ManualResume"),TEXT("계산만 재개"));Caption(TEXT("ResumeBatch"),TEXT("남은 양\n이어서 투입"));Caption(TEXT("CheckBatch"),TEXT("배치 상태 확인"));
 Show(TEXT("AutoStartAContainer"),!Is3D()&&!Recovery);Show(TEXT("ResumeBatchContainer"),!Is3D()&&Recovery);
 Show(TEXT("OperatorOpenRecipe"),!Is3D()&&!Recovery&&!A.Active&&!RecipeOpen&&!M.IsStopping());
 Show(TEXT("CheckBatch"),!Is3D()&&Recovery);
 if(auto* B=Cast<UButton>(W->GetWidgetFromName(TEXT("CheckBatch"))))B->SetIsEnabled(Recovery&&!M.IsStopping());
 const auto R=C->GetAppliedRecipe();
 FString Applied=C->RecipeRevision>0?TEXT("현재 배치에 사용할 적용값 · ")+R.Name:TEXT("현재 개별 적용값 · 레시피 미적용");
 if(!R.Entries.IsEmpty())Applied+=TEXT("\n")+ProcessRecipe::Summary(R);
 else if(R.Mode==EProcessBatchMode::Full)Applied+=TEXT(" · A 전량");else Applied+=TEXT(" · A ")+ProcessDisplay::Mass(R.TargetAKg)+TEXT(" ± ")+ProcessDisplay::Mass(R.ToleranceAKg);
 if(R.Mode==EProcessBatchMode::SequentialAB)Applied+=TEXT(" / B ")+ProcessDisplay::Mass(R.TargetBKg)+TEXT(" ± ")+ProcessDisplay::Mass(R.ToleranceBKg);
 if(R.Entries.IsEmpty()&&R.AdditiveEnabled)Applied+=TEXT(" / C ")+ProcessDisplay::Mass(R.TargetCKg)+TEXT(" ± ")+ProcessDisplay::Mass(R.ToleranceCKg)+(R.Scheduled?TEXT(" · 혼합 시작 후 ")+ProcessDisplay::Number(R.DelaySeconds)+TEXT("초"):FString(TEXT(" · 초기 투입")));
 Applied+=TEXT("\n")+ProcessDisplay::Number(R.TargetRPM)+TEXT(" RPM · 혼합 ")+ProcessDisplay::Number(R.MixingSeconds)+TEXT("초 · ")+(StartWhy.IsEmpty()?FString(TEXT("시작 가능")):TEXT("시작 조건 확인 필요"));
 if(Same&&(A.Active||A.RecoveryPending))Applied=TEXT("새 배치용 설정 · 현재/중단 배치는 시작 시 확정값을 보존합니다.\n")+Applied.Replace(TEXT("현재 배치에 사용할 적용값"),TEXT("새 배치에 사용할 적용값"));
 Applied+=TEXT("\n호퍼 목표 압력: 공급→칭량 +0.5 / -0.5 · 칭량→수취 +0.5 / ")+ProcessDisplay::Number(R.TransferPressurePa.Z/1000.)+TEXT(" kPa(g)");
 Put(TEXT("OperatorAppliedRecipe"),Applied);
 FString Hint;
 if(Recovery){
  FString Title,Why;
  if(M.StopStage==EProcessStopStage::Failed){Title=TEXT("전체 정지 실패 · 재시도 필요");Why=M.StopDescription()+TEXT(" · ")+M.StopFailureEquipment;if(const auto* Stop=M.History.Find(M.StopRequest))Why+=TEXT(" · ")+Stop->Message;Hint=TEXT("정지 실패 · ")+M.StopFailureEquipment+TEXT("\n전체 정지를 다시 요청하고 적용 결과를 확인하세요.");}
  else if(M.IsStopping()){Title=TEXT("정지 요청됨 · 전체 정지 확인 중");Why=TEXT("미확인: ")+M.UnconfirmedEquipment();Hint=TEXT("장비 정지 적용을 기다리세요.\n공기·압력·밸브·RPM·계산 정지 확인 중");}
  else if(!S.Recovery.ForeignIds.IsEmpty()){
   Title=TEXT("배치 재개 차단 · 오원료 유입");Why=RecoverWhy;Hint=RecoverWhy+TEXT("\n배치 중단 후 초기화 절차로 진행하세요.");
  }
  else if(Ready){
   Title=Checked?TEXT("배치 상태 확인 완료 · 재개 가능"):TEXT("전체 정지 확인 완료 · 수동 조치 / 배치 상태 확인");
   Why=Checked?TEXT("확인된 배치 기록을 보존합니다. ‘남은 양 이어서 투입’을 명시적으로 누르세요."):TEXT("필요한 수동 조치 후 전체 정지를 다시 확인하고 ‘배치 상태 확인’을 누르세요. 계산만 재개해도 자동 투입은 시작되지 않습니다.");
   Hint=Checked?TEXT("재개 가능 · 확인된 양은 보존됩니다.\n남은 양 이어서 투입을 누르세요."):TEXT("전체 정지 확인 완료 · 배치 기록 보존\n배치 상태 확인으로 원료별 남은 양을 확인하세요.");
  }else{
   const bool Manual=Fresh&&!M.IsStopping()&&M.StopStage!=EProcessStopStage::Failed&&(S.RequestedRunning||S.Execution!=EPlantExecutionState::Paused);
   Title=Manual?TEXT("수동 조치 중 · 자동 배치 정지 유지"):TEXT("배치 재개 보류 · 상태 확인 필요");Why=RecoverWhy;
   Hint=RecoverWhy+TEXT("\n")+(Manual?TEXT("수동 조치 후 전체 정지 → 배치 상태 확인"):TEXT("사유를 해결한 뒤 배치 상태 확인을 다시 누르세요."));
  }
  // Preserve confirmed contamination/stop failures together with stale-data status.
  if(!Fresh){Why+=TEXT(" · 현재값 미확인: 최신 작업자 데이터 대기");if(!S.Error.IsEmpty())Why+=TEXT(" · ")+S.Error;}
  Put(TEXT("OperatorStage"),Title);Put(TEXT("OperatorReason"),Why);
  if(auto* Panel=Cast<UBorder>(W->GetWidgetFromName(TEXT("OperatorStatePanel"))))Panel->SetBrushColor(M.StopStage==EProcessStopStage::Failed||!S.Recovery.ForeignIds.IsEmpty()?FLinearColor(1,.86f,.85f):FLinearColor(.86f,.93f,.98f));
  FString Totals=TEXT("보존된 배치 · 원료별 상태\n");
  const bool Proven=Checked&&S.Recovery.Valid&&S.ModelParticleMassKg>0;
  for(int I=0;I<6;++I){if(!A.UsesSupplier(I))continue;
   const auto* Material=S.Recovery.Materials.FindByPredicate([&](const FPlantRecoveryMaterial& V){return V.Supplier==I;});
   Totals+=FString::Printf(TEXT("%c  "),TCHAR('A'+I));
   if(!Proven||!Material||Material->Unknown){Totals+=TEXT("확인 중 · 배치 상태 확인 필요\n");continue;}
   const auto& V=*Material;const double Unit=S.SourceKg(V.Supplier);const bool Final=!V.SelectedIds.IsEmpty();
   const double Passed=(V.SupplyConnector+V.Weighing+V.Pipe+V.Receiver+V.MixerConnector+V.Mixer)*Unit;
   const double Target=A.DoseFor(I,false);
   const double Remaining=Final?FMath::Max(0,V.SelectedIds.Num()-V.Mixer)*Unit:A.BatchMode==EProcessBatchMode::Full?V.Source*Unit:FMath::Max(0.,Target-Passed);
   Totals+=TEXT("이미 공급 ")+ProcessDisplay::Mass(Passed)+TEXT("\n수취 ")+ProcessDisplay::Mass(V.Receiver*Unit)+TEXT(" · 혼합기 ")+ProcessDisplay::Mass(V.Mixer*Unit);
   Totals+=TEXT("\n이동 중 ")+ProcessDisplay::Mass((V.SupplyConnector+V.Pipe+V.MixerConnector)*Unit)+TEXT(" · 남은 ")+(Final?TEXT("혼합기 이송 "):TEXT("공급 "))+ProcessDisplay::Mass(Remaining)+TEXT("\n");
   if(Final&&(V.Weighing+V.Pipe+V.SupplyConnector)>0)Totals+=TEXT("수취까지 남은 양 ")+ProcessDisplay::Mass((V.Weighing+V.Pipe+V.SupplyConnector)*Unit)+TEXT("\n");
  }
  if(A.MixerAppliedSimulationSeconds>=0&&A.MixingElapsedSeconds<A.MixingDurationSeconds)
   Totals+=FString::Printf(TEXT("%g RPM으로 남은 혼합 %.2g초 재개\n"),A.TargetMixerRPM,FMath::Max(0.,A.MixingDurationSeconds-A.MixingElapsedSeconds));
  if(Checked)Totals+=TEXT("이어갈 단계: ")+FProcessAutoRun::StageText(A.RecoveryNext)+TEXT("\n");
  Totals+=TEXT("시뮬레이션 계산값 · 모델 기준\n계산 적용 RPM ")+(Fresh&&S.AppliedRPMValid?ProcessDisplay::Number(S.AppliedRPM):FString(TEXT("미확인")));
  Put(TEXT("OperatorSummary"),Totals);
 }else if(A.Active){Hint=TEXT("자동 실행 중 · 현재 단계와 대기 이유는 위에 표시됩니다.\n전체 정지는 언제든 요청할 수 있습니다.");}
 else if(Same&&A.Stage==EProcessAutoStage::Complete){Hint=TEXT("배치 완료 · 확정 결과 보존\n")+(StartWhy.IsEmpty()?FString(TEXT("새 배치 시작 가능 · 적용값을 확인한 뒤 시작하세요.")):TEXT("새 배치 시작 불가: ")+StartWhy);}
 else{
  const FString RecipeState=C->RecipeRevision>0?TEXT("레시피 적용됨"):TEXT("레시피 미적용 · 개별 설정 사용 가능");
  Hint=StartWhy.IsEmpty()?RecipeState+TEXT(" · 시작 가능\n자동 배치 시작을 눌러 실행하세요."):RecipeState+TEXT("\n시작 불가: ")+StartWhy;
  if(!A.RunId.IsValid()||!Same){Put(TEXT("OperatorStage"),StartWhy.IsEmpty()?TEXT("시작 가능"):TEXT("시작 조건 확인 필요"));Put(TEXT("OperatorReason"),Hint);}
 }
 // Stop confirmation has precedence even when no recoverable batch exists or a batch already completed.
 if(!Recovery&&M.StopStage==EProcessStopStage::Failed){
  FString Why=M.StopDescription()+TEXT(" · ")+M.StopFailureEquipment;if(const auto* Stop=M.History.Find(M.StopRequest))Why+=TEXT(" · ")+Stop->Message;
  Put(TEXT("OperatorStage"),TEXT("전체 정지 실패 · 재시도 필요"));Put(TEXT("OperatorReason"),Why);Hint=Why+TEXT("\n전체 정지를 다시 요청하고 적용 결과를 확인하세요.");
 }else if(!Recovery&&M.IsStopping()){
  Put(TEXT("OperatorStage"),TEXT("정지 요청됨 · 전체 정지 확인 중"));Put(TEXT("OperatorReason"),TEXT("미확인: ")+M.UnconfirmedEquipment());Hint=TEXT("전체 정지 적용 확인 중 · ")+M.UnconfirmedEquipment();
 }else if(!Recovery&&!A.Active&&Same&&A.RecoveryAbandoned){
  Put(TEXT("OperatorStage"),TEXT("배치 중단 · 이력 보존"));Put(TEXT("OperatorReason"),A.RecoveryReason);Hint=TEXT("중단된 배치는 재개하지 않습니다.\n초기화 후 새 배치의 시작 조건을 확인하세요.");
 }
 if(Is3D())Hint=TEXT("3D 관찰 중 · 조작은 2D 운전 화면에서 진행하세요.\n전체 정지는 계속 사용할 수 있습니다.");
 if(!S.Error.IsEmpty()){
  if(auto* T=Cast<UTextBlock>(W->GetWidgetFromName(TEXT("OperatorReason"))))if(!T->GetText().ToString().Contains(S.Error))T->SetText(FText::FromString(T->GetText().ToString()+TEXT(" · 작업자 오류: ")+S.Error));
  Hint+=TEXT("\n작업자 오류: ")+S.Error;
 }
 Put(TEXT("OperatorActionHint"),ProcessDisplay::CompactText(Hint));
}
