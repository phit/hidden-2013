//========= Hidden: Source =====================================================//
//
// Purpose: Sonic mines (Ice's hdn_sonicmines 0.9.0): sonic alarms only go off
//			for the Hidden, and support marines can turn them into trip mines.
//			See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=163916
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_sonic_alarm.h"
#include "ammodef.h"
#include "explode.h"
#include "te_effect_dispatch.h"
#include "itempents.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define MINES_MAX_SOUNDING		4		// alarms sounding at once
#define MINES_SOUND_TIME		5.7f
#define MINES_ARM_RANGE			75.0f
#define MINES_BEAM_INTERVAL		4.0f
#define MINES_DAMAGE_DELAY		0.1f
#define MINES_NOTIFY_EVERY		7		// "one in every fifth", as the plugin counted it
#define MINES_EXPLOSION			400		// env_explosion's magnitude and radius

#define MINES_SOUND_ARM			"weapons/slam/mine_mode.wav"
#define MINES_SOUND_EXPLODE		"weapons/hegrenade/explode3.wav"
#define MINES_BEAM_SPRITE		"sprites/combineball_trail_red_1.vmt"
#define MINES_BEAM_HALO			"sprites/glow01.vmt"

static ConVar sm_sonicmines_enable( "sm_sonicmines_enable", "0", FCVAR_NOTIFY, "Enable/disable phys kill ranking", true, 0.0f, true, 1.0f );
static ConVar sm_sonicmines_hiddencanhear( "sm_sonicmines_hiddencanhear", "0.0", FCVAR_NOTIFY, "Is the sonic alarm sound heard by the hidden?", true, 0.0f, false, 0.0f );
static ConVar sm_sonicmines_tripmines( "sm_sonicmines_tripmines", "1.0", FCVAR_NOTIFY, "Enables the trip mine ability", true, 0.0f, false, 0.0f );
static ConVar sm_sonicmines_notify( "sm_sonicmines_notify", "1.0", FCVAR_NOTIFY, "Notify clients that alarms are hidden only every fifth time an alarm is activated", true, 0.0f, false, 0.0f );

struct ArmedAlarm_t
{
	CHandle<CHiddenSonicAlarm> hAlarm;
	EHANDLE hArmer;
	EHANDLE hFirstHit;			// what the beam first reached, when it was something other than a player
	bool bFirstHitKnown;
	float flNextBeam;
};

class CHiddenPluginSonicMines : public CHiddenPlugin
{
public:
	virtual void LevelInit( void )
	{
		m_bMapAllowed = Q_strnicmp( STRING( gpGlobals->mapname ), "ovr", 3 ) != 0;	// not made for OverRun
		m_iBeamSprite = CBaseEntity::PrecacheModel( MINES_BEAM_SPRITE );
		m_iBeamHalo = CBaseEntity::PrecacheModel( MINES_BEAM_HALO );
		CBaseEntity::PrecacheSound( MINES_SOUND_ARM );
		CBaseEntity::PrecacheSound( MINES_SOUND_EXPLODE );
		Reset();
	}

