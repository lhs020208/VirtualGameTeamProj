#pragma once
#include "PlantTransferTelemetry.h"
#include "ProcessControlTypes.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformProcess.h"
#include "HAL/PlatformTime.h"
#include "Misc/CommandLine.h"
#include "Misc/FileHelper.h"
#include "Misc/Parse.h"
#include "Misc/Paths.h"
#include <iomanip>
#include <sstream>

/** Explicit development diagnostics; no controls, state restoration or physics steps.
 * Binary: uint32 count; float4 positions[count], velocities[count], air[count]; uint8 regions[count].
 * Original solver index = CSV ID; solver metres. Called only by the owning worker between batches.
 */
class FPlantTransferDiagnostics
{
public:
    bool Enabled=false;
    explicit FPlantTransferDiagnostics(const powder::GranularConfig& C)
    {
#if WITH_DEV_AUTOMATION_TESTS
        Enabled=FParse::Param(FCommandLine::Get(),TEXT("PlantTransferDiagnostics"))&&C.plant&&
            C.plant_data[11].x==0&&powder::plant_supplier_gates(C.plant_data).size()==6;
#endif
        if(Enabled)
        {
            Directory=FPaths::ProjectSavedDir()/TEXT("LowPolyTransferDiagnosis")/
                FString::Printf(TEXT("capture_%s_%u_%llu"),*FDateTime::UtcNow().ToString(TEXT("%Y%m%d_%H%M%S")),
                    FPlatformProcess::GetCurrentProcessId(),(unsigned long long)FPlatformTime::Cycles64());
            Enabled=IFileManager::Get().MakeDirectory(*Directory,true);
            UE_LOG(LogTemp,Display,TEXT("PLANT_TRANSFER_DIAGNOSTICS enabled=%d directory=%s"),Enabled,*Directory);
        }
    }
    void Capture(const char* Tag,powder::GranularGPU& GPU,const std::vector<powder::Grain>& P,
        const FPlantTransferTelemetry& Telemetry,const powder::GranularConfig& C,const std::vector<powder::PlantRoute>& Routes,
        const FPlantControlTargets& Requested,uint32 AppliedSuppliers,bool AppliedWH,bool AppliedRH,bool Force=false)
    {
        if(!Enabled||(!Force&&GPU.time()<NextSample))return;
        if(GPU.time()>=NextSample)NextSample=GPU.time()+.05;
        try
        {
            const auto V=GPU.velocity_snapshot(),Air=GPU.plant_air_snapshot();
            if(P.size()!=V.size()||P.size()!=Air.size())throw std::runtime_error("diagnostic array count mismatch");
            const uint32 Count=uint32(P.size());const int64 Bytes=Count*sizeof(powder::Grain);
            TArray<uint8> Data;Data.SetNumUninitialized(4+3*Bytes+Count);
            FMemory::Memcpy(Data.GetData(),&Count,4);FMemory::Memcpy(Data.GetData()+4,P.data(),Bytes);
            FMemory::Memcpy(Data.GetData()+4+Bytes,V.data(),Bytes);FMemory::Memcpy(Data.GetData()+4+2*Bytes,Air.data(),Bytes);
            TArray<int32> PipeIds;
            for(uint32 I=0;I<Count;++I){const auto R=Telemetry.Classify(P[I]);Data[4+3*Bytes+I]=uint8(R);if(R==EPlantMassRegion::Pipe)PipeIds.Add(int32(I));}
            const FString Base=Directory/FString::Printf(TEXT("%05u_%hs"),Serial++,Tag);
            if(!FFileHelper::SaveArrayToFile(Data,*(Base+TEXT(".bin"))))throw std::runtime_error("diagnostic binary write failed");
            FPlantSimulationSnapshot S;Telemetry.Publish(S);
            std::ostringstream M;M<<std::setprecision(17)<<"{\"tag\":"<<std::quoted(Tag)<<",\"count\":"<<Count
                <<",\"simulation_s\":"<<GPU.time()<<",\"wall_s\":"<<FPlatformTime::Seconds()-StartedWall
                <<",\"device\":"<<std::quoted(GPU.device())<<",\"dt\":"<<C.dt<<",\"radius_m\":"<<C.radius
                <<",\"density_kg_m3\":"<<C.plant_data[1].w<<",\"friction\":"<<C.friction
                <<",\"iterations\":"<<C.iterations<<",\"wall_mode\":"<<C.plant_wall_mode
                <<",\"control_revision\":"<<Requested.Revision<<",\"connection\":"<<Requested.ObservationConnection
                <<",\"requested_supplier_mask\":"<<Requested.Suppliers<<",\"applied_supplier_mask\":"<<AppliedSuppliers
                <<",\"requested_wh\":"<<Requested.WeighingOpen<<",\"applied_wh\":"<<AppliedWH
                <<",\"requested_rh\":"<<Requested.ReceiverOpen<<",\"applied_rh\":"<<AppliedRH
                <<",\"requested_running\":"<<Requested.Running<<",\"requested_pressure_pa\":"<<Requested.Pressure
                <<",\"weighing_pressure_pa\":"<<C.plant_data[4].z<<",\"receiver_pressure_pa\":"<<C.plant_data[4].w
                <<",\"network_mode\":"<<C.plant_data[4].y<<",\"phase\":"<<C.plant_data[4].x
                <<",\"transfer_id\":"<<S.Transfer.Id<<",\"opening_revision\":"<<S.Transfer.OpeningControlRevision
                <<",\"telemetry_s\":"<<S.Transfer.ObservedSimulationSeconds<<",\"baseline_kg\":"<<S.Transfer.BaselineKg
                <<",\"wh_kg\":"<<S.WeighingMass.Kg<<",\"pipe_kg\":"<<S.PipeMass.Kg<<",\"rh_kg\":"<<S.ReceiverMass.Kg
                <<",\"transfer_state\":"<<int(S.Transfer.State)<<",\"route_flows\":[";
            for(size_t I=0;I<Routes.size();++I){double Low=DBL_MAX,High=-DBL_MAX;
                for(int At:Routes[I].cells){Low=FMath::Min(Low,double(C.plant_data[At+2].x));High=FMath::Max(High,double(C.plant_data[At+2].x));}
                if(I)M<<',';M<<"{\"phase\":"<<Routes[I].phase<<",\"min_m3s\":"<<Low<<",\"max_m3s\":"<<High<<'}';}
            M<<"]}";
            if(!FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(M.str().c_str()),*(Base+TEXT(".json")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))throw std::runtime_error("diagnostic metadata write failed");
            // Separate read-only contact-shell probes, not recorded integrator impulses.
            if(GPU.time()>=NextWallProbe&&!PipeIds.IsEmpty())
            {
                NextWallProbe=GPU.time()+.2;std::vector<powder::Grain> Probes;TArray<int32> Ids;
                for(int32 I=0;I<PipeIds.Num();I+=FMath::Max(1,(PipeIds.Num()+31)/32)){Ids.Add(PipeIds[I]);Probes.push_back(P[PipeIds[I]]);}
                const auto Walls=GPU.inspect_boundaries(Probes,GPU.angle(),false,true);
                FString CSV=TEXT("id,x,y,z,vx,vy,vz,air_x,air_y,air_z,projected_x,projected_y,projected_z,nx,ny,nz\n");
                for(int32 I=0;I<Ids.Num();++I){const auto Id=Ids[I];const auto A=P[Id],B=V[Id],F=Air[Id];const auto H=Walls[I];
                    CSV+=FString::Printf(TEXT("%d,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g,%.9g\n"),
                        Id,A.x,A.y,A.z,B.x,B.y,B.z,F.x,F.y,F.z,H.position.x,H.position.y,H.position.z,H.normal.x,H.normal.y,H.normal.z);}
                if(!FFileHelper::SaveStringToFile(CSV,*(Base+TEXT("_walls.csv")),FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM))throw std::runtime_error("diagnostic wall probe write failed");
            }
        }
        catch(const std::exception& E){Enabled=false;UE_LOG(LogTemp,Warning,TEXT("PLANT_TRANSFER_DIAGNOSTICS disabled: %hs"),E.what());}
    }
private:
    FString Directory;
    uint32 Serial=0;
    double NextSample=0,NextWallProbe=3.6,StartedWall=FPlatformTime::Seconds();
};
