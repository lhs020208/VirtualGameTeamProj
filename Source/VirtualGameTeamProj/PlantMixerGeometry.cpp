#include "PlantMixerGeometry.h"

namespace {
double WallRay(const std::vector<powder::Grain>& D,FVector3d O,FVector3d V)
{
 double Hit=2.;TArray<int32> Stack;Stack.Add(int32(D[0].x));
 while(!Stack.IsEmpty()){
  const int32 N=Stack.Pop(EAllowShrinking::No);const auto A=D[N],B=D[N+1];
  double Lo=0,Hi=Hit;
  for(int K=0;K<3;++K){const double L=K==0?A.x:K==1?A.y:A.z,H=K==0?B.x:K==1?B.y:B.z;
   if(FMath::Abs(V[K])<1e-12){if(O[K]<L-1e-7||O[K]>H+1e-7){Hi=-1;break;}}
   else{const double P=(L-O[K])/V[K],Q=(H-O[K])/V[K];Lo=FMath::Max(Lo,FMath::Min(P,Q));Hi=FMath::Min(Hi,FMath::Max(P,Q));}}
  if(Hi<Lo)continue;
  if(A.w>=0){Stack.Add(int32(A.w));Stack.Add(int32(B.w));continue;}
  for(int I=0;I<int(-A.w);++I){const int At=int(B.w)+3*I;
   const auto P=D[At],Q=D[At+1],R=D[At+2];const FVector3d X(P.x,P.y,P.z),Y(Q.x,Q.y,Q.z),Z(R.x,R.y,R.z);
   const auto E1=Y-X,E2=Z-X,H=FVector3d::CrossProduct(V,E2);const double Det=FVector3d::DotProduct(E1,H);
   if(FMath::Abs(Det)<1e-13)continue;const auto S=O-X;const double U=FVector3d::DotProduct(S,H)/Det;
   if(U<-1e-6||U>1+1e-6)continue;const auto C=FVector3d::CrossProduct(S,E1);const double W=FVector3d::DotProduct(V,C)/Det;
   if(W<-1e-6||U+W>1+1e-6)continue;const double T=FVector3d::DotProduct(E2,C)/Det;
   if(T>1e-7&&T<Hit)Hit=T;
  }
 }return Hit;
}
}
FPlantMixerGeometry::FPlantMixerGeometry(const powder::GranularConfig& C,bool Enabled):Supported(Enabled)
{
 if(!Enabled)return;
 if(C.plant_data.size()<16){Error=TEXT("혼합기 해석 형상 데이터 없음");return;}
 const auto& D=C.plant_data;const auto G=D[powder::plant_receiver_gate(D)];ReceiverGateZ=G.z;
 CentreX=D[1].x;CentreY=D[1].y;AxisZ=D[1].z;
 if(FMath::Abs(G.x-CentreX)>1e-5||FMath::Abs(G.y-CentreY)>1e-5||FMath::Abs(G.z-2.16)>1e-5){Error=TEXT("현재 LowPoly 수취·혼합기 형상 계약 불일치");return;}
 // Read the active vessel instead of using the former half-size mixer bounds.
 // Probe away from the end shaft seals and the inlet hole. This supports the
 // existing elliptical trough and the revised circular trough, not arbitrary vessels.
 const FVector3d O(CentreX,CentreY,AxisZ+.1);
 MinX=CentreX-WallRay(D,O,{-1,0,0});MaxX=CentreX+WallRay(D,O,{1,0,0});
 FloorZ=O.Z-WallRay(D,O,{0,0,-1});RadiusZ=AxisZ-FloorZ;
 RadiusY=WallRay(D,{CentreX,CentreY,AxisZ+.01},{0,1,0});
 ChamberTopZ=O.Z+WallRay(D,{CentreX-.1,CentreY,O.Z},{0,0,1});
 if(MinX>=CentreX-.1||MaxX<=CentreX+.1||MaxX-MinX>1.8||RadiusY<.1||RadiusY>.5||
    RadiusZ<.05||RadiusZ>.5||ChamberTopZ<=AxisZ+.1||ChamberTopZ>=1.31){Error=TEXT("혼합기 집계 경계 측정 실패");return;}
 for(double X:{CentreX-.04,CentreX+.04})for(double F:{-.75,-.5,0.,.3})for(double Sign:{-1.,1.}){
  const double R=RadiusY*(F<0?FMath::Sqrt(1-F*F):1.);
  if(FMath::Abs(WallRay(D,{X,CentreY,AxisZ+F*RadiusZ},{0,Sign,0})-R)>1e-4){Error=TEXT("혼합기 집계 경계와 현재 충돌 삼각형 불일치");return;}
 }
 // The reducer and nozzle remain unchanged. Just above the lid avoids counting
 // coplanar lid triangles as the cylindrical inlet wall.
 const double Heights[]={ChamberTopZ,1.31,1.35,1.685,1.71,1.89,2.11,2.12,2.13,2.14,2.15,double(G.z)};
 for(double Z:Heights){double Radius=0;
  const double ProbeZ=Z==ChamberTopZ?Z+1e-5:Z;
  for(int I=0;I<8;++I){const double T=UE_DOUBLE_PI*2*I/8;Radius=FMath::Max(Radius,WallRay(D,{CentreX,CentreY,ProbeZ},{FMath::Cos(T),FMath::Sin(T),0}));}
  if(Radius<.021||Radius>.141){Error=TEXT("수취→혼합기 연결부 내경 확인 불가");return;}
  ConnectorProfile.Add({Z,Radius+1e-6}); // float triangle precision only; not a mass tolerance
 }
}
EPlantMixerRegion FPlantMixerGeometry::Classify(const powder::Grain& P) const
{
 if(!FMath::IsFinite(P.x)||!FMath::IsFinite(P.y)||!FMath::IsFinite(P.z))return EPlantMixerRegion::Invalid;
 if(!Supported||!Error.IsEmpty())return EPlantMixerRegion::Outside;
 const double X=double(P.x)-CentreX,Y=double(P.y)-CentreY,Z=P.z;
 if(Z>=ChamberTopZ&&Z<ReceiverGateZ){
  for(int I=1;I<ConnectorProfile.Num();++I)if(Z<=ConnectorProfile[I].X){
   const auto A=ConnectorProfile[I-1],B=ConnectorProfile[I];const double R=FMath::Lerp(A.Y,B.Y,(Z-A.X)/(B.X-A.X));
   return X*X+Y*Y<=R*R?EPlantMixerRegion::Connector:EPlantMixerRegion::Outside;
  }
 }
 if(P.x>MinX&&P.x<MaxX&&Z>=FloorZ&&Z<ChamberTopZ){
  const double R=Z<AxisZ?RadiusY*FMath::Sqrt(FMath::Max(0.,1-FMath::Square((Z-AxisZ)/RadiusZ))):RadiusY;
  if(FMath::Abs(Y)<=R)return EPlantMixerRegion::Mixer;
 }
 return EPlantMixerRegion::Outside;
}
