#include "LBMainMenuAssetMigrationCommandlet.h"

#include "BlueprintEditorLibrary.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "GameFramework/GameModeBase.h"
#include "GameMode/LB_MainMenuGameMode.h"
#include "GameState/LB_MainMenuGameState.h"
#include "K2Node_CallFunction.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_Event.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Player/LB_MainMenuPlayerController.h"
#include "Player/LB_PlayerState.h"
#include "Subsystems/EditorAssetSubsystem.h"
#include "UI/MainMenu/LB_CodenameEntryWidget.h"
#include "UI/MainMenu/LB_MainMenuRootWidget.h"
#include "UI/MainMenu/LB_MainMenuWaitingWidget.h"
#include "WidgetBlueprint.h"

DEFINE_LOG_CATEGORY_STATIC(LogLBMainMenuMigration, Log, All);

namespace
{
	const TCHAR* MainMenuGameModePath = TEXT("/Game/LeftBehind/Blueprints/System/BP_MainMenuGameMode.BP_MainMenuGameMode");
	const TCHAR* MainMenuWidgetPath = TEXT("/Game/LeftBehind/UI/MainMenu/WBP_MainMenu.WBP_MainMenu");
	const TCHAR* CodenameWidgetPath = TEXT("/Game/LeftBehind/UI/MainMenu/WBP_CodenameEntry.WBP_CodenameEntry");
	const TCHAR* WaitingWidgetPath = TEXT("/Game/LeftBehind/UI/MainMenu/WBP_WaitingRoom.WBP_WaitingRoom");

	UBlueprint* LoadBlueprintChecked(const TCHAR* Path)
	{
		UBlueprint* Blueprint = LoadObject<UBlueprint>(nullptr, Path);
		if (!IsValid(Blueprint))
		{
			UE_LOG(LogLBMainMenuMigration, Error, TEXT("Could not load Blueprint: %s"), Path);
		}
		return Blueprint;
	}

	void BreakExecutionFromEvent(UK2Node_Event* EventNode)
	{
		if (!IsValid(EventNode))
		{
			return;
		}
		for (UEdGraphPin* Pin : EventNode->Pins)
		{
			if (Pin
				&& Pin->Direction == EGPD_Output
				&& Pin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec)
			{
				Pin->BreakAllPinLinks(true);
			}
		}
	}

	void DisconnectComponentEvent(UBlueprint* Blueprint, FName ComponentName)
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!IsValid(Graph))
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_ComponentBoundEvent* BoundEvent = Cast<UK2Node_ComponentBoundEvent>(Node);
				if (IsValid(BoundEvent) && BoundEvent->GetComponentPropertyName() == ComponentName)
				{
					BreakExecutionFromEvent(BoundEvent);
				}
			}
		}
	}

	void DisconnectNamedEvent(UBlueprint* Blueprint, FName EventName)
	{
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!IsValid(Graph))
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node);
				if (IsValid(EventNode) && EventNode->EventReference.GetMemberName() == EventName)
				{
					BreakExecutionFromEvent(EventNode);
				}
			}
		}
	}

	void RemoveLegacyNameRpcNodes(UBlueprint* Blueprint)
	{
		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (!IsValid(Graph))
			{
				continue;
			}
			for (UEdGraphNode* Node : Graph->Nodes)
			{
				const UK2Node_CallFunction* Call = Cast<UK2Node_CallFunction>(Node);
				if (IsValid(Call) && Call->FunctionReference.GetMemberName() == TEXT("ServerRPCSetPlayerName"))
				{
					NodesToRemove.Add(Node);
				}
			}
		}

		for (UEdGraphNode* Node : NodesToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	void ClearUbergraph(UBlueprint* Blueprint)
	{
		TArray<UEdGraphNode*> NodesToRemove;
		for (UEdGraph* Graph : Blueprint->UbergraphPages)
		{
			if (IsValid(Graph))
			{
				NodesToRemove.Append(Graph->Nodes);
			}
		}
		for (UEdGraphNode* Node : NodesToRemove)
		{
			FBlueprintEditorUtils::RemoveNode(Blueprint, Node, true);
		}
	}

	bool CompileAndSave(UBlueprint* Blueprint)
	{
		FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
		FKismetEditorUtilities::CompileBlueprint(Blueprint, EBlueprintCompileOptions::SkipSave);
		if (Blueprint->Status == BS_Error)
		{
			UE_LOG(LogLBMainMenuMigration, Error, TEXT("Blueprint compilation failed: %s"), *Blueprint->GetPathName());
			return false;
		}

		UEditorAssetSubsystem* AssetSubsystem = GEditor ? GEditor->GetEditorSubsystem<UEditorAssetSubsystem>() : nullptr;
		if (!IsValid(AssetSubsystem) || !AssetSubsystem->SaveLoadedAsset(Blueprint, false))
		{
			UE_LOG(LogLBMainMenuMigration, Error, TEXT("Blueprint save failed: %s"), *Blueprint->GetPathName());
			return false;
		}
		UE_LOG(LogLBMainMenuMigration, Display, TEXT("Migrated %s"), *Blueprint->GetPathName());
		return true;
	}
}

