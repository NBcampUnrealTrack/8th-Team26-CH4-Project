#include "GameplayTags/LBTags.h"

namespace LBTags
{
	UE_DEFINE_GAMEPLAY_TAG_COMMENT(None, "CCTags.None", "None")
	namespace LBAbilities
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven,"LBTags.LBAbilities.ActivateOnGiven","Tag for the Abilities that sould activate immediately");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Death,"LBTags.LBAbilities.Death","Tag for the Abilities that should die");
		
		namespace Enemy
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack, "CCTags.CCAbilities.Enemy.Attack", "Enemy Attack Tag")
		}
	}
	
	
	namespace Events
	{
		namespace Enemy
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitReact,"LBTags.Events.Enemy.HitReact","Hit");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(EndAttack, "LBTags.Events.Enemy.EndAttack", "Tag for the Enemy Ending an Attack")
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase1,"LBTags.Events.Enemy.Phase1","Phase1 for BOSS");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase2,"LBTags.Events.Enemy.Phase2","Phase2 for BOSS");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Phase3,"LBTags.Events.Enemy.Phase3","Phase3 for BOSS");
		}
	}
}