//========= Hidden: Source =====================================================//
//
// Purpose: Anti-Blur (Paegus's hsm_antiblur 1.0.0): a marine hit by a
//			teammate's FN303 bolt loses the blur. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?p=1823597
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ANTIBLUR_DELAY	0.1f	// the plugin's timer

static ConVar hsm_antiblur( "hsm_antiblur", "0", FCVAR_NOTIFY, "Clear the blur of IRIS hit by a teammate's FN303? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

class CHiddenPluginAntiBlur : public CHiddenPlugin
{
public:
	CHiddenPluginAntiBlur() { Reset(); }

	virtual void LevelInit( void ) { Reset(); }

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		if ( !hsm_antiblur.GetBool() || pVictim->GetTeamNumber() != TEAM_IRIS || !pVictim->IsAlive() )
			return;

		// The plugin told the bolt by its 3 damage.
		CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
		if ( !pAttacker || pAttacker->GetTeamNumber() != TEAM_IRIS || !info.GetInflictor() || !FClassnameIs( info.GetInflictor(), "fn303_bolt" ) )
			return;

		m_flUnblur[pVictim->entindex()] = gpGlobals->curtime + ANTIBLUR_DELAY;
	}

	virtual void Think( void )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			if ( m_flUnblur[i] <= 0.0f || gpGlobals->curtime < m_flUnblur[i] )
				continue;

			m_flUnblur[i] = 0.0f;
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer )
				pPlayer->ClearBlur();
		}
	}

private:
	void Reset( void )
	{
		for ( int i = 0; i < ARRAYSIZE( m_flUnblur ); i++ )
			m_flUnblur[i] = 0.0f;
	}

	float m_flUnblur[MAX_PLAYERS + 1];
};

static CHiddenPluginAntiBlur s_AntiBlur;
