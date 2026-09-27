//========= Hidden: Source =====================================================//
//
// Purpose: Physics vs Pistols (Paegus's hsm_pvp 2.0.1): marines keep only their
//			pistols and the knife barely hurts. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78952
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define PVP_PISTOL_AMMO		6	// magazines, above the usual 3

static ConVar hsm_pvp( "hsm_pvp", "0", FCVAR_NOTIFY, "Physics vs Pistols mode. 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_pvp_knifedmg( "hsm_pvp_knifedmg", "0", FCVAR_NOTIFY, "How much damage the knife does while PVP mode is enabled.", true, 0.0f, true, 37.0f );

static const char *s_pszPrimaries[] = { "weapon_fn2000", "weapon_p90", "weapon_shotgun", "weapon_fn303" };

class CHiddenPluginPvP : public CHiddenPlugin
{
public:
	virtual void RoundStart( void )
	{
		if ( !hsm_pvp.GetBool() )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || !pPlayer->IsAlive() )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
			{
				for ( int j = 0; j < ARRAYSIZE( s_pszPrimaries ); j++ )
					HiddenPlugins_RemoveWeapon( pPlayer, s_pszPrimaries[j] );

				const bool bPistol2 = ( pPlayer->GetSecondary() == HIDDEN_SECONDARY_PISTOL2 );
				CBaseCombatWeapon *pPistol = pPlayer->Weapon_OwnsThisType( bPistol2 ? "weapon_pistol2" : "weapon_pistol" );
				if ( pPistol )
					pPlayer->Weapon_Switch( pPistol );

				// Straight into the ammo count, past the usual limit, as the plugin did.
				pPlayer->SetAmmoCount( PVP_PISTOL_AMMO, GetAmmoDef()->Index( bPistol2 ? "AMMO_9MM" : "AMMO_PISTOL" ) );
			}
			else
			{
				HiddenPlugins_RemoveWeapon( pPlayer, "weapon_grenade" );
			}
		}
	}

	// Support marines get ammo from each other; the plugin made everyone support for the next round.
	virtual void RoundEnd( void )
	{
		if ( !hsm_pvp.GetBool() )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
				pPlayer->SetPlayerClass( HIDDEN_CLASS_SUPPORT );
		}
	}

	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
	{
		if ( !hsm_pvp.GetBool() || pVictim->GetTeamNumber() == TEAM_HIDDEN )
			return true;

		// The knife: slashes and the pigstick.
		CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
		if ( pAttacker && pAttacker->GetTeamNumber() == TEAM_HIDDEN && info.GetDamageType() == DMG_SLASH )
			info.SetDamage( hsm_pvp_knifedmg.GetFloat() );

		return true;
	}
};

static CHiddenPluginPvP s_PvP;
