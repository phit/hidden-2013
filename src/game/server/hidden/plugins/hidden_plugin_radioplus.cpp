//========= Hidden: Source =====================================================//
//
// Purpose: Radio+ (Paegus's hsm_radioplus 1.0.0): two more radio calls, radio 9
//			(Negative) and radio 11 (Secure Location). See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=199195
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "engine/IEngineSound.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define RADIO_NEGATIVE		9
#define RADIO_SECURE		11

static ConVar hsm_radioplus( "hsm_radioplus", "0", FCVAR_NOTIFY, "Add radio 9 (Negative) and radio 11 (Secure Location)? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

class CHiddenPluginRadioPlus : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		for ( int i = 1; i <= 4; i++ )
		{
			enginesound->PrecacheSound( UTIL_VarArgs( "player/iris/IRIS-negative%02i.wav", i ), true );
			enginesound->PrecacheSound( UTIL_VarArgs( "player/iris/IRIS-securearea%02i.wav", i ), true );
		}
	}

	virtual void Radio( CHidden_Player *pCaller, int iMessage )
	{
		if ( !hsm_radioplus.GetBool() || pCaller->GetTeamNumber() != TEAM_IRIS )
			return;

		const char *pszSound;
		if ( iMessage == RADIO_NEGATIVE )
		{
			pszSound = "negative";
			engine->ClientCommand( pCaller->edict(), "say_team Negative!\n" );
		}
		else if ( iMessage == RADIO_SECURE )
		{
			pszSound = "securearea";
			// The location comes from the map; keep it from ending the command on the caller's client.
			char szLocation[64];
			Q_strncpy( szLocation, pCaller->GetCurrentLocation(), sizeof( szLocation ) );
			for ( char *pch = szLocation; *pch; pch++ )
			{
				if ( *pch == ';' || *pch == '"' || *pch == '\n' || *pch == '\r' )
					*pch = ' ';
			}
			engine->ClientCommand( pCaller->edict(), "say_team \"Secure %s!\"\n", szLocation );
		}
		else
		{
			return;
		}

		char szSound[64];
		Q_snprintf( szSound, sizeof( szSound ), "player/iris/IRIS-%s%02i.wav", pszSound, random->RandomInt( 1, 4 ) );

		// The marines hear it over the radio.
		CRecipientFilter marines;
		marines.AddRecipientsByTeam( GetGlobalTeam( TEAM_IRIS ) );
		EmitSound_t radio;
		radio.m_pSoundName = szSound;
		radio.m_SoundLevel = SNDLVL_NORM;
		radio.m_nChannel = CHAN_AUTO;
		radio.m_flVolume = VOL_NORM;
		CBaseEntity::EmitSound( marines, SOUND_FROM_LOCAL_PLAYER, radio );

		// The Hidden hears the caller.
		CRecipientFilter hidden;
		hidden.AddRecipientsByTeam( GetGlobalTeam( TEAM_HIDDEN ) );
		const Vector vecEyes = pCaller->EyePosition();
		EmitSound_t voice;
		voice.m_pSoundName = szSound;
		voice.m_SoundLevel = SNDLVL_80dB;
		voice.m_nChannel = CHAN_AUTO;
		voice.m_flVolume = VOL_NORM;
		voice.m_pOrigin = &vecEyes;
		CBaseEntity::EmitSound( hidden, pCaller->entindex(), voice );
	}
};

static CHiddenPluginRadioPlus s_RadioPlus;
