#pragma once

#include "CoreMinimal.h"
#include "Blueprint/UserWidget.h"
#include "System/MainMenu/LBMainMenuTypes.h"
#include "LB_CodenameEntryWidget.generated.h"

class UEditableText;
class UTextBlock;
class UWidget;

/** Native behavior for the existing WBP_CodenameEntry designer asset. */
UCLASS(Abstract, Blueprintable)
class LEFTBEHIND_API ULB_CodenameEntryWidget : public UUserWidget
{
	GENERATED_BODY()

protected:
	virtual void NativeConstruct() override;
	virtual void NativeDestruct() override;
	
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|UI")
	void BP_OnConfirmClicked();

	UFUNCTION(BlueprintImplementableEvent, Category="LB|UI")
	void BP_OnBackClicked();

	UFUNCTION(BlueprintImplementableEvent, Category="LB|UI")
	void BP_OnSubmissionResult(ELBCodenameSubmitResult Result);
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|UI")
	void BP_OnTextChanged(const FText& Text, bool bIsValid);
	
	UFUNCTION(BlueprintCallable, Category="LB|UI")
	void SubmitCurrentText();
	
	UFUNCTION(BlueprintImplementableEvent, Category="LB|UI")
	void BP_OnWidgetShown();
	
private:
	UPROPERTY(Transient)
	TObjectPtr<UWidget> ConfirmButton;

	UPROPERTY(Transient)
	TObjectPtr<UWidget> BackButton;

	UPROPERTY(Transient)
	TObjectPtr<UEditableText> NameInput;

	UPROPERTY(Transient)
	TObjectPtr<UTextBlock> ErrorText;

	UFUNCTION()
	void HandleConfirmClicked();

	UFUNCTION()
	void HandleBackClicked();

	UFUNCTION()
	void HandleTextChanged(const FText& Text);

	UFUNCTION()
	void HandleTextCommitted(const FText& Text, ETextCommit::Type CommitMethod);

	UFUNCTION()
	void HandleSubmissionResult(ELBCodenameSubmitResult Result, const FString& SanitizedCodename);

	void SetErrorText(const FText& Message);
	FText SubmissionErrorText(ELBCodenameSubmitResult Result) const;
};
