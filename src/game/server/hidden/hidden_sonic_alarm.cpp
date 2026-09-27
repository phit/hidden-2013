//========= Hidden: Source =====================================================//
//
// Purpose: The sonic alarm (npc_tripmine): HL2's tripmine turned into a reusable
//			alarm that never explodes. See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_sonic_alarm.h"
#include "beam_shared.h"
#include "props_shared.h"
#include "te_effect_dispatch.h"
#include "igameevents.h"
#include "plugins/hidden_plugins.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define SONIC_ALARM_MODEL		"models/weapons/tripalarm/w_tripalarm.mdl"
#define SONIC_ALARM_BEAM_RANGE	2048.0f
#define SONIC_ALARM_ARM_TIME	2.5f	// after placing
#define SONIC_ALARM_REARM_TIME	2.5f	// after going off
#define SONIC_ALARM_TRIGGER_DELAY	0.25f

// Beta 4b sets this beam type, one past the last valid one.
#define SONIC_ALARM_BEAM_TYPE	6

extern const char *g_pModelNameLaser;
extern ConVar sk_plr_dmg_tripmine;
extern ConVar sk_tripmine_radius;

LINK_ENTITY_TO_CLASS( npc_tripmine, CHiddenSonicAlarm );

BEGIN_DATADESC( CHiddenSonicAlarm )
	DEFINE_FIELD( m_hOwner, FIELD_EHANDLE ),
	DEFINE_FIELD( m_hBreaker, FIELD_EHANDLE ),
	DEFINE_FIELD( m_flPowerUp, FIELD_TIME ),
	DEFINE_FIELD( m_vecDir, FIELD_VECTOR ),
	DEFINE_FIELD( m_vecEnd, FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flBeamLength, FIELD_FLOAT ),
	DEFINE_FIELD( m_pBeam, FIELD_CLASSPTR ),

	DEFINE_THINKFUNC( PowerupThink ),
	DEFINE_THINKFUNC( BeamBreakThink ),
	DEFINE_THINKFUNC( AlarmThink ),
END_DATADESC()

CHiddenSonicAlarm::CHiddenSonicAlarm()
{
	m_flPowerUp = 0.0f;
	m_vecDir.Init();
	m_vecEnd.Init();
	m_flBeamLength = 0.0f;
	m_pBeam = NULL;
}

void CHiddenSonicAlarm::Spawn( void )
{
	Precache();

	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetModel( SONIC_ALARM_MODEL );

	SetCycle( 0.0f );
	m_nBody = 3;
	m_flDamage = sk_plr_dmg_tripmine.GetFloat();
	m_DmgRadius = sk_tripmine_radius.GetFloat();

	ResetSequenceInfo();
	m_flPlaybackRate = 0.0f;

	UTIL_SetSize( this, Vector( -4, -4, -2 ), Vector( 4, 4, 2 ) );

	m_flPowerUp = gpGlobals->curtime + SONIC_ALARM_ARM_TIME;

	SetThink( &CHiddenSonicAlarm::PowerupThink );
	SetNextThink( gpGlobals->curtime + 0.2f );

	// Any damage knocks it off the wall.
	m_takedamage = DAMAGE_YES;
	m_iHealth = 1;

	EmitSound( "TripmineGrenade.Place" );
	SetDamage( 200 );

	// The alarm sits at 90 degrees on the wall, so rotate back to get the beam direction.
	QAngle angles = GetAbsAngles();
	angles.x -= 90.0f;

	AngleVectors( angles, &m_vecDir );
	m_vecEnd = GetAbsOrigin() + m_vecDir * SONIC_ALARM_BEAM_RANGE;

	AddEffects( EF_NOSHADOW );
}

void CHiddenSonicAlarm::Precache( void )
{
	PrecacheModel( SONIC_ALARM_MODEL );
	PrecacheModel( "models/gibs/tripalarm_gib1.mdl" );
	PrecacheModel( "models/gibs/tripalarm_gib2.mdl" );
	PrecacheModel( "models/gibs/tripalarm_gib3.mdl" );

	PrecacheScriptSound( "TripmineGrenade.Place" );
	PrecacheScriptSound( "TripmineGrenade.Activate" );
	PrecacheScriptSound( "TripmineGrenade.StopSound" );	// no sound script defines it, so it's silent, as in Beta 4b
	PrecacheScriptSound( "Weapon_Sonic.Alarm" );
}

