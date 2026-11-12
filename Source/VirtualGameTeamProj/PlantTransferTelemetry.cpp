#include "PlantTransferTelemetry.h"

namespace
{
bool Near(double A,double B){return FMath::Abs(A-B)<1e-5;}
bool ValidCriteria(const FPlantTransferSettings& S)
{
    return FMath::IsFinite(S.WeighingResidualKg)&&S.WeighingResidualKg>=0&&
        FMath::IsFinite(S.PipeResidualKg)&&S.PipeResidualKg>=0&&FMath::IsFinite(S.MassErrorKg)&&S.MassErrorKg>=0&&
        FMath::IsFinite(S.StableSimulationSeconds)&&S.StableSimulationSeconds>0;
}
}

FPlantTransferTelemetry::FPlantTransferTelemetry(const powder::GranularConfig& C,const std::vector<powder::PlantRoute>& Routes,const FPlantTransferSettings& Settings)
    : ParticleCount(C.count), Criteria(Settings)
{
    // Read the actual plant drag density and actual float solver radius. No parcel/batch multiplier exists.
    if(!C.plant||C.binary_materials||C.plant_data.size()<16||C.plant_data[11].x!=0||!FMath::IsFinite(C.radius)||C.radius<=0)
        GeometryError=TEXT("현재 질량 구역은 FlowTest 전체 공정의 동일 구형 입자 모델만 지원합니다.");
    else
    {
        const double Density=C.plant_data[1].w;
        ParticleMassKg=(4.0/3.0)*UE_DOUBLE_PI*double(C.radius)*double(C.radius)*double(C.radius)*Density;
        const auto W=C.plant_data[powder::plant_weighing_gate(C.plant_data)];
        const auto R=C.plant_data[powder::plant_receiver_gate(C.plant_data)];
        WeighingGateZ=W.z;ReceiverGateZ=R.z;
        if(!FMath::IsFinite(ParticleMassKg)||ParticleMassKg<=0||!Near(W.x,0)||!Near(W.y,0)||!Near(W.z,.6)||!Near(W.w,.022)||
            !Near(R.x,3)||!Near(R.y,0)||!Near(R.z,2.16)||!Near(R.w,.139))
            GeometryError=TEXT("질량 집계 구역과 현재 해석 형상이 일치하지 않습니다.");
        int32 TransferRoutes=0;
        for(const auto& Route:Routes)if(Route.phase==1)
        {
            ++TransferRoutes;
            if(Route.cells.empty()){GeometryError=TEXT("이송 배관 구역 정보가 없습니다.");continue;}
            bool ValidIndices=true;
            for(int Cell:Route.cells)if(Cell<16||size_t(Cell)+2>=C.plant_data.size())ValidIndices=false;
            if(!ValidIndices){GeometryError=TEXT("이송 배관 인덱스가 유효하지 않습니다.");continue;}
            auto First=C.plant_data[Route.cells.front()], Last=C.plant_data[Route.cells.back()+1];
            ReceiverInletEnd=Last;
            if(!Near(First.x,0)||!Near(First.y,0)||!Near(First.z,1.8)||!Near(First.w,.35)||!Near(Last.x,2.77)||!Near(Last.z,2.682))
                GeometryError=TEXT("WH101→RH201 경계 계약과 이송 경로가 다릅니다.");
            bool Downstream=false;
            for(int Cell:Route.cells)
            {
                auto A=C.plant_data[Cell],B=C.plant_data[Cell+1];
                // Upstream gas cells also cover WH101. Cut the transfer pipe at the outlet gate centre plane.
                if(!Downstream)
                {
                    if(B.z>=WeighingGateZ)continue;
                    if(A.z>WeighingGateZ)
                    {
                        const double T=(A.z-WeighingGateZ)/(A.z-B.z);
                        A={float(A.x+(B.x-A.x)*T),float(A.y+(B.y-A.y)*T),float(WeighingGateZ),float(A.w+(B.w-A.w)*T)};
                    }
                    Downstream=true;
                }
                Pipes.Add({FVector3d(A.x,A.y,A.z),FVector3d(B.x,B.y,B.z),A.w,B.w});
                const FVector3d Margin(FMath::Max(A.w,B.w));
                PipeBounds+=FVector3d(A.x,A.y,A.z)-Margin;PipeBounds+=FVector3d(A.x,A.y,A.z)+Margin;
                PipeBounds+=FVector3d(B.x,B.y,B.z)-Margin;PipeBounds+=FVector3d(B.x,B.y,B.z)+Margin;
            }
        }
        if(TransferRoutes!=1||Pipes.IsEmpty())GeometryError=TEXT("유일한 WH101→RH201 배관을 확인할 수 없습니다.");
    }
    ResetConnection(0);
}

