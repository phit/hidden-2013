//========= Hidden: Source =====================================================//
//
// Purpose: Radio Taunts (Paegus's hsm_radiotaunts 1.0.2): a Hidden taunting
//			next to a corpse is heard over the marines' radio, and can taunt by
//			itself after a kill. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=79147
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "engine/IEngineSound.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define RPT_MAX_RANGE		256.0f	// from the Hidden's eyes to a corpse
#define RPT_DEATH_DELAY		0.1f	// the plugin checked the victim stayed dead
#define RADIO_FRESHMEAT		6
#define RADIO_YOUARENEXT	7

static ConVar hsm_rpt( "hsm_rpt", "0", FCVAR_NOTIFY, "Do the Hidden's taunts near a corpse go out over the IRIS radio? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_rpt_chat( "hsm_rpt_chat", "1", 0, "Successfully radioed taunts appear in global chat? 0: No, 1:Yes.", true, 0.0f, true, 1.0f );
static ConVar hsm_rpt_auto( "hsm_rpt_auto", "0", 0, "Chance for Hidden to automatically taunt when he kills. 0: Never. 1: Always.", true, 0.0f, true, 1.0f );
static ConVar hsm_rpt_last( "hsm_rpt_last", "0.333333", 0, "Alternate chance for Hidden to automatically taunt on the 2nd to last possible kill. 0: No alternate. 1: Always.", true, 0.0f, true, 1.0f );

class CHiddenPluginRadioTaunts : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		for ( int i = 1; i <= 8; i++ )
			enginesound->PrecacheSound( UTIL_VarArgs( "player/iris/IRIS-617radiotaunts%02i.mp3", i ), true );
		m_Taunts.RemoveAll();
	}

	virtual void HiddenTaunt( CHidden_Player *pPlayer, const char *pszWave )
	{
		if ( !hsm_rpt.GetBool() || !Q_stristr( pszWave, "617-radiotaunts" ) )
			return;

		// The take's number, the last character before the extension.
		const char *pszExt = Q_stristr( pszWave, ".mp3" );
		if ( !pszExt || pszExt == pszWave )
			return;
		const char chTake = pszExt[-1];

		const Vector vecEyes = pPlayer->EyePosition();
		float flClosest = RPT_MAX_RANGE + 0.1f;
		for ( CBaseEntity *pCorpse = gEntList.FindEntityByClassname( NULL, "corpse_ragdoll" ); pCorpse; pCorpse = gEntList.FindEntityByClassname( pCorpse, "corpse_ragdoll" ) )
			flClosest = MIN( flClosest, ( pCorpse->GetAbsOrigin() - vecEyes ).Length() );

		if ( flClosest > RPT_MAX_RANGE )
			return;

		if ( hsm_rpt_chat.GetBool() )
		{
			switch ( chTake )
			{
			case '1': case '2':				engine->ClientCommand( pPlayer->edict(), "say You're Next!\n" ); break;
			case '3': case '7': case '8':	engine->ClientCommand( pPlayer->edict(), "say Fresh Meat!\n" ); break;
			case '4': case '5': case '6':	engine->ClientCommand( pPlayer->edict(), "say I'm coming for you!\n" ); break;
			}
		}

		// Living marines hear the radio version, quieter the further the corpse.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pMarine = UTIL_PlayerByIndex( i );
			if ( !pMarine || !pMarine->IsAlive() || pMarine->GetTeamNumber() != TEAM_IRIS )
				continue;

			CSingleUserRecipientFilter filter( pMarine );
			EmitSound_t params;
			params.m_pSoundName = UTIL_VarArgs( "player/iris/IRIS-617radiotaunts0%c.mp3", chTake );
			params.m_SoundLevel = SNDLVL_NORM;
			params.m_nChannel = CHAN_AUTO;
			params.m_flVolume = 1.0f - flClosest / RPT_MAX_RANGE;
			params.m_nFlags = SND_CHANGE_VOL;
			CBaseEntity::EmitSound( filter, pMarine->entindex(), params );
		}
	}

	// A kill by the Hidden may make it taunt a moment later.
	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		if ( !hsm_rpt.GetBool() || pVictim->GetTeamNumber() != TEAM_IRIS || pVictim->GetHealth() > 0 )
			return;

		CHidden_Player *pHidden = ToHiddenPlayer( info.GetAttacker() );
		if ( !pHidden || pHidden->GetTeamNumber() != TEAM_HIDDEN )
			return;

		const int iCount = CountLivingMarines();
		const float flAuto = hsm_rpt_auto.GetFloat(), flLast = hsm_rpt_last.GetFloat();
		bool bTaunt = false;
		if ( flAuto > 0.0f )
			bTaunt = iCount > 0 && random->RandomInt( 1, RoundFloatToInt( 1.0f / flAuto ) ) == 1;
		else if ( flLast > 0.0f )
			bTaunt = iCount < 2 && random->RandomInt( 1, RoundFloatToInt( 1.0f / flLast ) ) == 1;

		if ( bTaunt )
		{
			Taunt_t taunt = { pHidden, gpGlobals->curtime + RPT_DEATH_DELAY };
			m_Taunts.AddToTail( taunt );
		}
	}

	virtual void Think( void )
	{
		for ( int i = m_Taunts.Count() - 1; i >= 0; i-- )
		{
			if ( gpGlobals->curtime < m_Taunts[i].flTime )
				continue;

			CHidden_Player *pHidden = ToHiddenPlayer( m_Taunts[i].hHidden.Get() );
			m_Taunts.Remove( i );
			if ( !pHidden || !pHidden->IsAlive() )
				continue;

			// One marine left: "You're next". More: either.
			pHidden->Radio( CountLivingMarines() < 2 ? RADIO_YOUARENEXT : random->RandomInt( RADIO_FRESHMEAT, RADIO_YOUARENEXT ) );
		}
	}

private:
	static int CountLivingMarines( void )
	{
		int iCount = 0;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
				iCount++;
		}
		return iCount;
	}

	struct Taunt_t
	{
		EHANDLE hHidden;
		float flTime;
	};

	CUtlVector< Taunt_t > m_Taunts;
};

static CHiddenPluginRadioTaunts s_RadioTaunts;
