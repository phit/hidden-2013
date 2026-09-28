//========= Hidden: Source =====================================================//
//
// Purpose: ScuzTools Spectator Hidden Trail (Scuz's scuztools_hdnspec 1.0):
//			spectators and dead players see the Hidden as a trail coloured by its
//			health. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=75480
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "itempents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SPECTRAIL_SPRITE	"sprites/crystal_beam1.vmt"
#define SPECTRAIL_INTERVAL	2.0f	// a new trail segment this often, living as long

static ConVar hsm_spectrail( "hsm_spectrail", "0", FCVAR_NOTIFY, "Do spectators and the dead see the Hidden as a trail coloured by its health? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

class CHiddenPluginSpecTrail : public CHiddenPlugin
{
public:
	CHiddenPluginSpecTrail() { m_flNextTrail = 0.0f; m_iSprite = 0; }

	virtual void LevelInit( void )
	{
		m_iSprite = CBaseEntity::PrecacheModel( SPECTRAIL_SPRITE );
		m_flNextTrail = 0.0f;
	}

	virtual void Think( void )
	{
		if ( !hsm_spectrail.GetBool() || gpGlobals->curtime < m_flNextTrail )
			return;

		m_flNextTrail = gpGlobals->curtime + SPECTRAIL_INTERVAL;

		// Human spectators and the dead.
		CRecipientFilter filter;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && !pPlayer->IsBot() && ( pPlayer->GetTeamNumber() == TEAM_SPECTATOR || !pPlayer->IsAlive() ) )
				filter.AddRecipient( pPlayer );
		}
		if ( !filter.GetRecipientCount() )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pHidden = UTIL_PlayerByIndex( i );
			if ( !pHidden || !pHidden->IsAlive() || pHidden->GetTeamNumber() != TEAM_HIDDEN )
				continue;

			// The plugin's colours: green to yellow down to 50 health, then red towards magenta.
			const int iHealth = pHidden->GetHealth();
			int r, g, b;
			if ( iHealth > 50 )
			{
				r = 255 - ( iHealth - 50 ) * 5;
				g = 255;
				b = 0;
			}
			else
			{
				r = 255;
				g = 0;
				b = 255 - iHealth * 5;
			}

			te->BeamFollow( filter, 0.0f, pHidden->entindex(), m_iSprite, 0, SPECTRAIL_INTERVAL, 20.0f, 1.0f, 0.0f,
				clamp( r, 0, 255 ), clamp( g, 0, 255 ), clamp( b, 0, 255 ), 255 );
		}
	}

private:
	float m_flNextTrail;
	int m_iSprite;
};

static CHiddenPluginSpecTrail s_SpecTrail;
