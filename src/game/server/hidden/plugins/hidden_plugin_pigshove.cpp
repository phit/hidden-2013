//========= Hidden: Source =====================================================//
//
// Purpose: Beta 5 PigShove (Paegus's hsm_pigshove 1.1.5): the pigstick shoves
//			marines until the Hidden is badly hurt. See docs/spec/plugins.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_cvars.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SHOVE_HEALTH_BARRIER	20
#define SHOVE_SCALE_LATERAL		0.4f
#define SHOVE_SCALE_VERTICAL	0.7f
#define SHOVE_BOOST_VERTICAL	20.0f
#define SHOVE_DAMAGE			925.0f		// the pigstick's

#define RAGE_NOTIFY_DELAY		0.5f
#define RAGE_NOTIFY_BUFFER		20.0f
#define RAGE_ALERT_DELAY		1.0f

static ConVar hsm_pigshove( "hsm_pigshove", "0", FCVAR_NOTIFY, "Does the pigstick shove marines instead? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_pigstick( "hsm_pigstick", "1", FCVAR_NOTIFY, "Does normal pigstick kick in when hidden is enraged? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );

class CHiddenPluginPigShove : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		RoundStart();
		m_flRageBufferEnd = 0.0f;
	}

	virtual void RoundStart( void )
	{
		m_bRageAnnounced = false;
		m_bShoved = false;
		m_flRageAlertTime = 0.0f;
		m_hRageNotify = NULL;
	}

	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
	{
		if ( !hsm_pigshove.GetBool() || !sv_pigstick.GetBool() )
			return true;

		// The pigstick: the knife's DMG_SLASH, over 100 (925, or twice that to the head, which the
		// plugin's check for exactly 925 missed).
		CHidden_Player *pHidden = ToHiddenPlayer( info.GetAttacker() );
		if ( !pHidden || pHidden->GetTeamNumber() != TEAM_HIDDEN || info.GetDamageType() != DMG_SLASH || info.GetDamage() <= 100.0f )
			return true;

		// Enraged: a normal pigstick.
		if ( pHidden->GetHealth() < SHOVE_HEALTH_BARRIER && hsm_pigstick.GetBool() )
		{
			if ( !m_bRageAnnounced )
			{
				HiddenPlugins_PrintCenter( pHidden, "< PS:%s >", random->RandomInt( 1, 2 ) == 1 ? "Rage" : "Pigstick" );
				m_bRageAnnounced = true;
				m_flRageAlertTime = gpGlobals->curtime + RAGE_ALERT_DELAY;
			}
			return true;
		}

		if ( !m_bShoved )
		{
			static const char *s_pszShoves[] = { "Shove", "Punt", "Push" };
			HiddenPlugins_PrintCenter( pHidden, "< PS:%s >", s_pszShoves[random->RandomInt( 0, ARRAYSIZE( s_pszShoves ) - 1 )] );
			m_bShoved = true;
		}

		// Along the Hidden's model angles (no pitch), raised 20 degrees.
		QAngle angShove = pHidden->GetAbsAngles();
		angShove[PITCH] = -angShove[PITCH];
		if ( angShove[PITCH] <= 90.0f - SHOVE_BOOST_VERTICAL )
			angShove[PITCH] += SHOVE_BOOST_VERTICAL;

		const float flPitch = DEG2RAD( angShove[PITCH] );
		const float flYaw = DEG2RAD( angShove[YAW] );
		Vector vecShove;
		vecShove.x = cos( flPitch ) * cos( flYaw ) * SHOVE_DAMAGE * SHOVE_SCALE_LATERAL;
		vecShove.y = cos( flPitch ) * sin( flYaw ) * SHOVE_DAMAGE * SHOVE_SCALE_LATERAL;
		vecShove.z = sin( flPitch ) * SHOVE_DAMAGE * SHOVE_SCALE_VERTICAL;
		pVictim->Teleport( NULL, NULL, &vecShove );

		UTIL_LogPrintf( "\"%s\" shoved \"%s\"\n", HiddenPlugins_LogName( pHidden ), HiddenPlugins_LogName( pVictim ) );

		// No damage, no hurt event.
		return false;
	}

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		// A Hidden hurt down to the barrier hears, shortly after, that the pigstick is back.
		if ( hsm_pigshove.GetBool() && sv_pigstick.GetBool() && hsm_pigstick.GetBool() && pVictim->GetTeamNumber() == TEAM_HIDDEN &&
			 pVictim->GetHealth() <= SHOVE_HEALTH_BARRIER && gpGlobals->curtime >= m_flRageBufferEnd && !m_hRageNotify.Get() )
		{
			m_hRageNotify = pVictim;
			m_flRageNotifyTime = gpGlobals->curtime + RAGE_NOTIFY_DELAY;
		}
	}

	virtual void Think( void )
	{
		CBasePlayer *pHidden = ToBasePlayer( m_hRageNotify.Get() );
		if ( pHidden && gpGlobals->curtime >= m_flRageNotifyTime )
		{
			m_hRageNotify = NULL;
			if ( pHidden->GetHealth() > 0 && gpGlobals->curtime >= m_flRageBufferEnd )
			{
				HiddenPlugins_PrintCenter( pHidden, "< RAGE MODE >" );
				HiddenPlugins_PrintChat( pHidden, "[PigShove] Rage Mode: Normal PS Enabled" );
				HiddenPlugins_PrintConsole( pHidden, "[PigShove] Rage Mode: Normal PS Enabled" );
				m_flRageBufferEnd = gpGlobals->curtime + RAGE_NOTIFY_BUFFER;
			}
		}

		if ( m_flRageAlertTime > 0.0f && gpGlobals->curtime >= m_flRageAlertTime )
		{
			m_flRageAlertTime = 0.0f;
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
				{
					CSingleUserRecipientFilter filter( pPlayer );
					CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "IRIS.RageAlert" );
				}
			}
		}
	}

private:
	bool m_bRageAnnounced;		// the first rage pigstick of the round
	bool m_bShoved;				// the first shove of the round
	float m_flRageAlertTime;	// marines hear IRIS.RageAlert then
	EHANDLE m_hRageNotify;		// a Hidden about to be told rage mode is on
	float m_flRageNotifyTime;
	float m_flRageBufferEnd;	// no rage mode message before this
};

static CHiddenPluginPigShove s_PigShove;