EPlantMassRegion FPlantTransferTelemetry::Classify(const powder::Grain& P) const
{
    if(!FMath::IsFinite(P.x)||!FMath::IsFinite(P.y)||!FMath::IsFinite(P.z)||!FMath::IsFinite(P.w))return EPlantMassRegion::Invalid;
    // Centre-of-particle ownership. Half-open height cuts; priority makes intersecting ports disjoint.
    // Collision_WH101_Body_ID700 / Cone_ID700_to_44_final / Outlet_Neck and ASSUMED_VT gate.
    if(P.z>=WeighingGateZ&&P.z<double(1.8f))
    {
        const double Radius=P.z<.7?.022:P.z<.95?.022+(P.z-.7)/.25*(.35-.022):.35;
        if(double(P.x)*P.x+double(P.y)*P.y<=Radius*Radius)return EPlantMassRegion::Weighing;
    }
    // RH201_130_Conical_reducer, 140_Discharge_neck, 110_Main_shell, 120_Upper_module, 121_Dished_lid.
    // Neck extends down to the BV272 gate. Inlet spigot stays pipe until the centre enters the receiver bore.
    if(P.z>=ReceiverGateZ&&P.z<3.688)
    {
        const double Profile[][2]={{2.16,.136},{2.292,.136},{2.307,.141},{2.487,.246},{3.63,.246},
            {3.641,.242},{3.662,.215},{3.679,.15},{3.687,.07},{3.688,.0001}};
        for(int I=1;I<UE_ARRAY_COUNT(Profile);++I)if(P.z<Profile[I][0])
        {
            const double T=(P.z-Profile[I-1][0])/(Profile[I][0]-Profile[I-1][0]);
            const double R=FMath::Lerp(Profile[I-1][1],Profile[I][1],T),X=double(P.x)-3;
            // RH201_210_Inlet_weld_neck protrudes into the body up to the route's final x plane.
            // Material still inside that inlet must remain pipe inventory even where the body bore overlaps.
            const double InletY=double(P.y)-ReceiverInletEnd.y, InletZ=double(P.z)-ReceiverInletEnd.z;
            const bool InInlet=P.x<ReceiverInletEnd.x && InletY*InletY+InletZ*InletZ<=double(ReceiverInletEnd.w)*ReceiverInletEnd.w;
            if(X*X+double(P.y)*P.y<=R*R&&!InInlet)return EPlantMassRegion::Receiver;
            break;
        }
    }
    const FVector3d Point(P.x,P.y,P.z);
    if(!PipeBounds.IsInsideOrOn(Point))return EPlantMassRegion::Outside;
    for(const auto& Cell:Pipes)
    {
        const double MaxR=FMath::Max(Cell.RA,Cell.RB);
        if(Point.X<FMath::Min(Cell.A.X,Cell.B.X)-MaxR||Point.X>FMath::Max(Cell.A.X,Cell.B.X)+MaxR||
           Point.Y<FMath::Min(Cell.A.Y,Cell.B.Y)-MaxR||Point.Y>FMath::Max(Cell.A.Y,Cell.B.Y)+MaxR||
           Point.Z<FMath::Min(Cell.A.Z,Cell.B.Z)-MaxR||Point.Z>FMath::Max(Cell.A.Z,Cell.B.Z)+MaxR)continue;
        const auto Axis=Cell.B-Cell.A;
        // Round joint caps avoid unclassified wedges between short bend segments.
        const double T=FMath::Clamp(FVector3d::DotProduct(Point-Cell.A,Axis)/Axis.SquaredLength(),0.0,1.0);
        const double R=FMath::Lerp(Cell.RA,Cell.RB,T);
        if((Point-(Cell.A+T*Axis)).SquaredLength()<=R*R)return EPlantMassRegion::Pipe;
    }
    return EPlantMassRegion::Outside;
}

void FPlantTransferTelemetry::ResetConnection(int64 Connection)
{
    Transfer=FPlantTransferSnapshot();Transfer.Connection=Connection;Transfer.Criteria=Criteria;
    Transfer.Validity=GeometryError.IsEmpty()?EPlantValueValidity::Valid:EPlantValueValidity::Unsupported;
    Transfer.Reason=GeometryError.IsEmpty()?TEXT("칭량호퍼 출구를 열면 수동 이송 관찰을 시작합니다."):GeometryError;
    Membership.Empty();StableSince=LastTime=-1;
}

