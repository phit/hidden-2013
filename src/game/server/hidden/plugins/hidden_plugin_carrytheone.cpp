//========= Hidden: Source =====================================================//
//
// Purpose: Carry the one (Paegus's hsm_carrytheone 1.0.2): marines who weren't
//			picked as the Hidden keep their weighting. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78947
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar mp_chattime;

static ConVar hsm_ct1( "hsm_ct1", "0", FCVAR_NOTIFY, "Carry IRIS's weighting over to the next round? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_ct1_keep( "hsm_ct1_keep", "1.0", FCVAR_NOTIFY, "existing weight to carry forward. 0: None (0%) Disables plugin, 1: Full (100%).", true, 0.0f, true, 2.0f );
static ConVar hsm_ct1_purge( "hsm_ct1_purge", "0.0", FCVAR_NOTIFY, "purge weighting on map change? 0: Disabled, 1: Enabled.", true, 0.0f, true, 1.0f );

class CHiddenPluginCarryTheOne : public CHiddenPlugin
{
public:
	CHiddenPluginCarryTheOne()
	{
		m_flSaveTime = 0.0f;
		for ( int i = 0; i < ARRAYSIZE( m_iWeights ); i++ )
			m_iWeights[i] = 0;
	}

	virtual void LevelInit( void )
	{
		m_flSaveTime = 0.0f;

		if ( IsActive() && hsm_ct1_purge.GetBool() )
		{
			for ( int i = 0; i < ARRAYSIZE( m_iWeights ); i++ )
				m_iWeights[i] = 0;
		}
	}

	virtual void ClientDisconnect( CHidden_Player *pPlayer )
	{
		m_iWeights[pPlayer->entindex()] = 0;
	}

	// Saved just before the next round spawns everyone, which clears weighting.
	virtual void RoundEnd( void )
	{
		if ( IsActive() )
			m_flSaveTime = gpGlobals->curtime + mp_chattime.GetFloat() - 0.1f;
	}

	virtual void Think( void )
	{
		if ( m_flSaveTime <= 0.0f || gpGlobals->curtime < m_flSaveTime )
			return;

		m_flSaveTime = 0.0f;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || !pPlayer->IsConnected() )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_IRIS && !pPlayer->GetNoHidden() )
			{
				m_iWeights[i] = MAX( pPlayer->GetWeighting(), 0 );
			}
			else
			{
				m_iWeights[i] = 0;
				pPlayer->AddWeighting( -pPlayer->GetWeighting() );
			}
		}
	}

	virtual void RoundStart( void )
	{
		if ( !IsActive() )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsConnected() && pPlayer->GetTeamNumber() == TEAM_IRIS && !pPlayer->GetNoHidden() && m_iWeights[i] > 0 )
				pPlayer->AddWeighting( RoundFloatToInt( hsm_ct1_keep.GetFloat() * m_iWeights[i] ) - pPlayer->GetWeighting() );
		}
	}

private:
	// Weighted selection, and something to keep.
	bool IsActive( void ) const { return hsm_ct1.GetBool() && hdn_selectmethod.GetInt() == 0 && hsm_ct1_keep.GetFloat() >= 0.000001f; }

	int m_iWeights[MAX_PLAYERS + 1];
	float m_flSaveTime;
};

static CHiddenPluginCarryTheOne s_CarryTheOne;
