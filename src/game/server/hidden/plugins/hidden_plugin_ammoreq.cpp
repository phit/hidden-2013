//========= Hidden: Source =====================================================//
//
// Purpose: Automatic Ammo Requester (Paegus's hsm_ammoreq 1.0.1): a marine
//			out of spare magazines asks for ammo. See docs/spec/plugins.md.
//
//			Original: http://forum.hidden-source.com/forumdisplay.php?f=13
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "ammodef.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define AMMOREQ_LOCATION_QUIET	2.0f	// location changes the plugin ignored after one it acted on
#define RADIO_AMMO				3

static ConVar hsm_ammoreq( "hsm_ammoreq", "0", FCVAR_NOTIFY, "Do IRIS out of spare magazines ask for ammo? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

// The ammo of each loadout weapon (HIDDEN_PRIMARY_*, HIDDEN_SECONDARY_*), as CHidden_Player gives it.
static const char *s_pszPrimaryAmmo[] = { "AMMO_556", "AMMO_BULLETS", "AMMO_BUCKSHOT", "XBowBolt" };
static const char *s_pszSecondaryAmmo[] = { "AMMO_PISTOL", "AMMO_9MM" };

class CHiddenPluginAmmoReq : public CHiddenPlugin
{
public:
	CHiddenPluginAmmoReq() { m_flLocationQuiet = 0.0f; }

	virtual void LevelInit( void ) { m_flLocationQuiet = 0.0f; }

	virtual void PlayerLocation( CHidden_Player *pPlayer )
	{
		if ( gpGlobals->curtime < m_flLocationQuiet )
			return;

		m_flLocationQuiet = gpGlobals->curtime + AMMOREQ_LOCATION_QUIET;
		Check( pPlayer );
	}

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		Check( pVictim );
		Check( ToHiddenPlayer( info.GetAttacker() ) );
	}

private:
	void Check( CHidden_Player *pPlayer )
	{
		if ( !hsm_ammoreq.GetBool() || !pPlayer || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_IRIS || pPlayer->IsRequestingAmmo() )
			return;

		if ( HasSpare( pPlayer, s_pszPrimaryAmmo, ARRAYSIZE( s_pszPrimaryAmmo ), pPlayer->GetPrimary() ) &&
			 HasSpare( pPlayer, s_pszSecondaryAmmo, ARRAYSIZE( s_pszSecondaryAmmo ), pPlayer->GetSecondary() ) )
			return;

		// Only when a support marine is alive to answer. Through the radio, so hdn_radio_limit applies.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pOther = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( pOther && pOther != pPlayer && pOther->IsAlive() && pOther->GetTeamNumber() == TEAM_IRIS && pOther->GetPlayerClass() == HIDDEN_CLASS_SUPPORT )
			{
				pPlayer->Radio( RADIO_AMMO );
				return;
			}
		}
	}

	static bool HasSpare( CHidden_Player *pPlayer, const char **ppszAmmo, int iCount, int iWeapon )
	{
		if ( iWeapon < 0 || iWeapon >= iCount )
			return true;

		return pPlayer->GetAmmoCount( GetAmmoDef()->Index( ppszAmmo[iWeapon] ) ) > 0;
	}

	float m_flLocationQuiet;
};

static CHiddenPluginAmmoReq s_AmmoReq;
