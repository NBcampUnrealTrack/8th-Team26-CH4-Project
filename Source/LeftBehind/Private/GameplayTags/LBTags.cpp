#include "GameplayTags/LBTags.h"

namespace LBTags
{
	namespace LBAbilities
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Primary, "LBTags.LBAbilities.Primary", "Tag for the Primary Ability");
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(ActivateOnGiven,"CCTags.CCAbilities.ActivateOnGiven","Tag for the Abilities that sould activate immediately");
	}
	
	
	namespace Events
	{
		namespace Enemy
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(HitReact,"CCTags.Events.Enemy.HitReact","Hit");
		}
	}
}