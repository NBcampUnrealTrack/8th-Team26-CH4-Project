#include "LBRaidPauseMenuAssetCommandlet.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/UserWidget.h"
#include "Blueprint/WidgetBlueprintGeneratedClass.h"
#include "Blueprint/WidgetTree.h"
#include "Components/Border.h"
#include "Components/HorizontalBox.h"
#include "Components/HorizontalBoxSlot.h"
#include "Components/Overlay.h"
#include "Components/OverlaySlot.h"
#include "Components/SizeBox.h"
#include "Components/TextBlock.h"
#include "Components/VerticalBox.h"
#include "Components/VerticalBoxSlot.h"
#include "Editor.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "Styling/CoreStyle.h"
#include "UI/Popup/LB_RaidPauseMenuWidget.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/UnrealType.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBRaidPauseMenuAsset, Log, All);

namespace
{
	const TCHAR* PauseMenuPackagePath = TEXT("/Game/LeftBehind/UI/BattleHUD/Popup/WBP_LB_RaidPauseMenuWidget");
	const TCHAR* PauseMenuObjectPath = TEXT("/Game/LeftBehind/UI/BattleHUD/Popup/WBP_LB_RaidPauseMenuWidget.WBP_LB_RaidPauseMenuWidget");
	const TCHAR* CommonButtonClassPath = TEXT("/Game/LeftBehind/UI/SubWidgets/WBP_Common_ButtonPrimary.WBP_Common_ButtonPrimary_C");
	const TCHAR* BodyFontPath = TEXT("/Game/LeftBehind/UI/Assets/Fonts/F_Body.F_Body");

	void MarkContractVariable(UWidgetBlueprint* Blueprint, UWidget* Widget)
	{
		check(Blueprint);
		check(Widget);
		Widget->bIsVariable = true;
	}

	FSlateFontInfo MakeFont(const UObject* FontObject, const float Size)
	{
		return IsValid(FontObject)
			? FSlateFontInfo(FontObject, Size)
			: FCoreStyle::GetDefaultFontStyle(TEXT("Regular"), Size);
	}

	UTextBlock* MakeText(
		UWidgetTree* Tree,
		const FName Name,
		const FText& Text,
		const UObject* FontObject,
		const float FontSize,
		const FLinearColor& Color)
	{
		UTextBlock* TextBlock = Tree->ConstructWidget<UTextBlock>(UTextBlock::StaticClass(), Name);
		TextBlock->SetText(Text);
		TextBlock->SetFont(MakeFont(FontObject, FontSize));
		TextBlock->SetColorAndOpacity(FSlateColor(Color));
		TextBlock->SetJustification(ETextJustify::Center);
		TextBlock->SetAutoWrapText(true);
		return TextBlock;
	}

	bool SetCommonButtonLabel(UUserWidget* Button, const FText& Label)
	{
		if (!IsValid(Button))
		{
			return false;
		}

		// The current common-button asset stores the editable text as Label while
		// exposing DisplayLabel in editor metadata.  Prefer the semantic name so
		// regenerated versions can migrate without changing this commandlet, then
		// fall back to the serialized property used by the shipped asset.
		FTextProperty* LabelProperty = FindFProperty<FTextProperty>(Button->GetClass(), TEXT("DisplayLabel"));
		if (!LabelProperty)
		{
			LabelProperty = FindFProperty<FTextProperty>(Button->GetClass(), TEXT("Label"));
		}

		// Blueprint member variables may be serialized with a GUID suffix (for
		// example Label_12_A1B2...).  Match the typed label property instead of
		// depending on that generated suffix so this remains deterministic after
		// the common button Blueprint is recompiled.
		if (!LabelProperty)
		{
			for (TFieldIterator<FTextProperty> PropertyIt(Button->GetClass()); PropertyIt; ++PropertyIt)
			{
				const FString PropertyName = PropertyIt->GetName();
				if (PropertyName.Contains(TEXT("DisplayLabel"), ESearchCase::IgnoreCase)
					|| PropertyName.Contains(TEXT("Label"), ESearchCase::IgnoreCase))
				{
					LabelProperty = *PropertyIt;
					break;
				}
			}
		}
		if (!LabelProperty)
		{
			UE_LOG(
				LogLBRaidPauseMenuAsset,
				Error,
				TEXT("Common button class %s exposes no label-like FText property."),
				*Button->GetClass()->GetPathName());
			return false;
		}

		LabelProperty->SetPropertyValue_InContainer(Button, Label);
		return true;
	}