	virtual void RoundStart( void )
	{
		Reset();
		m_iNotifyCount = 0;

		if ( !IsActive() || !sm_sonicmines_tripmines.GetBool() )
			return;

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( IsSupport( pPlayer ) && GetSonics( pPlayer ) >= 3 )
				HiddenPlugins_PrintChat( pPlayer, "Type in chat tripmine to exchange a sonic alarm for a trip mine kit" );
		}
	}

	virtual void RoundEnd( void ) { Reset(); }

	virtual void PlayerSay( CHidden_Player *pPlayer, const char *pszText )
	{
		if ( !IsActive() || !sm_sonicmines_tripmines.GetBool() || !pPlayer->IsAlive() || pPlayer->GetTeamNumber() != TEAM_IRIS )
			return;

		if ( !Q_stricmp( pszText, "tripmine" ) )
			TakeKit( pPlayer );
		else if ( !Q_stricmp( pszText, "arm" ) )
			Arm( pPlayer );
	}

	virtual bool AlarmTriggered( CHiddenSonicAlarm *pAlarm, CBaseEntity *pBreaker, CRecipientFilter &filter )
	{
		if ( !IsActive() )
			return true;

		const bool bHidden = pBreaker && pBreaker->IsPlayer() && pBreaker->GetTeamNumber() == TEAM_HIDDEN;
		ArmedAlarm_t *pArmed = FindArmed( pAlarm );

		// An armed alarm goes off for the Hidden, or when something moves into its beam.
		if ( pArmed && ( bHidden || ( pBreaker && !pBreaker->IsPlayer() && pArmed->bFirstHitKnown && pBreaker != pArmed->hFirstHit.Get() ) ) )
		{
			Explode( pArmed );
			return false;
		}

		if ( !bHidden )
		{
			Notify();
			return false;
		}

		// At most four at a time.
		int iSounding = 0;
		for ( int i = m_Sounding.Count() - 1; i >= 0; i-- )
		{
			if ( !m_Sounding[i].hAlarm.Get() || gpGlobals->curtime >= m_Sounding[i].flEnd )
				m_Sounding.Remove( i );
			else if ( m_Sounding[i].hAlarm.Get() == pAlarm )
				return false;
			else
				iSounding++;
		}
		if ( iSounding >= MINES_MAX_SOUNDING )
			return false;

		Sounding_t sounding = { pAlarm, gpGlobals->curtime + MINES_SOUND_TIME };
		m_Sounding.AddToTail( sounding );

		// Marines hear it, the Hidden only if allowed.
		if ( !sm_sonicmines_hiddencanhear.GetBool() )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer && pPlayer->GetTeamNumber() != TEAM_IRIS )
					filter.RemoveRecipient( pPlayer );
			}
		}

		return true;
	}

	virtual void Think( void )
	{
		for ( int i = m_Armed.Count() - 1; i >= 0; i-- )
		{
			ArmedAlarm_t &armed = m_Armed[i];
			if ( !armed.hAlarm.Get() )
			{
				m_Armed.Remove( i );
				continue;
			}

			if ( gpGlobals->curtime >= armed.flNextBeam )
			{
				ShowBeam( armed );
				armed.flNextBeam = gpGlobals->curtime + MINES_BEAM_INTERVAL;
			}
		}

		// Blown alarms fall apart a moment after the blast.
		for ( int i = m_Blown.Count() - 1; i >= 0; i-- )
		{
			if ( gpGlobals->curtime < m_Blown[i].flTime )
				continue;

			CHiddenSonicAlarm *pAlarm = m_Blown[i].hAlarm.Get();
			if ( pAlarm )
			{
				CBaseEntity *pArmer = m_Blown[i].hArmer.Get();
				CTakeDamageInfo info( pAlarm, pArmer ? pArmer : pAlarm, 50.0f, DMG_GENERIC );
				pAlarm->TakeDamage( info );
				UTIL_Remove( pAlarm );
			}
			m_Blown.Remove( i );
		}
	}

