//========= Hidden: Source =====================================================//
//
// Purpose: Friendly Fire Radio (Paegus's hsm_ffire 1.0.3): a marine hurt by a
//			teammate calls it out over the radio. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?p=699591
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "engine/IEngineSound.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

// The plugin's own radio number, which no client plays anything for; it only flashes the caller on
// the radar.
#define RADIO_FRIENDLYFIRE	15328
#define FFIRE_CHAT_QUIET	0.75f	// sm_flood_time, the plugin's gap between named call-outs

static ConVar hsm_ffire( "hsm_ffire", "0", FCVAR_NOTIFY, "Do IRIS hurt by a teammate call it out? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_ffire_named( "hsm_ffire_named", "1.0", 0, "Does the victim name their attacker? 0: No, 1: Yes.", true, 0.0f, true, 1.0f );

class CHiddenPluginFFire : public CHiddenPlugin
{
public:
	CHiddenPluginFFire() { m_flChatQuiet = 0.0f; }

	virtual void LevelInit( void )
	{
		m_flChatQuiet = 0.0f;

		CBaseEntity::PrecacheScriptSound( "IRIS.FriendlyFireWarning" );
		for ( int i = 1; i <= 5; i++ )
			enginesound->PrecacheSound( UTIL_VarArgs( "player/iris/IRIS-ff%02i.wav", i ), true );
	}

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		if ( !hsm_ffire.GetBool() || gpGlobals->curtime < m_flChatQuiet )
			return;

		CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
		if ( !pAttacker || pAttacker == pVictim || pAttacker->GetTeamNumber() != TEAM_IRIS || pVictim->GetTeamNumber() != TEAM_IRIS )
			return;

		if ( hsm_ffire_named.GetBool() )
		{
			// The victim's client runs this, so the name mustn't be able to end the command.
			char szName[MAX_PLAYER_NAME_LENGTH];
			Q_strncpy( szName, pAttacker->GetPlayerName(), sizeof( szName ) );
			for ( char *pch = szName; *pch; pch++ )
			{
				if ( *pch == ';' || *pch == '"' || *pch == '\n' || *pch == '\r' )
					*pch = ' ';
			}
			engine->ClientCommand( pVictim->edict(), "say_team \"Watch your fire, %s!\"\n", szName );
			m_flChatQuiet = gpGlobals->curtime + FFIRE_CHAT_QUIET;
		}

		pVictim->Radio( RADIO_FRIENDLYFIRE );
	}

	virtual void Radio( CHidden_Player *pCaller, int iMessage )
	{
		if ( !hsm_ffire.GetBool() || iMessage != RADIO_FRIENDLYFIRE || pCaller->GetTeamNumber() != TEAM_IRIS )
			return;

		if ( !hsm_ffire_named.GetBool() )
			engine->ClientCommand( pCaller->edict(), "say_team Watch your fire!\n" );

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
			{
				CSingleUserRecipientFilter filter( pPlayer );
				CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "IRIS.FriendlyFireWarning" );
			}
		}

		// The Hidden hears the victim's own voice. The plugin sent it to one Hidden; OverRun has more.
		CRecipientFilter filter;
		filter.AddRecipientsByTeam( GetGlobalTeam( TEAM_HIDDEN ) );
		const Vector vecEyes = pCaller->EyePosition();
		EmitSound_t params;
		params.m_pSoundName = UTIL_VarArgs( "player/iris/IRIS-ff%02i.wav", random->RandomInt( 1, 5 ) );
		params.m_SoundLevel = SNDLVL_TALKING;
		params.m_nChannel = CHAN_AUTO;
		params.m_flVolume = VOL_NORM;
		params.m_pOrigin = &vecEyes;
		CBaseEntity::EmitSound( filter, pCaller->entindex(), params );
	}

private:
	float m_flChatQuiet;
};

static CHiddenPluginFFire s_FFire;
