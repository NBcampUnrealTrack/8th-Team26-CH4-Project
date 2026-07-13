#include "GameplayTags/LBTags.h"

namespace LBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(None, "LBTags.None", "None")
	namespace LBAbilities
	{

		UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven,"LBTags.LBAbilities.ActivateOnGiven","Tag for the Abilities that sould activate immediately");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Death,"LBTags.LBAbilities.Death","Tag for the Abilities that should die");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Primary, "LBTags.LBAbilities.Primary", "Tag for the Primary Ability");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Secondary, "LBTags.LBAbilities.Secondary", "Tag for the Secondary Ability");
		namespace Enemy
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack, "LBTags.LBAbilities.Enemy.Attack", "Enemy Attack Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(WideAttack, "LBTags.LBAbilities.Enemy.WideAttack", "Enemy Wide Attack Tag");
		}

		

	}

	namespace SetByCaller
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage, "LBTags.SetByCaller.Damage", "Runtime damage magnitude for GameplayEffects")
	}
	
	
	namespace Events
	{
		namespace Player
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitReact, "LBTags.Events.Player.HitReact", "Player hit reaction event");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Death, "LBTags.Events.Player.Death", "Player death event");
		}

		namespace Enemy
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitReact,"LBTags.Events.Enemy.HitReact","Hit");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(EndAttack, "LBTags.Events.Enemy.EndAttack", "Tag for the Enemy Ending an Attack")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase1,"LBTags.Events.Enemy.Phase1","Phase1 for BOSS");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase2,"LBTags.Events.Enemy.Phase2","Phase2 for BOSS");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase3,"LBTags.Events.Enemy.Phase3","Phase3 for BOSS");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Death, "LBTags.Events.Enemy.Death", "Boss death event");
		}
	}
}
