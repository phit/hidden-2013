//========= Hidden: Source =====================================================//
//
// Purpose: Team Aura / Tracer / Tracker (Paegus's hsm_teamaura 1.1.1): Beta 6's
//			proposed team aura, radio call sprites and radio alarms. See
//			docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=85463
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_sonic_alarm.h"
#include "itempents.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define TA_MAX_ALARMS			4			// alarms showing at once, across the map
#define TA_SPRITE_UPDATE		0.1f
#define TA_MIN_SPRITE_SIZE		0.05f
#define TA_MIN_ALARM_RANGE		128.0f		// no second alarm this close to one going
#define TA_RANGE_MAX			16384.0f
#define TA_AURA_SIZE			0.125f
#define TA_RADIO_SIZE			0.5f

#define RADIO_REQUESTAMMO		3
#define RADIO_ALARM				9			// a radio slot nothing uses, for the alarm's sprite
#define RADIO_REPORTINGIN		10
#define RADIO_MAX				10

static ConVar hsm_ta( "hsm_ta", "0", FCVAR_NOTIFY, "Team aura, radio sprites and radio alarms? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_ta_anyone( "hsm_ta_anyone", "1", 0, "Can anyone see Hidden sprites when dead? 0: No, Admins only. 1: Yes.", true, 0.0f, true, 1.0f );
static ConVar hsm_ta_irisaura( "hsm_ta_irisaura", "1", 0, "Draw team-sprites for IRIS? 0: Never. 1: When +AURA is held. 2: Only spectators.", true, 0.0f, true, 2.0f );
static ConVar hsm_ta_radioalarm( "hsm_ta_radioalarm", "1", 0, "Enable radio-alarms? 0: No, Sonic alarms. 1: Yes, alarms transmit over radio instead.", true, 0.0f, true, 1.0f );
static ConVar hsm_ta_radiosprite( "hsm_ta_radiosprite", "1", 0, "Draw emitter sprites for IRIS & Alarm radio signals? 0: No, 1: Yes.", true, 0.0f, true, 1.0f );

// Per team (IRIS, Hidden): low, medium and high health.
static const char *s_pszAuraSprites[2][3] =
{
	{ "sprites/hidden2013/iris_low.vmt", "sprites/hidden2013/iris_med.vmt", "sprites/hidden2013/iris_high.vmt" },
	{ "sprites/hidden2013/hdn_low.vmt", "sprites/hidden2013/hdn_med.vmt", "sprites/hidden2013/hdn_high.vmt" },
};
static const int s_iAuraBrightness[2] = { 255, 128 };
static const int s_iZOffset[2] = { 9, -12 };		// above the eyes for marines, below for the Hidden
static const int s_iLowHealth[2] = { 37, 20 };		// red at or below
static const int s_iMedHealth[2] = { 74, 60 };		// orange at or below

// By radio message; 5-7 are taunts, which aren't radio calls.
static const char *s_pszRadioSprites[RADIO_MAX + 1] =
{
	"sprites/hidden2013/radio.vmt",			// agent down
	"sprites/hidden2013/radio.vmt",			// subject sighted
	"sprites/hidden2013/radio.vmt",			// affirmative
	"sprites/hidden2013/ammorequest.vmt",	// I need ammo
	"sprites/hidden2013/radio.vmt",			// report in
	NULL, NULL, NULL,
	"sprites/hidden2013/radio.vmt",			// backup
	"sprites/hidden2013/sonic.vmt",			// the alarm
	"sprites/hidden2013/radio.vmt",			// reporting in
};
static const float s_flRadioTimeouts[RADIO_MAX + 1] = { 2.0f, 4.0f, 2.0f, 60.0f, 2.0f, 0.0f, 0.0f, 0.0f, 4.0f, 5.7f, 4.0f };

class CHiddenPluginTeamAura : public CHiddenPlugin
{
public:
	CHiddenPluginTeamAura()
	{
		m_bInRound = true;
		m_flNextUpdate = 0.0f;
	}

	virtual void LevelInit( void )
	{
		for ( int i = 0; i < 2; i++ )
		{
			for ( int j = 0; j < 3; j++ )
				m_iAuraSprites[i][j] = CBaseEntity::PrecacheModel( s_pszAuraSprites[i][j] );
		}

		for ( int i = 0; i <= RADIO_MAX; i++ )
			m_iRadioSprites[i] = s_pszRadioSprites[i] ? CBaseEntity::PrecacheModel( s_pszRadioSprites[i] ) : 0;

		m_bInRound = true;
		m_flNextUpdate = 0.0f;
		m_Traces.Purge();
	}

	virtual void RoundStart( void ) { m_bInRound = true; }
	virtual void RoundEnd( void ) { m_bInRound = false; }

	virtual void Radio( CHidden_Player *pPlayer, int iMessage )
	{
		if ( !hsm_ta.GetBool() || !hsm_ta_radiosprite.GetBool() )
			return;

		if ( iMessage < 0 || iMessage > RADIO_MAX || !s_pszRadioSprites[iMessage] )
			iMessage = RADIO_REPORTINGIN;

		AddTrace( pPlayer, iMessage );
	}

	virtual bool AlarmTriggered( CHiddenSonicAlarm *pAlarm, CBaseEntity *pBreaker, CRecipientFilter &filter )
	{
		if ( !hsm_ta.GetBool() || !hsm_ta_radioalarm.GetBool() )
			return true;

		// Players walk through a tripped alarm.
		pAlarm->SetCollisionGroup( COLLISION_GROUP_DEBRIS );

		if ( !m_bInRound )
			return false;

		if ( hsm_ta_radiosprite.GetBool() )
		{
			int iActive = 0;
			for ( int i = 0; i < m_Traces.Count(); i++ )
			{
				CBaseEntity *pOther = m_Traces[i].hSource.Get();
				if ( m_Traces[i].iMessage != RADIO_ALARM || !pOther )
					continue;

				if ( pOther == pAlarm || ( pOther->GetAbsOrigin() - pAlarm->GetAbsOrigin() ).Length() <= TA_MIN_ALARM_RANGE )
					return false;

				iActive++;
			}

			if ( iActive >= TA_MAX_ALARMS )
				return false;

			AddTrace( pAlarm, RADIO_ALARM );
		}

		// Over the radio: only living marines hear it.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && ( !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_IRIS ) )
				filter.RemoveRecipient( pPlayer );
		}

		return true;
	}

	virtual void Think( void )
	{
		if ( !hsm_ta.GetBool() )
		{
			m_Traces.Purge();
			return;
		}

		if ( gpGlobals->curtime < m_flNextUpdate )
			return;

		m_flNextUpdate = gpGlobals->curtime + TA_SPRITE_UPDATE;

		if ( !m_bInRound )
		{
			m_Traces.Purge();
			return;
		}

		DrawAuraSprites();

		for ( int i = m_Traces.Count() - 1; i >= 0; i-- )
		{
			if ( !DrawTraceSprites( m_Traces[i] ) || --m_Traces[i].iPings <= 0 )
				m_Traces.Remove( i );
		}
	}

private:
	struct Trace_t
	{
		EHANDLE hSource;	// a player's radio call, or an alarm
		int iMessage;
		int iPings;			// updates left
	};

	void AddTrace( CBaseEntity *pSource, int iMessage )
	{
		Trace_t trace;
		trace.hSource = pSource;
		trace.iMessage = iMessage;
		trace.iPings = RoundFloatToInt( s_flRadioTimeouts[iMessage] / TA_SPRITE_UPDATE );
		m_Traces.AddToTail( trace );
	}

	static bool IsInAura( CHidden_Player *pPlayer )
	{
		if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
			return ( pPlayer->m_nButtons & IN_GRENADE1 ) != 0;
		return pPlayer->IsViewing();
	}

	int GetHealthRange( CBasePlayer *pPlayer, int iTeam ) const
	{
		const int iHealth = pPlayer->GetHealth();
		if ( iHealth <= s_iLowHealth[iTeam] )
			return 0;
		if ( iHealth <= s_iMedHealth[iTeam] )
			return 1;
		return 2;
	}

	// The sprite goes where a line from the viewer to the source first hits, sized by how far along
	// that is, so it shows through walls.
	static bool PlaceSprite( CBasePlayer *pViewer, Vector &vecSprite, float &flScale )
	{
		const Vector vecEyes = pViewer->EyePosition();
		const float flRange = ( vecSprite - vecEyes ).Length();
		if ( flRange >= TA_RANGE_MAX || flRange <= 0.0f )
			return false;

		trace_t tr;
		UTIL_TraceLine( vecEyes, vecSprite, MASK_ALL, pViewer, COLLISION_GROUP_NONE, &tr );
		vecSprite = tr.endpos;
		flScale = ( tr.endpos - vecEyes ).Length() / flRange;
		return true;
	}

	static void SendSprite( CBasePlayer *pViewer, const Vector &vecPos, int iSprite, float flSize, int iBrightness )
	{
		CSingleUserRecipientFilter filter( pViewer );
		te->GlowSprite( filter, 0.0f, &vecPos, iSprite, TA_SPRITE_UPDATE - 0.05f, MAX( flSize, TA_MIN_SPRITE_SIZE ), iBrightness );
	}

	static bool IsAdmin( CBasePlayer *pPlayer )
	{
		// No admin list here: the listen server's host.
		return !engine->IsDedicatedServer() && pPlayer->entindex() == 1;
	}

	void DrawAuraSprites( void )
	{
		const int iIRISAura = hsm_ta_irisaura.GetInt();

		for ( int iSource = 1; iSource <= gpGlobals->maxClients; iSource++ )
		{
			CHidden_Player *pSource = ToHiddenPlayer( UTIL_PlayerByIndex( iSource ) );
			if ( !pSource || !pSource->IsAlive() )
				continue;

			const int iSourceTeam = pSource->GetTeamNumber();
			if ( iSourceTeam != TEAM_IRIS && iSourceTeam != TEAM_HIDDEN )
				continue;

			const int iTeam = iSourceTeam - TEAM_IRIS;
			Vector vecSource = pSource->EyePosition();
			vecSource.z += s_iZOffset[iTeam];
			const int iSprite = m_iAuraSprites[iTeam][GetHealthRange( pSource, iTeam )];

			CRecipientFilter spectators;
			for ( int iTarget = 1; iTarget <= gpGlobals->maxClients; iTarget++ )
			{
				CHidden_Player *pTarget = ToHiddenPlayer( UTIL_PlayerByIndex( iTarget ) );
				if ( !pTarget || pTarget == pSource || pTarget->IsBot() || !pTarget->IsConnected() )
					continue;

				if ( !pTarget->IsAlive() )
				{
					// The dead see the marines with any IRIS aura setting, the Hidden if allowed, and not
					// the one they're watching through. (The plugin's check let everyone see the Hidden
					// whenever hsm_ta_irisaura was on.)
					const bool bAllowed = ( iSourceTeam == TEAM_IRIS ) ? ( iIRISAura > 0 ) : ( hsm_ta_anyone.GetBool() || IsAdmin( pTarget ) );
					if ( bAllowed && pTarget->GetObserverTarget() != pSource )
						spectators.AddRecipient( pTarget );
					continue;
				}

				if ( pTarget->GetTeamNumber() != iSourceTeam )
					continue;

				// A Hidden sees the others (OverRun) out of its aura; a marine holding +aura sees marines.
				const bool bShow = ( iSourceTeam == TEAM_HIDDEN ) ? !IsInAura( pTarget ) : ( iIRISAura == 1 && IsInAura( pTarget ) );
				if ( !bShow )
					continue;

				Vector vecSprite = vecSource;
				float flScale;
				if ( PlaceSprite( pTarget, vecSprite, flScale ) )
					SendSprite( pTarget, vecSprite, iSprite, flScale * TA_AURA_SIZE, s_iAuraBrightness[0] );
			}

			if ( spectators.GetRecipientCount() )
				te->GlowSprite( spectators, 0.0f, &vecSource, iSprite, TA_SPRITE_UPDATE - 0.05f, TA_AURA_SIZE, s_iAuraBrightness[iTeam] );
		}
	}

	// Returns false once the trace is over.
	bool DrawTraceSprites( const Trace_t &trace )
	{
		CBaseEntity *pSource = trace.hSource.Get();
		if ( !pSource )
			return false;

		int iSourceTeam = TEAM_UNASSIGNED;
		Vector vecSource;
		CHidden_Player *pPlayer = ToHiddenPlayer( pSource );
		if ( pPlayer )
		{
			// Until the caller dies, or their ammo call is answered.
			if ( !pPlayer->IsAlive() || ( trace.iMessage == RADIO_REQUESTAMMO && !pPlayer->IsRequestingAmmo() ) )
				return false;

			iSourceTeam = pPlayer->GetTeamNumber();
			if ( iSourceTeam != TEAM_IRIS && iSourceTeam != TEAM_HIDDEN )
				return false;

			vecSource = pPlayer->EyePosition();
			vecSource.z += s_iZOffset[iSourceTeam - TEAM_IRIS];
		}
		else
		{
			vecSource = pSource->GetAbsOrigin();
		}

		const bool bAlarm = ( trace.iMessage == RADIO_ALARM );
		for ( int iTarget = 1; iTarget <= gpGlobals->maxClients; iTarget++ )
		{
			CHidden_Player *pTarget = ToHiddenPlayer( UTIL_PlayerByIndex( iTarget ) );
			if ( !pTarget || pTarget == pSource || pTarget->IsBot() || !pTarget->IsAlive() )
				continue;

			const int iTargetTeam = pTarget->GetTeamNumber();
			if ( iTargetTeam != iSourceTeam && !( bAlarm && iTargetTeam == TEAM_IRIS ) )
				continue;

			Vector vecSprite = vecSource;
			float flScale;
			if ( !PlaceSprite( pTarget, vecSprite, flScale ) )
				continue;

			// In the aura a caller shows their health instead.
			if ( !bAlarm && pPlayer && IsInAura( pTarget ) )
			{
				const int iTeam = iSourceTeam - TEAM_IRIS;
				SendSprite( pTarget, vecSprite, m_iAuraSprites[iTeam][GetHealthRange( pPlayer, iTeam )], flScale * TA_AURA_SIZE, s_iAuraBrightness[0] );
			}
			else
			{
				SendSprite( pTarget, vecSprite, m_iRadioSprites[trace.iMessage], flScale * TA_RADIO_SIZE, s_iAuraBrightness[0] );
			}
		}

		return true;
	}

	bool m_bInRound;
	float m_flNextUpdate;
	int m_iAuraSprites[2][3];
	int m_iRadioSprites[RADIO_MAX + 1];
	CUtlVector<Trace_t> m_Traces;
};

static CHiddenPluginTeamAura s_TeamAura;
