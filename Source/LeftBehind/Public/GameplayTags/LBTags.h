#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace LBTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(None);
	
	namespace LBAbilities
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Secondary);
		
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateOnGiven);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
		
		namespace Enemy
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Telegraph);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(WideAttack);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Charge);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(JumpSmash);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Summon);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Buff);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Invincible);
		}
		
		namespace CoolDown
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(AttackCoolDown);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(ChargeCoolDown);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(JumpSmashCoolDown);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(WideAttackCoolDown);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(SummonCoolDown);
		}
	}

	namespace SetByCaller
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Heal);
	}
	
	namespace Events
	{
		namespace Player
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitReact);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
		}

		namespace Enemy
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(HitReact);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(EndAttack);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase1);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase2);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase3);
		}
	}
	
	namespace LBCues
	{
		namespace Enemy
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(TelegraphCue);
		}
	}
	

}
