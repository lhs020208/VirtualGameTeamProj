#include "PowderPlantTest.h"
#include "powder/demo_plant.hpp"
#include "Engine/GameViewportClient.h"
#include <iomanip>
#include "powder/plant_pressure.hpp"
#include "PlantSupplyPressureTelemetry.h"
#include "powder/plant_feed_brake.hpp"
#include "ProcessPlantBinding.h"
#include "PlantVisibilityController.h"
#include "ProcessUIStateAdapter.h"
#include "ProcessControlService.h"
#include "PlantTransferTelemetry.h"
#include "PlantFeedPreparation.h"
#include "PlantSupplyABatch.h"
#include "PlantDoseValidation.h"
#include "PlantDoseTrace.h"
#include "PlantWorkerDose.h"
#include "PlantTransferDiagnostics.h"
#include "PlantBendDiagnostics.h"
#include "PlantMixerObstacles.h"
#include "Camera/CameraActor.h"
#include "powder/plant.hpp"
#include "powder/plant_charge.hpp"
#include "Components/InstancedStaticMeshComponent.h"
#include "Engine/StaticMesh.h"
#include "Engine/World.h"
#include "Engine/Engine.h"
#include "EngineUtils.h"
#include "InputCoreTypes.h"
#include "GameFramework/PlayerController.h"
#include "Materials/MaterialInstanceDynamic.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "HAL/PlatformTime.h"
#include "HAL/PlatformMisc.h"
#include "HAL/IConsoleManager.h"
#include "UnrealClient.h"
#include "Widgets/SBoxPanel.h"
#include "Widgets/Layout/SBox.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include <atomic>
#include <thread>
#include <mutex>
#include <sstream>
#include <chrono>
namespace {
void FillParticleTransforms(const std::vector<powder::Grain>& Points,const TArray<FVector>& Offsets,const FVector& Scale,TArray<TArray<FTransform>>& Transforms){
 for(auto& Group:Transforms)Group.Reset();
 for(const auto& A:Points){const FVector Center(A.x*100,-A.y*100,A.z*100);auto& Group=Transforms[int(A.w)];for(const FVector& Offset:Offsets)Group.Emplace(FQuat::Identity,Center+Offset,Scale);}
}


powder::PlantPressureTargets Reservoirs(const FPlantControlTargets& C)
{
 powder::PlantPressureTargets P{C.GaugePressurePa.X,C.GaugePressurePa.Y,C.GaugePressurePa.Z};
 P.individual=!C.SupplierPa.IsEmpty();
 if(P.individual){
  if(C.SupplierPa.Num()!=6)throw std::invalid_argument("six supplier pressure targets required");
  for(int I=0;I<6;++I)P.sources[I]=C.SupplierPa[I];
 }
 return P;
}
std::vector<powder::Grain> ReadCharge(const FString& Path,float& Radius,TArray<int32>& InitialCounts,int32 SupplierCount,TArray<int32>& SupplierById){
 InitialCounts.Init(0,SupplierCount);SupplierById.Empty();
 FString Text;if(!FFileHelper::LoadFileToString(Text,*Path))throw std::runtime_error("particle charge file missing");
 std::istringstream File(TCHAR_TO_UTF8(*Text));std::string Line;std::getline(File,Line);std::vector<powder::Grain> P;
 while(std::getline(File,Line)){std::replace(Line.begin(),Line.end(),',',' ');std::istringstream Row(Line);unsigned Id,Type;std::string Supplier;float X,Y,Z,R,D;
  if(!(Row>>Id>>Type>>Supplier>>X>>Y>>Z>>R>>D)||Id!=P.size()||!std::isfinite(R)||R<=.001f||R>=.01f||D!=1200||Type>=32)throw std::runtime_error("invalid virtual charge; radius must be 1..10mm and density 1200");if(P.empty())Radius=R;else if(std::abs(R-Radius)>1e-8f)throw std::runtime_error("plant solver requires uniform particle radius");P.push_back({X,Y,Z,float(Type)});
  int32 Source=INDEX_NONE;if(Supplier.size()==6&&Supplier.substr(0,5)=="H-101"&&Supplier[5]>='A'&&Supplier[5]<'A'+SupplierCount){Source=Supplier[5]-'A';++InitialCounts[Source];}SupplierById.Add(Source);
 }if(P.empty())throw std::runtime_error("empty particle charge");return P;
}
std::vector<powder::Grain> ReadDemoCharge(const FString& Path,float& Radius){TArray<int32> Counts,Ids;return ReadCharge(Path,Radius,Counts,6,Ids);}
void Count(const std::vector<powder::Grain>&P,int&WH,int&RH,int&MX,const FVector&Min,const FVector&Max){WH=RH=MX=0;for(auto A:P){if(std::hypot(A.x,A.y)<.355&&A.z>.603&&A.z<1.78)++WH;if(std::hypot(A.x-3,A.y)<.255&&A.z>2.163&&A.z<3.7)++RH;if(A.x>Min.X&&A.x<Max.X&&A.y>Min.Y&&A.y<Max.Y&&A.z>Min.Z&&A.z<Max.Z)++MX;}}
}
#include "PowderPlantLegacyDemoRuntime.inl"
#include "PowderPlantDemo.inl"
struct FPlantRuntime {
 FPlantSimulationSnapshot Published;
 std::thread Thread;std::mutex Mutex;std::atomic<bool> Quit{false};
 FPlantControlTargets Mailbox;
 int64 ScopedPressureUploadedRevision=0;double ScopedPressureUploadedPa=0;
 std::vector<powder::Grain> Points;std::string Error;double Time=0,Wall=0,Angle=0;
 int Phase=0,WH=0,RH=0,MX=0;bool HopperOpen=false,ReceiverOpen=false;uint32 OpenSuppliers=0;uint64 Generation=1;
 FPlantRuntime(powder::GranularConfig C,std::vector<powder::Grain> P,std::vector<powder::PlantRoute> Routes,double Timeout,double Fraction,double TransferWait,bool Start,double Pressure,double Speed,FPlantTransferSettings TransferSettings,FVector CountMin,FVector CountMax,bool Regulated,bool GuardOpening,TArray<int32> SupplierById):Points(P){
  Mailbox.Running=Start;Mailbox.Pressure=Pressure;Mailbox.RPM=15;
  Thread=std::thread([this,C=std::move(C),P=std::move(P),Routes=std::move(Routes),Timeout,Fraction,TransferWait,TransferSettings,CountMin,CountMax,Regulated,GuardOpening,SupplierById=MoveTemp(SupplierById)]()mutable{
   try{
    FPlantControlTargets Control;{std::lock_guard<std::mutex>L(Mutex);Control=Mailbox;}
    FPlantTransferTelemetry Telemetry(C,Routes,TransferSettings);
    FPlantFeedPreparation Preparation(C,Routes,Telemetry,TransferSettings.StableSimulationSeconds,GuardOpening&&C.count>0);
    FPlantSupplyABatch Batch(C,SupplierById,Telemetry,Preparation,Regulated&&GuardOpening);
    FPlantWorkerDose DoseCutoff(Regulated&&GuardOpening);
    Batch.Observe(P,0);int64 LastBatchCaptureAttempt=0,AuthorizedMixerRevision=0,LastDoseAttempt=0,LastTransitionAttempt=0;
    int64 ObservationConnection=Control.ObservationConnection;Telemetry.ResetConnection(ObservationConnection);
    Telemetry.Observe(P,0,false,false);
    double AppliedPressure=Control.Pressure;auto Gates=powder::plant_supplier_gates(C.plant_data);
    int HopperGate=powder::plant_weighing_gate(C.plant_data),ReceiverGate=powder::plant_receiver_gate(C.plant_data);bool OpenHopper=false,PreviousHopper=false,OpenReceiver=false,PreviousReceiver=false;
    uint32 Open=0,PreviousRequest=0,All=Gates.size()==32?~uint32(0):(uint32(1)<<Gates.size())-1;
    powder::PlantPressureState PressureState;
    TArray<double> AppliedSources=Control.SupplierPa;FVector AppliedTargets=Control.GaugePressurePa;int64 PressureRevision=Control.Revision;
    auto SuppliesAtAtmosphere=[&](){
     for(double Pa:AppliedSources)if(Pa!=0)return false;
     for(double Pa:Control.SupplierPa)if(Pa!=0)return false;
     return true;
    };
    double PressureAppliedTime=0;int64 RejectedTransferPressure=0;
    const bool PurgeSupported=Regulated&&GuardOpening;
    powder::PlantSupplyAPurgePath PurgePaths[6];
    if(PurgeSupported)for(int I=0;I<6;++I)PurgePaths[I]=powder::plant_supply_purge_path(C.plant_data,Routes,I);
    int64 PurgeAppliedRevision=0,CancelledPurgeRevision=0;
    int32 SupplyResidual[6]={-1,-1,-1,-1,-1,-1};
    auto ObserveResidual=[&](const std::vector<powder::Grain>& Current){if(C.count==0){for(int32& N:SupplyResidual)N=0;}else if(PurgeSupported)Preparation.CountSupplyConnectors(Current,SupplyResidual);};ObserveResidual(P);
    if(Regulated)PressureState=powder::plant_set_regulated_pressures(C.plant_data,Routes,
        Reservoirs(Control),0,0,false,false,false);
    else {powder::plant_set_manual_valves(C.plant_data,Routes,AppliedPressure,0,0,false,false);powder::plant_set_receiver_valve(C.plant_data,false);}
    FPlantSupplyPressureTelemetry ValvePressure(C.plant_data,Routes);
    int64 ValvePressureRevision=-1;TArray<FPlantSupplyValvePressure> ValvePressureValues;
    powder::GranularGPU GPU(C,P,std::vector<powder::Grain>(P.size()));
    auto MixerState=GPU.plant_mixer_state();
    auto PhysicalAngle=[&](){return C.plant_powered_mixer?MixerState.angle_rad:double(GPU.angle());};
    auto MixerTargetReady=[&](double Target){return C.plant_powered_mixer?(!MixerState.tripped&&std::abs(MixerState.target_rad_s/0.10471975511965977-Target)<.001&&MixerState.rpm()>0):true;};
    auto ValveStates=GPU.plant_valve_states();std::vector<double> ValveApertures(ValveStates.size(),0),LastValveAngles(ValveStates.size(),0);
    auto PhysicalSupplyMask=[&](){uint32 Mask=0;for(size_t I=0;I<Gates.size()&&I<ValveStates.size();++I)if(!ValveStates[I].closed())Mask|=1u<<I;return Mask;};
    auto ValvesClosed=[&](){return std::all_of(ValveStates.begin(),ValveStates.end(),[](const auto& V){return V.closed();});};
    auto PhysicalHopperOpen=[&](){return C.plant_powered_valves?!ValveStates[Gates.size()].closed():OpenHopper;};
    auto PhysicalReceiverOpen=[&](){return C.plant_powered_valves?!ValveStates[Gates.size()+1].closed():(Control.ReceiverOpen||OpenReceiver);};
    FPlantFeedBrakeSnapshot Brakes[6];powder::PlantFeedBrake BrakeControls[6];int64 BrakePendingRevisions[6]{};
    for(int I=0;I<6;++I){auto& Brake=Brakes[I];auto& BrakeControl=BrakeControls[I];Brake.Supported=Regulated&&GuardOpening&&!C.plant_powered_valves;
     if(Brake.Supported){BrakeControl=powder::plant_feed_brake(C.plant_data,I,C.radius);Brake.LowerFaceMetres=BrakeControl.region.z;Brake.ResponseTimeSeconds=BrakeControl.response_time_s;}}
    auto ObserveBrake=[&](const std::vector<powder::Grain>& State,const std::vector<powder::Grain>& Velocity,double At,int32 Only=INDEX_NONE){
     for(int Supplier=0;Supplier<6;++Supplier){auto& Brake=Brakes[Supplier];const auto& BrakeControl=BrakeControls[Supplier];
      if(!Brake.Supported||(Only!=INDEX_NONE&&Only!=Supplier))continue;
      Brake.Valid=State.size()==Velocity.size()&&State.size()==size_t(SupplierById.Num());if(!Brake.Valid)continue;
      Brake.GateOccupancyCount=powder::plant_closure_occupancy(C.plant_data[Gates[Supplier]],State,C.radius);Brake.PredictedSpillCount=0;
      for(int32 I=0;I<SupplierById.Num();++I)if(SupplierById[I]==Supplier&&powder::plant_feed_brake_spill_candidate(State[I],Velocity[I],C.plant_data[Gates[Supplier]],BrakeControl))++Brake.PredictedSpillCount;
      Brake.PredictedSpillKg=Brake.PredictedSpillCount*Telemetry.ParticleMassKg;Brake.ObservedSimulationSeconds=At;
     }
    };
    ObserveBrake(P,std::vector<powder::Grain>(P.size()),0);
    FPlantTransferDiagnostics Diagnostics(C);FPlantBendDiagnostics BendDiagnostics;
    if(C.count>0)Diagnostics.Capture("initial",GPU,P,Telemetry,C,Routes,Control,Open,OpenHopper,OpenReceiver,true);
    UE_LOG(LogTemp,Display,TEXT("Plant actual OpenCL device: %hs"),GPU.device().c_str());
    const auto WallMemory=GPU.plant_memory_bytes();UE_LOG(LogTemp,Display,TEXT("Plant collision mode=%u particles=%u plant_bytes=%llu acceleration_bytes=%llu cache_bytes=%llu"),C.plant_wall_mode,C.count,(unsigned long long)WallMemory[0],(unsigned long long)WallMemory[1],(unsigned long long)WallMemory[2]);
    int PhaseNow=0;double StageStart=0,Active=0;
    int64 LastPreparationRevision=-1;
    int64 SpeedRun=0,SpeedApplied=0,SpeedRejected=0;
    double SpeedRPM=0,SpeedTime=-1,SpeedAngle=0;
    auto UseConfirmedSpeed=[&](){
     if(Control.MixRun>0&&Control.MixRun==SpeedRun&&Control.MixerEnabled)Control.RPM=SpeedRPM;
    };
    double LastMotorCommandRPM=0;double LastRPMStartAngle=0;double LastAppliedRPM=0,LastRPMApplicationTime=-1;int64 LastRPMRevision=0;bool HasAppliedRPM=false,WasPaused=false;
    int TransferFlowCell=-1;for(const auto& Route:Routes)if(Route.phase==1&&!Route.cells.empty())TransferFlowCell=Route.cells.front()+2;
    // Telemetry only: called under Mutex, with results from a completed GPU batch.
    auto Publish=[&](EPlantExecutionState Execution){
     Published.Validity=EPlantValueValidity::Valid;Published.Execution=Execution;
     Published.PhysicalSeconds=Time;Published.InWeighingHopper=WH;Published.InReceiver=RH;Published.InMixer=MX;Telemetry.Publish(Published);
     Published.Recovery=Batch.Recovery;Published.FeedPreparation=Preparation.Snapshot();Published.DoseCutoff=DoseCutoff.State;
     Batch.ObserveAdditiveReady(Time,ValvesClosed()&&Open==0&&!OpenHopper&&!OpenReceiver&&
       !Control.Suppliers&&!Control.WeighingOpen&&!Control.ReceiverOpen&&SuppliesAtAtmosphere()&&AppliedTargets==FVector::ZeroVector&&
       Control.GaugePressurePa==FVector::ZeroVector&&!PressureState.purge.enabled&&PressureState.purge.gauge_pa==0&&PressureState.purge_m3s==0&&
       !Control.SupplyAPurgeEnabled&&Control.SupplyAPurgeGaugePa==0);
     Published.SupplyABrake=Brakes[0];Published.SupplyBBrake=Brakes[1];Published.SupplyCBrake=Brakes[2];Published.SupplyDBrake=Brakes[3];Published.SupplyEBrake=Brakes[4];Published.SupplyFBrake=Brakes[5];Published.SequentialBatch=Batch.SequentialSnapshot();Published.SupplyABatch=Batch.Snapshot();Published.MixerFeed=Batch.MixerSnapshot();Published.InitialBatchMixerFeed=Batch.InitialMixerSnapshot();Published.GPUDevice=UTF8_TO_TCHAR(GPU.device().c_str());
     for(int I=0;I<6;++I){auto& Purge=PlantPurgeFor(Published,I);
     const bool Selected=PressureState.purge.supplier==unsigned(I);Purge.Supported=PurgeSupported;Purge.Valid=PurgeSupported&&SupplyResidual[I]>=0;
     Purge.AppliedEnabled=Selected&&PressureState.purge.enabled;Purge.AppliedGaugePa=Selected?PressureState.purge.gauge_pa:0;Purge.FlowM3s=Selected?PressureState.purge_m3s:0;
     Purge.AppliedRevision=PurgeAppliedRevision;Purge.CancelledRevision=CancelledPurgeRevision;
     Purge.Diagnostic=Control.SupplyAPurgeRevision>0&&Control.SupplyAPurgeRevision<=CancelledPurgeRevision?
        TEXT("연결부 공기 요청 취소 · 연결/갱신/계산 상태를 확인하고 새 요청으로 재시도하세요."):FString();
     Purge.ResidualCount=FMath::Max(0,SupplyResidual[I]);Purge.ResidualKg=Purge.ResidualCount*Telemetry.ParticleMassKg;
     Purge.ObservedSimulationSeconds=Time;Purge.InletMetres=FVector(PurgePaths[I].inlet.x,PurgePaths[I].inlet.y,PurgePaths[I].inlet.z);
     }
     if(TransferFlowCell>=0&&size_t(TransferFlowCell)<C.plant_data.size()){
      Published.Transfer.CalculatedCarrierFlowM3s=C.plant_data[TransferFlowCell].x;
      Published.Transfer.CarrierFlowValid=FMath::IsFinite(Published.Transfer.CalculatedCarrierFlowM3s)&&(Regulated||Published.Transfer.CalculatedCarrierFlowM3s>=0);
     }
     Published.AppliedSupplierMask=int32(OpenSuppliers);Published.AppliedHopperOpen=HopperOpen;Published.AppliedReceiverOpen=ReceiverOpen;
     Published.ValveActuators.Empty();
     for(const auto& V:ValveStates){FPlantValveActuatorSnapshot A;A.AngleDegrees=FMath::RadiansToDegrees(V.angle_rad);A.AngularSpeedDegrees=FMath::RadiansToDegrees(V.omega_rad_s);A.MotorTorqueNm=V.motor_nm;A.ContactTorqueNm=V.contact_nm;A.NormalLoadN=V.normal_n;A.ElectricalWatts=V.electrical_w;A.EnergyJoules=V.energy_j;A.Voltage=V.voltage;A.Closed=V.closed();A.Open=V.open();A.Tripped=V.tripped;Published.ValveActuators.Add(A);}
     if(C.plant_powered_valves){Published.AppliedSupplierMask=0;for(size_t I=0;I<Gates.size();++I)if(!ValveStates[I].closed())Published.AppliedSupplierMask|=1<<I;Published.AppliedHopperOpen=!ValveStates[Gates.size()].closed();Published.AppliedReceiverOpen=!ValveStates[Gates.size()+1].closed();}

     Published.MixRun=Control.AutomaticMixClock&&Control.MixerEnabled?AuthorizedMixerRevision:0;
     Published.SpeedAppliedRevision=SpeedApplied;Published.SpeedRejectedRevision=SpeedRejected;
     Published.SpeedAppliedSeconds=SpeedTime;Published.SpeedStartAngleRadians=SpeedAngle;
     {auto& D=Published.MixerDrive;D.Supported=C.plant_powered_mixer;D.TargetRPM=MixerState.target_rad_s/0.10471975511965977;D.ActualRPM=MixerState.rpm();D.Tripped=MixerState.tripped;D.Stopped=MixerState.stopped();D.MotorTorqueNm=MixerState.motor_nm;D.ContactTorqueNm=MixerState.contact_nm;D.ElectricalWatts=MixerState.electrical_w;D.EnergyJoules=MixerState.energy_j;D.PowerFraction=MixerState.power_fraction;D.NormalLoadN=MixerState.normal_n;}
     Published.AppliedRPM=LastAppliedRPM;Published.AppliedRPMValid=HasAppliedRPM;
     Published.AppliedRPMRevision=LastRPMRevision;Published.AppliedRPMSimulationSeconds=LastRPMApplicationTime;
     Published.Transfer.CarrierRegulated=Regulated;
     Published.PressureControlEnabled=Regulated;Published.AppliedPressureValid=Regulated;
     Published.AppliedSupplierPa.SetNum(6);for(int I=0;I<6;++I)Published.AppliedSupplierPa[I]=PressureState.applied.source(I);
     Published.AppliedGaugePressurePa=FVector(PressureState.applied.supply,PressureState.applied.weighing,PressureState.applied.receiver);
     if(ValvePressureRevision!=Control.Revision){
      ValvePressureValues=ValvePressure.Observe(C.plant_data,Routes,Published.AppliedGaugePressurePa,C.plant_powered_valves?uint32(Published.AppliedSupplierMask):Open,C.plant_powered_valves?uint32(Published.AppliedSupplierMask):Control.Suppliers,Regulated,PressureState.purge.supplier,PressureState.purge.enabled,Published.AppliedSupplierPa,C.plant_powered_valves?&ValveApertures:nullptr);ValvePressureRevision=Control.Revision;
     }
     Published.SupplyValvePressures=ValvePressureValues;Published.MixerAngleRadians=Angle;Published.AppliedRPMStartAngleRadians=LastRPMStartAngle;
     Published.PressureAppliedRevision=PressureRevision;Published.PressureAppliedSimulationSeconds=PressureAppliedTime;
     Published.TransferPressureRejectedRevision=RejectedTransferPressure;Published.WeighingRegulatorM3s=PressureState.weighing_regulator_m3s;
     Published.ProcessedControlRevision=Control.Revision;Published.WorkerTargetRPM=Control.RPM;Published.WorkerMixerEnabled=Control.MixerEnabled;
     Published.UpdatedUtc=FDateTime::UtcNow();Published.PublishedWallSeconds=FPlatformTime::Seconds();++Published.Sequence;
     if(Batch.ActiveSupplier()==1&&((Control.Suppliers&2)||(Open&2)))PlantDoseTrace::Snapshot(TEXT("worker_publish"),Published);
    };
    {std::lock_guard<std::mutex>L(Mutex);Count(Points,WH,RH,MX,CountMin,CountMax);Publish(Control.Running?EPlantExecutionState::Running:EPlantExecutionState::Paused);}
    auto LastPublish=std::chrono::steady_clock::now(),LastControl=LastPublish;
    auto EmptyDeadline=LastPublish;
    while(!Quit){
     {std::lock_guard<std::mutex>L(Mutex);Control=Mailbox;}
     UseConfirmedSpeed();
     if(Control.RecoveryRevision==0)Batch.RecoveryRevoked();
     if(Control.RecoveryRevision>0&&Control.RecoveryRevision>FMath::Max(Batch.Recovery.AcceptedRevision,Batch.Recovery.RejectedRevision)){
      auto Current=GPU.snapshot();std::lock_guard<std::mutex>L(Mutex);
      if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.RecoveryRevision!=Control.RecoveryRevision||Mailbox.RecoveryRequestId!=Control.RecoveryRequestId||Mailbox.ObservationConnection!=Control.ObservationConnection)continue;
      const bool Safe=ValvesClosed()&&!Control.Running&&Open==0&&!OpenHopper&&!OpenReceiver&&Control.Suppliers==0&&!Control.WeighingOpen&&!Control.ReceiverOpen&&
       !Control.MixerEnabled&&HasAppliedRPM&&LastAppliedRPM==0&&SuppliesAtAtmosphere()&&AppliedTargets==FVector::ZeroVector&&Control.GaugePressurePa==FVector::ZeroVector&&
       !PressureState.purge.enabled&&PressureState.purge.gauge_pa==0&&PressureState.purge_m3s==0&&!Control.SupplyAPurgeEnabled&&Control.SupplyAPurgeGaugePa==0&&FPlatformTime::Seconds()<=Control.RecoveryDeadline;
      if(Safe)Batch.AdmitRecovery(Current,GPU.time(),Control.RecoveryCapture,Control.RecoveryRevision,Control.RecoveryRequestId);
      else{Batch.Recovery.RejectedRevision=Control.RecoveryRevision;Batch.Recovery.Reason=TEXT("재개 작업자 검증: 전체 정지 미확인 또는 요청 만료");}
     }
     const int64 PreviousDoseArm=DoseCutoff.State.ArmRevision;
     DoseCutoff.Sync(Control,Batch.ActiveSupplier(),Batch.Snapshot().CaptureRevision,GPU.time(),FPlatformTime::Seconds(),Telemetry.ParticleMassKg,Batch.Snapshot().InitialKg);
     if(DoseCutoff.State.ArmRevision!=PreviousDoseArm)PlantDoseTrace::Write(TEXT("worker_arm_received"),DoseCutoff.State.Supplier,GPU.time(),Published.Sequence,
        Control.Revision,Batch.Snapshot().CaptureRevision,0,0,0,0,DoseCutoff.State.RequestId.ToString());
     Control.Suppliers=DoseCutoff.Requests(Control); // worker-owned close latch defeats repeated stale open mailbox copies
     const bool PurgeIntent=Control.SupplyAPurgeEnabled||Control.SupplyAPurgeGaugePa!=0;
     const bool PurgeLease=Control.SupplyAPurgeRevision>0&&Control.SupplyAPurgeRevision>CancelledPurgeRevision&&
         FMath::IsFinite(Control.SupplyAPurgeLeaseUntil)&&FPlatformTime::Seconds()<=Control.SupplyAPurgeLeaseUntil;
     if(PurgeIntent&&(!PurgeSupported||!Control.Running||!PurgeLease))
         CancelledPurgeRevision=FMath::Max(CancelledPurgeRevision,Control.SupplyAPurgeRevision);
     powder::PlantSupplyAPurge DesiredPurge;
     if(PurgeSupported&&Control.Running&&PurgeLease)DesiredPurge={Control.SupplyAPurgeEnabled,Control.SupplyAPurgeGaugePa,unsigned(Control.SupplyPurgeSupplier)};
     if(Control.BatchCaptureRevision>0&&Control.BatchCaptureRevision!=LastBatchCaptureAttempt){
      auto Current=GPU.snapshot();std::lock_guard<std::mutex>L(Mutex);
      if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.BatchCaptureRevision!=Control.BatchCaptureRevision||Mailbox.ObservationConnection!=Control.ObservationConnection)continue;
      LastBatchCaptureAttempt=Control.BatchCaptureRevision;
      if(Open==0&&!OpenHopper&&!OpenReceiver&&!Control.Suppliers&&!Control.WeighingOpen&&!Control.ReceiverOpen)
          Batch.Capture(Current,GPU.time(),Control.BatchCaptureRevision,Control.BatchSupplier);
     }
     if(Control.DoseFinalizeRevision>0&&Control.DoseFinalizeRevision!=LastDoseAttempt){
      auto Current=GPU.snapshot();std::lock_guard<std::mutex>L(Mutex);
      if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.DoseFinalizeRevision!=Control.DoseFinalizeRevision||Mailbox.ObservationConnection!=Control.ObservationConnection)continue;
      LastDoseAttempt=Control.DoseFinalizeRevision;
      const bool Ready=ValvesClosed()&&Control.Running&&FPlatformTime::Seconds()<=Control.DoseDeadline&&Open==0&&!OpenHopper&&!OpenReceiver&&
          !Control.Suppliers&&!Control.WeighingOpen&&!Control.ReceiverOpen&&
          ((!Control.MixerEnabled&&LastAppliedRPM==0)||(Batch.AdditiveScheduled()&&Control.MixerEnabled&&(C.plant_powered_mixer?MixerTargetReady(Control.RPM):LastAppliedRPM==double(float(Control.RPM)))))&&
          !PressureState.purge.enabled&&PressureState.purge.gauge_pa==0&&PressureState.purge_m3s==0&&
          !Control.SupplyAPurgeEnabled&&Control.SupplyAPurgeGaugePa==0;
      if(Ready)Batch.FinalizeDose(Current,GPU.time(),Control.DoseCaptureRevision,Control.DoseFinalizeRevision,Control.DoseTargetKg,Control.DoseToleranceKg);
      else Batch.RejectDose(Control.DoseFinalizeRevision,TEXT("최종 계량 취소/만료 또는 실제 공급·출구 닫힘/공기 차단 미확인"));
      const auto& Dose=Batch.Snapshot();
      PlantDoseTrace::Write(TEXT("dose_finalized"),Batch.ActiveSupplier(),GPU.time(),Published.Sequence,Control.Revision,Dose.CaptureRevision,Dose.DoseFinalKg,Brakes[Batch.ActiveSupplier()].PredictedSpillKg);
      UE_LOG(LogTemp,Display,TEXT("A_DOSE_FINAL revision=%lld time=%.9f target_kg=%.12g tolerance_kg=%.12g measured_kg=%.12g selected=%d source=%d accepted=%d reason=%s"),LastDoseAttempt,GPU.time(),Control.DoseTargetKg,Control.DoseToleranceKg,Dose.DoseFinalKg,Dose.TotalCount,Dose.InventorySourceCount,Dose.DoseFinalized,*Dose.DoseReason);
     }
     if(Control.BatchTransitionRevision>0&&Control.BatchTransitionRevision!=LastTransitionAttempt){
      auto Current=GPU.snapshot();std::lock_guard<std::mutex>L(Mutex);
      if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.BatchTransitionRevision!=Control.BatchTransitionRevision||Mailbox.ObservationConnection!=Control.ObservationConnection)continue;
      LastTransitionAttempt=Control.BatchTransitionRevision;
      const bool Ready=ValvesClosed()&&Control.Running&&FPlatformTime::Seconds()<=Control.BatchTransitionDeadline&&Open==0&&!OpenHopper&&!OpenReceiver&&
       !Control.Suppliers&&!Control.WeighingOpen&&!Control.ReceiverOpen&&
       ((Control.BatchAdditive&&Control.AdditiveScheduled&&Control.AdditiveDelaySeconds>0)?
        (Control.MixerEnabled&&(C.plant_powered_mixer?MixerTargetReady(Control.RPM):LastAppliedRPM==double(float(Control.RPM)))&&LastAppliedRPM>0):(!Control.MixerEnabled&&LastAppliedRPM==0))&&
       SuppliesAtAtmosphere()&&AppliedTargets==FVector::ZeroVector&&Control.GaugePressurePa==FVector::ZeroVector&&
       !PressureState.purge.enabled&&PressureState.purge.gauge_pa==0&&PressureState.purge_m3s==0&&!Control.SupplyAPurgeEnabled&&Control.SupplyAPurgeGaugePa==0;
      if(Ready){const bool Accepted=Control.BatchAdditive?
       Batch.BeginAdditive(Current,GPU.time(),Control.BatchTransitionCapture,Control.BatchTransitionRevision,Control.AdditiveScheduled,Control.AdditiveDelaySeconds,Control.BatchSupplier):
       Batch.Transition(Current,GPU.time(),Control.BatchTransitionCapture,Control.BatchTransitionRevision,Control.BatchCombine,Control.BatchSupplier);
       // Retire only the completed A transfer at the atomic B admission. The retained A ID ledger remains authoritative.
       if(Accepted&&(Control.BatchAdditive||!Control.BatchCombine)){Telemetry.ResetConnection(ObservationConnection);Telemetry.Observe(Current,GPU.time(),false,false);}
      }
      else Batch.RejectTransition(Control.BatchTransitionRevision,TEXT("원료 전환 취소/만료 또는 밸브·압력·공기·RPM 정지 미확인"));
      UE_LOG(LogTemp,Display,TEXT("AB_TRANSITION revision=%lld time=%.9f combine=%d accepted=%d reason=%s"),Control.BatchTransitionRevision,GPU.time(),Control.BatchCombine,Batch.SequentialSnapshot().TransitionRevision==Control.BatchTransitionRevision,*Batch.SequentialSnapshot().Reason);
     }
     // A fresh open releases the feeder. Closing/whole-stop engages it and a pause
     // keeps it engaged; resuming alone can never release upstream material.
     for(int Supplier=0;Supplier<6;++Supplier){auto& Brake=Brakes[Supplier];auto& BrakeControl=BrakeControls[Supplier];auto& BrakePendingRevision=BrakePendingRevisions[Supplier];
      if(!Brake.Supported)continue;const uint32 Bit=1u<<Supplier;
      const bool Wanted=(Control.Suppliers&Bit)?false:((Open&Bit)||Brake.Required);
      if(Wanted!=BrakeControl.enabled){
       auto Current=GPU.snapshot(),Velocity=GPU.velocity_snapshot();std::lock_guard<std::mutex>L(Mutex);
       if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection)continue;
       ObserveBrake(Current,Velocity,GPU.time());BrakeControl.enabled=Wanted;Brake.Required=Wanted;
       BrakePendingRevision=Control.Revision;
       if(Wanted){
        if(Batch.ActiveSupplier()==Supplier)Batch.SupplyCloseRequested(Current,GPU.time(),Control.Revision);
        PlantDoseTrace::Write(TEXT("worker_close_received"),Supplier,GPU.time(),Published.Sequence,Control.Revision,Batch.Snapshot().CaptureRevision,Batch.Snapshot().CloseRequestedPassedKg,Brake.PredictedSpillKg);
        Brake.RequestRevision=Control.Revision;Brake.RequestSimulationSeconds=GPU.time();Brake.OccupancyAtRequest=Brake.GateOccupancyCount;
        Brake.GuardDeferrals=0;Brake.GateClearSimulationSeconds=Brake.GateClosedSimulationSeconds=-1;Brake.PredictedSpillAtRequestKg=Brake.PredictedSpillKg;
        UE_LOG(LogTemp,Display,TEXT("FEED_BRAKE_REQUEST supplier=%d revision=%lld time=%.9f occupancy=%d predicted_extra_kg=%.12g passed_kg=%.12g source=%d"),Supplier,Control.Revision,GPU.time(),Brake.OccupancyAtRequest,Brake.PredictedSpillKg,Batch.Snapshot().CloseRequestedPassedKg,Batch.Snapshot().InventorySourceCount);
       }
       GPU.set_plant_feed_brake(BrakeControl,Supplier);
      }
     }
     Preparation.SyncIntent(Control,GPU.time(),FPlatformTime::Seconds());
     // Even a paused worker removes a revoked gas boundary before acknowledging the pause.
     if(!Control.Running&&(PressureState.purge.enabled||PressureState.purge.gauge_pa!=0)){
      auto Cleared=powder::plant_set_regulated_pressures(C.plant_data,Routes,
          PressureState.applied,Open,PreviousRequest,OpenHopper,PreviousHopper,OpenReceiver,{},C.plant_powered_valves?&ValveApertures:nullptr);
      if(C.plant_powered_valves)GPU.update_plant_controls(C.plant_data);else GPU.update_plant_field(C.plant_data);PressureState=Cleared;ValvePressureRevision=-1;PurgeAppliedRevision=Control.Revision;
     }
     if(Control.ObservationConnection!=ObservationConnection){ObservationConnection=Control.ObservationConnection;Batch.InvalidateCapture();LastBatchCaptureAttempt=LastDoseAttempt=LastTransitionAttempt=0;AuthorizedMixerRevision=0;Telemetry.ResetConnection(ObservationConnection);Telemetry.Observe(GPU.snapshot(),GPU.time(),PhysicalHopperOpen(),PhysicalReceiverOpen());}
     if(PhysicalReceiverOpen())Telemetry.ReceiverDischarging();
     if(!Control.Running){
      auto Now=std::chrono::steady_clock::now();
      if(!WasPaused||std::chrono::duration<double>(Now-LastPublish).count()>.1){
       // Capture the final completed batch once on pause; heartbeats do not step the solver.
       if(!WasPaused){BendDiagnostics.Flush();auto Final=GPU.snapshot();Batch.Observe(Final,GPU.time());ObserveBrake(Final,GPU.velocity_snapshot(),GPU.time());ObserveResidual(Final);Telemetry.Observe(Final,GPU.time(),PhysicalHopperOpen(),PhysicalReceiverOpen());if(C.count>0)Diagnostics.Capture("pause",GPU,Final,Telemetry,C,Routes,Control,Open,OpenHopper,OpenReceiver,true);std::lock_guard<std::mutex>L(Mutex);Points=std::move(Final);Time=GPU.time();Angle=PhysicalAngle();Wall=Active;Count(Points,WH,RH,MX,CountMin,CountMax);OpenSuppliers=Open;HopperOpen=OpenHopper;ReceiverOpen=OpenReceiver;++Generation;}
       {std::lock_guard<std::mutex>L(Mutex);Publish(EPlantExecutionState::Paused);}LastPublish=Now;
      }
      EmptyDeadline=std::chrono::steady_clock::now();WasPaused=true;std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;
     }
     if(WasPaused)EmptyDeadline=std::chrono::steady_clock::now();WasPaused=false;
     auto Begin=std::chrono::steady_clock::now();double S=Control.MixerEnabled?Control.RPM:0,D=Control.Pressure;
     if(!std::isfinite(Control.RPM)||Control.RPM<PlantControl::MinRPM||Control.RPM>PlantControl::MaxRPM)throw std::runtime_error("invalid live RPM");
     bool WantHopper=Control.WeighingOpen,WantReceiver=Control.ReceiverOpen;
     const bool Preparing=GuardOpening&&C.count>0&&WantHopper&&!OpenHopper;
     const bool PurgeChanged=DesiredPurge.enabled!=PressureState.purge.enabled||DesiredPurge.gauge_pa!=PressureState.purge.gauge_pa||DesiredPurge.supplier!=PressureState.purge.supplier;
     if(Control.TransferPressureRevision>0&&(Control.TransferPressureRevision>PressureRevision||Control.TransferPressureRevision<=RejectedTransferPressure)){
      FPlantSimulationSnapshot Current;Telemetry.Publish(Current);
      const bool BatchMatches=Control.PressureCapture==0||(Batch.Snapshot().CaptureRevision==Control.PressureCapture&&Batch.ActiveSupplier()==Control.PressureSource);
      const bool Feeding=Control.TransferPressureId==0;
      const uint32 SourceBit=Control.PressureSource>=0&&Control.PressureSource<6?1u<<Control.PressureSource:0;
      const bool FeedOpen=(Control.DoseCutoffRevision||Batch.Snapshot().SourceCount>0)&&SourceBit&&Open==SourceBit&&Control.Suppliers==SourceBit&&!OpenHopper&&!Control.WeighingOpen&&
        (!Control.DoseCutoffRevision||(DoseCutoff.State.Armed&&!DoseCutoff.State.Triggered&&!DoseCutoff.State.Cancelled));
      const bool Transferring=OpenHopper&&Control.WeighingOpen&&Open==0&&!Control.Suppliers&&Current.Transfer.Id==Control.TransferPressureId&&
        Current.Transfer.State!=EPlantTransferState::Complete&&Current.Transfer.State!=EPlantTransferState::Indeterminate&&!Current.Transfer.AdditionalInflowCount;
      const bool Admit=Control.TransferPressureRevision>RejectedTransferPressure&&Control.Running&&BatchMatches&&!OpenReceiver&&!Control.ReceiverOpen&&
       !DesiredPurge.enabled&&FPlatformTime::Seconds()<=Control.TransferPressureDeadline&&(Feeding?FeedOpen:Transferring);
      if(!Admit){
       RejectedTransferPressure=FMath::Max(RejectedTransferPressure,Control.TransferPressureRevision);
       Control.GaugePressurePa=AppliedTargets;Control.SupplierPa=AppliedSources;
      }
     }
     const bool NewPressureRequest=Control.GaugePressureRevision>PressureRevision&&Control.GaugePressureRevision>RejectedTransferPressure;
     const bool PressureChanged=Regulated?(Control.GaugePressurePa!=AppliedTargets||Control.SupplierPa!=AppliedSources||NewPressureRequest):D!=AppliedPressure;
     uint32 Request=Control.Suppliers&All;
     if((Open&(1u<<Batch.ActiveSupplier()))&&!(Request&(1u<<Batch.ActiveSupplier()))&&Batch.Snapshot().CaptureRevision>0&&Batch.Snapshot().SupplyCloseRequestedRevision==0){
      Batch.SupplyCloseRequested(GPU.snapshot(),GPU.time(),Control.Revision);const auto& B=Batch.Snapshot();
      UE_LOG(LogTemp,Display,TEXT("A_CLOSE_REQUEST_WORKER revision=%lld time=%.9f weighing_kg=%.12g connector_kg=%.12g passed_kg=%.12g"),Control.Revision,GPU.time(),B.CloseRequestedWeighingKg,B.CloseRequestedConnectorKg,B.CloseRequestedPassedKg);
     }
     if(PurgeChanged||PressureChanged||Request!=PreviousRequest||(!Preparing&&WantHopper!=PreviousHopper)||(Preparing&&Control.WeighingOpenRevision!=LastPreparationRevision)||WantReceiver!=PreviousReceiver||((Preparing||Open!=Request||OpenHopper!=WantHopper||OpenReceiver!=WantReceiver)&&std::chrono::duration<double>(Begin-LastControl).count()>.1)){
      std::vector<powder::Grain> BeforePreparedOpen;
      std::unique_lock<std::mutex> OpeningCommit(Mutex,std::defer_lock);
      if(Control.TransferPressureRevision>PressureRevision&&Control.TransferPressureRevision>RejectedTransferPressure){
       OpeningCommit.lock();
       if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.TransferPressureRevision!=Control.TransferPressureRevision||
          Mailbox.ObservationConnection!=Control.ObservationConnection||!Mailbox.Running)continue;
       if(FPlatformTime::Seconds()>Control.TransferPressureDeadline){RejectedTransferPressure=FMath::Max(RejectedTransferPressure,Control.TransferPressureRevision);continue;}
      }
      if(Preparing){
       BeforePreparedOpen=GPU.snapshot();
       FPlantSimulationSnapshot CurrentTransfer;Telemetry.Publish(CurrentTransfer);
       const bool Continuing=CurrentTransfer.Transfer.Id>0&&CurrentTransfer.Transfer.BaselineKg>0&&
          CurrentTransfer.Transfer.State!=EPlantTransferState::Complete&&
          (CurrentTransfer.Transfer.RemainingKg>0||CurrentTransfer.Transfer.InTransitKg>0);
       WantHopper=Preparation.Evaluate(BeforePreparedOpen,GPU.time(),FPlatformTime::Seconds(),Control,C.plant_powered_valves?PhysicalSupplyMask():Open,Continuing,PressureState.purge.enabled,PressureState.purge_m3s,PressureState.purge.gauge_pa);
       if(WantHopper){
        // Linearize the final admission, baseline capture and field upload against Stop/Detach/Cancel.
        // No GPU step occurs between this particle state and the actual opening upload.
        OpeningCommit.lock();
        if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection||
           Mailbox.WeighingOpenRevision!=Control.WeighingOpenRevision||!Mailbox.WeighingOpen||!Mailbox.Running)continue;
        Control=Mailbox;UseConfirmedSpeed(); // latest service lease; same command revision/connection
        WantHopper=Preparation.Evaluate(BeforePreparedOpen,GPU.time(),FPlatformTime::Seconds(),Control,C.plant_powered_valves?PhysicalSupplyMask():Open,Continuing,PressureState.purge.enabled,PressureState.purge_m3s,PressureState.purge.gauge_pa);
        if(WantHopper&&Control.RequiredABatchRevision>0&&!Batch.VerifyOpening(BeforePreparedOpen,GPU.time(),Control.RequiredABatchRevision))
            WantHopper=Preparation.RejectOpening(TEXT("A 배치 ID·총량과 개방 직전 칭량 원료가 일치하지 않습니다."));
       }
      }
      if(WantReceiver&&!OpenReceiver&&Control.AutomaticAction==1){
       auto Before=GPU.snapshot();if(!OpeningCommit.owns_lock())OpeningCommit.lock();
       if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection||
          Mailbox.AutomaticActionRevision!=Control.AutomaticActionRevision)continue;
       const bool Ready=ValvesClosed()&&Control.Running&&FPlatformTime::Seconds()<=Control.AutomaticDeadline&&Control.AutomaticActionRevision>0&&
          !OpenHopper&&!Control.WeighingOpen&&Open==0&&Control.Suppliers==0&&
          (Batch.AdditiveScheduled()?(Control.MixerEnabled&&(C.plant_powered_mixer?MixerTargetReady(Control.RPM):LastAppliedRPM==double(float(Control.RPM)))&&LastAppliedRPM>0):(!Control.MixerEnabled&&LastAppliedRPM==0))&&
          SuppliesAtAtmosphere()&&AppliedTargets==FVector::ZeroVector&&!PressureState.purge.enabled&&PressureState.purge_m3s==0;
       if(!Ready||Batch.MixerSnapshot().RejectedRevision==Control.AutomaticActionRevision){
        Batch.RejectDownstream(Control.AutomaticActionRevision,TEXT("수취 개방 취소/만료 또는 실제 밸브·압력·RPM 조건 미확인"));WantReceiver=false;
       }else WantReceiver=Batch.BeginMixerFeed(Before,GPU.time(),Control.AutomaticBatchRevision,Control.AutomaticActionRevision);
      }
      uint32 NextOpen=Open|Request,Closing=Open&~Request;bool NextHopper=OpenHopper||WantHopper,NextReceiver=OpenReceiver||WantReceiver;
      if(!C.plant_powered_valves&&(Closing||(OpenHopper&&!WantHopper)||(OpenReceiver&&!WantReceiver))){auto Current=GPU.snapshot();if(OpenReceiver&&!WantReceiver&&powder::plant_supplier_closure_clear(C.plant_data[ReceiverGate],Current,C.radius))NextReceiver=false;if(OpenHopper&&!WantHopper&&powder::plant_supplier_closure_clear(C.plant_data[HopperGate],Current,C.radius))NextHopper=false;for(size_t i=0;i<Gates.size();++i)if(Closing&(uint32(1)<<i)){
       const bool Clear=powder::plant_supplier_closure_clear(C.plant_data[Gates[i]],Current,C.radius);
       if(i<6&&Brakes[i].Supported){auto& Brake=Brakes[i];
        Brake.GateOccupancyCount=powder::plant_closure_occupancy(C.plant_data[Gates[i]],Current,C.radius);
        if(!Clear){++Brake.GuardDeferrals;if(Brake.GuardDeferrals==1)UE_LOG(LogTemp,Display,TEXT("FEED_GATE_GUARD supplier=%d revision=%lld time=%.9f occupancy=%d closed=0"),int32(i),Control.Revision,GPU.time(),Brake.GateOccupancyCount);}
        if(!Brake.Applied)continue;
        if(Clear&&Brake.GateClearSimulationSeconds<0){
         Batch.Observe(Current,GPU.time());Brake.GateClearSimulationSeconds=GPU.time();Brake.GateClearPassedKg=(Batch.Snapshot().InitialCount-Batch.Snapshot().InventorySourceCount)*Telemetry.ParticleMassKg;
         UE_LOG(LogTemp,Display,TEXT("FEED_GATE_CLEAR supplier=%d revision=%lld time=%.9f passed_kg=%.12g deferrals=%d"),int32(i),Control.Revision,GPU.time(),Brake.GateClearPassedKg,Brake.GuardDeferrals);
        }
       }
       if(Clear)NextOpen&=~(uint32(1)<<i);
      }}
      if(C.plant_powered_valves){NextOpen=Request;NextHopper=WantHopper;NextReceiver=WantReceiver;}
      if(PurgeChanged||NextOpen!=Open||Request!=PreviousRequest||PressureChanged||NextHopper!=OpenHopper||WantHopper!=PreviousHopper||NextReceiver!=OpenReceiver||WantReceiver!=PreviousReceiver){
       if(PurgeChanged&&(DesiredPurge.enabled||DesiredPurge.gauge_pa!=0)){
        if(!OpeningCommit.owns_lock())OpeningCommit.lock();
        if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection||
           Mailbox.SupplyAPurgeRevision!=Control.SupplyAPurgeRevision||!Mailbox.Running||
           FPlatformTime::Seconds()>Mailbox.SupplyAPurgeLeaseUntil)continue;
       }
       // A target-dose opening and cancellation must have a single ordering at the field upload.
       if(Control.DoseCutoffRevision>0&&(NextOpen&~Open)){
        if(Control.RecoveryRevision>0&&!Batch.RecoveryValid())continue;
        if(!OpeningCommit.owns_lock())OpeningCommit.lock();
        if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection||
           Mailbox.DoseCutoffRevision!=Control.DoseCutoffRevision||Mailbox.DoseCutoffRequestId!=Control.DoseCutoffRequestId||
           !Mailbox.Running||FPlatformTime::Seconds()>Mailbox.DoseCutoffDeadline||DoseCutoff.Requests(Mailbox)!=Request)continue;
       }
       const bool OpeningEdge=!OpenHopper&&NextHopper;
       double CapturedBaselineTime=GPU.time();
       if(OpeningEdge){auto BeforeOpen=Preparing?MoveTemp(BeforePreparedOpen):GPU.snapshot();Telemetry.Begin(BeforeOpen,GPU.time(),Preparing?Control.WeighingOpenRevision:Control.Revision,PhysicalReceiverOpen());FPlantSimulationSnapshot Captured;Telemetry.Publish(Captured);CapturedBaselineTime=Captured.Transfer.StartedSimulationSeconds;if(C.count>0)Diagnostics.Capture("before_open",GPU,BeforeOpen,Telemetry,C,Routes,Control,Open,OpenHopper,OpenReceiver,true);}
       int NextPhase=NextHopper?1:0;if(NextPhase!=PhaseNow){PhaseNow=NextPhase;StageStart=GPU.time();C.plant_data[4].x=float(PhaseNow);}
       auto NextPressure=PressureState;
       const bool LiveUpload=Control.TransferPressureRevision>PressureRevision&&Control.TransferPressureRevision>RejectedTransferPressure;
       if(Regulated){
        try{
         NextPressure=powder::plant_set_regulated_pressures(C.plant_data,Routes,Reservoirs(Control),NextOpen,Request,NextHopper,WantHopper,NextReceiver,DesiredPurge,C.plant_powered_valves?&ValveApertures:nullptr);
        }catch(const std::invalid_argument& Error){
         if(!LiveUpload)throw;
         // The solver validates all branches before changing the field. Keep the last field on rejection.
         RejectedTransferPressure=Control.TransferPressureRevision;
         UE_LOG(LogTemp,Warning,TEXT("STAGE_PRESSURE_REJECT revision=%lld reason=%s"),RejectedTransferPressure,UTF8_TO_TCHAR(Error.what()));
         continue;
        }
       }
       else {powder::plant_set_manual_valves(C.plant_data,Routes,D,NextOpen,Request,NextHopper,WantHopper);powder::plant_set_receiver_valve(C.plant_data,NextReceiver);}
       if(C.plant_powered_valves)GPU.update_plant_controls(C.plant_data);else GPU.update_plant_field(C.plant_data);
       for(int Supplier=0;Supplier<6;++Supplier)if(!C.plant_powered_valves&&!(Open&(1u<<Supplier))&&(NextOpen&(1u<<Supplier)))DoseCutoff.Opened(Supplier,GPU.time());
       for(int Supplier=0;Supplier<6;++Supplier)if(!C.plant_powered_valves&&(Open&(1u<<Supplier))&&!(NextOpen&(1u<<Supplier))){
        if(Brakes[Supplier].Supported)Brakes[Supplier].GateClosedSimulationSeconds=GPU.time();
        DoseCutoff.Closed(Supplier,GPU.time());
        if(Supplier==1)PlantDoseTrace::Write(TEXT("gate_actual_closed"),Supplier,GPU.time(),Published.Sequence,Control.Revision,Batch.Snapshot().CaptureRevision,(Batch.Snapshot().InitialCount-Batch.Snapshot().InventorySourceCount)*Telemetry.ParticleMassKg,Brakes[Supplier].PredictedSpillKg);
        if(Batch.ActiveSupplier()!=Supplier)continue;
        Batch.SupplyClosed(GPU.snapshot(),GPU.time(),Control.Revision);const auto& B=Batch.Snapshot();
        UE_LOG(LogTemp,Display,TEXT("FEED_CLOSED_WORKER supplier=%d revision=%lld time=%.9f weighing_kg=%.12g connector_kg=%.12g passed_kg=%.12g additional_kg=%.12g source_count=%d"),Supplier,Control.Revision,GPU.time(),B.ClosedWeighingKg,B.ClosedConnectorKg,B.ClosedPassedKg,B.ClosingExtraKg,B.InventorySourceCount);
       }
       if(!OpenReceiver&&NextReceiver&&Control.AutomaticAction==1)
        UE_LOG(LogTemp,Display,TEXT("MIXER_FEED_OPEN revision=%lld batch=%lld time=%.9f kg=%.12g"),Control.AutomaticActionRevision,Control.AutomaticBatchRevision,GPU.time(),Batch.MixerSnapshot().BaselineKg);
       if(!C.plant_powered_valves&&OpenReceiver&&!NextReceiver){Batch.ReceiverClosed(GPU.time(),Control.Revision);
        UE_LOG(LogTemp,Display,TEXT("MIXER_FEED_CLOSED revision=%lld time=%.9f"),Control.Revision,GPU.time());}
       if(OpeningEdge&&Preparing){Preparation.Opened(CapturedBaselineTime,GPU.time());UE_LOG(LogTemp,Display,TEXT("FEED_READY_OPEN revision=%lld supply_confirmed=%.9f baseline=%.9f applied_open=%.9f residual=%d stable=%.9f"),Control.WeighingOpenRevision,Preparation.Snapshot().SupplyConfirmedSimulationSeconds,CapturedBaselineTime,GPU.time(),Preparation.Snapshot().SupplyResidualCount,Preparation.Snapshot().StableSeconds);}
       // Publish settings only after the entire field upload succeeds. No pressure command bypasses this worker.
       PressureState=NextPressure;AppliedSources=Control.SupplierPa;AppliedTargets=Control.GaugePressurePa;PressureRevision=Control.Revision;PressureAppliedTime=GPU.time();ValvePressureRevision=-1;
       if(LiveUpload&&OpeningCommit.owns_lock()){ScopedPressureUploadedRevision=Control.TransferPressureRevision;ScopedPressureUploadedPa=AppliedTargets[Control.PressureAxis];}
       if(LiveUpload)UE_LOG(LogTemp,Display,TEXT("STAGE_PRESSURE_APPLIED capture=%lld source=%d axis=%d transfer=%lld revision=%lld time=%.9f applied_pa=%.9g"),
           Control.PressureCapture,Control.PressureSource,Control.PressureAxis,Control.TransferPressureId,Control.TransferPressureRevision,GPU.time(),ScopedPressureUploadedPa);
       PurgeAppliedRevision=Control.Revision;
       if(PurgeChanged)UE_LOG(LogTemp,Display,TEXT("SUPPLY_PURGE supplier=%d applied revision=%lld time=%.9f enabled=%d pressure_pa=%.9g flow_m3s=%.12g"),Control.SupplyPurgeSupplier,PurgeAppliedRevision,GPU.time(),PressureState.purge.enabled,PressureState.purge.gauge_pa,PressureState.purge_m3s);
       OpenReceiver=NextReceiver;PreviousReceiver=WantReceiver;Open=NextOpen;OpenHopper=NextHopper;PreviousHopper=WantHopper;PreviousRequest=Request;AppliedPressure=D;
       if(Diagnostics.Enabled){auto Applied=GPU.snapshot();if(C.count>0)Diagnostics.Capture("control_applied",GPU,Applied,Telemetry,C,Routes,Control,Open,OpenHopper,OpenReceiver,true);}
      }LastControl=Begin;LastPreparationRevision=Control.WeighingOpenRevision;
     }
     std::unique_lock<std::mutex> MixerCommit(Mutex,std::defer_lock);
     if(Control.MixerEnabled&&Control.AutomaticAction==2&&AuthorizedMixerRevision!=Control.AutomaticActionRevision){
      auto Before=GPU.snapshot();MixerCommit.lock();
      if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection||
         Mailbox.AutomaticActionRevision!=Control.AutomaticActionRevision)continue;
      const bool Ready=ValvesClosed()&&Control.Running&&FPlatformTime::Seconds()<=Control.AutomaticDeadline&&Control.AutomaticActionRevision>0&&
         !OpenReceiver&&!Control.ReceiverOpen&&!OpenHopper&&!Control.WeighingOpen&&Open==0&&Control.Suppliers==0&&
         Control.RPM>0&&Control.RPM<=PlantControl::MaxRPM&&SuppliesAtAtmosphere()&&AppliedTargets==FVector::ZeroVector&&!PressureState.purge.enabled;
      if(!Ready||Batch.MixerSnapshot().RejectedRevision==Control.AutomaticActionRevision||
         !Batch.AdmitMixerStart(Before,GPU.time(),Control.AutomaticBatchRevision,Control.AutomaticActionRevision)){
       Batch.RejectDownstream(Control.AutomaticActionRevision,TEXT("혼합 시작 취소/만료 또는 전량 투입·실제 닫힘 미확인"));S=0;
      }else{AuthorizedMixerRevision=Control.AutomaticActionRevision;
       UE_LOG(LogTemp,Display,TEXT("MIXER_START_ADMITTED revision=%lld close_revision=%lld time=%.9f rpm=%.9g"),AuthorizedMixerRevision,Batch.MixerStartSnapshot().CloseRevision,GPU.time(),S);}
     }
     // The start token survives additive routing. A speed edit never admits a motor start.
     bool ApplySpeed=false;
     if(Control.SpeedRevision>0&&Control.SpeedRevision>FMath::Max(SpeedApplied,SpeedRejected)){
      if(!MixerCommit.owns_lock())MixerCommit.lock();
      if(Quit||Mailbox.Revision!=Control.Revision||Mailbox.ObservationConnection!=Control.ObservationConnection||
         Mailbox.SpeedRevision!=Control.SpeedRevision||Mailbox.MixRun!=Control.MixRun)continue;
      ApplySpeed=Control.Running&&Control.MixerEnabled&&Control.AutomaticMixClock&&Control.MixRun>0&&
       Control.MixRun==AuthorizedMixerRevision&&LastAppliedRPM>0&&GPU.time()<Control.MixEndSeconds&&
       FPlatformTime::Seconds()<=Control.SpeedDeadline&&std::isfinite(Control.SpeedRPM)&&Control.SpeedRPM>0&&Control.SpeedRPM<=PlantControl::MaxRPM;
      if(ApplySpeed){S=Control.SpeedRPM;SpeedTime=GPU.time();SpeedAngle=PhysicalAngle();}
      else SpeedRejected=Control.SpeedRevision;
     }
     GPU.advance(16,float(S),false);MixerState=GPU.plant_mixer_state();
     const double ActualRPM=C.plant_powered_mixer?(MixerState.stopped()?0.:MixerState.rpm()):double(float(S));
     if(C.plant_powered_valves){
      auto PreviousValves=ValveStates;ValveStates=GPU.plant_valve_states();bool Refresh=false;
      for(size_t I=0;I<Gates.size();++I){
       if(!PreviousValves[I].open()&&ValveStates[I].open())DoseCutoff.Opened(int(I),GPU.time());
       if(!PreviousValves[I].closed()&&ValveStates[I].closed()){DoseCutoff.Closed(int(I),GPU.time());if(Batch.ActiveSupplier()==int(I))Batch.SupplyClosed(GPU.snapshot(),GPU.time(),Control.Revision);}
      }
      if(!PreviousValves.back().closed()&&ValveStates.back().closed())Batch.ReceiverClosed(GPU.time(),Control.Revision);
      for(size_t I=0;I<ValveStates.size();++I){const auto& V=ValveStates[I];if(std::abs(V.angle_rad-LastValveAngles[I])>.002||(V.closed()&&LastValveAngles[I]!=0)||(V.open()&&LastValveAngles[I]!=V.angle_rad)){Refresh=true;}}
      if(Refresh){for(size_t I=0;I<ValveStates.size();++I){ValveApertures[I]=ValveStates[I].aperture();LastValveAngles[I]=ValveStates[I].angle_rad;}
       if(Regulated){PressureState=powder::plant_set_regulated_pressures(C.plant_data,Routes,Reservoirs(Control),Open,Open,OpenHopper,OpenHopper,OpenReceiver,PressureState.purge,&ValveApertures);if(C.plant_powered_valves)GPU.update_plant_controls(C.plant_data);else GPU.update_plant_field(C.plant_data);ValvePressureRevision=-1;}
      }
     }
     if(ApplySpeed){SpeedRPM=Control.RPM=Control.SpeedRPM;SpeedRun=Control.MixRun;SpeedApplied=Control.SpeedRevision;}
     Batch.ObserveMix(GPU.time(),Control.AutomaticMixClock&&!MixerState.tripped,ActualRPM);
     for(int Supplier=0;Supplier<6;++Supplier){auto& Brake=Brakes[Supplier];auto& BrakeControl=BrakeControls[Supplier];auto& BrakePendingRevision=BrakePendingRevisions[Supplier];
      if(!Brake.Supported||BrakePendingRevision<=0)continue;
      Brake.Applied=BrakeControl.enabled;Brake.AppliedRevision=BrakePendingRevision;Brake.AppliedSimulationSeconds=GPU.time();BrakePendingRevision=0;
      auto Current=GPU.snapshot();Batch.Observe(Current,GPU.time());Brake.BrakeAppliedPassedKg=(Batch.Snapshot().InitialCount-Batch.Snapshot().InventorySourceCount)*Telemetry.ParticleMassKg;
      PlantDoseTrace::Write(TEXT("brake_applied"),Supplier,GPU.time(),Published.Sequence,Control.Revision,Batch.Snapshot().CaptureRevision,Brake.BrakeAppliedPassedKg,Brake.PredictedSpillKg);
      UE_LOG(LogTemp,Display,TEXT("FEED_BRAKE_APPLIED supplier=%d revision=%lld time=%.9f enabled=%d lower_z=%.9g tau=%.9g passed_kg=%.12g"),Supplier,Brake.AppliedRevision,GPU.time(),Brake.Applied,Brake.LowerFaceMetres,Brake.ResponseTimeSeconds,Brake.BrakeAppliedPassedKg);
     }
     if(!HasAppliedRPM||LastRPMRevision!=Control.Revision||LastMotorCommandRPM!=double(float(S))||(LastAppliedRPM!=0&&ActualRPM==0)){
      LastRPMRevision=Control.Revision;LastRPMApplicationTime=GPU.time();LastRPMStartAngle=PhysicalAngle();
      if(HasAppliedRPM&&LastAppliedRPM!=0&&ActualRPM==0)
       UE_LOG(LogTemp,Display,TEXT("MIXER_RPM_ZERO_APPLIED revision=%lld time=%.9f"),LastRPMRevision,LastRPMApplicationTime);
     }
     LastMotorCommandRPM=double(float(S));LastAppliedRPM=ActualRPM;HasAppliedRPM=true;
     if(S>0&&ActualRPM>0&&!MixerState.tripped&&Control.AutomaticAction==2&&AuthorizedMixerRevision==Control.AutomaticActionRevision){
      const bool First=Batch.MixerStartSnapshot().MixerAppliedRevision!=AuthorizedMixerRevision;
      Batch.MixerApplied(GPU.time(),AuthorizedMixerRevision,LastAppliedRPM);
      if(First)UE_LOG(LogTemp,Display,TEXT("MIXER_RPM_APPLIED revision=%lld time=%.9f rpm=%.9g"),AuthorizedMixerRevision,GPU.time(),LastAppliedRPM);
     }
     if(MixerCommit.owns_lock())MixerCommit.unlock();
     bool DoseObserved=false,DoseTriggered=false;
     std::vector<powder::Grain> DosePoints,DoseVelocity;
     if(DoseCutoff.State.Armed&&!DoseCutoff.State.Triggered&&!DoseCutoff.State.Cancelled&&
        DoseCutoff.State.Supplier>=0&&(Open&(1u<<DoseCutoff.State.Supplier))){
      DosePoints=GPU.snapshot();DoseVelocity=GPU.velocity_snapshot();DoseObserved=true;
      Batch.Observe(DosePoints,GPU.time());ObserveBrake(DosePoints,DoseVelocity,GPU.time(),DoseCutoff.State.Supplier);
      // Linearize the one-shot decision against stop/reset/detach mailbox updates. No actor/UI calls here.
      std::lock_guard<std::mutex> Commit(Mutex);
      if(!Quit&&Mailbox.Revision==Control.Revision&&Mailbox.ObservationConnection==Control.ObservationConnection&&
         Mailbox.DoseCutoffRevision==Control.DoseCutoffRevision&&Mailbox.Running){
       const auto& B=Batch.Snapshot();const auto& Brake=Brakes[DoseCutoff.State.Supplier];
       if(Brake.Valid)DoseTriggered=DoseCutoff.Observe((B.WeighingCount+B.ConnectorCount)*Telemetry.ParticleMassKg,
          Brake.PredictedSpillKg,B.InventorySourceCount,GPU.time(),FPlatformTime::Seconds(),Published.Sequence+1);
       else DoseCutoff.ObservationUnavailable();
       if(DoseTriggered)PlantDoseTrace::Write(TEXT("worker_cutoff"),DoseCutoff.State.Supplier,GPU.time(),Published.Sequence+1,
         Control.Revision,B.CaptureRevision,DoseCutoff.State.PassedKg,DoseCutoff.State.SpillKg,DoseCutoff.State.EstimatedKg,0,DoseCutoff.State.RequestId.ToString());
      }
     }
     // Equipment-only mode advances real drive dynamics at wall-clock 1x; never dispatch particles.
     if(C.count==0){EmptyDeadline+=std::chrono::duration_cast<std::chrono::steady_clock::duration>(std::chrono::duration<double>(16*double(C.dt)));std::this_thread::sleep_until(EmptyDeadline);}
     Active+=std::chrono::duration<double>(std::chrono::steady_clock::now()-Begin).count();
     if(PhaseNow==0||(PhaseNow==1&&!WantHopper))StageStart=GPU.time(); // An intentional closed supply is not a timeout.
     // Empty equipment publishes every 16 ms batch so 60 fps rendering gets fresh shaft angles.
     if(C.count==0||DoseTriggered||std::chrono::duration<double>(std::chrono::steady_clock::now()-LastPublish).count()>.1){
      auto Next=DoseObserved?std::move(DosePoints):GPU.snapshot();auto Velocities=DoseObserved?std::move(DoseVelocity):GPU.velocity_snapshot();Batch.Observe(Next,GPU.time());ObserveBrake(Next,Velocities,GPU.time());ObserveResidual(Next);
      if(PurgeSupported&&C.count>0)BendDiagnostics.Capture(GPU,Next,Velocities,Preparation,C,Routes,PressureState,Control.Revision,SupplyResidual[1]);Telemetry.Observe(Next,GPU.time(),PhysicalHopperOpen(),PhysicalReceiverOpen());int W,H,M;Count(Next,W,H,M,CountMin,CountMax);
      if(C.count>0)Diagnostics.Capture("sample",GPU,Next,Telemetry,C,Routes,Control,Open,OpenHopper,OpenReceiver);
      {std::lock_guard<std::mutex>L(Mutex);Points=std::move(Next);Time=GPU.time();Angle=PhysicalAngle();Wall=Active;WH=W;RH=H;MX=M;Phase=PhaseNow;OpenSuppliers=Open;HopperOpen=OpenHopper;ReceiverOpen=OpenReceiver;++Generation;Publish(EPlantExecutionState::Running);}
      // Manual valve operation may intentionally retain material indefinitely.
      LastPublish=std::chrono::steady_clock::now();
     }
    }
   }catch(const std::exception&E){std::lock_guard<std::mutex>L(Mutex);Error=E.what();Published.Error=UTF8_TO_TCHAR(E.what());Published.Validity=EPlantValueValidity::Error;Published.AppliedPressureValid=false;Published.SupplyAPurge.Valid=false;Published.SupplyABrake.Valid=false;Published.SupplyBBrake.Valid=false;Published.SupplyCBrake.Valid=false;Published.SupplyDBrake.Valid=Published.SupplyEBrake.Valid=Published.SupplyFBrake.Valid=false;Published.SupplyBPurge.Valid=false;Published.SupplyCPurge.Valid=false;Published.SupplyDPurge.Valid=Published.SupplyEPurge.Valid=Published.SupplyFPurge.Valid=false;Published.SupplyABatch.Valid=false;Published.MixerFeed.Valid=false;Published.Execution=EPlantExecutionState::Fault;Published.UpdatedUtc=FDateTime::UtcNow();Published.PublishedWallSeconds=FPlatformTime::Seconds();++Published.Sequence;}
  });
 }
 ~FPlantRuntime(){{std::lock_guard<std::mutex>L(Mutex);Quit=true;Mailbox.WeighingOpen=false;Mailbox.WeighingOpenRevision=0;Mailbox.SupplyAPurgeEnabled=false;Mailbox.SupplyAPurgeGaugePa=0;Mailbox.SupplyAPurgeRevision=0;}if(Thread.joinable())Thread.join();}
};
APowderPlantTest::APowderPlantTest(){PrimaryActorTick.bCanEverTick=true;RootComponent=CreateDefaultSubobject<USceneComponent>(TEXT("Root"));}
APowderPlantTest::~APowderPlantTest(){Stop();}
void APowderPlantTest::Stop(){delete DemoRuntime;DemoRuntime=nullptr;delete Runtime;Runtime=nullptr;}
void APowderPlantTest::CacheRotor()
{
 Rotor.Empty();
 for(TActorIterator<AActor> It(GetWorld());It;++It){
  const FString N=It->GetActorNameOrLabel();
  // Revised imports explicitly identify the complete rotating assembly. Keep
  // the name fallback for older maps, but never rotate tagged static shaft seals.
  const bool Legacy=!It->ActorHasTag(TEXT("PlantMixerStatic"))&&N.StartsWith(TEXT("MX-301_"))&&
   (N.Contains(TEXT("Paddle"))||N.Contains(TEXT("Shaft")))&&!N.Contains(TEXT("Drive"))&&!N.Contains(TEXT("Support"));
  if(It->ActorHasTag(TEXT("PlantMixerRotor"))||Legacy){
   FPlantRotorVisual V;V.Actor=*It;V.Initial=It->GetActorTransform();
   if(auto* Root=It->GetRootComponent())Root->SetMobility(EComponentMobility::Movable);
   Rotor.Add(V);
  }
 }
}
bool APowderPlantTest::SetInitialParticleCounts(const TArray<int32>& Counts,FString& Reason){
 if(DemoRealtime){Reason=TEXT("입자 수 설정은 OpenCL 공정 레벨에서 사용하세요.");return false;}
 try{
  FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);float Radius=ParticleRadiusM;TArray<int32> OldCounts,Ids;
  auto Data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));
  auto Original=ReadCharge(Root/TEXT("test_charge.csv"),Radius,OldCounts,int32(powder::plant_supplier_gates(Data).size()),Ids);
  std::vector<int> C(Counts.begin(),Counts.end()),Sources(Ids.begin(),Ids.end());
  auto Charge=powder::resize_plant_charge(Data,Routes,Original,Sources,C,Radius);
  if(!Charge.particles.empty()){powder::GranularConfig Config;Config.plant=true;Config.plant_data=Data;Config.radius=Radius;Config.count=unsigned(Charge.particles.size());powder::validate_granular_initial(Config,Charge.particles);}
  InitialParticleCountsOverride=Counts;OverrideParticleCounts=true;ResetSimulation();Reason=Error;return Error.IsEmpty();
 }catch(const std::exception& E){Reason=UTF8_TO_TCHAR(E.what());return false;}
}
void APowderPlantTest::ApplySimulationFrameRate(){
 if(!HasActorBegunPlay())return;
 if(auto* FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))){
  AppliedMaxFPS=(!DemoRealtime&&ParticleCount==0)?60.f:30.f;
  FPS->Set(AppliedMaxFPS,ECVF_SetByCode);
 }
}
void APowderPlantTest::ResetSimulation(){if(DemoRealtime){ResetDemoSimulation();return;}AutomaticCancelStopPending=false;ControlTargets=FPlantControlTargets();++SnapshotEpoch;InitialSupplierCounts.Empty();Stop();Error.Empty();ParticleCount=0;PhysicalSeconds=ActualRate=0;Generation=0;CurrentPhase=0;AppliedSupplierMask=0;WeighingHopperOpen=AppliedHopperOpen=false;ReceiverOpen=AppliedReceiverOpen=MixerEnabled=false;InWeighingHopper=InReceiver=InMixer=0;try{
  InitializeSuppliers();for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=false;
  if(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0)throw std::runtime_error("invalid plant test controls");
  powder::plant_discharge_ready(0,1,0,MinimumReceiverFraction,ReceiverTransferSeconds);
  FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);TArray<int32> SupplierById;auto P=ReadCharge(Root/TEXT("test_charge.csv"),ParticleRadiusM,InitialSupplierCounts,SupplierOpen.Num(),SupplierById);if(OverrideParticleCounts){auto Data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));std::vector<int> Counts(InitialParticleCountsOverride.begin(),InitialParticleCountsOverride.end()),Ids(SupplierById.begin(),SupplierById.end());auto Charge=powder::resize_plant_charge(Data,Routes,P,Ids,Counts,ParticleRadiusM);P=std::move(Charge.particles);SupplierById.Empty();for(int Id:Charge.suppliers)SupplierById.Add(Id);InitialSupplierCounts=InitialParticleCountsOverride;}ParticleCount=int32(P.size());int Types=0;for(auto A:P)Types=FMath::Max(Types,int(A.w)+1);
  while(Groups.Num()<Types){auto*G=NewObject<UInstancedStaticMeshComponent>(this);G->SetupAttachment(RootComponent);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCastShadow(false);G->RegisterComponent();AddInstanceComponent(G);Groups.Add(G);}
  auto*Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/ProcessPlant_20261111_LowPoly/Particles/SM_GrainSphere.SM_GrainSphere"));if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));auto*Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ProcessPlant_20261111_LowPoly/Particles/M_BulkFlow.M_BulkFlow"));FLinearColor Colors[3]{FLinearColor(.9,.025,.01),FLinearColor(.01,.2,.9),FLinearColor(.9,.7,.01)};
  for(int I=0;I<Groups.Num();++I){auto*G=Groups[I].Get();G->ClearInstances();G->SetStaticMesh(Mesh);if(Mat){G->SetMaterial(0,Mat);if(auto*M=G->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I%3]);}}
  double Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;for(auto A:P)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));
  Stage=FString::Printf(TEXT("Initial charge: %d particles"),ParticleCount);if(HasActorBegunPlay()){powder::GranularConfig C;C.backend=static_cast<powder::GranularBackend>(ComputeBackend);C.cuda_ptx_file=TCHAR_TO_UTF8(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("SourceAssets/PowderCUDA/plant_sm89.ptx")));C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));RotorPivotCm=FVector(C.plant_data[1].x,-C.plant_data[1].y,C.plant_data[1].z)*100;C.plant_wall_mode=(C.plant_data[10].x==2&&C.plant_data[13].z>0&&C.plant_data[13].w>0)?(P.size()>=3000?5u:3u):0u;if(int(C.plant_data[11].x)!=SectionTest)throw std::runtime_error("section mode/data mismatch");if(C.backend!=powder::GranularBackend::OpenCL&&C.plant_wall_mode==3)C.plant_wall_mode=5;C.plant_powered_mixer=PoweredMixer;C.mixer_drive.input_w=MixerInputPowerW;C.mixer_drive.torque_nm=MixerMaxTorqueNm;C.mixer_drive.inertia_kg_m2=MixerInertiaKgM2;C.mixer_drive.efficiency=MixerDriveEfficiency;C.mixer_drive.supply_fraction=MixerPowerFraction;C.mixer_drive.ramp_seconds=MixerRampSeconds;C.mixer_drive.brake_nm=MixerBrakeTorqueNm;C.mixer_drive.trip_seconds=MixerTripSeconds;C.plant_powered_valves=PoweredValves&&C.backend==powder::GranularBackend::OpenCL;C.small_valve.torque_nm=SmallValveTorqueNm;C.small_valve.holding_nm=SmallValveTorqueNm;C.large_valve.torque_nm=LargeValveTorqueNm;C.large_valve.holding_nm=LargeValveTorqueNm;C.small_valve.electrical_w=SmallValveElectricalW;C.large_valve.electrical_w=LargeValveElectricalW;C.small_valve.travel_seconds=SmallValveTravelSeconds;C.large_valve.travel_seconds=LargeValveTravelSeconds;C.small_valve.supply_fraction=C.large_valve.supply_fraction=ValvePowerFraction;
  ConfigureFixedMixerObstacles(GetWorld(),C);
  ValvePivotsCm.Empty();for(int I=0;I<int(C.plant_data[5].y);++I){auto G=C.plant_data[int(C.plant_data[5].x)+2*I];ValvePivotsCm.Add(FVector(G.x,-G.y,G.z)*100);}
  C.radius=ParticleRadiusM;C.count=unsigned(P.size());C.dt=P.empty()?.001f:float(FixedDt);C.iterations=unsigned(ContactIterations);auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));for(auto V:Rotor)if(V.Actor)V.Actor->SetActorTransform(V.Initial);ControlTargets.Running=Running;ControlTargets.RPM=MixerRPM;ControlTargets.Pressure=TestPressureDropPa;Runtime=new FPlantRuntime(std::move(C),std::move(P),std::move(Routes),StageTimeoutSeconds,MinimumReceiverFraction,ReceiverTransferSeconds,Running,TestPressureDropPa,0,TransferSettings,MixerBoundsMinM,MixerBoundsMaxM,RegulatedPressureEnabled,ProcessPlantBinding::IsLowPoly(GetPathName())&&SectionTest==0,MoveTemp(SupplierById));PublishControls();ApplySimulationFrameRate();}
 }catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}
