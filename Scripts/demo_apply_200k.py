"""Apply the measured visual subdivision and incremental updates to demo mode only."""
import pathlib
root=pathlib.Path(__file__).resolve().parents[1];p=root/'Source/VirtualGameTeamProj/PowderPlantTest.cpp';s=p.read_text(encoding='utf-8')
def change(a,b):
 global s
 assert s.count(a)==1,a[:100]
 s=s.replace(a,b)
change('namespace {','''namespace {
void FillParticleTransforms(const std::vector<powder::Grain>& Points,const TArray<FVector>& Offsets,const FVector& Scale,TArray<TArray<FTransform>>& Transforms){
 for(auto& Group:Transforms)Group.Reset();
 for(const auto& A:Points){const FVector Center(A.x*100,-A.y*100,A.z*100);auto& Group=Transforms[int(A.w)];for(const FVector& Offset:Offsets)Group.Emplace(FQuat::Identity,Center+Offset,Scale);}
}
''')
change('Generation=0;CurrentPhase=0;', 'Generation=0;DisplayedParticleCount=SolverParticleCount=0;CurrentPhase=0;')
change('double Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;for(auto A:P)Groups[int(A.w)]->AddInstance(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));','''
  const int Copies=DemoRealtime?FMath::Clamp(DemoVisualCopies,1,10):1;
  const double RadiusScale=1./std::cbrt(double(Copies)),Diameter=Mesh->GetBounds().BoxExtent.GetMax()*2;
  VisualScale=FVector(200.*ParticleRadiusM*RadiusScale/Diameter);VisualOffsets.Reset(Copies);
  for(int K=0;K<Copies;++K){double Z=1.-2.*(K+.5)/Copies,R=std::sqrt(1-Z*Z),Phi=K*2.399963229728653;VisualOffsets.Add(Copies==1?FVector::ZeroVector:FVector(R*std::cos(Phi),R*std::sin(Phi),Z)*(100*ParticleRadiusM*(1-RadiusScale)));}
  InstanceTransforms.SetNum(Groups.Num());TArray<int32> Counts;Counts.Init(0,Groups.Num());for(const auto& A:P)++Counts[int(A.w)];
  for(int I=0;I<Groups.Num();++I)InstanceTransforms[I].Reserve(Counts[I]*Copies);
  FillParticleTransforms(P,VisualOffsets,VisualScale,InstanceTransforms);
  for(int I=0;I<Groups.Num();++I){Groups[I]->AddInstances(InstanceTransforms[I],false,false,false);DisplayedParticleCount+=Groups[I]->GetInstanceCount();}
  SolverParticleCount=DemoRealtime?ParticleCount/8:ParticleCount;''')
change('TArray<TArray<FTransform>> T;T.SetNum(Groups.Num());for(auto A:DisplayPoints){auto*G=Groups[int(A.w)].Get();double Diameter=G->GetStaticMesh()->GetBounds().BoxExtent.GetMax()*2;T[int(A.w)].Add(FTransform(FQuat::Identity,FVector(A.x,-A.y,A.z)*100,FVector((200.*ParticleRadiusM)/Diameter)));}for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,T[I],false,true,true);','''FillParticleTransforms(DisplayPoints,VisualOffsets,VisualScale,InstanceTransforms);
 // UE 5.8 tracks instance-transform changes automatically. Demo mode avoids
 // recreating the entire scene proxy for each frame; other modes keep their path.
 for(int I=0;I<Groups.Num();++I)Groups[I]->BatchUpdateInstancesTransforms(0,InstanceTransforms[I],false,!DemoRealtime,true);''')
change('Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text(FText::FromString(DemoRealtime?TEXT("시연용 · 목표 1배속 · 표시 20,000 / 계산 2,500"):TEXT("시뮬레이션 정보 · 배속 제한 없음")))];','''Panel->AddSlot().AutoHeight().Padding(0,0,0,6)[SNew(STextBlock).Text_Lambda([Weak](){if(!Weak.IsValid())return FText::GetEmpty();return FText::FromString(Weak->DemoRealtime?FString::Printf(TEXT("시연용 · 목표 1배속 · 표시 %d / 계산 %d"),Weak->DisplayedParticleCount,Weak->SolverParticleCount):TEXT("시뮬레이션 정보 · 배속 제한 없음"));})];''')
change('if(!Weak.IsValid())return FText::GetEmpty();const auto*P=Weak.Get();','if(!Weak.IsValid())return FText::GetEmpty();const auto*P=Weak.Get();const int VisualFactor=FMath::Max(1,P->VisualOffsets.Num());')
change('P->ActualRate,P->ParticleCount,','P->ActualRate,P->DisplayedParticleCount,')
change('P->Stage),P->InWeighingHopper,P->InReceiver,P->InMixer,P->ParticleCount-P->InWeighingHopper-P->InReceiver-P->InMixer,','P->Stage),P->InWeighingHopper*VisualFactor,P->InReceiver*VisualFactor,P->InMixer*VisualFactor,(P->ParticleCount-P->InWeighingHopper-P->InReceiver-P->InMixer)*VisualFactor,')
p.write_text(s,encoding='utf-8')
p=root/'Source/VirtualGameTeamProj/PowderPlantDemo.inl';s=p.read_text(encoding='utf-8')
change('bool Pass=Error.IsEmpty()', 'bool ConfigurationPass=DisplayedParticleCount==200000&&SolverParticleCount==2500&&ParticleCount==20000;\n bool Pass=ConfigurationPass&&Error.IsEmpty()')
change(r'\"solver_particles\":2500',r'\"solver_particles\":%d')
change('Percentile(.99),ParticleCount,InWeighingHopper','Percentile(.99),DisplayedParticleCount,SolverParticleCount,InWeighingHopper')
change('FFileHelper::SaveStringToFile(Result,',r'''Result.RemoveFromEnd(TEXT("}"));Result+=FString::Printf(TEXT(",\"physical_input_particles\":%d,\"configuration_pass\":%s,\"visual_copies\":%d}"),ParticleCount,ConfigurationPass?TEXT("true"):TEXT("false"),VisualOffsets.Num());
 FFileHelper::SaveStringToFile(Result,''')
p.write_text(s,encoding='utf-8')
print('Production demo uses configurable 10x visual subdivision; physics untouched')
