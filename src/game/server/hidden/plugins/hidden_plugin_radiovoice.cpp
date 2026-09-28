//========= Hidden: Source =====================================================//
//
// Purpose: Radio to Voice (Paegus's hsm_radiovoice 1.0.2): the Hidden hears
//			the marines' radio calls from their headsets. See
//			docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?p=699598
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "engine/IEngineSound.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

static ConVar hsm_radiovoice( "hsm_radiovoice", "0", FCVAR_NOTIFY, "Does the Hidden hear IRIS radio calls from their headsets? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

// The calls, by radio number, and how many takes each has.
static const struct { int iMessage; const char *pszSound; int iTakes; } s_RadioVoices[] =
{
	{ 0, "agentdown", 4 },
	{ 1, "sighted", 10 },
	{ 2, "affirmative", 6 },
	{ 3, "requestingammo", 7 },
	{ 4, "reportin", 5 },
	{ 10, "reportingin", 10 },
};

class CHiddenPluginRadioVoice : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		for ( int i = 0; i < ARRAYSIZE( s_RadioVoices ); i++ )
		{
			for ( int j = 1; j <= s_RadioVoices[i].iTakes; j++ )
				enginesound->PrecacheSound( UTIL_VarArgs( "player/iris/IRIS-%s%02i.wav", s_RadioVoices[i].pszSound, j ), true );
		}
	}

	// The plugin checked the caller was the Hidden, who never sends iris_radio, so it did nothing;
	// this is what it was written to do.
	virtual void Radio( CHidden_Player *pCaller, int iMessage )
	{
		if ( !hsm_radiovoice.GetBool() || pCaller->GetTeamNumber() != TEAM_IRIS )
			return;

		for ( int i = 0; i < ARRAYSIZE( s_RadioVoices ); i++ )
		{
			if ( s_RadioVoices[i].iMessage != iMessage )
				continue;

			CRecipientFilter filter;
			filter.AddRecipientsByTeam( GetGlobalTeam( TEAM_HIDDEN ) );
			const Vector vecEyes = pCaller->EyePosition();
			EmitSound_t params;
			params.m_pSoundName = UTIL_VarArgs( "player/iris/IRIS-%s%02i.wav", s_RadioVoices[i].pszSound, random->RandomInt( 1, s_RadioVoices[i].iTakes ) );
			params.m_SoundLevel = SNDLVL_80dB;	// the plugin's SNDLEVEL_MINIBIKE, as loud as a marine's taunt
			params.m_nChannel = CHAN_AUTO;
			params.m_flVolume = VOL_NORM;
			params.m_pOrigin = &vecEyes;
			CBaseEntity::EmitSound( filter, pCaller->entindex(), params );
			return;
		}
	}
};

static CHiddenPluginRadioVoice s_RadioVoice;
