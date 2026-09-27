//========= Hidden: Source =====================================================//
//
// Purpose: Mirror Damage (Paegus's hsm_mirrordmg 1.0.2): team damage between
//			marines is given back to the victim and reflected on the attacker.
//			See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78951
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar hsm_mirror( "hsm_mirror", "0", FCVAR_NOTIFY, "Mirror or absorb team-damage? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_mirror_armour( "hsm_mirror_armour", "0.0", FCVAR_NOTIFY, "Restore hsm_mirror_ratio health to victim but don't take it from attacker", true, 0.0f, true, 1.0f );
static ConVar hsm_mirror_ratio( "hsm_mirror_ratio", "0.666666", FCVAR_NOTIFY, "Team-damage to restore or reflect. 0: Disable (0%), 1: Full (100%).", true, -1.0f, true, 1.0f );
static ConVar hsm_mirror_spawnguard( "hsm_mirror_spawnguard", "10.0", FCVAR_NOTIFY, "Seconds from round start have 100% restore/reflect. Automatically disabled on hidden attack. 0: Disable", true, 0.0f, true, 15.0f );
static ConVar hsm_mirror_weightadd( "hsm_mirror_weightadd", "0.0", FCVAR_NOTIFY, "Add the remaining damage to the victim's weight-points? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

class CHiddenPluginMirrorDamage : public CHiddenPlugin
{
public:
	CHiddenPluginMirrorDamage() { m_flSpawnGuardEnd = 0.0f; }

	virtual void LevelInit( void ) { m_flSpawnGuardEnd = 0.0f; }

	virtual void RoundStart( void )
	{
		m_flSpawnGuardEnd = 0.0f;
		if ( hdn_selectmethod.GetInt() == 0 && hsm_mirror_spawnguard.GetFloat() > 0.0f )
			m_flSpawnGuardEnd = gpGlobals->curtime + hsm_mirror_spawnguard.GetFloat();
	}

	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
	{
		if ( !IsActive() )
			return true;

		CHidden_Player *pAttacker = ToHiddenPlayer( info.GetAttacker() );
		if ( !pAttacker || pAttacker == pVictim )
			return true;

		// Once the Hidden is in the fight, the spawn guard is over.
		if ( pAttacker->GetTeamNumber() == TEAM_HIDDEN || pVictim->GetTeamNumber() == TEAM_HIDDEN )
		{
			m_flSpawnGuardEnd = 0.0f;
			return true;
		}

		if ( pAttacker->GetTeamNumber() != TEAM_IRIS || pVictim->GetTeamNumber() != TEAM_IRIS )
			return true;

		// The plugin gave the health back after the hit, so a lethal hit still killed; taking it off
		// before, the victim survives what they'd survive with the reduced damage.
		const float flRatio = ( gpGlobals->curtime < m_flSpawnGuardEnd ) ? 1.0f : hsm_mirror_ratio.GetFloat();
		const float flDamage = info.GetDamage();
		const float flUndamage = flDamage * flRatio;
		info.SetDamage( flDamage - flUndamage );

		if ( hsm_mirror_weightadd.GetBool() )
			pVictim->AddWeighting( RoundFloatToInt( ( 1.0f - flRatio ) * flDamage ) );

		// Armour: the attacker only gets back the weighting the restored part cost them, which the
		// reduced damage already does.
		if ( hsm_mirror_armour.GetBool() )
			return true;

		// Otherwise they lose the weighting for the full hit, and take the restored part themselves.
		pAttacker->AddWeighting( -RoundFloatToInt( flUndamage ) );

		if ( flUndamage > 0.0f && pAttacker->IsAlive() )
		{
			CTakeDamageInfo mirror( GetContainingEntity( INDEXENT( 0 ) ), GetContainingEntity( INDEXENT( 0 ) ), flUndamage, DMG_DROWN );
			pAttacker->TakeDamage( mirror );
		}

		return true;
	}

private:
	bool IsActive( void ) const { return hsm_mirror.GetBool() && hsm_mirror_ratio.GetFloat() != 0.0f && hdn_selectmethod.GetInt() == 0; }

	float m_flSpawnGuardEnd;	// all team damage is given back until then
};

static CHiddenPluginMirrorDamage s_MirrorDamage;
