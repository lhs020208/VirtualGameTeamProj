#include "PlantFeedPreparation.h"

FPlantFeedPreparation::FPlantFeedPreparation(const powder::GranularConfig& C,const std::vector<powder::PlantRoute>& Routes,
    const FPlantTransferTelemetry& Telemetry,double Required,bool Enabled)
    : Mass(Telemetry), Count(C.count), Radius(C.radius), RequiredStable(Required)
{
    State.Enabled=Enabled;State.RequiredStableSeconds=Required;
    if(!Enabled)return;
    FPlantSimulationSnapshot Initial;Mass.Publish(Initial);
    if(!Initial.MassDiagnostic.IsEmpty()){GeometryError=Initial.MassDiagnostic;return;}
    const auto Gates=powder::plant_supplier_gates(C.plant_data);
    if(Gates.size()!=6||!FMath::IsFinite(Required)||Required<=0||Mass.ParticleMassKg<=0)
    {GeometryError=TEXT("공급 준비 구역 또는 안정 시간 설정을 확인할 수 없습니다.");return;}
    int32 Index=0;
    for(const auto& Route:Routes)if(Route.phase==0)
    {
        if(Index>=int32(Gates.size())){GeometryError=TEXT("공급 밸브와 유로 수가 다릅니다.");break;}
        FSupply Supply;Supply.Gate=C.plant_data[Gates[Index++]];bool Downstream=false;
        for(int At:Route.cells)
        {
            if(At<16||size_t(At)+2>=C.plant_data.size()){GeometryError=TEXT("공급 유로 인덱스가 유효하지 않습니다.");break;}
            auto A=C.plant_data[At],B=C.plant_data[At+1],Meta=C.plant_data[At+2];
            if(!Downstream)
            {
                if(!(A.z>=Supply.Gate.z&&B.z<Supply.Gate.z))continue;
                const double T=(A.z-Supply.Gate.z)/(A.z-B.z);
                A={float(A.x+(B.x-A.x)*T),float(A.y+(B.y-A.y)*T),Supply.Gate.z,float(A.w+(B.w-A.w)*T)};
                if(FMath::Abs(A.x-Supply.Gate.x)>1e-5||FMath::Abs(A.y-Supply.Gate.y)>1e-5)
                    GeometryError=TEXT("공급 유로가 실제 밸브 평면과 일치하지 않습니다.");
                Downstream=true;
            }
            const FVector3d V(A.x,A.y,A.z),W(B.x,B.y,B.z);
            if((W-V).SquaredLength()<1e-14||A.w<=0||B.w<=0||Meta.w!=0||(Meta.z!=0&&Meta.z!=1))
            {GeometryError=TEXT("공급 연결부 형상 형식을 확인할 수 없습니다.");break;}
            Supply.Cells.Add({V,W,A.w,B.w,Meta.z==1});
            // A/C inlet boxes use half-width A.w and half-height B.w, as plant_air does.
            // Particle-radius padding covers joints conservatively; it does not change any collision/flow field.
            const FVector3d Margin(FMath::Max(A.w,B.w)+Radius);
            Supply.Bounds+=V-Margin;Supply.Bounds+=V+Margin;Supply.Bounds+=W-Margin;Supply.Bounds+=W+Margin;
        }
        if(!Downstream||Supply.Cells.IsEmpty())GeometryError=TEXT("공급 밸브 아래 연결부를 찾을 수 없습니다.");
        Supplies.Add(MoveTemp(Supply));
    }
    if(Index!=6)GeometryError=TEXT("LowPoly 공급 6개 연결부를 모두 확인해야 합니다.");
}

