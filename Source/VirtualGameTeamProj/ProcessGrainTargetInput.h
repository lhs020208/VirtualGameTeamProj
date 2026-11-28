#pragma once

#include "Components/EditableTextBox.h"
#include "ProcessGrainTargetInput.generated.h"

// A UMG text event does not include its sender. Bind the field identity so that
// accepting an estimate as an exact count also works when the digits are identical.
UCLASS()
class UProcessGrainTargetInput : public UEditableTextBox
{
    GENERATED_BODY()
public:
    FSimpleDelegate Edited;

    void BindEditing()
    {
        OnTextChanged.AddUniqueDynamic(this, &UProcessGrainTargetInput::HandleChange);
        OnTextCommitted.AddUniqueDynamic(this, &UProcessGrainTargetInput::HandleCommit);
    }

private:
    UFUNCTION() void HandleChange(const FText& EnteredText) { Edited.ExecuteIfBound(); }
    UFUNCTION() void HandleCommit(const FText& EnteredText, ETextCommit::Type How)
    {
        if (How == ETextCommit::OnEnter) Edited.ExecuteIfBound();
    }
};