void APowderPlantTest::BeginPlay(){if(DemoRealtime){BeginDemoPlay();return;}Super::BeginPlay();TArray<int32> SavedParticleCounts;if(UProcessControlService::LoadParticleCounts(DataDirectory,SavedParticleCounts)){OverrideParticleCounts=true;InitialParticleCountsOverride=MoveTemp(SavedParticleCounts);}if(GEngine)GEngine->RemoveOnScreenDebugMessage(3101);if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))){PreviousMaxFPS=FPS->GetFloat();AppliedMaxFPS=30.f;FPS->Set(AppliedMaxFPS,ECVF_SetByCode);}FParse::Value(FCommandLine::Get(),TEXT("PlantSmokeSeconds="),SmokeSeconds);FParse::Value(FCommandLine::Get(),TEXT("PlantWallLimit="),SmokeWallLimit);SmokeStart=FPlatformTime::Seconds();CacheRotor();ValveVisuals.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){if(auto*C=Cast<APlantVisibilityController>(*It))Display=C;const FString N=It->GetActorNameOrLabel();if((N.StartsWith(TEXT("V101"))&&N.EndsWith(TEXT("_Disc")))||N==TEXT("ASSUMED_VT_MovingGate")||N==TEXT("BV272_210_Disc")||N==TEXT("BV272_212_Disc_hub")||N==TEXT("BV122_210_Disc")||N==TEXT("BV122_212_Disc_hub")){ValveVisuals.Add(*It);if(auto* Mesh=It->GetComponentByClass<UStaticMeshComponent>())Mesh->SetMobility(EComponentMobility::Movable);}}ResetSimulation();}
void APowderPlantTest::Show(){if(DemoRealtime){ShowDemo();return;}if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);if(!Runtime->Error.empty()){Error=UTF8_TO_TCHAR(Runtime->Error.c_str());Running=false;}if(Generation==Runtime->Generation)return;Generation=Runtime->Generation;ComputeDevice=Runtime->Published.GPUDevice;AppliedGaugePa=Runtime->Published.AppliedGaugePressurePa;PressureApplied=Runtime->Published.AppliedPressureValid;WeighingRegulatorM3s=Runtime->Published.WeighingRegulatorM3s;TransferFlowM3s=Runtime->Published.Transfer.CalculatedCarrierFlowM3s;PhysicalSeconds=Runtime->Time;ActualRate=Runtime->Wall>0?PhysicalSeconds/Runtime->Wall:0;InWeighingHopper=Runtime->WH;InReceiver=Runtime->RH;InMixer=Runtime->MX;CurrentPhase=Runtime->Phase;AppliedSupplierMask=Runtime->Published.AppliedSupplierMask;AppliedHopperOpen=Runtime->Published.AppliedHopperOpen;AppliedReceiverOpen=Runtime->Published.AppliedReceiverOpen;Stage=Runtime->Phase==0?TEXT("공급기 → 칭량 호퍼"):Runtime->Phase==1?TEXT("공급기 → 칭량 → 수취부 (연결 유동)"):TEXT("수신기 → 혼합기");
 TArray<TArray<FTransform>> T;T.SetNum(Groups.Num());for(auto A:Runtime->Points){auto*G=Groups[int(A.w)].Get();double Diameter=G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2;T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));}for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,T[I],false,true,true);
 for(auto V:ValveVisuals)if(V){const FString N=V->GetActorNameOrLabel();int I=N.StartsWith(TEXT("V101"))?int(N[4]-TCHAR('A')):N.StartsWith(TEXT("ASSUMED"))?SupplierOpen.Num():N.StartsWith(TEXT("BV272"))?SupplierOpen.Num()+1:INDEX_NONE;
  if(Runtime->Published.ValveActuators.IsValidIndex(I)&&ValvePivotsCm.IsValidIndex(I)){auto& Initial=ValveInitialTransforms.FindOrAdd(V,V->GetActorTransform());const auto& A=Runtime->Published.ValveActuators[I];FQuat Q(FVector::XAxisVector,-FMath::DegreesToRadians(A.AngleDegrees));FTransform ValveTransform=Initial;ValveTransform.SetLocation(ValvePivotsCm[I]+Q.RotateVector(Initial.GetLocation()-ValvePivotsCm[I]));ValveTransform.SetRotation(Q*Initial.GetRotation());V->SetActorTransform(ValveTransform);V->SetActorHiddenInGame(Display&&Display->ViewMode==EPlantViewMode::Hidden);}
  else {const bool LegacyOpen=N.StartsWith(TEXT("BV122"))||(N.StartsWith(TEXT("V101"))&&I>=0&&(Runtime->OpenSuppliers&(1u<<I)))||(N.StartsWith(TEXT("ASSUMED"))&&Runtime->HopperOpen)||(N.StartsWith(TEXT("BV272"))&&Runtime->ReceiverOpen);V->SetActorHiddenInGame(LegacyOpen||(Display&&Display->ViewMode==EPlantViewMode::Hidden));}
 }
 FQuat Q(FVector(1,0,0),-Runtime->Angle);const FVector Pivot=RotorPivotCm;for(auto V:Rotor)if(V.Actor){FTransform X=V.Initial;X.SetLocation(Pivot+Q.RotateVector(V.Initial.GetLocation()-Pivot));X.SetRotation(Q*V.Initial.GetRotation());V.Actor->SetActorTransform(X);}}
