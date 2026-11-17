#pragma once
#include "ProcessControlTypes.h"
#include "ProcessAutoTypes.h"
using FProcessAutoCommand = TFunction<FProcessCommandRecord(EProcessCommand,const FString&,double)>;

/** Service-owned sequence. Commands use the existing transport; no actor or physics writes here. */
class FProcessAutoRun
{
public:
 FProcessAutoSnapshot State;
 FProcessAutoSettings Settings;
 bool SetMixingDuration(double Seconds,FString& Reason);
 double ConfiguredRPM(const FPlantSimulationSnapshot& S) const { return Settings.TargetMixerRPM!=0?Settings.TargetMixerRPM:S.RequestedRPM; }
 FString CanStart(const FProcessControlMachine& Machine,double Now) const;
 bool Start(FGuid Id,FProcessControlMachine& Machine,double Now);
 void Tick(FProcessControlMachine& Machine,double Now,const FProcessAutoCommand& Send);
 void Cancel(const FString& Reason);
 FString ReceiverPressureUnavailable(const FProcessControlMachine& M,double Now) const;
 FProcessCommandRecord RequestReceiverPressure(FGuid Id,int64 Epoch,int64 TransferId,double Pa,FProcessControlMachine& M,double Now,const FProcessControlTransport& Send);
 bool ObserveReceiverPressure(FProcessControlMachine& M,FString& Reason);
 void EndReceiverPressure();
 EProcessAutoStage RecoveryPlan(const FPlantSimulationSnapshot& S) const;
 FString CanRecover(const FProcessControlMachine& M,double Now) const;
 bool Recover(FProcessControlMachine& M,double Now);
 void ObserveRecovery(const FProcessControlMachine& M);
 double MixElapsed(const FPlantSimulationSnapshot& S) const {return S.Recovery.Supported?S.Recovery.AutomaticMixSeconds:S.PhysicalSeconds-State.MixerAppliedSimulationSeconds;}

 void ConfirmUI(int64 Epoch,int64 TransferId,double Time);
 static FString StageText(EProcessAutoStage Stage);
private:
 bool TickRecovery(FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send);
 bool TickAdditiveClock(FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send);
 int64 LastClockSequence=-1;
 double EnteredWall=0,EnteredSim=0,ProgressSim=0,StableSince=-1;
 int64 LastSequence=-1,LastWeighingSetRevision=-1;
 double PreviousPassedKg=0,PreviousPassedTime=-1;
 FString ProgressKey;
 void Enter(EProcessAutoStage Stage,const FPlantSimulationSnapshot& S,double Now);
 void Fail(const FString& Reason,FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send);
 bool Command(EProcessCommand C,const TCHAR* Id,double Value,EProcessAutoStage Next,
              FProcessControlMachine& M,double Now,const FProcessAutoCommand& Send);
 bool WatchProgress(const FPlantSimulationSnapshot& S,double Limit,FString& Reason);
};
