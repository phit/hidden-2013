//========= Hidden: Source =====================================================//
//
// Purpose: Dynamic Round Timer (Phaedrus and Paegus's hsm_drt 1.0.0): the
//			round length grows with the number of players. See
//			docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?p=712877
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar hsm_drt( "hsm_drt", "0", FCVAR_NOTIFY, "Set mp_roundtime from the number of players? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_drt_basetime( "hsm_drt_basetime", "150", 0, "Base time for round.", true, 0.0f, false, 0.0f );
static ConVar hsm_drt_inctime( "hsm_drt_inctime", "30", 0, "Additional time for each IRIS player.", true, 0.0f, false, 0.0f );
static ConVar hsm_drt_randtime( "hsm_drt_randtime", "30", 0, "Additional random time added per round. 0: None", true, 0.0f, false, 0.0f );

class CHiddenPluginDRT : public CHiddenPlugin
{
public:
	// On every team change. The plugin counted everyone on a team but the player changing (the
	// Hidden too, whatever its help text says), so a player who joins isn't counted until the next.
	virtual void PlayerTeam( CHidden_Player *pPlayer )
	{
		if ( !hsm_drt.GetBool() )
			return;

		int iPlayers = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pOther = UTIL_PlayerByIndex( i );
			if ( pOther && pOther != pPlayer && pOther->GetTeamNumber() > TEAM_SPECTATOR )
				iPlayers++;
		}

		mp_roundtime.SetValue( hsm_drt_basetime.GetInt() + random->RandomInt( 0, hsm_drt_randtime.GetInt() ) + iPlayers * hsm_drt_inctime.GetInt() );
	}
};

static CHiddenPluginDRT s_DRT;
