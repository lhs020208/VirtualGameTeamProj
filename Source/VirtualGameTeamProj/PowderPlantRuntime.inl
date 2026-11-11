// Included only by PowderPlantTest.cpp. Owns all GPU work on one worker thread.
struct FPlantRuntime {
 std::thread Thread;std::mutex Mutex;std::atomic<bool> Quit{false},Run{true};
 std::atomic<double> Drop{1000},RPM{15};std::atomic<uint32> RequestedSuppliers{0};std::atomic<bool> RequestedHopper{false},RequestedReceiver{false};
 std::vector<powder::Grain> Points;std::string Error;double Time=0,Wall=0,Angle=0;
 int Phase=0,WH=0,RH=0,MX=0;bool HopperOpen=false,ReceiverOpen=false,Ready=false;uint32 OpenSuppliers=0;uint64 Generation=1;
 const bool Regulated;FString LogPath;
 powder::PlantPressureTargets Targets;int RequestedStage;bool RequestedManual;
 powder::PlantPressureState AppliedState;
 static bool Same(powder::PlantPressureTargets a,powder::PlantPressureTargets b){return a.supply==b.supply&&a.weighing==b.weighing&&a.receiver==b.receiver;}
 void RequestPressure(powder::PlantPressureTargets p,int stage,bool manual){powder::validate_plant_pressure_targets(p);powder::plant_pressure_preset(stage);std::lock_guard<std::mutex> lock(Mutex);Targets=p;RequestedStage=stage;RequestedManual=manual;}
 FPlantRuntime(powder::GranularConfig C,std::vector<powder::Grain> P,std::vector<powder::PlantRoute> Routes,double Timeout,double Fraction,double TransferWait,bool Start,double Pressure,double Speed,FVector MixerMin,FVector MixerMax,bool regulated,powder::PlantPressureTargets targets,int stage,bool manual):Points(P),Regulated(regulated),Targets(targets),RequestedStage(stage),RequestedManual(manual){
  Run=Start;Drop=Pressure;RPM=Speed;
  if(Regulated){FString folder=FPaths::ProjectSavedDir()/TEXT("PlantPressureRuns");IFileManager::Get().MakeDirectory(*folder,true);LogPath=FPaths::ConvertRelativePathToFull(folder/(FDateTime::UtcNow().ToString(TEXT("%Y%m%dT%H%M%SZ_"))+FGuid::NewGuid().ToString(EGuidFormats::Digits)+TEXT(".csv")));}
  Thread=std::thread([this,C=std::move(C),P=std::move(P),Routes=std::move(Routes),MixerMin,MixerMax]()mutable{
   try{
    using Clock=std::chrono::steady_clock;auto prepareStart=Clock::now();
    double AppliedPressure=Drop;auto Gates=powder::plant_supplier_gates(C.plant_data);
    int HopperGate=powder::plant_weighing_gate(C.plant_data),ReceiverGate=powder::plant_receiver_gate(C.plant_data);
    bool OpenHopper=false,PreviousHopper=false,OpenReceiver=false,PreviousReceiver=false;
    uint32 Open=0,PreviousRequest=0,All=Gates.size()==32?~uint32(0):(uint32(1)<<Gates.size())-1;
    powder::PlantPressureTargets previousTargets,wanted;int previousStage,stageNow;bool previousManual,manualNow;
    {std::lock_guard<std::mutex> lock(Mutex);previousTargets=wanted=Targets;previousStage=stageNow=RequestedStage;previousManual=manualNow=RequestedManual;}
    powder::PlantPressureState pressureState;
    if(Regulated)pressureState=powder::plant_set_regulated_pressures(C.plant_data,Routes,wanted,0,0,false,false,false);
    else {powder::plant_set_manual_valves(C.plant_data,Routes,AppliedPressure,0,0,false,false);powder::plant_set_receiver_valve(C.plant_data,false);}
    powder::GranularGPU GPU(C,P,std::vector<powder::Grain>(P.size()));
    auto readyAt=Clock::now();double setup=std::chrono::duration<double>(readyAt-prepareStart).count(),Active=0;
    auto LastPublish=readyAt,LastControl=readyAt,LastLog=readyAt;bool logControl=false;
    int PhaseNow=0;uint32 requestedMask=0;bool requestedWH=false,requestedRH=false;
    std::ofstream log;if(Regulated){log.open(TCHAR_TO_UTF8(*LogPath));if(!log)throw std::runtime_error("cannot create pressure run log");log<<std::setprecision(12)<<"event,utc,sim_s,active_wall_s,elapsed_since_ready_s,setup_s,pressure_stage,manual,supply_set_Pa_g,WH_set_Pa_g,RH_set_Pa_g,supply_applied_Pa_g,WH_applied_Pa_g,RH_applied_Pa_g,supplier_requested_mask,supplier_applied_mask,WH_requested_open,WH_applied_open,RH_requested_open,RH_applied_open,mixer_RPM,WH_regulator_m3_s,transfer_m3_s";for(const char* zone:{"SupplyA","SupplyB","SupplyC","SupplyD","SupplyE","SupplyF","WH","RH","MX","Other"})log<<','<<zone<<"_speciesA,"<<zone<<"_speciesB";log<<'\n';}
    auto publish=[&](const char* event,bool force){
     auto next=GPU.snapshot();int w,h,m;Count(next,w,h,m,MixerMin,MixerMax);auto now=Clock::now();
     if(Regulated&&(force||logControl||std::chrono::duration<double>(now-LastLog).count()>=5)){
      unsigned zones[10][2]{};
      for(auto a:next){int zone=9;for(size_t i=0;i<Gates.size()&&i<6;++i){auto g=C.plant_data[Gates[i]];if(std::hypot(a.x-g.x,a.y-g.y)<.355&&a.z>g.z+.003&&a.z<3.7){zone=int(i);break;}}
       if(zone==9){if(std::hypot(a.x,a.y)<.355&&a.z>.603&&a.z<1.78)zone=6;else if(std::hypot(a.x-3,a.y)<.255&&a.z>2.163&&a.z<3.7)zone=7;else if(a.x>MixerMin.X&&a.x<MixerMax.X&&a.y>MixerMin.Y&&a.y<MixerMax.Y&&a.z>MixerMin.Z&&a.z<MixerMax.Z)zone=8;}
       if(a.w==0||a.w==1)++zones[zone][unsigned(a.w)];
      }
      log<<event<<','<<TCHAR_TO_UTF8(*FDateTime::UtcNow().ToIso8601())<<','<<GPU.time()<<','<<Active<<','<<std::chrono::duration<double>(now-readyAt).count()<<','<<setup<<','<<stageNow<<','<<manualNow<<','<<wanted.supply<<','<<wanted.weighing<<','<<wanted.receiver<<','<<pressureState.applied.supply<<','<<pressureState.applied.weighing<<','<<pressureState.applied.receiver<<','<<requestedMask<<','<<Open<<','<<requestedWH<<','<<OpenHopper<<','<<requestedRH<<','<<OpenReceiver<<','<<RPM.load()<<','<<pressureState.weighing_regulator_m3s<<','<<pressureState.transfer_m3s;for(auto&zone:zones)log<<','<<zone[0]<<','<<zone[1];log<<'\n';log.flush();LastLog=now;logControl=false;
     }
     {std::lock_guard<std::mutex> lock(Mutex);Points=std::move(next);Time=GPU.time();Angle=GPU.angle();Wall=Active;WH=w;RH=h;MX=m;Phase=PhaseNow;OpenSuppliers=Open;HopperOpen=OpenHopper;ReceiverOpen=OpenReceiver;AppliedState=pressureState;Ready=true;++Generation;}
     LastPublish=now;
    };
    publish("ready",true);
    while(!Quit){
     {std::lock_guard<std::mutex> lock(Mutex);wanted=Targets;stageNow=RequestedStage;manualNow=RequestedManual;}
     requestedMask=RequestedSuppliers.load()&All;requestedWH=RequestedHopper.load();requestedRH=RequestedReceiver.load();
     if(!Run){if(std::chrono::duration<double>(Clock::now()-LastLog).count()>5)publish("paused_pending",false);std::this_thread::sleep_for(std::chrono::milliseconds(5));continue;}
     auto begin=Clock::now();double speed=RPM,drop=Drop;
     if(!std::isfinite(speed)||speed<0||speed>30)throw std::runtime_error("invalid live RPM");
     bool pressureChanged=Regulated?(!Same(wanted,previousTargets)||stageNow!=previousStage||manualNow!=previousManual):drop!=AppliedPressure;
     if(pressureChanged||requestedMask!=PreviousRequest||requestedWH!=PreviousHopper||requestedRH!=PreviousReceiver||((Open!=requestedMask||OpenHopper!=requestedWH||OpenReceiver!=requestedRH)&&std::chrono::duration<double>(begin-LastControl).count()>.1)){
      uint32 nextOpen=Open|requestedMask,closing=Open&~requestedMask;bool nextWH=OpenHopper||requestedWH,nextRH=OpenReceiver||requestedRH;
      if(closing||(OpenHopper&&!requestedWH)||(OpenReceiver&&!requestedRH)){auto current=GPU.snapshot();if(OpenReceiver&&!requestedRH&&powder::plant_supplier_closure_clear(C.plant_data[ReceiverGate],current,C.radius))nextRH=false;if(OpenHopper&&!requestedWH&&powder::plant_supplier_closure_clear(C.plant_data[HopperGate],current,C.radius))nextWH=false;for(size_t i=0;i<Gates.size();++i)if((closing&(uint32(1)<<i))&&powder::plant_supplier_closure_clear(C.plant_data[Gates[i]],current,C.radius))nextOpen&=~(uint32(1)<<i);}
      if(pressureChanged||nextOpen!=Open||requestedMask!=PreviousRequest||nextWH!=OpenHopper||requestedWH!=PreviousHopper||nextRH!=OpenReceiver||requestedRH!=PreviousReceiver){
       PhaseNow=nextWH?1:0;C.plant_data[4].x=float(PhaseNow);
       if(Regulated)pressureState=powder::plant_set_regulated_pressures(C.plant_data,Routes,wanted,nextOpen,requestedMask,nextWH,requestedWH,nextRH);
       else {powder::plant_set_manual_valves(C.plant_data,Routes,drop,nextOpen,requestedMask,nextWH,requestedWH);powder::plant_set_receiver_valve(C.plant_data,nextRH);}
       GPU.update_plant_field(C.plant_data);Open=nextOpen;OpenHopper=nextWH;OpenReceiver=nextRH;PreviousRequest=requestedMask;PreviousHopper=requestedWH;PreviousReceiver=requestedRH;AppliedPressure=drop;previousTargets=wanted;previousStage=stageNow;previousManual=manualNow;logControl=true;
      }LastControl=begin;
     }
     GPU.advance(16,float(speed),false);Active+=std::chrono::duration<double>(Clock::now()-begin).count();
     if(std::chrono::duration<double>(Clock::now()-LastPublish).count()>.1)publish(logControl?"control_applied":"sample",false);
    }
    publish("stopped_not_completion",true);
   }catch(const std::exception& e){if(Regulated)FFileHelper::SaveStringToFile(UTF8_TO_TCHAR(e.what()),*(LogPath+TEXT(".error.txt")));std::lock_guard<std::mutex> lock(Mutex);Error=e.what();Run=false;}
  });
 }
 ~FPlantRuntime(){Quit=true;if(Thread.joinable())Thread.join();}
};