bool FPlantTransferTelemetry::Collect(const std::vector<powder::Grain>& Positions)
{
    const FDateTime Now=FDateTime::UtcNow();
    for(auto* M:{&Weighing,&Pipe,&Receiver}){M->Validity=EPlantValueValidity::Unknown;M->UpdatedUtc=Now;M->Kg=0;}
    if(!GeometryError.IsEmpty()||Positions.size()!=ParticleCount)return false;
    Regions.SetNum(int32(Positions.size()));int32 Counts[3]={};
    for(int32 I=0;I<Regions.Num();++I)
    {
        const auto Region=Classify(Positions[I]);Regions[I]=Region;
        if(Region==EPlantMassRegion::Invalid)return false;
        if(Region==EPlantMassRegion::Weighing)++Counts[0];
        else if(Region==EPlantMassRegion::Pipe)++Counts[1];
        else if(Region==EPlantMassRegion::Receiver)++Counts[2];
    }
    Weighing.Kg=Counts[0]*ParticleMassKg;Pipe.Kg=Counts[1]*ParticleMassKg;Receiver.Kg=Counts[2]*ParticleMassKg;
    Weighing.Validity=Pipe.Validity=Receiver.Validity=EPlantValueValidity::Valid;return true;
}

void FPlantTransferTelemetry::Indeterminate(const FString& Reason)
{
    if(Transfer.State!=EPlantTransferState::Indeterminate)Transfer.Reason=Reason;
    Transfer.State=EPlantTransferState::Indeterminate;Transfer.StableSeconds=0;StableSince=-1;
}

void FPlantTransferTelemetry::Begin(const std::vector<powder::Grain>& Positions,double Time,int64 Revision,bool ReceiverOpen)
{
    const auto Connection=Transfer.Connection;ResetConnection(Connection);
    Transfer.Id=++NextId;Transfer.OpeningControlRevision=Revision;Transfer.StartedSimulationSeconds=Time;
    if(!Collect(Positions))
    {Indeterminate(GeometryError.IsEmpty()?TEXT("입자 좌표·개수가 유효하지 않아 시작 기준량을 확보하지 못했습니다."):GeometryError);Transfer.Validity=EPlantValueValidity::Unknown;return;}
    Transfer.InitialReceiverKg=Receiver.Kg;Transfer.BaselineKg=Weighing.Kg;
    Membership.Init(0,Regions.Num());
    for(int32 I=0;I<Regions.Num();++I)Membership[I]=Regions[I]==EPlantMassRegion::Weighing?1:Regions[I]==EPlantMassRegion::Receiver?2:0;
    Transfer.State=Transfer.BaselineKg==0?EPlantTransferState::Empty:EPlantTransferState::Moving;
    if(!ValidCriteria(Criteria))Indeterminate(TEXT("이송 시험 기준이 유효하지 않습니다. 허용치는 0 이상, 안정 시간은 양수여야 합니다."));
    if(Pipe.Kg>0)Indeterminate(TEXT("시작 시 배관에 이전 원료가 남아 있습니다. 새 이송과 분리된 완료 확인이 필요합니다."));
    Observe(Positions,Time,false,ReceiverOpen);
}

void FPlantTransferTelemetry::ReceiverDischarging()
{
    if(Transfer.Id>0)Indeterminate(TEXT("수취호퍼 출구 개방·배출 요청으로 이번 도착량의 보존을 확인할 수 없습니다."));
}