void APowderPlantTest::Tick(float D){if(DemoRealtime){TickDemoRuntime(D);return;}Super::Tick(D);
 // Finish a service-requested stop even if that UI owner has detached. Never pause before actual closure.
 if(AutomaticCancelStopPending&&Runtime){
  const auto S=GetSimulationSnapshot();const auto& P=S.SupplyAPurge;
  if(S.Validity==EPlantValueValidity::Valid&&S.ProcessedControlRevision>=ControlTargets.Revision&&
     !ControlTargets.Suppliers&&!ControlTargets.WeighingOpen&&!ControlTargets.ReceiverOpen&&!ControlTargets.MixerEnabled&&
     S.AppliedSupplierMask==0&&!S.AppliedHopperOpen&&!S.AppliedReceiverOpen&&S.AppliedRPMValid&&S.AppliedRPM==0&&
     S.AppliedPressureValid&&S.SuppliersVented()&&S.AppliedGaugePressurePa.IsZero()&&ControlTargets.GaugePressurePa.IsZero()&&
     P.Valid&&!P.RequestedEnabled&&!P.AppliedEnabled&&P.RequestedGaugePa==0&&P.AppliedGaugePa==0&&P.FlowM3s==0){
   AutomaticCancelStopPending=false;Running=ControlTargets.Running=false;++ControlTargets.Revision;PublishControls();
  }
 }
if(auto*PC=GetWorld()->GetFirstPlayerController()){const auto* UI=GetWorld()->GetSubsystem<UProcessUIStateSubsystem>();if(LegacyControlsAllowed()&&(!UI||!UI->IsReadOnlyScreenActive())){if(PC->WasInputKeyJustPressed(EKeys::SpaceBar))Running=!Running;if(PC->WasInputKeyJustPressed(EKeys::R))ResetSimulation();if(PC->WasInputKeyJustPressed(EKeys::O))ExportSnapshot();}if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){int View=-1;if(PC->WasInputKeyJustPressed(EKeys::One))View=0;if(PC->WasInputKeyJustPressed(EKeys::Two))View=1;if(PC->WasInputKeyJustPressed(EKeys::Three))View=2;if(PC->WasInputKeyJustPressed(EKeys::Four))View=3;if(View>=0){FVector Targets[]{FVector(0,-140,260),FVector(0,0,115),FVector(300,0,275),FVector(300,0,90)};FVector Target=Targets[View];Camera->SetActorLocation(Target+FVector(80,View==0?300:150,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}}}if(Runtime){if(ControlOwner.IsValid()){Running=ControlTargets.Running;MixerRPM=ControlTargets.RPM;MixerEnabled=ControlTargets.MixerEnabled;WeighingHopperOpen=ControlTargets.WeighingOpen;ReceiverOpen=ControlTargets.ReceiverOpen;for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=(ControlTargets.Suppliers&(uint32(1)<<i))!=0;}else{if(ControlTargets.WeighingOpenRevision)RevokePreparedOpening();if(ControlTargets.SupplyAPurgeRevision||ControlTargets.SupplyAPurgeEnabled||ControlTargets.SupplyAPurgeGaugePa!=0)RevokeSupplyAPurge();ControlTargets.Running=Running;ControlTargets.Pressure=TestPressureDropPa;ControlTargets.RPM=MixerRPM;ControlTargets.MixerEnabled=MixerEnabled;ControlTargets.Suppliers=0;for(int i=0;i<SupplierOpen.Num();++i)if(SupplierOpen[i])ControlTargets.Suppliers|=uint32(1)<<i;ControlTargets.WeighingOpen=WeighingHopperOpen;ControlTargets.ReceiverOpen=ReceiverOpen;}PublishControls();}LastShow+=D;if(ParticleCount==0||LastShow>.1){Show();LastShow=0;}if(SmokeSeconds>0){if(!SmokeShot&&PhysicalSeconds>=SmokeSeconds*.4){if(auto*PC=GetWorld()->GetFirstPlayerController())if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){FVector Target(0,-140,260);Camera->SetActorLocation(Target+FVector(30,300,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlantFlowPreview.png"),true,false);SmokeShot=true;}
 if(PhysicalSeconds>=SmokeSeconds||!Error.IsEmpty()||FPlatformTime::Seconds()-SmokeStart>SmokeWallLimit){ExportSnapshot();FString Report=FString::Printf(TEXT("{\"count\":%d,\"physical_s\":%.9g,\"actual_rate\":%.9g,\"wh\":%d,\"rh\":%d,\"mixer\":%d,\"failed\":%s}"),ParticleCount,PhysicalSeconds,ActualRate,InWeighingHopper,InReceiver,InMixer,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?TEXT("false"):TEXT("true"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PlantSmoke.json")));if(!Error.IsEmpty())FFileHelper::SaveStringToFile(Error,*(FPaths::ProjectSavedDir()/TEXT("PlantSmokeError.txt")));FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);SmokeSeconds=0;}}}
void APowderPlantTest::ExportSnapshot(){if(DemoRealtime){ExportSnapshotToFile(FPaths::ProjectSavedDir()/TEXT("PlantSnapshot.csv"));return;}if(!Runtime)return;std::lock_guard<std::mutex>L(Runtime->Mutex);std::ostringstream S;S<<"id,species,x_m,y_m,z_m\n";unsigned Id=0;for(auto A:Runtime->Points)S<<Id++<<','<<A.w<<','<<A.x<<','<<A.y<<','<<A.z<<'\n';FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*(FPaths::ProjectSavedDir()/TEXT("PlantSnapshot.csv")));}
void APowderPlantTest::EndPlay(const EEndPlayReason::Type Reason){Stop();if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS")))if(AppliedMaxFPS>0&&FPS->GetFloat()==AppliedMaxFPS)FPS->Set(PreviousMaxFPS,ECVF_SetByCode);Super::EndPlay(Reason);}

void APowderPlantTest::InitializeSuppliers(){
 FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);
 auto Data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));
 SupplierOpen.Init(false,int32(powder::plant_supplier_gates(Data).size()));
}
void APowderPlantTest::ToggleSupplier(int32 Index){
 if(DemoRealtime)DemoAutoCycle=false;if(LegacyControlsAllowed()&&HasActorBegunPlay()&&CurrentPhase<2&&Error.IsEmpty()&&SupplierOpen.IsValidIndex(Index))SupplierOpen[Index]=!SupplierOpen[Index];
}
void APowderPlantTest::ToggleWeighingHopper(){
 if(DemoRealtime)DemoAutoCycle=false;if(LegacyControlsAllowed()&&HasActorBegunPlay()&&CurrentPhase<2&&Error.IsEmpty())WeighingHopperOpen=!WeighingHopperOpen;
}
void APowderPlantTest::ToggleReceiver(){if(DemoRealtime)DemoAutoCycle=false;if(LegacyControlsAllowed()&&HasActorBegunPlay()&&Error.IsEmpty())ReceiverOpen=!ReceiverOpen;}
void APowderPlantTest::ToggleMixer(){if(DemoRealtime)DemoAutoCycle=false;if(LegacyControlsAllowed()&&HasActorBegunPlay()&&Error.IsEmpty())MixerEnabled=!MixerEnabled;}
TSharedRef<SWidget> APowderPlantTest::BuildSimulationPanel(){if(DemoRealtime){return BuildDemoPanel();}
 if(SupplierOpen.IsEmpty()){try{InitializeSuppliers();}catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());}}
 const TWeakObjectPtr<APowderPlantTest> Weak(this);
 TSharedRef<SVerticalBox> Panel=SNew(SVerticalBox);
 if(SectionTest>0){const TCHAR*Names[]{TEXT(""),TEXT("공급기 → 칭량호퍼 | 칭량부 시험 흡입압"),TEXT("칭량호퍼 → 수취부 | 칭량부 대기압·수취부 시험 흡입압"),TEXT("수취부 → 혼합기 | 양쪽 대기압·중력 배출"),TEXT("혼합기 내부 | 공기 이송 없음")};Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(Names[FMath::Clamp(SectionTest,0,4)]))];}
 if(DataDirectory.Contains(TEXT("ProcessPlantHybrid")))Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("혼합 충돌 시험: 관·연결부 메시 / 호퍼·혼합기 OBB")))];
 if(DataDirectory.Contains(TEXT("ProcessPlantOBB")))Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("OBB 통로 근사 시험 · 원본 형상과 다름")))];
 Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text(FText::FromString(TEXT("시뮬레이션 정보 · 배속 제한 없음")))];
 Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([Weak](){
  if(!Weak.IsValid())return FText::GetEmpty();const auto*P=Weak.Get();
  return FText::FromString(FString::Printf(TEXT("시간 %.3f초 | 실제 속도 %.4f배\n입자 %d개 | %s\n칭량 %d · 수신기 %d · 혼합기 %d\n통로/기타 %d | %s"),P->PhysicalSeconds,P->ActualRate,P->ParticleCount,*(P->SectionTest>0?FString::Printf(TEXT("구간 %d 독립 시험"),P->SectionTest):P->Stage),P->InWeighingHopper,P->InReceiver,P->InMixer,P->ParticleCount-P->InWeighingHopper-P->InReceiver-P->InMixer,P->Running?TEXT("계산 중"):TEXT("일시정지")));
 })];
 if(SectionTest==0||SectionTest==1)Panel->AddSlot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("공급기 밸브 · 시작/초기화 시 모두 닫힘")))];
 for(int32 I=0;I<SupplierOpen.Num()&&(SectionTest==0||SectionTest==1);++I){
  Panel->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,I](){
    if(!Weak.IsValid()||!Weak->SupplierOpen.IsValidIndex(I))return FText::GetEmpty();
    bool Want=Weak->SupplierOpen[I],Open=(Weak->AppliedSupplierMask&(uint32(1)<<I))!=0;
    const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
    return FText::FromString(FString::Printf(TEXT("공급기 %c · %s"),TCHAR('A'+I),State));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->LegacyControlsAllowed()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,I](){if(Weak.IsValid())Weak->ToggleSupplier(I);return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,I](){return FText::FromString(Weak.IsValid()&&Weak->SupplierOpen.IsValidIndex(I)&&Weak->SupplierOpen[I]?TEXT("닫기"):TEXT("열기"));})]]];
 }
 if(SectionTest==0||SectionTest==2)Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
  + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak](){
   if(!Weak.IsValid())return FText::GetEmpty();bool Want=Weak->WeighingHopperOpen,Open=Weak->AppliedHopperOpen;
   const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
   return FText::FromString(FString::Printf(TEXT("공용 칭량 호퍼 · %s"),State));
  })]
  + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
   .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->LegacyControlsAllowed()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
   .OnClicked_Lambda([Weak](){if(Weak.IsValid())Weak->ToggleWeighingHopper();return FReply::Handled();})
   [SNew(STextBlock).Text_Lambda([Weak](){return FText::FromString(Weak.IsValid()&&Weak->WeighingHopperOpen?TEXT("닫기"):TEXT("열기"));})]]];
 for(int Control=0;Control<2;++Control){
  if(SectionTest!=0&&!(Control==0?SectionTest==3:(SectionTest==3||SectionTest==4)))continue;
  Panel->AddSlot().AutoHeight().Padding(0,6,0,0)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,Control](){
    if(!Weak.IsValid())return FText::GetEmpty();
    if(Control==1)return FText::FromString(FString::Printf(TEXT("혼합기 · %s (%.1f RPM)"),Weak->MixerEnabled?TEXT("가동 요청"):TEXT("정지"),Weak->MixerRPM));
    bool Want=Weak->ReceiverOpen,Open=Weak->AppliedReceiverOpen;
    return FText::FromString(FString::Printf(TEXT("수취부 → 혼합기 · %s"),Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기"):TEXT("닫힘"))));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->LegacyControlsAllowed()&&Weak->HasActorBegunPlay()&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,Control](){if(Weak.IsValid()){if(Control)Weak->ToggleMixer();else Weak->ToggleReceiver();}return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,Control](){return FText::FromString(Control?(Weak.IsValid()&&Weak->MixerEnabled?TEXT("정지"):TEXT("시작")):(Weak.IsValid()&&Weak->ReceiverOpen?TEXT("닫기"):TEXT("열기")));})]]];
 }
 Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("1~4: 장치 확대 | Space: 일시정지\nR: 초기화 | O: 입자 저장\n관로 압력 근사 + GPU 입자 시험 · 미보정")))];
 Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).WrapTextAt(350).ColorAndOpacity(FLinearColor(1,.25,.2)).Text_Lambda([Weak](){return Weak.IsValid()?FText::FromString(Weak->Error):FText::GetEmpty();})];
 return SNew(SBox).WidthOverride(360)[Panel];
}