	UUserWidget* MakeCommonButton(
		UWidgetBlueprint* Blueprint,
		UWidgetTree* Tree,
		const TSubclassOf<UUserWidget> CommonButtonClass,
		const FName Name,
		const FText& Label,
		bool& bOutSucceeded)
	{
		UUserWidget* Button = Tree->ConstructWidget<UUserWidget>(CommonButtonClass, Name);
		if (!IsValid(Button))
		{
			UE_LOG(LogLBRaidPauseMenuAsset, Error, TEXT("Failed to construct %s."), *Name.ToString());
			bOutSucceeded = false;
			return nullptr;
		}

		MarkContractVariable(Blueprint, Button);
		bOutSucceeded &= SetCommonButtonLabel(Button, Label);
		return Button;
	}

	void AddVerticalChild(
		UVerticalBox* Parent,
		UWidget* Child,
		const FMargin Padding,
		const EHorizontalAlignment HorizontalAlignment = HAlign_Fill)
	{
		if (!IsValid(Parent) || !IsValid(Child))
		{
			return;
		}

		if (UVerticalBoxSlot* Slot = Parent->AddChildToVerticalBox(Child))
		{
			Slot->SetPadding(Padding);
			Slot->SetHorizontalAlignment(HorizontalAlignment);
			Slot->SetVerticalAlignment(VAlign_Center);
		}
	}

	void AddOverlayChild(
		UOverlay* Parent,
		UWidget* Child,
		const EHorizontalAlignment HorizontalAlignment,
		const EVerticalAlignment VerticalAlignment,
		const FMargin Padding = FMargin(0.0f))
	{
		if (UOverlaySlot* Slot = Parent->AddChildToOverlay(Child))
		{
			Slot->SetHorizontalAlignment(HorizontalAlignment);
			Slot->SetVerticalAlignment(VerticalAlignment);
			Slot->SetPadding(Padding);
		}
	}

	bool ResetWidgetTree(UWidgetBlueprint* Blueprint)
	{
		if (!IsValid(Blueprint))
		{
			return false;
		}

		if (IsValid(Blueprint->WidgetTree) && IsValid(Blueprint->WidgetTree->RootWidget))
		{
			UWidgetTree* PreviousTree = Blueprint->WidgetTree;
			const FName DiscardedName = MakeUniqueObjectName(
				GetTransientPackage(),
				UWidgetTree::StaticClass(),
				TEXT("LBRaidPauseMenuDiscardedTree"));
			if (!PreviousTree->Rename(
				*DiscardedName.ToString(),
				GetTransientPackage(),
				REN_DontCreateRedirectors | REN_NonTransactional))
			{
				UE_LOG(LogLBRaidPauseMenuAsset, Error, TEXT("Failed to discard the previous widget tree."));
				return false;
			}
			Blueprint->WidgetTree = nullptr;
		}

		if (!IsValid(Blueprint->WidgetTree))
		{
			Blueprint->WidgetTree = NewObject<UWidgetTree>(
				Blueprint,
				UWidgetTree::StaticClass(),
				TEXT("WidgetTree"),
				RF_Transactional);
		}

#if WITH_EDITORONLY_DATA
		Blueprint->Bindings.Reset();
		Blueprint->Animations.Reset();
#endif
		// Leave the GUID map empty so UE 5.7's widget compiler can populate a
		// deterministic GUID for every source widget in one pass.  Adding GUIDs
		// only for BindWidget members makes the compiler report every decorative
		// widget as an invalid late addition.
		Blueprint->WidgetVariableNameToGuidMap.Reset();
		return IsValid(Blueprint->WidgetTree);
	}

	bool BuildWidgetTree(
		UWidgetBlueprint* Blueprint,
		const TSubclassOf<UUserWidget> CommonButtonClass,
		const UObject* FontObject)
	{
		if (!ResetWidgetTree(Blueprint))
		{
			return false;
		}

		UWidgetTree* Tree = Blueprint->WidgetTree;
		bool bSucceeded = true;

		UOverlay* Root = Tree->ConstructWidget<UOverlay>(UOverlay::StaticClass(), TEXT("OVR_PauseRoot"));
		Tree->RootWidget = Root;

		UBorder* Backdrop = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("BG_PauseBackdrop"));
		Backdrop->SetBrushColor(FLinearColor(0.005f, 0.008f, 0.016f, 0.82f));
		AddOverlayChild(Root, Backdrop, HAlign_Fill, VAlign_Fill);

		UBorder* ActionPanel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PNL_ActionMenu"));
		MarkContractVariable(Blueprint, ActionPanel);
		ActionPanel->SetBrushColor(FLinearColor(0.018f, 0.028f, 0.050f, 0.96f));
		ActionPanel->SetPadding(FMargin(36.0f, 30.0f, 36.0f, 36.0f));
		AddOverlayChild(Root, ActionPanel, HAlign_Center, VAlign_Center);

		USizeBox* ActionSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SB_ActionMenu"));
		ActionSize->SetWidthOverride(440.0f);
		ActionPanel->SetContent(ActionSize);

