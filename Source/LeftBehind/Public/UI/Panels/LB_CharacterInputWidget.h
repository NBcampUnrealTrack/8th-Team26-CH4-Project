// Fill out your copyright notice in the Description page of Project Settings.

#pragma once

#include "CoreMinimal.h"
#include "UI/Base/LB_BaseUserWidget.h"
#include "LB_CharacterInputWidget.generated.h"

class UHorizontalBox;
class ULB_SkillSlotWidget;
struct FLBCharacterData;

UCLASS()
class LEFTBEHIND_API ULB_CharacterInputWidget : public ULB_BaseUserWidget
{
	GENERATED_BODY()
	
protected:
	virtual void NativeConstruct() override;
	
	UPROPERTY(meta=(BindWidget))
	TObjectPtr<UHorizontalBox> HB_Root;

	UPROPERTY(EditDefaultsOnly, Category="LB|UI")
	TSubclassOf<ULB_SkillSlotWidget> SkillSlotWidgetClass;
	
	UFUNCTION(BlueprintCallable, Category="LB|UI")
	void RefreshSkills();

	UPROPERTY(EditDefaultsOnly, BlueprintReadOnly, Category="LB|UI")
	TObjectPtr<UDataTable> CharacterDataTable;

private:
	UPROPERTY()
	TArray<TObjectPtr<ULB_SkillSlotWidget>> SkillSlots;
};
