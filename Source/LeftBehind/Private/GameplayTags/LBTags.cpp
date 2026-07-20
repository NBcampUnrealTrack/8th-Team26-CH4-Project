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
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Telegraph, "LBTags.LBAbilities.Enemy.Telegraph", "Enemy Telegraph Skill");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Attack, "LBTags.LBAbilities.Enemy.Attack", "Enemy Attack Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(WideAttack, "LBTags.LBAbilities.Enemy.WideAttack", "Enemy Wide Attack Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Charge, "LBTags.LBAbilities.Enemy.Charge", "Enemy Charge Attack Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(JumpSmash, "LBTags.LBAbilities.Enemy.JumpSmash", "Enemy JumpSmash Attack Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Summon, "LBTags.LBAbilities.Enemy.Summon", "Enemy Summon Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Buff, "LBTags.LBAbilities.Enemy.Buff", "Enemy Buff Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(Invincible, "LBTags.LBAbilities.Enemy.Invincible", "Enemy do not apply Damage for Phase change");
			

		}

		namespace CoolDown
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(AttackCoolDown, "LBTags.LBAbilities.CoolDown.Attack", "Enemy Attack CoolDown Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(SummonCoolDown, "LBTags.LBAbilities.CoolDown.Summon", "Enemy Summon CoolDown Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(ChargeCoolDown, "LBTags.LBAbilities.CoolDown.Charge", "Enemy Charge Attack CoolDown Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(JumpSmashCoolDown, "LBTags.LBAbilities.CoolDown.JumpSmash", "Enemy JumpSmash Attack CoolDown Tag");
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(WideAttackCoolDown, "LBTags.LBAbilities.CoolDown.WideAttack", "Enemy Wide Attack CoolDown Tag");
		}

	}
	


	namespace SetByCaller
	{
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Damage, "LBTags.SetByCaller.Damage", "Runtime damage magnitude for GameplayEffects")
		UE_DEFINE_GAMEPLAY_TAG_COMMENT(Heal, "LBTags.SetByCaller.Heal", "Runtime heal magnitude for GameplayEffects")
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
	
	namespace LBCues
	{
		namespace Enemy
		{
			UE_DEFINE_GAMEPLAY_TAG_COMMENT(TelegraphCue,"LBTags.LBCues.Enemy.TelegraphCue","TelegraphCue for Enemy");
		}
	}
}
