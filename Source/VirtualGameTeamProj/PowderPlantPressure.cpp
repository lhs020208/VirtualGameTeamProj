#include "PowderPlantTest.h"
#include "powder/plant_pressure.hpp"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Text/STextBlock.h"
namespace {
FString ProfilePath(){return FPaths::ProjectSavedDir()/TEXT("PlantPressure_LowPoly20261110.json");}
const TCHAR* Names[]{TEXT("대기"),TEXT("충전"),TEXT("칭량"),TEXT("이송"),TEXT("배출"),TEXT("혼합")};
const TCHAR* Hints[]{TEXT("권장: 모든 밸브 닫힘·혼합기 정지"),TEXT("권장: 선택 공급기 열기, 칭량·수취 출구 닫기"),TEXT("권장: 공급기·칭량 출구 닫기"),TEXT("권장: 공급기 닫기, 칭량 열기, 수취 출구 닫기"),TEXT("권장: 칭량 닫기, 수취→혼합기 열기"),TEXT("권장: 수취 출구 닫고 혼합기 시작")};
}
bool APowderPlantTest::SetPressureTargets(double s,double w,double r){
 if(!RegulatedPressureEnabled){PressureMessage=TEXT("이 레벨은 기존 압력 모드입니다.");return false;}
 try{powder::validate_plant_pressure_targets({s,w,r});}catch(const std::exception&){PressureMessage=TEXT("거부: 유한한 공급/칭량 -1.0~+1.0, 수취 -1.0~0 kPa(g)만 입력하세요.");return false;}
 SupplyGaugePa=s;WeighingGaugePa=w;ReceiverGaugePa=r;PressureManualOverride=true;PressureMessage=TEXT("수동값 유지. 계산 중 다음 배치에 적용; 일시정지 중에는 대기합니다.");return true;
}
bool APowderPlantTest::ApplyPressurePreset(int32 stage){
 if(!RegulatedPressureEnabled)return false;
 try{auto p=powder::plant_pressure_preset(stage);if(!SetPressureTargets(p.supply,p.weighing,p.receiver))return false;PressureStage=stage;PressureManualOverride=false;PressureMessage=TEXT("단계 시험값 적용. 밸브와 혼합기는 자동 변경하지 않습니다.");return true;}
 catch(const std::exception&){PressureMessage=TEXT("거부: 잘못된 단계입니다.");return false;}
}
bool APowderPlantTest::SavePressureProfile(){
 if(!RegulatedPressureEnabled)return false;
 try{powder::validate_plant_pressure_targets({SupplyGaugePa,WeighingGaugePa,ReceiverGaugePa});auto preset=powder::plant_pressure_preset(PressureStage);if(!PressureManualOverride&&(SupplyGaugePa!=preset.supply||WeighingGaugePa!=preset.weighing||ReceiverGaugePa!=preset.receiver))throw std::invalid_argument("preset/profile mismatch");}catch(const std::exception&){PressureMessage=TEXT("거부: 유효하지 않은 설정은 저장하지 않습니다.");return false;}
 auto obj=MakeShared<FJsonObject>();obj->SetNumberField(TEXT("schema"),1);obj->SetNumberField(TEXT("supply_gauge_Pa"),SupplyGaugePa);obj->SetNumberField(TEXT("weighing_gauge_Pa"),WeighingGaugePa);obj->SetNumberField(TEXT("receiver_gauge_Pa"),ReceiverGaugePa);obj->SetNumberField(TEXT("stage"),PressureStage);obj->SetBoolField(TEXT("manual"),PressureManualOverride);
 FString text;auto writer=TJsonWriterFactory<>::Create(&text);bool ok=FJsonSerializer::Serialize(obj,writer)&&FFileHelper::SaveStringToFile(text,*ProfilePath());PressureMessage=ok?TEXT("압력 프로필 저장 완료 (다음 시작에 자동 로드하지 않음)"):TEXT("압력 프로필 저장 실패");return ok;
}
bool APowderPlantTest::LoadPressureProfile(){
 if(!RegulatedPressureEnabled)return false;
 FString text;TSharedPtr<FJsonObject> obj;double schema,s,w,r,stage;bool manual;
 bool valid=FFileHelper::LoadFileToString(text,*ProfilePath())&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(text),obj)&&obj.IsValid();
 if(valid)valid=obj->TryGetNumberField(TEXT("schema"),schema)&&schema==1&&obj->TryGetNumberField(TEXT("supply_gauge_Pa"),s)&&obj->TryGetNumberField(TEXT("weighing_gauge_Pa"),w)&&obj->TryGetNumberField(TEXT("receiver_gauge_Pa"),r)&&obj->TryGetNumberField(TEXT("stage"),stage)&&std::isfinite(stage)&&stage>=0&&stage<=5&&std::floor(stage)==stage&&obj->TryGetBoolField(TEXT("manual"),manual);
 if(valid){try{powder::validate_plant_pressure_targets({s,w,r});auto preset=powder::plant_pressure_preset(int(stage));if(!manual&&(s!=preset.supply||w!=preset.weighing||r!=preset.receiver))valid=false;}catch(const std::exception&){valid=false;}}
 if(!valid){PressureMessage=TEXT("불러오기 거부: 파일 없음/형식·범위 오류. 현재 설정을 유지합니다.");return false;}
 SupplyGaugePa=s;WeighingGaugePa=w;ReceiverGaugePa=r;PressureStage=int32(stage);PressureManualOverride=manual;PressureMessage=TEXT("압력 프로필 불러옴. 밸브·입자는 변경하지 않았습니다.");return true;
}
TSharedRef<SWidget> APowderPlantTest::BuildPressurePanel(){
 const TWeakObjectPtr<APowderPlantTest> weak(this);auto panel=SNew(SVerticalBox);
 panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("시험 압력 · 실제 설비 안전값 아님\nkPa(g): 대기=0 / 절대압=101.325+표시값\n이상적 조절기 유지압 · 압축 과도해석 아님")))];
 for(int row=0;row<2;++row){auto bar=SNew(SHorizontalBox);for(int col=0;col<3;++col){int stage=row*3+col;bar->AddSlot().FillWidth(1).Padding(1)[SNew(SButton).IsFocusable(false).OnClicked_Lambda([weak,stage](){if(weak.IsValid())weak->ApplyPressurePreset(stage);return FReply::Handled();})[SNew(STextBlock).Text(FText::FromString(Names[stage]))]];}panel->AddSlot().AutoHeight()[bar];}
 panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([weak](){if(!weak.IsValid())return FText::GetEmpty();int s=FMath::Clamp(weak->PressureStage,0,5);return FText::FromString(FString::Printf(TEXT("%s · %s\n%s"),Names[s],weak->PressureManualOverride?TEXT("수동값 우선"):TEXT("단계 시험값"),Hints[s]));})];
 const TCHAR* labels[]{TEXT("공급 공통"),TEXT("칭량"),TEXT("수취 흡입")};
 for(int i=0;i<3;++i)panel->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
  +SHorizontalBox::Slot().FillWidth(1)[SNew(STextBlock).Text(FText::FromString(labels[i]))]
  +SHorizontalBox::Slot().FillWidth(1)[SNew(SNumericEntryBox<double>).AllowSpin(true).MinValue(-1.0).MaxValue(i==2?0.:1.0).MinSliderValue(-1.0).MaxSliderValue(i==2?0.:1.0).Delta(.1)
   .Value_Lambda([weak,i]()->TOptional<double>{if(!weak.IsValid())return {};return (i==0?weak->SupplyGaugePa:i==1?weak->WeighingGaugePa:weak->ReceiverGaugePa)/1000.;})
   .OnValueCommitted_Lambda([weak,i](double value,ETextCommit::Type){if(!weak.IsValid())return;weak->SetPressureTargets(i==0?value*1000:weak->SupplyGaugePa,i==1?value*1000:weak->WeighingGaugePa,i==2?value*1000:weak->ReceiverGaugePa);})]];
 panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([weak](){if(!weak.IsValid())return FText::GetEmpty();if(!weak->PressureApplied)return FText::FromString(TEXT("적용값 준비 중 · 입력 후 Enter"));auto a=weak->AppliedGaugePa;return FText::FromString(FString::Printf(TEXT("적용 S/W/R: %.3f / %.3f / %.3f kPa(g)\n차압 S→W %.3f / W→R %.3f kPa\n이송 %.2f L/s · 칭량 보충/배기 %+.2f L/s\n수취 출구 열림: 수취 적용압 0으로 전환"),a.X/1000,a.Y/1000,a.Z/1000,(a.X-a.Y)/1000,(a.Y-a.Z)/1000,weak->TransferFlowM3s*1000,weak->WeighingRegulatorM3s*1000));})];
 auto controls=SNew(SHorizontalBox);for(int i=0;i<3;++i){const TCHAR* text=i==0?TEXT("저장"):i==1?TEXT("불러오기"):TEXT("단계값 복원");controls->AddSlot().FillWidth(1).Padding(1)[SNew(SButton).IsFocusable(false).OnClicked_Lambda([weak,i](){if(weak.IsValid()){if(i==0)weak->SavePressureProfile();else if(i==1)weak->LoadPressureProfile();else weak->ApplyPressurePreset(weak->PressureStage);}return FReply::Handled();})[SNew(STextBlock).Text(FText::FromString(text))]];}panel->AddSlot().AutoHeight()[controls];
 panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([weak](){return weak.IsValid()?FText::FromString(weak->PressureMessage):FText::GetEmpty();})];return panel;
}