		UVerticalBox* ActionColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VB_ActionMenu"));
		ActionSize->SetContent(ActionColumn);

		UTextBlock* Title = MakeText(
			Tree,
			TEXT("TXT_PauseTitle"),
			NSLOCTEXT("LBRaidPauseMenu", "PauseTitle", "설정"),
			FontObject,
			34.0f,
			FLinearColor(0.92f, 0.96f, 1.0f, 1.0f));
		AddVerticalChild(ActionColumn, Title, FMargin(0.0f, 0.0f, 0.0f, 26.0f), HAlign_Fill);

		UUserWidget* ResumeButton = MakeCommonButton(
			Blueprint,
			Tree,
			CommonButtonClass,
			TEXT("BTN_Resume"),
			NSLOCTEXT("LBRaidPauseMenu", "Resume", "계속하기"),
			bSucceeded);
		AddVerticalChild(ActionColumn, ResumeButton, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		UUserWidget* ReturnButton = MakeCommonButton(
			Blueprint,
			Tree,
			CommonButtonClass,
			TEXT("BTN_ReturnToRoom"),
			NSLOCTEXT("LBRaidPauseMenu", "ReturnToRoom", "방으로 이동"),
			bSucceeded);
		AddVerticalChild(ActionColumn, ReturnButton, FMargin(0.0f, 0.0f, 0.0f, 12.0f));

		UUserWidget* QuitButton = MakeCommonButton(
			Blueprint,
			Tree,
			CommonButtonClass,
			TEXT("BTN_Quit"),
			NSLOCTEXT("LBRaidPauseMenu", "Quit", "게임 종료"),
			bSucceeded);
		AddVerticalChild(ActionColumn, QuitButton, FMargin(0.0f));

		UBorder* QuitPanel = Tree->ConstructWidget<UBorder>(UBorder::StaticClass(), TEXT("PNL_QuitConfirm"));
		MarkContractVariable(Blueprint, QuitPanel);
		QuitPanel->SetBrushColor(FLinearColor(0.018f, 0.028f, 0.050f, 0.98f));
		QuitPanel->SetPadding(FMargin(36.0f));
		QuitPanel->SetVisibility(ESlateVisibility::Collapsed);
		AddOverlayChild(Root, QuitPanel, HAlign_Center, VAlign_Center);

		USizeBox* QuitSize = Tree->ConstructWidget<USizeBox>(USizeBox::StaticClass(), TEXT("SB_QuitConfirm"));
		QuitSize->SetWidthOverride(500.0f);
		QuitPanel->SetContent(QuitSize);

		UVerticalBox* QuitColumn = Tree->ConstructWidget<UVerticalBox>(UVerticalBox::StaticClass(), TEXT("VB_QuitConfirm"));
		QuitSize->SetContent(QuitColumn);

		UTextBlock* QuitTitle = MakeText(
			Tree,
			TEXT("TXT_QuitTitle"),
			NSLOCTEXT("LBRaidPauseMenu", "QuitTitle", "게임 종료"),
			FontObject,
			30.0f,
			FLinearColor(1.0f, 0.93f, 0.90f, 1.0f));
		AddVerticalChild(QuitColumn, QuitTitle, FMargin(0.0f, 0.0f, 0.0f, 14.0f));

		UTextBlock* QuitQuestion = MakeText(
			Tree,
			TEXT("TXT_QuitQuestion"),
			NSLOCTEXT("LBRaidPauseMenu", "QuitQuestion", "게임을 종료하시겠습니까?"),
			FontObject,
			20.0f,
			FLinearColor(0.80f, 0.84f, 0.90f, 1.0f));
		AddVerticalChild(QuitColumn, QuitQuestion, FMargin(0.0f, 0.0f, 0.0f, 28.0f));

		UHorizontalBox* QuitButtons = Tree->ConstructWidget<UHorizontalBox>(
			UHorizontalBox::StaticClass(),
			TEXT("HB_QuitButtons"));
		AddVerticalChild(QuitColumn, QuitButtons, FMargin(0.0f));

		UUserWidget* ConfirmButton = MakeCommonButton(
			Blueprint,
			Tree,
			CommonButtonClass,
			TEXT("BTN_ConfirmQuit"),
			NSLOCTEXT("LBRaidPauseMenu", "ConfirmQuit", "종료"),
			bSucceeded);
		if (UHorizontalBoxSlot* Slot = QuitButtons->AddChildToHorizontalBox(ConfirmButton))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(0.0f, 0.0f, 8.0f, 0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		UUserWidget* CancelButton = MakeCommonButton(
			Blueprint,
			Tree,
			CommonButtonClass,
			TEXT("BTN_CancelQuit"),
			NSLOCTEXT("LBRaidPauseMenu", "CancelQuit", "취소"),
			bSucceeded);
		if (UHorizontalBoxSlot* Slot = QuitButtons->AddChildToHorizontalBox(CancelButton))
		{
			Slot->SetSize(FSlateChildSize(ESlateSizeRule::Fill));
			Slot->SetPadding(FMargin(8.0f, 0.0f, 0.0f, 0.0f));
			Slot->SetHorizontalAlignment(HAlign_Fill);
		}

		UTextBlock* HostNotice = MakeText(
			Tree,
			TEXT("TXT_HostPauseNotice"),
			NSLOCTEXT("LBRaidPauseMenu", "HostPauseNotice", "방장이 게임을 일시정지했습니다."),
			FontObject,
			30.0f,
			FLinearColor(0.92f, 0.96f, 1.0f, 1.0f));
		MarkContractVariable(Blueprint, HostNotice);
		HostNotice->SetMinDesiredWidth(720.0f);
		HostNotice->SetVisibility(ESlateVisibility::Collapsed);
		AddOverlayChild(Root, HostNotice, HAlign_Center, VAlign_Center, FMargin(48.0f));

		return bSucceeded;
	}

	UWidgetBlueprint* LoadOrCreatePauseMenuBlueprint(bool& bOutCreated)
	{
		bOutCreated = false;
		if (UWidgetBlueprint* Existing = LoadObject<UWidgetBlueprint>(nullptr, PauseMenuObjectPath))
		{
			return Existing;
		}

		UPackage* Package = CreatePackage(PauseMenuPackagePath);
		if (!IsValid(Package))
		{
			UE_LOG(LogLBRaidPauseMenuAsset, Error, TEXT("Could not create package %s."), PauseMenuPackagePath);
			return nullptr;
		}

		UWidgetBlueprint* Blueprint = Cast<UWidgetBlueprint>(FKismetEditorUtilities::CreateBlueprint(
			ULB_RaidPauseMenuWidget::StaticClass(),
			Package,
			TEXT("WBP_LB_RaidPauseMenuWidget"),
			BPTYPE_Normal,
			UWidgetBlueprint::StaticClass(),
			UWidgetBlueprintGeneratedClass::StaticClass(),
			TEXT("LBRaidPauseMenuAssetCommandlet")));
		if (IsValid(Blueprint))
		{
			FAssetRegistryModule::AssetCreated(Blueprint);
			bOutCreated = true;
		}
		return Blueprint;
	}

	bool CompileAndSave(UWidgetBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave);
		if (Blueprint->Status == BS_Error || !IsValid(Blueprint->GeneratedClass))
		{
			UE_LOG(LogLBRaidPauseMenuAsset, Error, TEXT("Pause menu Widget Blueprint compilation failed."));
			return false;
		}

		UEditorAssetSubsystem* AssetSubsystem = GEditor
			? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>()
			: nullptr;
		if (!IsValid(AssetSubsystem) || !AssetSubsystem->SaveLoadedAsset(Blueprint, false))
		{
			UE_LOG(LogLBRaidPauseMenuAsset, Error, TEXT("Failed to save %s."), *Blueprint->GetPathName());
			return false;
		}

		return true;
	}
}