int32 FPlantFeedPreparation::SupplyPath(const powder::Grain& P) const
{
    // The existing WH centre-of-particle boundary owns particles that have already entered the vessel.
    if(Mass.Classify(P)==EPlantMassRegion::Weighing)return INDEX_NONE;
    const FVector3d Point(P.x,P.y,P.z);
    for(int32 I=0;I<Supplies.Num();++I)
    {
        const auto& S=Supplies[I];if(!S.Bounds.IsInsideOrOn(Point))continue;
        if(P.z>=S.Gate.z&&FMath::Square(double(P.x)-S.Gate.x)+FMath::Square(double(P.y)-S.Gate.y)<=FMath::Square(S.Gate.w+Radius))continue;
        for(const auto& C:S.Cells)
        {
            const auto Axis=C.B-C.A;const double Length=Axis.Length();
            const double Along=FVector3d::DotProduct(Point-C.A,Axis)/Length;
            if(Along < -Radius || Along > Length+Radius)continue;
            const double T=FMath::Clamp(Along/Length,0.,1.);
            const auto Delta=Point-(C.A+T*Axis);
            if(C.Rectangular)
            {
                const FVector3d Side(-Axis.Y/Length,Axis.X/Length,0);
                if(FMath::Abs(FVector3d::DotProduct(Delta,Side))<=C.RA+Radius&&FMath::Abs(Delta.Z)<=C.RB+Radius)return I;
            }
            else if(Delta.SquaredLength()<=FMath::Square(FMath::Lerp(C.RA,C.RB,T)+Radius))return I;
        }
    }
    return INDEX_NONE;
}

void FPlantFeedPreparation::ResetStability()
{
    StableSince=-1;StableIds.Empty();State.StableSeconds=0;State.SupplyConfirmedSimulationSeconds=-1;
}

bool FPlantFeedPreparation::Fail(const FString& Reason,EPlantFeedPreparationState Result)
{
    ResetStability();State.State=Result;State.Reason=Reason+TEXT(" · 출구 개방 취소 · 확인 후 새 요청으로 재시도하세요.");return false;
}

void FPlantFeedPreparation::SyncIntent(const FPlantControlTargets& C,double Time,double Now)
{
    if(!State.Enabled)return;
    if(State.Connection!=C.ObservationConnection)
    {State=FPlantFeedPreparationSnapshot();State.Enabled=true;State.Connection=C.ObservationConnection;State.RequiredStableSeconds=RequiredStable;ResetStability();LastTime=-1;}
    if(!C.WeighingOpen||!C.WeighingOpenRevision)
    {
        if(State.RequestRevision&&State.State!=EPlantFeedPreparationState::Opened&&State.State!=EPlantFeedPreparationState::Failed&&State.State!=EPlantFeedPreparationState::Empty)
            Fail(TEXT("닫기·전체 정지·연결 변경으로 준비 요청이 취소되었습니다."),EPlantFeedPreparationState::Cancelled);
        return;
    }
    if(State.RequestRevision!=C.WeighingOpenRevision)
    {
        State=FPlantFeedPreparationSnapshot();State.Enabled=true;State.Connection=C.ObservationConnection;
        State.RequiredStableSeconds=RequiredStable;State.RequestRevision=C.WeighingOpenRevision;State.RequestedSimulationSeconds=Time;
        State.State=EPlantFeedPreparationState::WaitingValves;State.Reason=TEXT("공급 밸브 닫힘 확인 중");ResetStability();LastTime=-1;
    }
    if(State.State==EPlantFeedPreparationState::Opened||State.State==EPlantFeedPreparationState::Failed||State.State==EPlantFeedPreparationState::Empty||State.State==EPlantFeedPreparationState::Cancelled)return;
    if(LastControlRevision!=C.Revision){ResetStability();LastControlRevision=C.Revision;}
    if(C.Suppliers||C.SupplyAPurgeEnabled||C.SupplyAPurgeGaugePa!=0)ResetStability();
    if(!C.Running)Fail(TEXT("계산 일시정지로 준비가 취소되었습니다."),EPlantFeedPreparationState::Cancelled);
    else if(!FMath::IsFinite(Now)||!FMath::IsFinite(C.WeighingOpenDeadline)||Now>=C.WeighingOpenDeadline)
        Fail(TEXT("공급 완료 확인 대기 시간이 초과되었습니다."));
    else if(!FMath::IsFinite(C.WeighingOpenLeaseUntil)||Now>C.WeighingOpenLeaseUntil)
        Fail(TEXT("제어 서비스의 최신 상태 확인이 끊겼습니다."));
}