void FPlantTransferTelemetry::Observe(const std::vector<powder::Grain>& Positions,double Time,bool OutletOpen,bool ReceiverOpen)
{
    Transfer.OutletAppliedOpen=OutletOpen;Transfer.UpdatedUtc=FDateTime::UtcNow();Transfer.ObservedSimulationSeconds=Time;
    if(!Collect(Positions)||!FMath::IsFinite(Time)||Time<LastTime)
    {if(Transfer.Id>0)Indeterminate(TEXT("좌표·입자 ID 개수·해석 시간 확인 불가"));Transfer.Validity=EPlantValueValidity::Unknown;return;}
    LastTime=Time;
    if(!GeometryError.IsEmpty())return;
    Transfer.Validity=EPlantValueValidity::Valid;
    if(Transfer.Id==0)return;
    if(Membership.Num()!=Regions.Num()){Indeterminate(TEXT("입자 ID 연결이 변경되었습니다."));return;}
    int32 Remaining=0,Transit=0,Arrived=0,Unknown=0;
    FString ForeignReason;
    for(int32 I=0;I<Regions.Num();++I)
    {
        const auto R=Regions[I];
        if(Membership[I]==1)
        {
            if(R==EPlantMassRegion::Weighing)++Remaining;
            else if(R==EPlantMassRegion::Pipe)++Transit;
            else if(R==EPlantMassRegion::Receiver)++Arrived;
            else ++Unknown;
        }
        else if(ForeignReason.IsEmpty())
        {
            if(Membership[I]==2&&R!=EPlantMassRegion::Receiver)
                ForeignReason=TEXT("기존 수취 원료가 수취호퍼 구역을 벗어났습니다. 이번 도착량의 보존을 확인할 수 없습니다.");
            else if(R==EPlantMassRegion::Weighing)
                ForeignReason=TEXT("이송 시작 후 칭량호퍼에 새 원료가 유입되었습니다. 이번 회차의 단독 이송 완료를 판정할 수 없습니다.");
            else if(R==EPlantMassRegion::Pipe)
                ForeignReason=TEXT("이송 기준에 없는 입자가 배관에 들어왔습니다. 이번 이송분만의 완료를 판정할 수 없습니다.");
            else if(R==EPlantMassRegion::Receiver&&Membership[I]!=2)
                ForeignReason=TEXT("이송 기준에 없는 입자가 수취호퍼에 들어왔습니다. 이번 이송분만의 완료를 판정할 수 없습니다.");
        }
    }
    Transfer.RemainingKg=Remaining*ParticleMassKg;Transfer.InTransitKg=Transit*ParticleMassKg;
    Transfer.ArrivedKg=Arrived*ParticleMassKg;Transfer.UnclassifiedKg=Unknown*ParticleMassKg;
    if(!ForeignReason.IsEmpty())Indeterminate(ForeignReason);
    if(Unknown)Indeterminate(TEXT("이번 이송 입자가 칭량호퍼·배관·수취호퍼 밖에 있습니다. 미분류량을 확인하세요."));
    if(ReceiverOpen)ReceiverDischarging();
    if(Transfer.State==EPlantTransferState::Indeterminate)return;
    if(Transfer.BaselineKg==0){Transfer.State=EPlantTransferState::Empty;Transfer.Reason=TEXT("이송할 원료 없음 · 개방 직전 칭량호퍼 0 g");return;}
    // Require both total inventories (foreign residues included) and cohort mass conservation.
    const bool Within=Weighing.Kg<=Criteria.WeighingResidualKg&&Pipe.Kg<=Criteria.PipeResidualKg&&Unknown==0&&
        FMath::Abs(Transfer.ArrivedKg-Transfer.BaselineKg)<=Criteria.MassErrorKg;
    if(!Within){StableSince=-1;Transfer.StableSeconds=0;Transfer.State=EPlantTransferState::Moving;
        Transfer.Reason=Transit>0?TEXT("배관에 원료가 남아 있습니다."):!OutletOpen?TEXT("칭량호퍼 출구 닫힘 · 잔량 이동 대기"):TEXT("잔량·도착량 조건 확인 중");return;}
    if(StableSince<0)StableSince=Time;
    Transfer.StableSeconds=Time-StableSince;
    if(Transfer.StableSeconds>=Criteria.StableSimulationSeconds)
    {Transfer.State=EPlantTransferState::Complete;Transfer.Reason=TEXT("잔량·이번 도착량·안정 관찰 조건 충족 · 시험 기준");}
    else {Transfer.State=EPlantTransferState::Moving;Transfer.Reason=TEXT("도착량 일치 · 해석 시간으로 안정 관찰 중");}
}

void FPlantTransferTelemetry::Publish(FPlantSimulationSnapshot& S) const
{
    S.WeighingMass=Weighing;S.PipeMass=Pipe;S.ReceiverMass=Receiver;S.ModelParticleMassKg=ParticleMassKg;
    S.MassDiagnostic=GeometryError;S.Transfer=Transfer;
    if(Weighing.Validity==EPlantValueValidity::Valid&&ParticleMassKg>0)
    {S.InWeighingHopper=FMath::RoundToInt(Weighing.Kg/ParticleMassKg);S.InReceiver=FMath::RoundToInt(Receiver.Kg/ParticleMassKg);}
}