ULBRaidPauseMenuAssetCommandlet::ULBRaidPauseMenuAssetCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 ULBRaidPauseMenuAssetCommandlet::Main(const FString& Params)
{
	TSubclassOf<UUserWidget> CommonButtonClass = LoadClass<UUserWidget>(nullptr, CommonButtonClassPath);
	if (!CommonButtonClass)
	{
		UE_LOG(LogLBRaidPauseMenuAsset, Error, TEXT("Could not load %s."), CommonButtonClassPath);
		return 1;
	}

	UObject* FontObject = LoadObject<UObject>(nullptr, BodyFontPath);
	if (!IsValid(FontObject))
	{
		UE_LOG(LogLBRaidPauseMenuAsset, Warning, TEXT("Could not load %s; using the engine fallback font."), BodyFontPath);
	}

	bool bCreated = false;
	UWidgetBlueprint* Blueprint = LoadOrCreatePauseMenuBlueprint(bCreated);
	if (!IsValid(Blueprint))
	{
		return 2;
	}

	if (Blueprint->ParentClass != ULB_RaidPauseMenuWidget::StaticClass())
	{
		Blueprint->ParentClass = ULB_RaidPauseMenuWidget::StaticClass();
	}

	if (!BuildWidgetTree(Blueprint, CommonButtonClass, FontObject))
	{
		return 3;
	}

	if (!CompileAndSave(Blueprint))
	{
		return 4;
	}

	UE_LOG(
		LogLBRaidPauseMenuAsset,
		Display,
		TEXT("LB_RAID_PAUSE_MENU_ASSET_SUCCESS (%s)"),
		bCreated ? TEXT("created") : TEXT("rebuilt"));
	return 0;
}
