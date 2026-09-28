//========= Hidden: Source =====================================================//
//
// Purpose: Radio Trip Alarm (Paegus's hsm_radioalarm 1.1.1): a tripped sonic
//			alarm sounds over the radio for everyone but the Hidden, with a
//			marker on their HUD. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78954
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_sonic_alarm.h"
#include "itempents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define RTA_SPRITE			"vgui/hud/hdn_retrieve_icon.vmt"	// upside down, as the plugin said
#define RTA_SPRITE_LIFE		6.0f
#define RTA_SOUND_TIME		2.5f	// the alarm is stopped after this

static ConVar hsm_radioalarm( "hsm_radioalarm", "0", FCVAR_NOTIFY, "Do tripped sonic alarms sound over the radio, for everyone but the Hidden? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_rta_hud( "hsm_rta_hud", "1", 0, "Radio Trip Alarm HUD indicator. 0: Disable. 1: Enable", true, 0.0f, true, 1.0f );

class CHiddenPluginRadioAlarm : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		m_iSprite = CBaseEntity::PrecacheModel( RTA_SPRITE );
		m_Silence.RemoveAll();
	}

	virtual bool AlarmTriggered( CHiddenSonicAlarm *pAlarm, CBaseEntity *pBreaker, CRecipientFilter &filter )
	{
		if ( !hsm_radioalarm.GetBool() )
			return true;

		// Players walk through a tripped alarm.
		pAlarm->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

		// Everyone but the Hidden hears it, wherever they are.
		filter.RemoveAllRecipients();
		CRecipientFilter others;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->GetTeamNumber() != TEAM_HIDDEN )
			{
				filter.AddRecipient( pPlayer );
				others.AddRecipient( pPlayer );
			}
		}

		if ( hsm_rta_hud.GetBool() )
		{
			const Vector vecPos = pAlarm->GetAbsOrigin();
			te->GlowSprite( others, 0.0f, &vecPos, m_iSprite, RTA_SPRITE_LIFE, 0.5f, 255 );
		}

		Silence_t silence = { pAlarm, gpGlobals->curtime + RTA_SOUND_TIME };
		m_Silence.AddToTail( silence );
		return true;
	}

	virtual void Think( void )
	{
		for ( int i = m_Silence.Count() - 1; i >= 0; i-- )
		{
			if ( gpGlobals->curtime < m_Silence[i].flTime )
				continue;

			CBaseEntity *pAlarm = m_Silence[i].hAlarm.Get();
			if ( pAlarm )
				pAlarm->StopSound( "Weapon_Sonic.Alarm" );
			m_Silence.Remove( i );
		}
	}

private:
	struct Silence_t
	{
		EHANDLE hAlarm;
		float flTime;
	};

	int m_iSprite;
	CUtlVector< Silence_t > m_Silence;
};

static CHiddenPluginRadioAlarm s_RadioAlarm;