FPlantSimulationSnapshot APowderPlantTest::GetSimulationSnapshot() const {
 check(IsInGameThread());
 FPlantSimulationSnapshot Result;
 if(Runtime){std::lock_guard<std::mutex> Lock(Runtime->Mutex);Result=Runtime->Published;}
 else {Result.Validity=EPlantValueValidity::Initializing;Result.Execution=EPlantExecutionState::Initializing;}
 if(Result.Transfer.Connection!=ControlTargets.ObservationConnection){
  Result.WeighingMass.Validity=Result.PipeMass.Validity=Result.ReceiverMass.Validity=EPlantValueValidity::Unknown;
  Result.Transfer=FPlantTransferSnapshot();Result.Transfer.Connection=ControlTargets.ObservationConnection;
  Result.Transfer.State=EPlantTransferState::Indeterminate;Result.Transfer.Reason=TEXT("연결 변경으로 이전 이송 결과를 무효화했습니다. 새 확인값을 기다리세요.");
  Result.DoseCutoff.Valid=false;Result.DoseCutoff.Armed=false;Result.DoseCutoff.Cancelled=true;
  Result.FeedPreparation=FPlantFeedPreparationSnapshot();Result.SupplyAPurge.Valid=false;Result.SupplyBPurge.Valid=false;Result.SupplyCPurge.Valid=false;Result.SupplyDPurge.Valid=Result.SupplyEPurge.Valid=Result.SupplyFPurge.Valid=false;Result.SupplyABatch.Valid=false;Result.MixerFeed.Valid=false;
 }
 Result.Epoch=SnapshotEpoch;Result.SourceActorPath=GetPathName();Result.RequestedControlRevision=ControlTargets.Revision;
 Result.EquipmentOnly=ParticleCount==0;Result.TotalParticleCount=ParticleCount;Result.InitialSupplierCounts=InitialSupplierCounts;Result.RequestedSuppliers=SupplierOpen;
 Result.RequestedHopperOpen=WeighingHopperOpen;Result.RequestedReceiverOpen=ReceiverOpen;
 Result.RequestedRunning=Running;Result.RequestedMixerEnabled=MixerEnabled;Result.RequestedRPM=ControlTargets.SpeedRevision>0?ControlTargets.SpeedRPM:MixerRPM;
 Result.RequestedSupplierPa=ControlTargets.SupplierPa;Result.RequestedGaugePressurePa=ControlTargets.GaugePressurePa;
 for(int I=0;I<6;++I){auto& Purge=PlantPurgeFor(Result,I);Purge.RequestedEnabled=ControlTargets.SupplyPurgeSupplier==I&&ControlTargets.SupplyAPurgeEnabled;Purge.RequestedGaugePa=ControlTargets.SupplyPurgeSupplier==I?ControlTargets.SupplyAPurgeGaugePa:0;}
 if(!Error.IsEmpty()){Result.Error=Error;Result.Validity=EPlantValueValidity::Error;Result.Execution=EPlantExecutionState::Fault;}
 if(Result.Validity!=EPlantValueValidity::Valid){Result.WeighingMass.Validity=Result.PipeMass.Validity=Result.ReceiverMass.Validity=Result.Validity;Result.Transfer.Validity=Result.Validity;Result.SupplyAPurge.Valid=false;Result.SupplyBPurge.Valid=false;Result.SupplyCPurge.Valid=false;Result.SupplyDPurge.Valid=Result.SupplyEPurge.Valid=Result.SupplyFPurge.Valid=false;Result.SupplyABatch.Valid=false;Result.MixerFeed.Valid=false;}
 if(Result.Validity==EPlantValueValidity::Valid){
  if(!Running&&Result.Execution==EPlantExecutionState::Running)Result.Execution=EPlantExecutionState::PauseRequested;
  if(Running&&Result.Execution==EPlantExecutionState::Paused)Result.Execution=EPlantExecutionState::ResumeRequested;
 }
 return Result;
}