private:
	struct Sounding_t
	{
		CHandle<CHiddenSonicAlarm> hAlarm;
		float flEnd;
	};

	struct Blown_t
	{
		CHandle<CHiddenSonicAlarm> hAlarm;
		EHANDLE hArmer;
		float flTime;
	};

	bool IsActive( void ) const { return sm_sonicmines_enable.GetBool() && m_bMapAllowed; }

	void Reset( void )
	{
		m_Armed.Purge();
		m_Sounding.Purge();
		for ( int i = 0; i <= MAX_PLAYERS; i++ )
			m_iKits[i] = 0;
	}

	static bool IsSupport( CHidden_Player *pPlayer )
	{
		return pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS && pPlayer->GetPlayerClass() == HIDDEN_CLASS_SUPPORT;
	}

	static int GetSonics( CHidden_Player *pPlayer ) { return pPlayer->GetAmmoCount( "sonic" ); }

	ArmedAlarm_t *FindArmed( CHiddenSonicAlarm *pAlarm )
	{
		for ( int i = 0; i < m_Armed.Count(); i++ )
		{
			if ( m_Armed[i].hAlarm.Get() == pAlarm )
				return &m_Armed[i];
		}
		return NULL;
	}

	void TakeKit( CHidden_Player *pPlayer )
	{
		if ( pPlayer->GetPlayerClass() != HIDDEN_CLASS_SUPPORT )
		{
			HiddenPlugins_PrintChat( pPlayer, "You must be support to use tripmines" );
			return;
		}

		if ( GetSonics( pPlayer ) < 1 )
		{
			HiddenPlugins_PrintChat( pPlayer, "You must have 1 or more sonic alarms!" );
			return;
		}

		pPlayer->RemoveAmmo( 1, "sonic" );
		m_iKits[pPlayer->entindex()]++;
		HiddenPlugins_PrintChat( pPlayer, "You changed a sonic alarm for a sonic alarm bomb kit" );
		HiddenPlugins_PrintChat( pPlayer, "To arm a sonic alarm go close to it, look at it and type in chat arm" );
	}

	void Arm( CHidden_Player *pPlayer )
	{
		const int iPlayer = pPlayer->entindex();
		if ( m_iKits[iPlayer] <= 0 )
		{
			HiddenPlugins_PrintChat( pPlayer, "You do not have an arming kit! Type in chat tripmine to get one!" );
			return;
		}

		Vector vecForward;
		pPlayer->EyeVectors( &vecForward );
		trace_t tr;
		UTIL_TraceLine( pPlayer->EyePosition(), pPlayer->EyePosition() + vecForward * MAX_TRACE_LENGTH, MASK_SHOT, pPlayer, COLLISION_GROUP_NONE, &tr );

		CHiddenSonicAlarm *pAlarm = dynamic_cast<CHiddenSonicAlarm *>( tr.m_pEnt );
		if ( !pAlarm )
		{
			HiddenPlugins_PrintChat( pPlayer, "You must look directly at a sonic alarm" );
			return;
		}

		if ( ( pAlarm->GetAbsOrigin() - pPlayer->GetAbsOrigin() ).Length() >= MINES_ARM_RANGE )
		{
			HiddenPlugins_PrintChat( pPlayer, "You must be closer to the alarm to arm it!" );
			return;
		}

		if ( FindArmed( pAlarm ) )
			return;

		m_iKits[iPlayer]--;

		pAlarm->SetRenderMode( kRenderTransAlpha );
		pAlarm->SetRenderColor( 200, 0, 0, 75 );
		HiddenPlugins_PrintChat( pPlayer, "You rigged this sonic alarm to blow!" );

		ArmedAlarm_t armed;
		armed.hAlarm = pAlarm;
		armed.hArmer = pPlayer;
		armed.bFirstHitKnown = false;
		armed.flNextBeam = gpGlobals->curtime + 0.1f;
		m_Armed.AddToTail( armed );

		CPASAttenuationFilter filter( pAlarm, SNDLVL_NORM );
		EmitSound_t params;
		params.m_pSoundName = MINES_SOUND_ARM;
		params.m_SoundLevel = SNDLVL_NORM;
		params.m_pOrigin = &pAlarm->GetAbsOrigin();
		CBaseEntity::EmitSound( filter, pAlarm->entindex(), params );
	}

	// A red beam every few seconds; its first reading of what the beam hits decides when it blows.
	void ShowBeam( ArmedAlarm_t &armed )
	{
		CHiddenSonicAlarm *pAlarm = armed.hAlarm.Get();

		CTraceFilterNoNPCsOrPlayer filter( pAlarm, COLLISION_GROUP_NONE );
		trace_t tr;
		UTIL_TraceLine( pAlarm->GetAbsOrigin(), pAlarm->GetAbsOrigin() + pAlarm->GetBeamDir() * MAX_TRACE_LENGTH, MASK_ALL, &filter, &tr );
		if ( tr.fraction >= 1.0f )
			return;

		if ( !armed.bFirstHitKnown )
		{
			armed.hFirstHit = tr.m_pEnt;
			armed.bFirstHitKnown = true;
		}

		CBroadcastRecipientFilter all;
		te->BeamPoints( all, 0.0f, &pAlarm->GetAbsOrigin(), &tr.endpos, m_iBeamSprite, m_iBeamHalo, 0, 0, 2.5f, 1.0f, 1.0f, 0, 1.0f, 255, 0, 0, 200, 10 );
	}

	void Explode( ArmedAlarm_t *pArmed )
	{
		CHiddenSonicAlarm *pAlarm = pArmed->hAlarm.Get();
		CBaseEntity *pArmer = pArmed->hArmer.Get();
		const Vector vecOrigin = pAlarm->GetAbsOrigin();

		QAngle angExplosion = pAlarm->GetAbsAngles();
		angExplosion.x -= 90.0f;
		ExplosionCreate( vecOrigin, angExplosion, NULL, MINES_EXPLOSION, MINES_EXPLOSION, true );

		CPASAttenuationFilter filter( pAlarm, SNDLVL_NORM );
		EmitSound_t params;
		params.m_pSoundName = MINES_SOUND_EXPLODE;
		params.m_SoundLevel = SNDLVL_NORM;
		params.m_pOrigin = &vecOrigin;
		CBaseEntity::EmitSound( filter, pAlarm->entindex(), params );

		CBasePlayer *pHidden = NULL;
		for ( int i = 1; i <= gpGlobals->maxClients && !pHidden; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_HIDDEN )
				pHidden = pPlayer;
		}

		// On top of the blast: the Hidden's damage goes to whoever armed it, the marines' to the Hidden.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( !pPlayer || !pPlayer->IsAlive() )
				continue;

			const float flDistance = ( vecOrigin - pPlayer->GetAbsOrigin() ).Length();
			float flDamage = 0.0f;
			CBaseEntity *pAttacker = NULL;
			if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
			{
				flDamage = ( flDistance <= 150.0f ) ? 40.0f : ( flDistance < 250.0f ) ? 20.0f : ( flDistance < 300.0f ) ? 10.0f : 0.0f;
				pAttacker = pArmer;
			}
			else
			{
				flDamage = ( flDistance <= 120.0f ) ? 60.0f : ( flDistance < 200.0f ) ? 30.0f : ( flDistance < 250.0f ) ? 10.0f : 0.0f;
				pAttacker = pHidden;
				if ( flDamage > 0.0f )
					pPlayer->Stun( pPlayer );	// the plugin's "blur"
			}

			if ( flDamage > 0.0f )
			{
				CTakeDamageInfo info( pAlarm, pAttacker ? pAttacker : GetContainingEntity( INDEXENT( 0 ) ), flDamage, DMG_GENERIC );
				pPlayer->TakeDamage( info );
			}
		}

		Blown_t blown = { pAlarm, pArmer, gpGlobals->curtime + MINES_DAMAGE_DELAY };
		m_Blown.AddToTail( blown );
		m_Armed.Remove( pArmed - m_Armed.Base() );
	}

	// Tells the marines, one alarm in seven, why it stayed quiet.
	void Notify( void )
	{
		if ( !sm_sonicmines_notify.GetBool() )
			return;

		if ( m_iNotifyCount == 0 )
		{
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( pPlayer && pPlayer->GetTeamNumber() != TEAM_HIDDEN )
					HiddenPlugins_PrintChat( pPlayer, "Alarms are set to hidden only!" );
			}
		}

		m_iNotifyCount = ( m_iNotifyCount + 1 ) % MINES_NOTIFY_EVERY;
	}

	bool m_bMapAllowed;
	int m_iBeamSprite;
	int m_iBeamHalo;
	int m_iNotifyCount;
	int m_iKits[MAX_PLAYERS + 1];
	CUtlVector<ArmedAlarm_t> m_Armed;
	CUtlVector<Sounding_t> m_Sounding;
	CUtlVector<Blown_t> m_Blown;
};

static CHiddenPluginSonicMines s_SonicMines;
