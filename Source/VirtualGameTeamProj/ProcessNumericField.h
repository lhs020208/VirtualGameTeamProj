#pragma once
#include "CoreMinimal.h"
#include "ProcessNumericField.generated.h"
class UUserWidget;
class UEditableTextBox;
class UVerticalBox;
class UButton;
class UTextBlock;

/** A draft editor around an existing text box. It has no service/actor/command reference. */
UCLASS()
class UProcessNumericField : public UObject
{
 GENERATED_BODY()
public:
 bool Attach(UUserWidget* W,const TCHAR* Name,const FString& Unit,double Step,double Minimum,double Maximum,bool Positive=false);
 void Refresh(double Applied,bool HasApplied=true,const FString& CrossFieldError=FString());
 void Detach();
 UFUNCTION() void Increase();
 UFUNCTION() void Decrease();
 UFUNCTION() void Changed(const FText& Value);
 FString Name,Unit;
 double Minimum=0,Maximum=1.e12,Step=1;
 bool Positive=false,Touched=false;
 UPROPERTY(Transient) TObjectPtr<UEditableTextBox> Input;
 UPROPERTY(Transient) TObjectPtr<UVerticalBox> Frame;
 UPROPERTY(Transient) TObjectPtr<UTextBlock> Feedback;
 UPROPERTY(Transient) TObjectPtr<UButton> Plus;
 UPROPERTY(Transient) TObjectPtr<UButton> Minus;
private:
 void Adjust(double Direction);
};