bool APowderPlantTest::LegacyControlsAllowed() const { return !ControlOwner.IsValid(); }

bool APowderPlantTest::ClaimControl(UProcessControlService* ServiceOwner)
{
 check(IsInGameThread());
 if(DemoRealtime)return false;
 if(ControlOwner.IsValid() && ControlOwner.Get()!=ServiceOwner)return false;
 if(!ControlOwner.IsValid()){
  ++ControlTargets.ObservationConnection;
  ControlTargets.Running=Running;ControlTargets.Pressure=TestPressureDropPa;ControlTargets.RPM=MixerRPM;
  ControlTargets.MixerEnabled=MixerEnabled;ControlTargets.WeighingOpen=WeighingHopperOpen;ControlTargets.ReceiverOpen=ReceiverOpen;
  ControlTargets.Suppliers=0;for(int32 I=0;I<SupplierOpen.Num();++I)if(SupplierOpen[I])ControlTargets.Suppliers|=uint32(1)<<I;
 }
 ControlOwner=ServiceOwner;PublishControls();return true;
}

void APowderPlantTest::ReleaseControl(UProcessControlService* ServiceOwner)
{
 check(IsInGameThread());
 if(ControlOwner.Get()!=ServiceOwner)return;
 RevokeTransferPressure();
 if(ControlTargets.MixRun)MixerEnabled=ControlTargets.MixerEnabled=false;
 ControlTargets.SpeedRevision=ControlTargets.MixRun=0;
 ControlTargets.RecoveryRevision=0;ControlTargets.AutomaticMixClock=false;
 RevokeDoseCutoff();RevokePreparedOpening();RevokeSupplyAPurge();
 if(ControlTargets.AutomaticAction==1)ReceiverOpen=ControlTargets.ReceiverOpen=false;
 if(ControlTargets.AutomaticAction==2)MixerEnabled=ControlTargets.MixerEnabled=false;
 ControlTargets.DoseFinalizeRevision=ControlTargets.DoseCaptureRevision=0;ControlTargets.DoseDeadline=0;
 ControlTargets.BatchTransitionRevision=ControlTargets.BatchTransitionCapture=0;ControlTargets.AutomaticAction=0;ControlTargets.AutomaticActionRevision=ControlTargets.AutomaticBatchRevision=0;
 ControlOwner.Reset();++ControlTargets.ObservationConnection;PublishControls();
}