void CHiddenSonicAlarm::PowerupThink( void )
{
	if ( gpGlobals->curtime > m_flPowerUp )
	{
		MakeBeam();
		RemoveSolidFlags( FSOLID_NOT_SOLID );
		m_bIsLive = true;

		EmitSound( "TripmineGrenade.Activate" );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}

void CHiddenSonicAlarm::KillBeam( void )
{
	if ( m_pBeam )
	{
		UTIL_Remove( m_pBeam );
		m_pBeam = NULL;
	}
}

void CHiddenSonicAlarm::MakeBeam( void )
{
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	m_flBeamLength = tr.fraction;

	// If something living is in the way, measure the beam through it.
	const float flDrawLength = tr.fraction;
	CBaseCombatCharacter *pBCC = ToBaseCombatCharacter( tr.m_pEnt );
	if ( pBCC )
	{
		SetOwnerEntity( pBCC );
		UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );
		m_flBeamLength = tr.fraction;
		SetOwnerEntity( NULL );
	}

	SetThink( &CHiddenSonicAlarm::BeamBreakThink );
	SetNextThink( gpGlobals->curtime + 1.0f );

	const Vector vecTmpEnd = GetLocalOrigin() + m_vecDir * SONIC_ALARM_BEAM_RANGE * flDrawLength;

	m_pBeam = CBeam::BeamCreate( g_pModelNameLaser, 0.35f );
	m_pBeam->PointEntInit( vecTmpEnd, this );
	m_pBeam->SetColor( 100, 200, 255 );
	m_pBeam->SetScrollRate( 25.0f );
	m_pBeam->SetBrightness( 64 );
	m_pBeam->SetType( SONIC_ALARM_BEAM_TYPE );
	m_pBeam->SetEndAttachment( LookupAttachment( "beamstart" ) );
}

void CHiddenSonicAlarm::BeamBreakThink( void )
{
	StopSound( "Weapon_Sonic.Alarm" );

	// Go solid once whoever placed it has moved out of the way.
	if ( IsSolidFlagSet( FSOLID_NOT_SOLID ) )
	{
		Vector vecUp = GetAbsOrigin();
		vecUp.z += 5.0f;

		trace_t tr;
		UTIL_TraceEntity( this, GetAbsOrigin(), vecUp, MASK_SHOT, &tr );
		if ( !tr.startsolid && tr.fraction == 1.0f )
			RemoveSolidFlags( FSOLID_NOT_SOLID );
	}

	// Simple hitboxes only, not MASK_SHOT.
	trace_t tr;
	UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	if ( !m_pBeam )
	{
		MakeBeam();
		if ( tr.m_pEnt )
			m_hOwner = tr.m_pEnt;
	}

	if ( ToBaseCombatCharacter( tr.m_pEnt ) || fabs( m_flBeamLength - tr.fraction ) > 0.001f )
	{
		// Something broke the beam.
		m_hBreaker = tr.m_pEnt;
		m_iHealth = 0;
		SetThink( &CHiddenSonicAlarm::AlarmThink );
		SetNextThink( gpGlobals->curtime + SONIC_ALARM_TRIGGER_DELAY );
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.05f );
}

void CHiddenSonicAlarm::AlarmThink( void )
{
	// The built-in plugins can decide who hears it, or that nobody does.
	CPASAttenuationFilter filter( this, "Weapon_Sonic.Alarm" );
	if ( HiddenPlugins_AlarmTriggered( this, m_hBreaker, filter ) )
	{
		EmitSound( filter, entindex(), "Weapon_Sonic.Alarm" );

		// The marines' radar shows where it went off.
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "alarm_trigger" );
		if ( pEvent )
		{
			const Vector &vecOrigin = GetAbsOrigin();
			pEvent->SetFloat( "posx", vecOrigin.x );
			pEvent->SetFloat( "posy", vecOrigin.y );
			pEvent->SetFloat( "posz", vecOrigin.z );
			gameeventmanager->FireEvent( pEvent );
		}
	}
	m_hBreaker = NULL;

	// Rebuild the beam and watch again once the alarm has sounded.
	KillBeam();
	MakeBeam();
	SetThink( &CHiddenSonicAlarm::BeamBreakThink );
	SetNextThink( gpGlobals->curtime + SONIC_ALARM_REARM_TIME );
}

int CHiddenSonicAlarm::OnTakeDamage( const CTakeDamageInfo &info )
{
	// Disarmed while powering up.
	if ( gpGlobals->curtime < m_flPowerUp && info.GetDamage() < m_iHealth )
	{
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
		KillBeam();
		return 0;
	}

	if ( info.GetDamage() <= 0.0f )
		return BaseClass::OnTakeDamage( info );

	// Knocked off the wall: sparks and pieces, no explosion.
	SetThink( &CBaseEntity::SUB_Remove );
	SetNextThink( gpGlobals->curtime + 0.1f );

	Vector vecNormal = m_vecEnd;
	VectorNormalize( vecNormal );

	CEffectData data;
	data.m_vOrigin = GetAbsOrigin();
	data.m_vAngles = GetAbsAngles();
	data.m_vNormal = vecNormal;
	data.m_flScale = 1.0f;
	DispatchEffect( "ManhackSparks", data );

	// Beta 4b throws the gibs along the beam's end point rather than its direction, so they fly fast.
	const Vector vecVelocity = m_vecEnd * -150.0f;
	const AngularImpulse angImpulse( RandomFloat( -500.0f, 500.0f ), RandomFloat( -500.0f, 500.0f ), RandomFloat( -500.0f, 500.0f ) );

	DevMsg( 1, "Spawning sonic gibs\n" );
	PropBreakableCreateAll( GetModelIndex(), NULL, GetAbsOrigin(), GetAbsAngles(), vecVelocity, angImpulse, 1.0f, 60.0f, COLLISION_GROUP_DEBRIS, this );

	StopSound( "Weapon_Sonic.Alarm" );
	KillBeam();
	return 0;
}

void CHiddenSonicAlarm::Event_Killed( const CTakeDamageInfo &info )
{
	// Never explodes.
	EmitSound( "TripmineGrenade.StopSound" );
}
