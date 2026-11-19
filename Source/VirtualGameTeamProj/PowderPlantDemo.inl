// Opt-in scripted demonstration and rendered, wall-clock benchmark.
void APowderPlantTest::TickDemo(double DeltaSeconds){
 if(DemoTestSeconds>0&&DemoWarmFrames<90){Running=false;++DemoWarmFrames;if(DemoWarmFrames==90){Running=true;DemoAutoCycle=true;DemoTestStart=DemoLastFrame=FPlatformTime::Seconds();DemoNextSample=0;DemoTrace=TEXT("wall_s,sim_s,rate,count,WH,RH,MX\n");if(Display)Display->ShowCutaway();}return;}
 if(DemoAutoCycle&&Running){int S=PhysicalSeconds<2?0:PhysicalSeconds<22?1:PhysicalSeconds<48?2:3;if(S!=DemoAutoStage){DemoAutoStage=S;for(int i=0;i<SupplierOpen.Num();++i)SupplierOpen[i]=S==1&&i<2;WeighingHopperOpen=S==2;ReceiverOpen=S>=3;MixerEnabled=S>=3;MixerRPM=15;if(RegulatedPressureEnabled)ApplyPressurePreset(S==1?1:S==2?3:S==3?4:0);}}
 if(DemoTestSeconds<=0||DemoTestStart<=0)return;
 double Now=FPlatformTime::Seconds(),Wall=Now-DemoTestStart;DemoFrames.Add(Now-DemoLastFrame);DemoLastFrame=Now;
 if(Wall>=DemoNextSample){DemoTrace+=FString::Printf(TEXT("%.9f,%.9f,%.9f,%d,%d,%d,%d\n"),Wall,PhysicalSeconds,Wall>0?PhysicalSeconds/Wall:0,ParticleCount,InWeighingHopper,InReceiver,InMixer);DemoNextSample+=1;if(int(DemoNextSample)%10==0){FString Progress=FString::Printf(TEXT("{\"wall_s\":%.3f,\"sim_s\":%.3f,\"fps\":%.3f,\"MX\":%d}"),Wall,PhysicalSeconds,DemoFrames.Num()/Wall,InMixer);FFileHelper::SaveStringToFile(Progress,*(DemoOutput+TEXT("_progress.json")));}}
 // Identical camera tour in every acceptance run, including the dense mixer.
 int CameraStage=Wall>=69?3:Wall>=34?2:Wall>=9?1:0;
 if(CameraStage>DemoCameraStage){DemoCameraStage=CameraStage;if(auto*PC=GetWorld()->GetFirstPlayerController())if(auto*Camera=Cast<ACameraActor>(PC->GetViewTarget())){FVector Target=CameraStage==1?FVector(0,0,85):CameraStage==2?FVector(300,0,245):FVector(300,0,110.5);FVector Offset=CameraStage==3?FVector(55,100,35):FVector(70,150,45);Camera->SetActorLocation(Target+Offset);Camera->SetActorRotation((Target-Camera->GetActorLocation()).Rotation());}}
 int Shot=Wall>=70?3:Wall>=35?2:Wall>=10?1:0;
 if(Shot>DemoShotStage){DemoShotStage=Shot;FString Name=DemoOutput+FString::Printf(TEXT("_%d.png"),Shot);FScreenshotRequest::RequestScreenshot(Name,true,false);}
 if(Wall<DemoTestSeconds&&Error.IsEmpty())return;
 Running=false;if(DemoRuntime)DemoRuntime->Run=false;Show();double Sim=PhysicalSeconds;Stop();
 DemoFrames.Sort();auto Percentile=[&](double q){return DemoFrames.IsEmpty()?0.:DemoFrames[FMath::Clamp(int(q*(DemoFrames.Num()-1)),0,DemoFrames.Num()-1)]*1000;};
 double FPS=DemoFrames.Num()/Wall,Rate=Sim/Wall;
 FIntPoint Pixels=GEngine&&GEngine->GameViewport&&GEngine->GameViewport->Viewport?GEngine->GameViewport->Viewport->GetSizeXY():FIntPoint::ZeroValue;
 bool ConfigurationPass=DisplayedParticleCount==200000&&SolverParticleCount==2500&&ParticleCount==20000;
 bool Pass=ConfigurationPass&&Error.IsEmpty()&&FPS>=29.5&&Percentile(.95)<=35&&Percentile(.99)<=50&&Rate>=.98&&Rate<=1.02&&ParticleCount==20000&&Pixels.X==1920&&Pixels.Y==1080;
 FString Result=FString::Printf(TEXT("{\"model\":\"demo constrained routes and coarse soft contacts\",\"rendered\":true,\"wall_s\":%.9f,\"sim_s\":%.9f,\"rate\":%.9f,\"frames\":%d,\"fps\":%.9f,\"p95_ms\":%.9f,\"p99_ms\":%.9f,\"display_particles\":%d,\"solver_particles\":%d,\"WH\":%d,\"RH\":%d,\"MX\":%d,\"performance_pass\":%s,\"error\":%s}"),Wall,Sim,Rate,DemoFrames.Num(),FPS,Percentile(.95),Percentile(.99),DisplayedParticleCount,SolverParticleCount,InWeighingHopper,InReceiver,InMixer,Pass?TEXT("true"):TEXT("false"),Error.IsEmpty()?TEXT("false"):TEXT("true"));
 if(DemoOutput.IsEmpty())DemoOutput=FPaths::ProjectSavedDir()/TEXT("DemoBenchmark");
 Result.RemoveFromEnd(TEXT("}"));Result+=FString::Printf(TEXT(",\"width\":%d,\"height\":%d}"),Pixels.X,Pixels.Y);
 Result.RemoveFromEnd(TEXT("}"));Result+=FString::Printf(TEXT(",\"physical_input_particles\":%d,\"configuration_pass\":%s,\"visual_copies\":%d}"),ParticleCount,ConfigurationPass?TEXT("true"):TEXT("false"),VisualOffsets.Num());
 FFileHelper::SaveStringToFile(Result,*(DemoOutput+TEXT(".json")));FFileHelper::SaveStringToFile(DemoTrace,*(DemoOutput+TEXT(".csv")));
 DemoTestSeconds=0;FPlatformMisc::RequestExitWithStatus(false,Pass?0:2);
}