bool APowderPlantTest::ValidateControl(int64 Epoch,FString& Reason) const
{
 check(IsInGameThread());
 if(!ControlOwner.IsValid()){Reason=TEXT("제어 서비스가 연결되지 않았습니다.");return false;}
 if(Epoch!=SnapshotEpoch){Reason=TEXT("초기화 세대가 변경되었습니다.");return false;}
 const auto State=GetSimulationSnapshot();
 if(!Runtime||!HasActorBegunPlay()||State.Validity!=EPlantValueValidity::Valid||State.Execution==EPlantExecutionState::Fault){Reason=TEXT("작업자를 사용할 수 없습니다. 오류 시 복구·초기화가 필요합니다.");return false;}
 if(SectionTest!=0||!ProcessPlantBinding::SupportedCount(SupplierOpen.Num())){Reason=TEXT("이 제어 연결은 검증된 전체 공정 A~C 또는 A~F 구성만 지원합니다.");return false;}
 return true;
}

void APowderPlantTest::PublishControls()
{
 if(!Runtime)return;
 std::lock_guard<std::mutex> Lock(Runtime->Mutex);
 Runtime->Mailbox=ControlTargets;
}

void APowderPlantTest::RevokeDoseCutoff()
{
 const int I=ControlTargets.DoseCutoffSupplier;
 if(ControlTargets.DoseCutoffRevision>0&&SupplierOpen.IsValidIndex(I)){
  SupplierOpen[I]=false;ControlTargets.Suppliers&=~(uint32(1)<<I);
 }
 ControlTargets.DoseCutoffRevision=0;ControlTargets.DoseCutoffBatch=0;ControlTargets.DoseCutoffRequestId.Invalidate();
}
bool APowderPlantTest::SetSupplierValve(int32 Index,bool Open,int64 Epoch,int64& Revision,FString& Reason,double TargetKg,double ToleranceKg,int64 BatchRevision,FGuid RequestId,double Deadline)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(!Open)RevokeTransferPressure();
 if(!SupplierOpen.IsValidIndex(Index)){Reason=TEXT("지원하지 않는 공급기입니다.");return false;}
 if(Open&&(ControlTargets.SupplyAPurgeEnabled||AnyPlantPurge(GetSimulationSnapshot(),[](const auto& P){return P.AppliedEnabled;}))){Reason=TEXT("연결부 공기 정지 확인 후 공급 밸브를 여세요.");return false;}
 const bool Dose=Open&&BatchRevision>0;
 if(Dose){
  const auto S=GetSimulationSnapshot();
  Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.ModelParticleMassKg,S.SupplyABatch.InitialKg);
  if(!Reason.IsEmpty())return false;
  if(!S.DoseCutoff.Supported||Index>=6||!RequestId.IsValid()||!FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()||
     S.SupplyABatch.CaptureRevision!=BatchRevision||S.SequentialBatch.ActiveSupplier!=Index||S.SupplyABatch.DoseFinalized||
     (S.SupplyABatch.SourceCount!=S.SupplyABatch.InitialCount&&!(ControlTargets.RecoveryRevision>0&&S.Recovery.Ticket==ControlTargets.RecoveryRevision&&S.Recovery.Valid&&S.Recovery.ForeignIds.IsEmpty()&&
      S.SupplyABatch.SourceCount+S.SupplyABatch.ConnectorCount+S.SupplyABatch.WeighingCount==S.SupplyABatch.InitialCount))||S.AppliedSupplierMask||ControlTargets.Suppliers||
     !ControlTargets.Running||!S.AppliedPressureValid||S.SupplierPressure(Index,true)!=S.SupplierPressure(Index,false)||S.AppliedGaugePressurePa!=ControlTargets.GaugePressurePa){
   Reason=TEXT("작업자 목표량 공급의 배치·재고·요청·실제 압력/밸브 조건 미확인");return false;
  }
 }
 RevokeDoseCutoff();
 SupplierOpen[Index]=Open;
 if(Open)ControlTargets.Suppliers|=uint32(1)<<Index;else ControlTargets.Suppliers&=~(uint32(1)<<Index);
 Revision=++ControlTargets.Revision;
 if(Dose){ControlTargets.DoseCutoffRequestId=RequestId;ControlTargets.DoseCutoffRevision=Revision;
  ControlTargets.DoseCutoffBatch=BatchRevision;ControlTargets.DoseCutoffEpoch=Epoch;ControlTargets.DoseCutoffSupplier=Index;
  ControlTargets.DoseCutoffTargetKg=TargetKg;ControlTargets.DoseCutoffToleranceKg=ToleranceKg;
  ControlTargets.DoseCutoffRequestedWall=FPlatformTime::Seconds();ControlTargets.DoseCutoffDeadline=Deadline;
 }
 PublishControls();return true;
}
bool APowderPlantTest::SetWeighingValve(bool Open,int64 Epoch,int64& Revision,FString& Reason,double PreparationDeadline,int64 RequiredBatchRevision)
{
 if(!ValidateControl(Epoch,Reason))return false;
 const bool Prepare=Open&&ParticleCount>0&&ProcessPlantBinding::IsLowPoly(GetPathName())&&!GetSimulationSnapshot().AppliedHopperOpen;
 if(Prepare&&(!FMath::IsFinite(PreparationDeadline)||PreparationDeadline<=FPlatformTime::Seconds())){Reason=TEXT("공급 준비 요청의 유효한 제한 시간이 필요합니다.");return false;}
 WeighingHopperOpen=ControlTargets.WeighingOpen=Open;Revision=++ControlTargets.Revision;
 ControlTargets.WeighingOpenRevision=Prepare?Revision:0;
 ControlTargets.RequiredABatchRevision=Prepare?RequiredBatchRevision:0;
 ControlTargets.WeighingOpenDeadline=Prepare?PreparationDeadline:0;
 ControlTargets.WeighingOpenLeaseUntil=Prepare?FPlatformTime::Seconds()+2.:0;
 PublishControls();return true;
}

void APowderPlantTest::MaintainPreparedOpening(int64 Revision,bool KeepAlive)
{
 if(ControlTargets.WeighingOpenRevision!=Revision||!Runtime)return;
 if(KeepAlive){ControlTargets.WeighingOpenLeaseUntil=FPlatformTime::Seconds()+2.;PublishControls();}
 else {WeighingHopperOpen=ControlTargets.WeighingOpen=false;ControlTargets.WeighingOpenRevision=0;ControlTargets.WeighingOpenLeaseUntil=0;++ControlTargets.Revision;PublishControls();}
}

