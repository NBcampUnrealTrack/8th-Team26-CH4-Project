#pragma once

#include "CoreMinimal.h"
#include "NativeGameplayTags.h"

namespace LBTags
{
	UE_DECLARE_GAMEPLAY_TAG_EXTERN(None);
	
	namespace LBAbilities
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Primary);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(ActivateOnGiven);
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Death);
		
		namespace Enemy
		{
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Attack);
		}
	}

	namespace SetByCaller
	{
		UE_DECLARE_GAMEPLAY_TAG_EXTERN(Damage);
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
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase1);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase2);
			UE_DECLARE_GAMEPLAY_TAG_EXTERN(Phase3);
		}
	}
	

}
