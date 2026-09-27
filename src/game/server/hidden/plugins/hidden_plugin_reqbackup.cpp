//========= Hidden: Source =====================================================//
//
// Purpose: Request Backup (Paegus's hsm_reqbackup 1.0.2): a badly hurt marine
//			calls for backup. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78957
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "engine/IEngineSound.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The plugin used radio 24381, which no client knows; 8 is the unused radio slot its Team Aura
// plugin names HDN_RADIO_BACKUP. Either way the call only flashes the caller on the radar.
#define RADIO_BACKUP	8

static ConVar hsm_reqbackup( "hsm_reqbackup", "0", FCVAR_NOTIFY, "Do hurt IRIS call for backup? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_reqbackup_threshold( "hsm_reqbackup_threshold", "37", FCVAR_NOTIFY, "Hitpoint threshold for IRIS to call for backup. 1~99", true, 1.0f, true, 99.0f );

class CHiddenPluginReqBackup : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		Reset();

		CBaseEntity::PrecacheScriptSound( "IRIS.RequestBackup" );
		for ( int i = 1; i <= 4; i++ )
			enginesound->PrecacheSound( UTIL_VarArgs( "player/iris/IRIS-backup%02i.wav", i ), true );
	}

	virtual void RoundStart( void ) { Reset(); }

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		if ( !hsm_reqbackup.GetBool() || pVictim->GetTeamNumber() != TEAM_IRIS )
			return;

		// Not from the world or another marine.
		CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
		if ( !pAttacker || pAttacker->GetTeamNumber() == TEAM_IRIS )
			return;

		const int iHealth = pVictim->GetHealth();
		if ( iHealth <= 0 || iHealth >= hsm_reqbackup_threshold.GetInt() || m_bCalled[pVictim->entindex()] || IsLastMarine( pVictim ) )
			return;

		// Through the radio, so hdn_radio_limit applies as it did to the plugin's radio command.
		if ( !pVictim->Radio( RADIO_BACKUP ) )
			return;

		m_bCalled[pVictim->entindex()] = true;

		// What the plugin meant to do next: its handler checked the caller was the Hidden, so it never
		// got here.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
			{
				CSingleUserRecipientFilter filter( pPlayer );
				CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "IRIS.RequestBackup" );
			}
		}

		engine->ClientCommand( pVictim->edict(), "say_team Backup! I need Backup!\n" );

		// The Hidden hears the marine's own voice.
		CRecipientFilter filter;
		filter.AddRecipientsByTeam( GetGlobalTeam( TEAM_HIDDEN ) );
		const Vector vecEyes = pVictim->EyePosition();
		EmitSound_t params;
		params.m_pSoundName = UTIL_VarArgs( "player/iris/IRIS-backup%02i.wav", random->RandomInt( 1, 4 ) );
		params.m_SoundLevel = SNDLVL_NORM;
		params.m_nChannel = CHAN_AUTO;
		params.m_flVolume = VOL_NORM;
		params.m_pOrigin = &vecEyes;
		CBaseEntity::EmitSound( filter, pVictim->entindex(), params );
	}

private:
	void Reset( void )
	{
		for ( int i = 0; i < ARRAYSIZE( m_bCalled ); i++ )
			m_bCalled[i] = false;
	}

	bool IsLastMarine( CHidden_Player *pVictim )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer != pVictim && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
				return false;
		}
		return true;
	}

	bool m_bCalled[MAX_PLAYERS + 1];
};

static CHiddenPluginReqBackup s_ReqBackup;