void APowderPlantTest::RevokePreparedOpening()
{
 if(!ControlTargets.WeighingOpenRevision)return;
 if(GetSimulationSnapshot().FeedPreparation.State!=EPlantFeedPreparationState::Opened)
 {WeighingHopperOpen=ControlTargets.WeighingOpen=false;}
 ControlTargets.WeighingOpenRevision=0;ControlTargets.WeighingOpenLeaseUntil=0;++ControlTargets.Revision;
}
bool APowderPlantTest::SetReceiverValve(bool Open,int64 Epoch,int64& Revision,FString& Reason,int64 RequiredBatchRevision,double Deadline)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(Open&&(ControlTargets.SupplyAPurgeEnabled||AnyPlantPurge(GetSimulationSnapshot(),[](const auto& P){return P.AppliedEnabled;}))){Reason=TEXT("연결부 공기 정지 확인 후 수취 출구를 여세요.");return false;}
 if(Open&&RequiredBatchRevision>0){const auto S=GetSimulationSnapshot();
  if(!FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()||S.AppliedHopperOpen||S.RequestedHopperOpen||
     !S.SupplyABatch.Valid||S.SupplyABatch.CaptureRevision!=RequiredBatchRevision||
     (S.SupplyABatch.ReceiverCount!=S.SupplyABatch.TotalCount&&!(ControlTargets.RecoveryRevision>0&&S.Recovery.Ticket==ControlTargets.RecoveryRevision&&S.Recovery.ForeignIds.IsEmpty()&&
      S.SupplyABatch.ReceiverCount+S.SupplyABatch.MixerConnectorCount+S.SupplyABatch.MixerCount==S.SupplyABatch.TotalCount))||
     S.SupplyABatch.ForeignCount||S.SupplyABatch.UnknownCount||!S.SuppliersVented()||S.AppliedGaugePressurePa!=FVector::ZeroVector)
   {Reason=TEXT("수취 개방 전 이번 배치 전량·칭량 닫힘·압력0 확인 필요");return false;}}
 ReceiverOpen=ControlTargets.ReceiverOpen=Open;Revision=++ControlTargets.Revision;
 ControlTargets.AutomaticAction=Open&&RequiredBatchRevision>0?1:0;
 ControlTargets.AutomaticActionRevision=ControlTargets.AutomaticAction?Revision:0;
 ControlTargets.AutomaticBatchRevision=ControlTargets.AutomaticAction?RequiredBatchRevision:0;
 ControlTargets.AutomaticDeadline=Deadline;PublishControls();return true;
}
bool APowderPlantTest::SetMixerEnabled(bool Enabled,int64 Epoch,int64& Revision,FString& Reason,int64 RequiredBatchRevision,double Deadline)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(Enabled&&RequiredBatchRevision>0){const auto S=GetSimulationSnapshot();
  const auto& F=S.SequentialBatch.AdditiveActive&&S.SequentialBatch.AdditiveScheduled?S.InitialBatchMixerFeed:S.MixerFeed;
  if(!FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()||S.AppliedReceiverOpen||S.RequestedReceiverOpen||
     F.State!=EPlantTransferState::Complete||F.CloseRevision<=F.OpeningRevision||
     S.SupplyABatch.CaptureRevision!=RequiredBatchRevision||ControlTargets.RPM<=0)
   {Reason=TEXT("수취 출구의 실제 닫힘·전량 투입·양수 목표 RPM 확인 전에는 혼합 시작 금지");return false;}}
 ControlTargets.AutomaticMixClock=Enabled&&RequiredBatchRevision>0;
 MixerEnabled=ControlTargets.MixerEnabled=Enabled;Revision=++ControlTargets.Revision;
 ControlTargets.SpeedRevision=0;ControlTargets.MixRun=ControlTargets.AutomaticMixClock?Revision:0;
 ControlTargets.AutomaticAction=Enabled&&RequiredBatchRevision>0?2:0;
 ControlTargets.AutomaticActionRevision=ControlTargets.AutomaticAction?Revision:0;
 ControlTargets.AutomaticBatchRevision=ControlTargets.AutomaticAction?RequiredBatchRevision:0;
 ControlTargets.AutomaticDeadline=Deadline;PublishControls();return true;
}
bool APowderPlantTest::SetLiveMixerRPM(double RPM,int64 Epoch,int64 MixRun,double EndSeconds,double Deadline,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(MixRun<=0||MixRun!=ControlTargets.MixRun||!ControlTargets.AutomaticMixClock||!ControlTargets.MixerEnabled||
    !ControlTargets.Running||!FMath::IsFinite(RPM)||RPM<=0||RPM>PlantControl::MaxRPM||float(RPM)==0.f||
    !FMath::IsFinite(EndSeconds)||!FMath::IsFinite(Deadline)){
  Reason=TEXT("현재 혼합 운전과 RPM 범위를 확인하세요.");return false;
 }
 ControlTargets.SpeedRPM=RPM;ControlTargets.SpeedDeadline=Deadline;ControlTargets.MixEndSeconds=EndSeconds;
 Revision=ControlTargets.SpeedRevision=++ControlTargets.SpeedSerial;PublishControls();return true;
}
bool APowderPlantTest::SetMixerRPM(double RPM,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(!FMath::IsFinite(RPM)||RPM<PlantControl::MinRPM||RPM>PlantControl::MaxRPM){Reason=TEXT("목표 회전속도는 유한한 0~30 rpm이어야 합니다.");return false;}
 MixerRPM=ControlTargets.RPM=RPM;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::SetGaugePressure(int32 Target,double Pa,int64 Epoch,int64& Revision,FString& Reason,int64 TransferId,double Deadline,int32 Supplier,int64 Capture,int32 ActiveSource)
{
 if(!ValidateControl(Epoch,Reason))return false;
 const auto S=GetSimulationSnapshot();
 if(!S.PressureControlEnabled||Target<0||Target>2){Reason=TEXT("이 해석 대상은 압력 설정을 지원하지 않습니다.");return false;}
 if(S.Execution!=EPlantExecutionState::Running||!Running){Reason=TEXT("계산 재개 후 새 압력 요청을 적용하세요. 일시정지 중에는 예약하지 않습니다.");return false;}
 if(Target==2&&Pa!=0&&(S.RequestedReceiverOpen||S.AppliedReceiverOpen)){Reason=TEXT("수취 출구 닫힘 확인 후 흡입 압력을 요청하세요.");return false;}
 if(Target==1&&ControlTargets.SupplyAPurgeEnabled&&Pa>=ControlTargets.SupplyAPurgeGaugePa){Reason=TEXT("연결부 공기 주입 압력은 칭량 압력보다 높아야 합니다. 공기 정지 후 변경하세요.");return false;}
 if(TransferId>0&&(Target==0||S.Transfer.Id!=TransferId||S.Transfer.State==EPlantTransferState::Complete||
    S.Transfer.State==EPlantTransferState::Indeterminate||!S.AppliedHopperOpen||!S.RequestedHopperOpen||S.AppliedReceiverOpen||S.RequestedReceiverOpen||
    !FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds())){Reason=TEXT("이송 종료·변경 또는 출구 상태 미확인 · 압력 조절 거부");return false;}
 if(Supplier!=INDEX_NONE&&(Target!=0||Supplier<0||Supplier>=6)){Reason=TEXT("공급원 압력 대상 오류");return false;}
 if(Capture>0&&(S.SupplyABatch.CaptureRevision!=Capture||S.SequentialBatch.ActiveSupplier!=ActiveSource)){
  Reason=TEXT("작업자 배치 또는 공급원이 변경되었습니다.");return false;
 }
 if(Capture>0){
  const bool SourceValid=ActiveSource>=0&&ActiveSource<6;
  const bool FeedTarget=SourceValid&&((Target==0&&Supplier==ActiveSource)||Target==1);
  if(!SourceValid||(TransferId==0&&(!FeedTarget||ControlTargets.Suppliers!=(1u<<ActiveSource)||S.AppliedSupplierMask!=(1u<<ActiveSource)||
     S.RequestedHopperOpen||S.AppliedHopperOpen||S.RequestedReceiverOpen||S.AppliedReceiverOpen||
     (ControlTargets.DoseCutoffRevision>0&&(S.DoseCutoff.Cancelled||S.DoseCutoff.Triggered))))){
   Reason=TEXT("현재 공급원이 바뀌었거나 보호 닫힘이 시작되었습니다.");return false;
  }
 }
 auto Next=ControlTargets.GaugePressurePa;Next[Target]=Pa;
 try{powder::validate_plant_pressure_targets({Next.X,Next.Y,Next.Z});}
 catch(const std::exception&){Reason=TEXT("압력 거부: 공급 공통·칭량 -1~+1, 수취 -1~0 kPa(g)의 유한한 값만 지원합니다.");return false;}
 if(Target==0){
  if(Supplier==INDEX_NONE)ControlTargets.SupplierPa.Reset();
  else{
   if(ControlTargets.SupplierPa.IsEmpty())ControlTargets.SupplierPa.Init(ControlTargets.GaugePressurePa.X,6);
   ControlTargets.SupplierPa[Supplier]=Pa;
  }
 }
 ControlTargets.PressureSource=ActiveSource;ControlTargets.PressureCapture=Capture;ControlTargets.PressureAxis=Target;
 ControlTargets.GaugePressurePa=Next;Revision=++ControlTargets.Revision;ControlTargets.GaugePressureRevision=Revision;
 ControlTargets.TransferPressureId=TransferId;ControlTargets.TransferPressureRevision=(TransferId>0||Capture>0)?Revision:0;ControlTargets.TransferPressureDeadline=Deadline;
 PublishControls();return true;
}
void APowderPlantTest::RevokeTransferPressure()
{
 if(ControlTargets.TransferPressureRevision<=0||!Runtime)return;
 // Linearize revocation with the worker field upload. Preserve a confirmed setting, retract an unprocessed target.
 std::lock_guard<std::mutex> Lock(Runtime->Mutex);
 const int32 Axis=ControlTargets.PressureAxis,Source=ControlTargets.PressureSource;
 const auto& Last=Runtime->Published;
 const bool Uploaded=Runtime->ScopedPressureUploadedRevision==ControlTargets.TransferPressureRevision;
 if(!Uploaded){
  RevokedPressureEpoch=SnapshotEpoch;
  RevokedPressureRevision=ControlTargets.TransferPressureRevision;
 }
 const double Applied=Uploaded?Runtime->ScopedPressureUploadedPa:Axis==0?Last.SupplierPressure(Source,true):Last.AppliedGaugePressurePa[Axis];
 ControlTargets.GaugePressurePa[Axis]=Applied;
 if(Axis==0&&ControlTargets.SupplierPa.IsValidIndex(Source))ControlTargets.SupplierPa[Source]=Applied;
 ControlTargets.PressureCapture=0;ControlTargets.GaugePressureRevision=0;
 ControlTargets.TransferPressureId=ControlTargets.TransferPressureRevision=0;ControlTargets.TransferPressureDeadline=0;
 ++ControlTargets.Revision;Runtime->Mailbox=ControlTargets;
}
bool APowderPlantTest::SetSimulationRunning(bool Run,int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(!Run){RevokeTransferPressure();if(ControlTargets.MixRun)MixerEnabled=ControlTargets.MixerEnabled=false;
 ControlTargets.SpeedRevision=ControlTargets.MixRun=0;
 ControlTargets.RecoveryRevision=0;ControlTargets.AutomaticMixClock=false;RevokeDoseCutoff();RevokePreparedOpening();RevokeSupplyAPurge();
  if(ControlTargets.AutomaticAction==1)ReceiverOpen=ControlTargets.ReceiverOpen=false;
  if(ControlTargets.AutomaticAction==2)MixerEnabled=ControlTargets.MixerEnabled=false;
  ControlTargets.DoseFinalizeRevision=ControlTargets.DoseCaptureRevision=0;ControlTargets.DoseDeadline=0;
 ControlTargets.BatchTransitionRevision=ControlTargets.BatchTransitionCapture=0;ControlTargets.AutomaticAction=0;ControlTargets.AutomaticActionRevision=ControlTargets.AutomaticBatchRevision=0;}
 if(Run)AutomaticCancelStopPending=false; // only a fresh explicit resume can supersede the cancelled run
 Running=ControlTargets.Running=Run;Revision=++ControlTargets.Revision;PublishControls();return true;
}
bool APowderPlantTest::RequestAllClosed(int64 Epoch,int64& Revision,FString& Reason)
{
 if(!ValidateControl(Epoch,Reason))return false;
 if(ControlTargets.MixRun)MixerEnabled=ControlTargets.MixerEnabled=false;
 ControlTargets.SpeedRevision=ControlTargets.MixRun=0;
 ControlTargets.RecoveryRevision=0;ControlTargets.AutomaticMixClock=false;
 RevokeDoseCutoff();
 ControlTargets.WeighingOpenRevision=0;ControlTargets.WeighingOpenLeaseUntil=0;
 ControlTargets.BatchCaptureRevision=ControlTargets.RequiredABatchRevision=0;
 ControlTargets.DoseFinalizeRevision=ControlTargets.DoseCaptureRevision=0;ControlTargets.DoseDeadline=0;
 ControlTargets.BatchTransitionRevision=ControlTargets.BatchTransitionCapture=0;ControlTargets.AutomaticAction=0;ControlTargets.AutomaticActionRevision=ControlTargets.AutomaticBatchRevision=0;ControlTargets.AutomaticDeadline=0;
 for(int32 I=0;I<SupplierOpen.Num();++I)SupplierOpen[I]=false;
 ControlTargets.Suppliers=0;WeighingHopperOpen=ControlTargets.WeighingOpen=false;
 ControlTargets.TransferPressureId=ControlTargets.TransferPressureRevision=0;ControlTargets.TransferPressureDeadline=0;
 // Cancel every previous pressure target in the same mailbox as closing/stop/resume.
 // The source pressure model already cuts requested-closed flow before the occupied disk can close.
 ControlTargets.SupplierPa.Reset();ControlTargets.GaugePressureRevision=0;ControlTargets.GaugePressurePa=FVector::ZeroVector;
 ControlTargets.SupplyAPurgeEnabled=false;ControlTargets.SupplyAPurgeGaugePa=0;ControlTargets.SupplyAPurgeRevision=0;ControlTargets.SupplyAPurgeLeaseUntil=0;
 ReceiverOpen=ControlTargets.ReceiverOpen=false;MixerEnabled=ControlTargets.MixerEnabled=false;
 // One atomic mailbox update: no running batch can see the old opening/rotation targets with this resume.
 Running=ControlTargets.Running=true;Revision=++ControlTargets.Revision;PublishControls();return true;
}


bool APowderPlantTest::SetSupplyAPurge(bool PressureCommand,double Value,int64 Epoch,int64& Revision,FString& Reason,int32 Supplier)
{
 if(!ValidateControl(Epoch,Reason))return false;
 const auto S=GetSimulationSnapshot();
 if(Supplier<0||Supplier>=6||!PlantPurgeFor(S,Supplier).Supported){Reason=TEXT("이 해석 구성은 선택한 공급 연결부 공기를 지원하지 않습니다.");return false;}
 if(S.Execution!=EPlantExecutionState::Running||!Running){Reason=TEXT("계산 재개 후 새 연결부 공기 요청을 적용하세요.");return false;}
 if(!FMath::IsFinite(Value)||(PressureCommand?(Value<0||Value>1000):(Value!=0&&Value!=1))){Reason=TEXT("연결부 공기 압력은 0~1 kPa(g), 시작/정지는 0 또는 1만 지원합니다.");return false;}
 if(Supplier!=ControlTargets.SupplyPurgeSupplier&&(ControlTargets.SupplyAPurgeEnabled||ControlTargets.SupplyAPurgeGaugePa!=0||AnyPlantPurge(S,[](const auto& P){return P.AppliedEnabled||P.AppliedGaugePa!=0;}))){Reason=TEXT("기존 연결부 공기 차단 확인 후 다른 공급기를 선택하세요.");return false;}
 const bool Enable=PressureCommand?ControlTargets.SupplyAPurgeEnabled:Value!=0;
 const double Pa=PressureCommand?Value:(Enable?ControlTargets.SupplyAPurgeGaugePa:0);
 if(Enable&&(ControlTargets.Suppliers||S.AppliedSupplierMask||S.AppliedHopperOpen||Pa<=ControlTargets.GaugePressurePa.Y))
 {Reason=TEXT("모든 공급 밸브와 칭량 출구 닫힘, 칭량보다 높은 공기 압력 설정을 확인하세요.");return false;}
 ControlTargets.SupplyPurgeSupplier=Supplier;ControlTargets.SupplyAPurgeEnabled=Enable;ControlTargets.SupplyAPurgeGaugePa=Pa;Revision=++ControlTargets.Revision;
 ControlTargets.SupplyAPurgeRevision=(!PressureCommand&&!Enable)?0:Revision;
 ControlTargets.SupplyAPurgeLeaseUntil=ControlTargets.SupplyAPurgeRevision?FPlatformTime::Seconds()+2.:0;
 PublishControls();return true;
}
void APowderPlantTest::MaintainSupplyAPurge(int64 Revision,bool KeepAlive)
{
 if(Revision!=ControlTargets.SupplyAPurgeRevision||!Runtime)return;
 if(KeepAlive&&PlantPurgeFor(GetSimulationSnapshot(),ControlTargets.SupplyPurgeSupplier).CancelledRevision<Revision){ControlTargets.SupplyAPurgeLeaseUntil=FPlatformTime::Seconds()+2.;PublishControls();}
 else RevokeSupplyAPurge();
}
void APowderPlantTest::RevokeSupplyAPurge()
{
 if(!ControlTargets.SupplyAPurgeRevision&&!ControlTargets.SupplyAPurgeEnabled&&ControlTargets.SupplyAPurgeGaugePa==0)return;
 ControlTargets.SupplyAPurgeEnabled=false;ControlTargets.SupplyAPurgeGaugePa=0;ControlTargets.SupplyAPurgeRevision=0;ControlTargets.SupplyAPurgeLeaseUntil=0;
 ++ControlTargets.Revision;PublishControls();
}

bool APowderPlantTest::CaptureInitialABatch(int64 Epoch,int64& Revision,FString& Reason,int32 Supplier)
{
 if(!ValidateControl(Epoch,Reason))return false;
 const auto S=GetSimulationSnapshot();const auto& B=S.SupplyABatch;
 if(!B.Supported||!B.Valid||B.CaptureRevision||!S.SequentialBatch.Materials.IsValidIndex(Supplier)||S.SequentialBatch.Materials[Supplier].InitialCount<=0||S.SequentialBatch.Materials[Supplier].SourceCount!=S.SequentialBatch.Materials[Supplier].InitialCount||B.ForeignCount||
    S.Execution!=EPlantExecutionState::Paused||S.AppliedSupplierMask||S.AppliedHopperOpen||S.AppliedReceiverOpen)
 {Reason=TEXT("정지 확인된 초기 A 배치를 작업자에서 확정해야 합니다. 초기화 상태를 확인하세요.");return false;}
 Revision=++ControlTargets.Revision;ControlTargets.BatchSupplier=Supplier;ControlTargets.BatchCaptureRevision=Revision;PublishControls();return true;
}
void APowderPlantTest::CancelAutomaticControls()
{
 // Same stop mailbox cancels supplier/WH/purge intents before releasing the owner.
 int64 Revision;FString Reason;if(RequestAllClosed(SnapshotEpoch,Revision,Reason))AutomaticCancelStopPending=true;
}

bool APowderPlantTest::FinalizeADose(double TargetKg,double ToleranceKg,int64 CaptureRevision,int64 Epoch,int64& Revision,FString& Reason,double Deadline)
{
 if(!ValidateControl(Epoch,Reason))return false;const auto S=GetSimulationSnapshot();
 Reason=PlantDose::Validate(TargetKg,ToleranceKg,S.ModelParticleMassKg,S.SupplyABatch.InitialKg);if(!Reason.IsEmpty())return false;
 if(CaptureRevision<=0||S.SupplyABatch.CaptureRevision!=CaptureRevision||S.SupplyABatch.DoseFinalized||
    !FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()||!Running||S.Execution!=EPlantExecutionState::Running)
 {Reason=TEXT("현재 배치의 실행·초기화·계량 확정 조건을 확인할 수 없습니다.");return false;}
 Revision=++ControlTargets.Revision;ControlTargets.DoseFinalizeRevision=Revision;ControlTargets.DoseCaptureRevision=CaptureRevision;
 ControlTargets.DoseTargetKg=TargetKg;ControlTargets.DoseToleranceKg=ToleranceKg;ControlTargets.DoseDeadline=Deadline;PublishControls();return true;
}

bool APowderPlantTest::TransitionBatch(bool Combine,int64 CaptureRevision,int64 Epoch,int64& Revision,FString& Reason,double Deadline,int32 Supplier)
{
 if(!ValidateControl(Epoch,Reason))return false;const auto S=GetSimulationSnapshot();
 if(CaptureRevision<=0||S.SupplyABatch.CaptureRevision!=CaptureRevision||!S.SupplyABatch.DoseFinalized||
  !FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()||!Running||S.Execution!=EPlantExecutionState::Running){Reason=TEXT("현재 배치의 원료 전환 조건 미확인");return false;}
 Revision=++ControlTargets.Revision;ControlTargets.BatchSupplier=Supplier;ControlTargets.BatchTransitionRevision=Revision;ControlTargets.BatchTransitionCapture=CaptureRevision;
 ControlTargets.BatchAdditive=false;ControlTargets.BatchCombine=Combine;ControlTargets.BatchTransitionDeadline=Deadline;PublishControls();return true;
}

bool APowderPlantTest::BeginAdditive(bool Scheduled,double Delay,int64 CaptureRevision,int64 Epoch,int64& Revision,FString& Reason,double Deadline,int32 Supplier)
{
 if(!ValidateControl(Epoch,Reason))return false;const auto S=GetSimulationSnapshot();
 if(CaptureRevision<=0||S.SupplyABatch.CaptureRevision!=CaptureRevision||!S.SequentialBatch.Combined||S.SequentialBatch.AdditiveActive||
    !FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()||!FMath::IsFinite(Delay)||Delay<0||!Running||S.Execution!=EPlantExecutionState::Running){
  Reason=TEXT("첨가제 전환의 실행·초기화·A·B 배치 조건 미확인");return false;
 }
 Revision=++ControlTargets.Revision;ControlTargets.BatchSupplier=Supplier;ControlTargets.BatchTransitionRevision=Revision;ControlTargets.BatchTransitionCapture=CaptureRevision;
 ControlTargets.BatchAdditive=true;ControlTargets.BatchCombine=false;ControlTargets.AdditiveScheduled=Scheduled;ControlTargets.AdditiveDelaySeconds=Delay;
 ControlTargets.BatchTransitionDeadline=Deadline;PublishControls();return true;
}

bool APowderPlantTest::RecoverBatch(int64 Capture,int64 Epoch,FGuid Request,int64& Revision,FString& Reason,double Deadline)
{
 if(!ValidateControl(Epoch,Reason))return false;
 const auto S=GetSimulationSnapshot();
 if(!Request.IsValid()||S.Execution!=EPlantExecutionState::Paused||ControlTargets.Running||Capture<=0||S.SupplyABatch.CaptureRevision!=Capture||
    !S.Recovery.Valid||!S.Recovery.ForeignIds.IsEmpty()||!FMath::IsFinite(Deadline)||Deadline<=FPlatformTime::Seconds()){
  Reason=TEXT("재개 금지: 정지 완료·현재 배치·공급원 이력 확인 필요");return false;
 }
 Revision=++ControlTargets.Revision;ControlTargets.RecoveryRevision=Revision;ControlTargets.RecoveryCapture=Capture;
 ControlTargets.RecoveryRequestId=Request;ControlTargets.RecoveryDeadline=Deadline;PublishControls();return true;
}

// Retained opt-in demonstration path. UI v2 controls bind only to the physical solver.
void APowderPlantTest::ResetDemoSimulation(){Stop();Error.Empty();PressureApplied=false;RunLogPath.Empty();PhysicalSeconds=ActualRate=0;Generation=0;DisplayedParticleCount=SolverParticleCount=0;CurrentPhase=0;DemoAutoStage=-1;AppliedSupplierMask=0;WeighingHopperOpen=AppliedHopperOpen=false;ReceiverOpen=AppliedReceiverOpen=MixerEnabled=false;InWeighingHopper=InReceiver=InMixer=0;try{
  if(RegulatedPressureEnabled){powder::validate_plant_pressure_targets({SupplyGaugePa,WeighingGaugePa,ReceiverGaugePa});powder::plant_pressure_preset(PressureStage);if(SectionTest!=0)throw std::runtime_error("regulated pressure needs full plant");}
  InitializeSuppliers();for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=false;
  if(!DemoRealtime&&(!std::isfinite(FixedDt)||FixedDt<1e-6||FixedDt>2.5e-5||ContactIterations<4||ContactIterations>32||!std::isfinite(StageTimeoutSeconds)||StageTimeoutSeconds<=0))throw std::runtime_error("invalid plant test controls");
  powder::plant_discharge_ready(0,1,0,MinimumReceiverFraction,ReceiverTransferSeconds);
  FString Root=FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/DataDirectory);auto P=ReadDemoCharge(Root/TEXT("test_charge.csv"),ParticleRadiusM);ParticleCount=int32(P.size());int Types=0;for(auto A:P)Types=FMath::Max(Types,int(A.w)+1);
  while(Groups.Num()<Types){auto*G=NewObject<UInstancedStaticMeshComponent>(this);G->SetupAttachment(RootComponent);G->SetCollisionEnabled(ECollisionEnabled::NoCollision);G->SetCastShadow(false);G->RegisterComponent();AddInstanceComponent(G);Groups.Add(G);}
  auto*Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Game/ProcessPlant_20261111_LowPoly/Particles/SM_GrainSphere.SM_GrainSphere"));if(!Mesh)Mesh=LoadObject<UStaticMesh>(nullptr,TEXT("/Engine/BasicShapes/Sphere.Sphere"));auto*Mat=LoadObject<UMaterialInterface>(nullptr,TEXT("/Game/ProcessPlant_20261111_LowPoly/Particles/M_BulkFlow.M_BulkFlow"));FLinearColor Colors[3]{FLinearColor(.9,.025,.01),FLinearColor(.01,.2,.9),FLinearColor(.9,.7,.01)};
  for(int I=0;I<Groups.Num();++I){auto*G=Groups[I].Get();G->ClearInstances();G->SetStaticMesh(Mesh);if(Mat){G->SetMaterial(0,Mat);if(auto*M=G->CreateDynamicMaterialInstance(0))M->SetVectorParameterValue(TEXT("Tint"),Colors[I%3]);}}

  const int Copies=DemoRealtime?FMath::Clamp(DemoVisualCopies,1,10):1;
  const double RadiusScale=1./std::cbrt(double(Copies)),Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;
  VisualScale=FVector(200.*ParticleRadiusM*RadiusScale/Diameter);VisualOffsets.Reset(Copies);
  for(int K=0;K<Copies;++K){double Z=1.-2.*(K+.5)/Copies,R=std::sqrt(1-Z*Z),Phi=K*2.399963229728653;VisualOffsets.Add(Copies==1?FVector::ZeroVector:FVector(R*std::cos(Phi),R*std::sin(Phi),Z)*(100*ParticleRadiusM*(1-RadiusScale)));}
  InstanceTransforms.SetNum(Groups.Num());TArray<int32> Counts;Counts.Init(0,Groups.Num());for(const auto& A:P)++Counts[int(A.w)];
  for(int I=0;I<Groups.Num();++I)InstanceTransforms[I].Reserve(Counts[I]*Copies);
  FillParticleTransforms(P,VisualOffsets,VisualScale,InstanceTransforms);
  for(int I=0;I<Groups.Num();++I){Groups[I]->AddInstances(InstanceTransforms[I],false,false,false);DisplayedParticleCount+=Groups[I]->GetInstanceCount();}
  SolverParticleCount=DemoRealtime?ParticleCount/8:ParticleCount;
  Stage=FString::Printf(TEXT("Initial charge: %d particles"),ParticleCount);if(HasActorBegunPlay()){powder::GranularConfig C;C.backend=static_cast<powder::GranularBackend>(ComputeBackend);C.cuda_ptx_file=TCHAR_TO_UTF8(*FPaths::ConvertRelativePathToFull(FPaths::ProjectDir()/TEXT("SourceAssets/PowderCUDA/plant_sm89.ptx")));C.plant=true;C.plant_threaded=true;C.kernel_optimize=true;C.plant_data=powder::read_plant_data(TCHAR_TO_UTF8(*(Root/TEXT("runtime.plant"))));if(FVector(C.plant_data[1].x,C.plant_data[1].y,C.plant_data[1].z).Equals(MixerPivotM,.0001)==false)throw std::runtime_error("mixer pivot differs from collision data");C.plant_wall_mode=(C.plant_data[10].x==2&&C.plant_data[13].z>0&&C.plant_data[13].w>0)?(P.size()>=3000?5u:3u):0u;if(int(C.plant_data[11].x)!=SectionTest)throw std::runtime_error("section mode/data mismatch");C.radius=ParticleRadiusM;C.count=unsigned(P.size());C.dt=DemoRealtime?1.f/120:float(FixedDt);C.iterations=DemoRealtime?1u:unsigned(ContactIterations);auto Routes=powder::read_plant_routes(TCHAR_TO_UTF8(*(Root/TEXT("runtime.routes"))));for(auto V:Rotor)if(V.Actor)V.Actor->SetActorTransform(V.Initial);DemoRuntime=new FPlantDemoRuntime(std::move(C),std::move(P),std::move(Routes),StageTimeoutSeconds,MinimumReceiverFraction,ReceiverTransferSeconds,Running,TestPressureDropPa,0,MixerBoundsMinM,MixerBoundsMaxM,RegulatedPressureEnabled,{SupplyGaugePa,WeighingGaugePa,ReceiverGaugePa},PressureStage,PressureManualOverride,DemoRealtime);RunLogPath=DemoRuntime->LogPath;}
 }catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());Running=false;}}