ULBMainMenuAssetMigrationCommandlet::ULBMainMenuAssetMigrationCommandlet()
{
	IsClient = false;
	IsEditor = true;
	IsServer = false;
	LogToConsole = true;
	ShowErrorCount = true;
}

int32 ULBMainMenuAssetMigrationCommandlet::Main(const FString& Params)
{
	UBlueprint* GameModeBlueprint = LoadBlueprintChecked(MainMenuGameModePath);
	UWidgetBlueprint* MainMenuBlueprint = Cast<UWidgetBlueprint>(LoadBlueprintChecked(MainMenuWidgetPath));
	UWidgetBlueprint* CodenameBlueprint = Cast<UWidgetBlueprint>(LoadBlueprintChecked(CodenameWidgetPath));
	UWidgetBlueprint* WaitingBlueprint = Cast<UWidgetBlueprint>(LoadBlueprintChecked(WaitingWidgetPath));
	if (!GameModeBlueprint || !MainMenuBlueprint || !CodenameBlueprint || !WaitingBlueprint)
	{
		return 1;
	}

	UBlueprintEditorLibrary::ReparentBlueprint(GameModeBlueprint, ALB_MainMenuGameMode::StaticClass());
	ClearUbergraph(GameModeBlueprint);
	FKismetEditorUtilities::CompileBlueprint(GameModeBlueprint, EBlueprintCompileOptions::SkipSave);
	if (!IsValid(GameModeBlueprint->GeneratedClass))
	{
		UE_LOG(LogLBMainMenuMigration, Error, TEXT("GameMode generated class is invalid after reparenting."));
		return 2;
	}

	AGameModeBase* GameModeDefaults = GameModeBlueprint->GeneratedClass->GetDefaultObject<AGameModeBase>();
	GameModeDefaults->bUseSeamlessTravel = true;
	if (FBoolProperty* StartAsSpectatorProperty = FindFProperty<FBoolProperty>(AGameModeBase::StaticClass(), TEXT("bStartPlayersAsSpectators")))
	{
		StartAsSpectatorProperty->SetPropertyValue_InContainer(GameModeDefaults, true);
	}
	GameModeDefaults->GameStateClass = ALB_MainMenuGameState::StaticClass();
	GameModeDefaults->PlayerControllerClass = ALB_MainMenuPlayerController::StaticClass();
	GameModeDefaults->PlayerStateClass = ALB_PlayerState::StaticClass();
	GameModeDefaults->DefaultPawnClass = nullptr;
	GameModeDefaults->HUDClass = nullptr;

	if (FIntProperty* MinPlayersProperty = FindFProperty<FIntProperty>(ALB_MainMenuGameMode::StaticClass(), TEXT("MinPlayersToStart")))
	{
		MinPlayersProperty->SetPropertyValue_InContainer(GameModeDefaults, 2);
	}
	if (FSoftObjectProperty* RaidMapProperty = FindFProperty<FSoftObjectProperty>(ALB_MainMenuGameMode::StaticClass(), TEXT("RaidMap")))
	{
		RaidMapProperty->SetPropertyValue_InContainer(
			GameModeDefaults,
			FSoftObjectPtr(FSoftObjectPath(TEXT("/Game/LeftBehind/Maps/Main.Main"))));
	}

	UBlueprintEditorLibrary::ReparentBlueprint(MainMenuBlueprint, ULB_MainMenuRootWidget::StaticClass());
	DisconnectComponentEvent(MainMenuBlueprint, TEXT("BTN_Start_Start"));

	UBlueprintEditorLibrary::ReparentBlueprint(CodenameBlueprint, ULB_CodenameEntryWidget::StaticClass());
	DisconnectNamedEvent(CodenameBlueprint, TEXT("Construct"));
	DisconnectComponentEvent(CodenameBlueprint, TEXT("BTN_Confirm"));
	DisconnectComponentEvent(CodenameBlueprint, TEXT("BTN_CodeName_Back"));
	DisconnectComponentEvent(CodenameBlueprint, TEXT("ETB_Name"));
	RemoveLegacyNameRpcNodes(CodenameBlueprint);

	UBlueprintEditorLibrary::ReparentBlueprint(WaitingBlueprint, ULB_MainMenuWaitingWidget::StaticClass());
	ClearUbergraph(WaitingBlueprint);

	const bool bSucceeded = CompileAndSave(GameModeBlueprint)
		&& CompileAndSave(MainMenuBlueprint)
		&& CompileAndSave(CodenameBlueprint)
		&& CompileAndSave(WaitingBlueprint);

	if (bSucceeded)
	{
		UE_LOG(LogLBMainMenuMigration, Display, TEXT("LB_MAIN_MENU_ASSET_MIGRATION_SUCCESS"));
	}
	else
	{
		UE_LOG(LogLBMainMenuMigration, Error, TEXT("LB_MAIN_MENU_ASSET_MIGRATION_FAILED"));
	}
	return bSucceeded ? 0 : 3;
}
