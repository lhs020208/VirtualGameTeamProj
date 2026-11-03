#pragma once
#include "ProcessContract.h"
#include "GameFramework/Actor.h"
#include "UObject/UnrealType.h"
namespace ProcessUI
{
/** Explicit read-only mapping audited against 74ac756 / PowderPlantTest.h.
 * Counts remain counts, configuration RPM remains a target. Private applied valve state is NOT inferred.
 */
class FDEMObserver : public IProvider
{
    TWeakObjectPtr<AActor> Actor;
    FSnapshot State;
    FReading Read(const TCHAR* Name,const TCHAR* Unit)
    {
        if(!Actor.IsValid()) { FReading R; R.Validity=EValidity::Missing; return R; }
        if(auto* P=FindFProperty<FNumericProperty>(Actor->GetClass(),Name))
        { const void* V=P->ContainerPtrToValuePtr<void>(Actor.Get()); return FReading::Known(P->IsFloatingPoint()?P->GetFloatingPointPropertyValue(V):double(P->GetSignedIntPropertyValue(V)),Unit,State.ReceivedSeconds); }
        return {};
    }
public:
    explicit FDEMObserver(AActor* InActor):Actor(InActor)
    {
        State.Source=TEXT("DEM 읽기 전용 · 규약 미합의 · 자동 운전 미지원");
        State.Capabilities={TEXT("equipment_particle_counts"),TEXT("simulation_time"),TEXT("error_text")};
        for(const auto& Pair:TArray<TPair<FString,FString>>{{TEXT("weigher"),TEXT("칭량호퍼")},{TEXT("receiver"),TEXT("수취호퍼")},{TEXT("mixer"),TEXT("혼합기")}})
        { FDevice D; D.Id=Pair.Key; D.Label=Pair.Value; State.Devices.Add(D); }
    }
    void Tick(double Dt) override
    {
        State.ReceivedSeconds+=Dt; ++State.Sequence;
        if(!Actor.IsValid()||Actor->GetClass()->GetFName()!=FName(TEXT("PowderPlantTest"))) { State.Error=TEXT("지원되는 PowderPlantTest 액터 연결 필요");for(auto& D:State.Devices)D.ParticleCount.Validity=EValidity::Missing; return; }
        State.Error.Empty();
        const FReading Clock=Read(TEXT("PhysicalSeconds"),TEXT("s")); if(Clock.Valid()) State.SourceSeconds=Clock.Value;
        State.Devices[0].ParticleCount=Read(TEXT("InWeighingHopper"),TEXT("개"));
        State.Devices[1].ParticleCount=Read(TEXT("InReceiver"),TEXT("개"));
        State.Devices[2].ParticleCount=Read(TEXT("InMixer"),TEXT("개"));
        if(auto* P=FindFProperty<FStrProperty>(Actor->GetClass(),TEXT("Error"))) State.Error=P->GetPropertyValue_InContainer(Actor.Get());
    }
    const FSnapshot& Snapshot() const override { return State; }
    FBounds Bounds() const override { return {}; }
    void Submit(const FCommand& C) override { State.Receipts.Add({C.Request,C.Generation,ECommandResult::Rejected,C.Device,TEXT("읽기 전용 연결: 명령 미지원")}); }
};
}