void APowderPlantTest::BeginDemoPlay(){Super::BeginPlay();if(DemoRealtime){FParse::Value(FCommandLine::Get(),TEXT("DemoBenchmarkSeconds="),DemoTestSeconds);FParse::Value(FCommandLine::Get(),TEXT("DemoBenchmarkOutput="),DemoOutput);if(DemoTestSeconds>0){Running=false;DemoFrames.Empty();DemoWarmFrames=0;}}if(GEngine)GEngine->RemoveOnScreenDebugMessage(3101);if(auto*FPS=IConsoleManager::Get().FindConsoleVariable(TEXT("t.MaxFPS"))){PreviousMaxFPS=FPS->GetFloat();AppliedMaxFPS=30.f;FPS->Set(AppliedMaxFPS,ECVF_SetByCode);}FParse::Value(FCommandLine::Get(),TEXT("PlantSmokeSeconds="),SmokeSeconds);FParse::Value(FCommandLine::Get(),TEXT("PlantWallLimit="),SmokeWallLimit);SmokeStart=FPlatformTime::Seconds();if(SmokeSeconds>0)Running=false;CacheRotor();ValveVisuals.Empty();for(TActorIterator<AActor>It(GetWorld());It;++It){if(auto*C=Cast<APlantVisibilityController>(*It))Display=C;const FString N=It->GetActorNameOrLabel();if((N.StartsWith(TEXT("V101"))&&N.EndsWith(TEXT("_Disc")))||N==TEXT("ASSUMED_VT_MovingGate")||N==TEXT("BV272_210_Disc")||N==TEXT("BV272_212_Disc_hub")||N==TEXT("BV122_210_Disc")||N==TEXT("BV122_212_Disc_hub"))ValveVisuals.Add(*It);}ResetSimulation();}

void APowderPlantTest::ShowDemo(){if(!DemoRuntime)return;std::unique_lock<std::mutex>L(DemoRuntime->Mutex);if(!DemoRuntime->Error.empty()){Error=UTF8_TO_TCHAR(DemoRuntime->Error.c_str());Running=false;}if(Generation==DemoRuntime->Generation)return;Generation=DemoRuntime->Generation;ComputeDevice=UTF8_TO_TCHAR(DemoRuntime->Device.c_str());PressureApplied=DemoRuntime->Ready;AppliedGaugePa=FVector(DemoRuntime->AppliedState.applied.supply,DemoRuntime->AppliedState.applied.weighing,DemoRuntime->AppliedState.applied.receiver);WeighingRegulatorM3s=DemoRuntime->AppliedState.weighing_regulator_m3s;TransferFlowM3s=DemoRuntime->AppliedState.transfer_m3s;PhysicalSeconds=DemoRuntime->Time;ActualRate=DemoRuntime->Wall>0?PhysicalSeconds/DemoRuntime->Wall:0;InWeighingHopper=DemoRuntime->WH;InReceiver=DemoRuntime->RH;InMixer=DemoRuntime->MX;CurrentPhase=DemoRuntime->Phase;AppliedSupplierMask=DemoRuntime->OpenSuppliers;AppliedHopperOpen=DemoRuntime->HopperOpen;AppliedReceiverOpen=DemoRuntime->ReceiverOpen;Stage=DemoRuntime->Phase==0?TEXT("공급기 → 칭량 호퍼"):DemoRuntime->Phase==1?TEXT("공급기 → 칭량 → 수취부 (연결 유동)"):TEXT("수신기 → 혼합기");
 const auto DisplayPoints=DemoRuntime->Points;const float DisplayAngle=DemoRuntime->Angle;L.unlock();
 FillParticleTransforms(DisplayPoints,VisualOffsets,VisualScale,InstanceTransforms);
 // UE 5.8 tracks instance-transform changes automatically. Demo mode avoids
 // recreating the entire scene proxy for each frame; other modes keep their path.
 for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,InstanceTransforms[I],false,!DemoRealtime,true);
 for(auto V:ValveVisuals)if(V){const FString N=V->GetActorNameOrLabel();const int SupplierIndex=N.Len()>4?int(N[4]-TCHAR('A')):-1;bool Open=N.StartsWith(TEXT("BV122"))||(N.StartsWith(TEXT("V101"))&&SupplierIndex>=0&&SupplierIndex<SupplierOpen.Num()&&CurrentPhase<2&&((AppliedSupplierMask&(uint32(1)<<SupplierIndex))!=0))||(N.StartsWith(TEXT("ASSUMED"))&&AppliedHopperOpen&&CurrentPhase<2)||(N.StartsWith(TEXT("BV272"))&&AppliedReceiverOpen);V->SetActorHiddenInGame(Open||(Display&&Display->ViewMode==EPlantViewMode::Hidden));}
 FQuat Q(FVector(1,0,0),-DisplayAngle);FVector Pivot(MixerPivotM.X*100,-MixerPivotM.Y*100,MixerPivotM.Z*100);for(auto V:Rotor)if(V.Actor){FTransform X=V.Initial;X.SetLocation(Pivot+Q.RotateVector(V.Initial.GetLocation()-Pivot));X.SetRotation(Q*V.Initial.GetRotation());V.Actor->SetActorTransform(X);}}

void APowderPlantTest::TickDemoRuntime(float D){Super::Tick(D);if(DemoRealtime)TickDemo(D);if(SmokeSeconds>0&&++SmokeWarmFrames==10){Running=true;SmokeStart=FPlatformTime::Seconds();if(Display)Display->HideEquipment();}if(auto*PC=GetWorld()->GetFirstPlayerController()){if(PC->WasInputKeyJustPressed(EKeys::SpaceBar))Running=!Running;if(PC->WasInputKeyJustPressed(EKeys::R))ResetSimulation();if(PC->WasInputKeyJustPressed(EKeys::O))ExportSnapshot();if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){int View=-1;if(PC->WasInputKeyJustPressed(EKeys::One))View=0;if(PC->WasInputKeyJustPressed(EKeys::Two))View=1;if(PC->WasInputKeyJustPressed(EKeys::Three))View=2;if(PC->WasInputKeyJustPressed(EKeys::Four))View=3;if(View>=0){FVector Targets[]{FVector(0,-140,260),FVector(0,0,115),FVector(300,0,275),FVector(MixerPivotM.X*100,-MixerPivotM.Y*100,MixerPivotM.Z*100)};FVector Target=Targets[View];Camera->SetActorLocation(Target+FVector(80,View==0?300:150,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}}}if(DemoRuntime){if(RegulatedPressureEnabled){try{DemoRuntime->RequestPressure({SupplyGaugePa,WeighingGaugePa,ReceiverGaugePa},PressureStage,PressureManualOverride);}catch(const std::exception&E){PressureMessage=UTF8_TO_TCHAR(E.what());}}DemoRuntime->Run=Running;DemoRuntime->Drop=TestPressureDropPa;DemoRuntime->RPM=MixerEnabled?MixerRPM:0;uint32 Mask=0;for(int i=0;i<SupplierOpen.Num();++i)if(SupplierOpen[i])Mask|=uint32(1)<<i;DemoRuntime->RequestedSuppliers=Mask;DemoRuntime->RequestedHopper=WeighingHopperOpen;DemoRuntime->RequestedReceiver=ReceiverOpen;}LastShow+=D;if(DemoRealtime||LastShow>.1){Show();LastShow=0;}if(SmokeSeconds>0){if(!SmokeShot&&PhysicalSeconds>=SmokeSeconds*.4){if(auto*PC=GetWorld()->GetFirstPlayerController())if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){FVector Target(-60,-140,267);Camera->SetActorLocation(Target+FVector(30,300,60));Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}FScreenshotRequest::RequestScreenshot(FPaths::ProjectSavedDir()/TEXT("PlantFlowPreview.png"),true,false);SmokeShot=true;}
 if(PhysicalSeconds>=SmokeSeconds||!Error.IsEmpty()||FPlatformTime::Seconds()-SmokeStart>SmokeWallLimit){ExportSnapshot();FString Report=FString::Printf(TEXT("{\"count\":%d,\"physical_s\":%.9g,\"actual_rate\":%.9g,\"wh\":%d,\"rh\":%d,\"mixer\":%d,\"failed\":%s}"),ParticleCount,PhysicalSeconds,ActualRate,InWeighingHopper,InReceiver,InMixer,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?TEXT("false"):TEXT("true"));FFileHelper::SaveStringToFile(Report,*(FPaths::ProjectSavedDir()/TEXT("PlantSmoke.json")));if(!Error.IsEmpty())FFileHelper::SaveStringToFile(Error,*(FPaths::ProjectSavedDir()/TEXT("PlantSmokeError.txt")));Stop();Running=false;FPlatformMisc::RequestExitWithStatus(false,Error.IsEmpty()&&PhysicalSeconds>=SmokeSeconds?0:1);SmokeSeconds=0;}}}

TSharedRef<SWidget> APowderPlantTest::BuildDemoPanel(){
 if(SupplierOpen.IsEmpty()){try{InitializeSuppliers();}catch(const std::exception&E){Error=UTF8_TO_TCHAR(E.what());}}
 const TWeakObjectPtr<APowderPlantTest> Weak(this);
 TSharedRef<SVerticalBox> Panel=SNew(SVerticalBox);
 Panel->AddSlot().AutoHeight().Padding(0,0,0,8)[BuildPressurePanel()];
 if(SectionTest>0){const TCHAR*Names[]{TEXT(""),TEXT("공급기 → 칭량호퍼 | 칭량부 시험 흡입압"),TEXT("칭량호퍼 → 수취부 | 칭량부 대기압·수취부 시험 흡입압"),TEXT("수취부 → 혼합기 | 양쪽 대기압·중력 배출"),TEXT("혼합기 내부 | 공기 이송 없음")};Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(Names[FMath::Clamp(SectionTest,0,4)]))];}
 if(DataDirectory.Contains(TEXT("ProcessPlantHybrid")))Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(TEXT("혼합 충돌 시험: 관·연결부 메시 / 호퍼·혼합기 OBB")))];
 if(DataDirectory.Contains(TEXT("ProcessPlantOBB")))Panel->AddSlot().AutoHeight()[SNew(STextBlock).Text(FText::FromString(TEXT("OBB 통로 근사 시험 · 원본 형상과 다름")))];
 Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text_Lambda([Weak](){if(!Weak.IsValid())return FText::GetEmpty();return FText::FromString(Weak->DemoRealtime?FString::Printf(TEXT("시연용 · 목표 1배속 · 표시 %d / 계산 %d"),Weak->DisplayedParticleCount,Weak->SolverParticleCount):TEXT("시뮬레이션 정보 · 배속 제한 없음"));})];
 Panel->AddSlot().AutoHeight()[SNew(STextBlock).WrapTextAt(350).Text_Lambda([Weak](){
  if(!Weak.IsValid())return FText::GetEmpty();const auto*P=Weak.Get();const int VisualFactor=FMath::Max(1,P->VisualOffsets.Num());
  return FText::FromString(FString::Printf(TEXT("시간 %.3f초 | 실제 속도 %.4f배\n입자 %d개 | %s\n칭량 %d · 수신기 %d · 혼합기 %d\n통로/기타 %d | %s"),P->PhysicalSeconds,P->ActualRate,P->DisplayedParticleCount,*(P->SectionTest>0?FString::Printf(TEXT("구간 %d 독립 시험"),P->SectionTest):P->Stage),P->InWeighingHopper*VisualFactor,P->InReceiver*VisualFactor,P->InMixer*VisualFactor,(P->ParticleCount-P->InWeighingHopper-P->InReceiver-P->InMixer)*VisualFactor,P->Running?TEXT("계산 중"):TEXT("일시정지")));
 })];
 if(SectionTest==0||SectionTest==1)Panel->AddSlot().AutoHeight().Padding(0,8,0,4)[SNew(STextBlock).Text(FText::FromString(TEXT("공급기 밸브 · 시작/초기화 시 모두 닫힘")))];
 for(int32 I=0;I<SupplierOpen.Num()&&(SectionTest==0||SectionTest==1);++I){
  Panel->AddSlot().AutoHeight().Padding(0,2)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,I](){
    if(!Weak.IsValid()||!Weak->SupplierOpen.IsValidIndex(I))return FText::GetEmpty();
    bool Want=Weak->SupplierOpen[I],Open=(Weak->AppliedSupplierMask&(uint32(1)<<I))!=0;
    const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
    return FText::FromString(FString::Printf(TEXT("공급기 %c · %s"),TCHAR('A'+I),State));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,I](){if(Weak.IsValid())Weak->ToggleSupplier(I);return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,I](){return FText::FromString(Weak.IsValid()&&Weak->SupplierOpen.IsValidIndex(I)&&Weak->SupplierOpen[I]?TEXT("닫기"):TEXT("열기"));})]]];
 }
 if(SectionTest==0||SectionTest==2)Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(SHorizontalBox)
  + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak](){
   if(!Weak.IsValid())return FText::GetEmpty();bool Want=Weak->WeighingHopperOpen,Open=Weak->AppliedHopperOpen;
   const TCHAR*State=Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기 (입자 통과 중)"):TEXT("닫힘"));
   return FText::FromString(FString::Printf(TEXT("공용 칭량 호퍼 · %s"),State));
  })]
  + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
   .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->HasActorBegunPlay()&&Weak->CurrentPhase<2&&Weak->Error.IsEmpty();})
   .OnClicked_Lambda([Weak](){if(Weak.IsValid())Weak->ToggleWeighingHopper();return FReply::Handled();})
   [SNew(STextBlock).Text_Lambda([Weak](){return FText::FromString(Weak.IsValid()&&Weak->WeighingHopperOpen?TEXT("닫기"):TEXT("열기"));})]]];
 for(int Control=0;Control<2;++Control){
  if(SectionTest!=0&&!(Control==0?SectionTest==3:(SectionTest==3||SectionTest==4)))continue;
  Panel->AddSlot().AutoHeight().Padding(0,6,0,0)[SNew(SHorizontalBox)
   + SHorizontalBox::Slot().FillWidth(1).VAlign(VAlign_Center)[SNew(STextBlock).Text_Lambda([Weak,Control](){
    if(!Weak.IsValid())return FText::GetEmpty();
    if(Control==1)return FText::FromString(FString::Printf(TEXT("혼합기 · %s (%.1f RPM)"),Weak->MixerEnabled?TEXT("가동 요청"):TEXT("정지"),Weak->MixerRPM));
    bool Want=Weak->ReceiverOpen,Open=Weak->AppliedReceiverOpen;
    return FText::FromString(FString::Printf(TEXT("수취부 → 혼합기 · %s"),Want?(Open?TEXT("열림"):TEXT("열림 대기")):(Open?TEXT("닫힘 대기"):TEXT("닫힘"))));
   })]
   + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).IsFocusable(false).ContentPadding(FMargin(14,5))
    .IsEnabled_Lambda([Weak](){return Weak.IsValid()&&Weak->HasActorBegunPlay()&&Weak->Error.IsEmpty();})
    .OnClicked_Lambda([Weak,Control](){if(Weak.IsValid()){if(Control)Weak->ToggleMixer();else Weak->ToggleReceiver();}return FReply::Handled();})
    [SNew(STextBlock).Text_Lambda([Weak,Control](){return FText::FromString(Control?(Weak.IsValid()&&Weak->MixerEnabled?TEXT("정지"):TEXT("시작")):(Weak.IsValid()&&Weak->ReceiverOpen?TEXT("닫기"):TEXT("열기")));})]]];
 }
 Panel->AddSlot().AutoHeight().Padding(0,8,0,0)[SNew(STextBlock).WrapTextAt(350).Text(FText::FromString(DemoRealtime?TEXT("1~4: 장치 확대 | Space: 일시정지\nR: 초기화 | O: 입자 저장\n시연용 근사 모델 · 정밀 분체 해석 아님"):TEXT("1~4: 장치 확대 | Space: 일시정지\nR: 초기화 | O: 입자 저장\n관로 압력 근사 + GPU 입자 시험 · 미보정")))];
 Panel->AddSlot().AutoHeight().Padding(0,4)[SNew(STextBlock).WrapTextAt(350).ColorAndOpacity(FLinearColor(1,.25,.2)).Text_Lambda([Weak](){return Weak.IsValid()?FText::FromString(Weak->Error):FText::GetEmpty();})];
 if(DemoRealtime)Panel->AddSlot().AutoHeight().Padding(0,8)[SNew(SButton).OnClicked_Lambda([Weak](){if(Weak.IsValid()){Weak->DemoAutoCycle=!Weak->DemoAutoCycle;Weak->DemoAutoStage=-1;}return FReply::Handled();})[SNew(STextBlock).Text_Lambda([Weak](){return FText::FromString(Weak.IsValid()&&Weak->DemoAutoCycle?TEXT("자동 시연 중 · 수동으로 전환"):TEXT("자동 시연 시작"));})]];
 return SNew(SBox).WidthOverride(360)[Panel];
}

bool APowderPlantTest::ExportSnapshotToFile(const FString& FilePath){if(DemoRealtime){if(!DemoRuntime)return false;std::ostringstream S;S<<std::setprecision(12)<<"sim_s,id,species,x_m,y_m,z_m\n";{std::lock_guard<std::mutex> L(DemoRuntime->Mutex);unsigned Id=0;for(auto A:DemoRuntime->Points)S<<DemoRuntime->Time<<','<<Id++<<','<<A.w<<','<<A.x<<','<<A.y<<','<<A.z<<'\n';}return FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*FilePath);}if(!Runtime)return false;std::ostringstream S;S<<std::setprecision(12)<<"sim_s,id,species,x_m,y_m,z_m\n";{std::lock_guard<std::mutex>L(Runtime->Mutex);unsigned Id=0;for(auto A:Runtime->Points)S<<Runtime->Time<<','<<Id++<<','<<A.w<<','<<A.x<<','<<A.y<<','<<A.z<<'\n';}return FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(S.str().c_str()),*FilePath);}
