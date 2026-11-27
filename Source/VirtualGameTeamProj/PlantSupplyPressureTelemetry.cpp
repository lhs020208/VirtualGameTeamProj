#include "PlantSupplyPressureTelemetry.h"

namespace {
FVector Position(const powder::Grain& G){return FVector(G.x,G.y,G.z);}
double Loss(const std::vector<powder::Grain>& D,int32 At,double Q)
{
 const auto A=D[At],B=D[At+1],V=D[At+2];
 const double L=std::hypot(std::hypot(B.x-A.x,B.y-A.y),B.z-A.z);
 return V.z==1?powder::plant_cell_loss(Q,L,2*A.w*B.w/(A.w+B.w),V.y,4*A.w*B.w):powder::plant_cell_loss(Q,L,FMath::Min(A.w,B.w),V.y);
}
}
FPlantSupplyPressureTelemetry::FPlantSupplyPressureTelemetry(const std::vector<powder::Grain>& D,const std::vector<powder::PlantRoute>& Routes)
{
 Paths.SetNum(6);
 try{
  if(D.size()<16)throw std::runtime_error("missing plant header");
  const auto Gates=powder::plant_supplier_gates(D);if(Gates.size()!=6)throw std::runtime_error("six supplier gates required");
  int32 Supplier=0;
  for(int32 Ri=0;Ri<int32(Routes.size());++Ri){const auto& R=Routes[Ri];if(R.phase!=0)continue;
   if(Supplier>=6)throw std::runtime_error("extra supply route");auto& P=Paths[Supplier];P.Route=Ri;P.Gate=Gates[Supplier++];
   const FVector Gate=Position(D[P.Gate]);int32 Matches=0;
   for(int32 Ci=0;Ci<int32(R.cells.size());++Ci){const int32 At=R.cells[Ci];
    if(At<int(D[0].z)||At+2>=int(D.size())||(At-int(D[0].z))%3||D[At+2].w!=0)throw std::runtime_error("route cell mismatch");
    const FVector A=Position(D[At]),B=Position(D[At+1]);
    // Use the existing cell's endpoints, not an invented valve coefficient or infinitesimal taps.
    if(A.Z>Gate.Z+1e-6&&B.Z<Gate.Z-1e-6){
     const FVector Hit=A+(B-A)*((Gate.Z-A.Z)/(B.Z-A.Z));
     if(FVector::Distance(Hit,Gate)<=1e-4){++Matches;P.Cell=Ci;P.Before=A;P.After=B;}
    }
   }
   // The existing loss solver follows route membership; remote terminal cells can
   // have a geometric gap. Validate the pressure taps and their upstream chain,
   // without silently inventing an extra downstream loss or changing that model.
   if(Matches==1)for(int32 Ci=1;Ci<=P.Cell+1&&Ci<int32(R.cells.size());++Ci)
    if(FVector::Distance(Position(D[R.cells[Ci-1]+1]),Position(D[R.cells[Ci]]))>1e-4)throw std::runtime_error("disconnected valve approach cells");
   if(Matches!=1){P.Cell=-1;P.Error=TEXT("게이트를 감싸는 단일 유로 셀 위치 미확인");}
  }
  if(Supplier!=6)throw std::runtime_error("missing supply route");
 }catch(const std::exception& E){for(auto& P:Paths){P.Cell=-1;P.Error=UTF8_TO_TCHAR(E.what());}}
}
TArray<FPlantSupplyValvePressure> FPlantSupplyPressureTelemetry::Observe(const std::vector<powder::Grain>& D,const std::vector<powder::PlantRoute>& Routes,
 const FVector& Applied,uint32 Open,uint32 Air,bool Regulated,int32 PurgeSupplier,bool Purging,const TArray<double>& Sources,const std::vector<double>* Apertures) const
{
 TArray<FPlantSupplyValvePressure> Out;Out.SetNum(6);
 for(int32 I=0;I<6;++I){auto& V=Out[I];const auto& P=Paths[I];V.Supplier=I;V.Route=P.Route;V.GateCell=P.Cell;V.GateDataIndex=P.Gate;
  V.BeforeMetres=P.Before;V.AfterMetres=P.After;V.Mapped=P.Cell>=0;const double Supply=Sources.IsValidIndex(I)?Sources[I]:Applied.X;V.HopperDifferencePa=Supply-Applied.Y;
  if(!Regulated){V.Reason=TEXT("압력 경계조건 모델 미지원");continue;}
  if(!(Open&(1u<<I))){V.Reason=Purging&&PurgeSupplier==I?TEXT("하류 연결부 공기만 작동 · 공급 밸브 닫힘 · 차압 미산출"):TEXT("닫힘 · 차압 미산출");continue;}
  if(!(Air&(1u<<I))){V.Reason=TEXT("닫힘 처리 중 · 공급 공기 차단 · 차압 미산출");continue;}
  if(!V.Mapped){V.Reason=TEXT("경로 위치 미확인 · 차압 미산출 · ")+P.Error;continue;}
  try{
   const auto& R=Routes[P.Route];const double Q=D[R.cells.front()+2].x;double Sum=0,Before=0,After=0;
   V.FlowValid=FMath::IsFinite(Q);if(V.FlowValid)V.FlowM3s=Q;
   if(!V.FlowValid){V.Reason=TEXT("유량 계산값 미확인 · 차압 미산출");continue;}
   if(Q==0){V.Reason=TEXT("밸브 열림 · 통과 유량 0 · 차압 미산출");continue;}
   for(int32 Ci=0;Ci<int32(R.cells.size());++Ci){const int32 At=R.cells[Ci];if(D[At+2].x!=Q)throw std::runtime_error("nonuniform uploaded flow");
    if(Ci==P.Cell)Before=Sum;Sum+=Loss(D,At,Q);if(Apertures&&Ci==P.Cell&&Apertures->at(I)>1.e-8){double Area=PI*D[P.Gate].w*D[P.Gate].w,Aperture=Apertures->at(I);Sum+=.6*Q*Q/(Area*Area)*FMath::Max(0.,1/(Aperture*Aperture)-1)/(.62*.62);}if(Ci==P.Cell)After=Sum;
   }
   V.FlowM3s=Q;V.RouteLossPa=Sum;V.BoundaryResidualPa=Supply-Sum-Applied.Y;
   if(FMath::Abs(V.BoundaryResidualPa)>.01)throw std::runtime_error("field pressure boundary residual exceeds telemetry precision");
   V.BeforeGaugePa=Supply-Before;V.AfterGaugePa=Supply-After;V.DifferencePa=V.BeforeGaugePa-V.AfterGaugePa;V.Valid=true;
   V.Reason=Apertures?TEXT("모델 추정값 · 실제 각도의 개구율/오리피스 손실 포함 · 센서값 아님"):TEXT("모델 추정값 · 기존 유로 셀 손실 · 밸브 전용 손실/센서값 아님");
  }catch(const std::exception& E){V.Valid=false;V.FlowValid=false;V.BeforeGaugePa=V.AfterGaugePa=V.DifferencePa=0;V.Reason=TEXT("유로 계산 미확인 · 차압 미산출 · ")+FString(UTF8_TO_TCHAR(E.what()));}
 }
 return Out;
}