bool FPlantFeedPreparation::Evaluate(const std::vector<powder::Grain>& P,double Time,double Now,const FPlantControlTargets& C,uint32 Applied,bool Continuing,bool PurgeApplied,double PurgeFlow,double PurgePressure)
{
    SyncIntent(C,Time,Now);
    if(!State.Enabled||!C.WeighingOpen||!C.WeighingOpenRevision||State.State==EPlantFeedPreparationState::Failed||State.State==EPlantFeedPreparationState::Empty||State.State==EPlantFeedPreparationState::Cancelled)return false;
    if(!GeometryError.IsEmpty())return Fail(GeometryError);
    if(P.size()!=Count||!FMath::IsFinite(Time)||Time<LastTime)return Fail(TEXT("입자 ID·좌표·해석 시간을 확인할 수 없습니다."));
    const bool NewTime=LastTime<0||Time>LastTime;LastTime=Time;State.ObservedSimulationSeconds=Time;
    TArray<int32> Ids;State.SupplyResidualCount=0;State.SupplyResidualByPath.Init(0,6);
    for(int32 I=0;I<int32(P.size());++I)
    {
        const auto Region=Mass.Classify(P[I]);if(Region==EPlantMassRegion::Invalid)return Fail(TEXT("입자 좌표가 유효하지 않습니다."));
        if(Region==EPlantMassRegion::Weighing)Ids.Add(I);
        else if(const int32 Path=SupplyPath(P[I]);Path!=INDEX_NONE){++State.SupplyResidualCount;++State.SupplyResidualByPath[Path];}
    }
    State.WeighingCount=Ids.Num();State.WeighingKg=Ids.Num()*Mass.ParticleMassKg;State.SupplyResidualKg=State.SupplyResidualCount*Mass.ParticleMassKg;
    if(C.Suppliers||Applied)
    {ResetStability();State.State=EPlantFeedPreparationState::WaitingValves;State.Reason=TEXT("공급 밸브 닫힘 확인 중 · 공급 A~F의 요청·실제 닫힘이 필요합니다.");return false;}
    if(State.SupplyResidualCount)
    {ResetStability();State.State=EPlantFeedPreparationState::WaitingSupplyResidual;State.Reason=FString::Printf(TEXT("공급 잔류 대기 · 연결부 %d개 (%.6f g)"),State.SupplyResidualCount,State.SupplyResidualKg*1000);return false;}
    if(C.SupplyAPurgeEnabled||C.SupplyAPurgeGaugePa!=0||PurgeApplied||PurgeFlow!=0||PurgePressure!=0)
    {ResetStability();State.State=EPlantFeedPreparationState::WaitingPurgeStop;State.Reason=TEXT("연결부 공기 정지 확인 중 · 공기 차단·압력 설정 0·유량 0 확인 후 칭량 안정을 시작합니다.");return false;}
    if(Ids.IsEmpty()&&!Continuing)return Fail(TEXT("이송할 원료 없음"),EPlantFeedPreparationState::Empty);
    if(StableSince<0||StableIds!=Ids)
    {ResetStability();StableIds=MoveTemp(Ids);StableSince=Time;}
    if(NewTime)State.StableSeconds=FMath::Max(0.,Time-StableSince);
    if(State.StableSeconds<RequiredStable)
    {State.State=EPlantFeedPreparationState::WaitingWeighingStability;State.Reason=FString::Printf(TEXT("칭량 안정 확인 중 · 동일 입자 구성·질량 %.3f / %.3f초 (해석 시간)"),State.StableSeconds,RequiredStable);return false;}
    State.State=EPlantFeedPreparationState::Ready;State.Reason=TEXT("공급 완료 확인 · 동일 작업자 상태에서 기준량 확정·출구 개방 확인 중");
    if(State.SupplyConfirmedSimulationSeconds<0)State.SupplyConfirmedSimulationSeconds=Time;
    return true;
}

void FPlantFeedPreparation::Opened(double BaselineTime,double AppliedTime)
{
    State.BaselineCapturedSimulationSeconds=BaselineTime;State.OutletAppliedSimulationSeconds=AppliedTime;
    State.State=EPlantFeedPreparationState::Opened;
    State.Reason=BaselineTime<State.SupplyConfirmedSimulationSeconds?TEXT("공급 완료 확인 · 기존 이송 기준량 유지 후 출구 재개방 적용 확인"):TEXT("공급 완료·기준량 확보 후 출구 열림 적용 확인");
}

int32 FPlantFeedPreparation::SupplyAInventory(const std::vector<powder::Grain>& Positions) const
{
    if(!State.Enabled||!GeometryError.IsEmpty()||Positions.size()!=Count)return -1;
    int32 N=0;
    for(const auto& P:Positions){if(Mass.Classify(P)==EPlantMassRegion::Invalid)return -1;if(SupplyPath(P)==0)++N;}
    return N;
}
