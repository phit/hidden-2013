//========= Hidden: Source =====================================================//
//
// Purpose: The marines' sonic alarm (Beta 4b's CWeapon_Sonic): HL2's SLAM in
//			tripmine mode only. See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "in_buttons.h"

#ifndef CLIENT_DLL
	#include "hidden_sonic_alarm.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponSonic C_WeaponSonic
#endif

#define SONIC_ATTACH_RANGE		64.0f
#define SONIC_CAN_ATTACH_RANGE	42.0f

// HL2's SLAM states; the sonic only ever uses the tripmine.
enum SonicState_t
{
	SONIC_TRIPMINE_READY = 0,
	SONIC_SATCHEL_THROW,
	SONIC_SATCHEL_ATTACH,
};

class CWeaponSonic : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponSonic, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponSonic();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual bool Reload( void );
	virtual void PrimaryAttack( void );
	virtual void SecondaryAttack( void ) {}
	virtual void WeaponIdle( void );
	virtual void ItemPostFrame( void );

	// Placing an alarm doesn't lower like a firearm.
	virtual void SetSafe( void ) {}

private:
	void SonicThink( void );
	bool CanAttachSonic( void );
	void StartTripmineAttach( void );
	void TripmineAttach( void );
	void GetAttachTrace( float flRange, int iMask, trace_t &tr );

	CNetworkVar( int, m_iSonicState );
	CNetworkVar( bool, m_bNeedReload );
	CNetworkVar( bool, m_bClearReload );
	CNetworkVar( bool, m_bAttachTripmine );
	CNetworkVar( float, m_flWallSwitchTime );

	CWeaponSonic( const CWeaponSonic & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponSonic, DT_WeaponSonic )

BEGIN_NETWORK_TABLE( CWeaponSonic, DT_WeaponSonic )
#ifdef CLIENT_DLL
	RecvPropInt( RECVINFO( m_iSonicState ) ),
	RecvPropBool( RECVINFO( m_bNeedReload ) ),
	RecvPropBool( RECVINFO( m_bClearReload ) ),
	RecvPropBool( RECVINFO( m_bAttachTripmine ) ),
	RecvPropTime( RECVINFO( m_flWallSwitchTime ) ),
#else
	SendPropInt( SENDINFO( m_iSonicState ), 2, SPROP_UNSIGNED ),
	SendPropBool( SENDINFO( m_bNeedReload ) ),
	SendPropBool( SENDINFO( m_bClearReload ) ),
	SendPropBool( SENDINFO( m_bAttachTripmine ) ),
	SendPropTime( SENDINFO( m_flWallSwitchTime ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponSonic )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_iSonicState, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bNeedReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bClearReload, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_bAttachTripmine, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flWallSwitchTime, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_sonic, CWeaponSonic );
PRECACHE_WEAPON_REGISTER( weapon_sonic );

CWeaponSonic::CWeaponSonic()
{
	m_iSonicState = SONIC_SATCHEL_THROW;
	m_bNeedReload = true;
	m_bClearReload = false;
	m_bAttachTripmine = false;
	m_flWallSwitchTime = 0.0f;
}

void CWeaponSonic::Spawn( void )
{
	BaseClass::Spawn();
	Precache();

	m_iSonicState = SONIC_SATCHEL_THROW;
	m_flWallSwitchTime = 0.0f;

	// One alarm comes with the weapon.
	m_iClip2 = 1;
}

void CWeaponSonic::Precache( void )
{
	BaseClass::Precache();

#ifndef CLIENT_DLL
	UTIL_PrecacheOther( "npc_tripmine" );
#endif

	// Beta 4b also precached the SLAM's Weapon_Sonic.TripMineMode, SatchelDetonate and SatchelThrow,
	// which it never played and no sound script defines.
	PrecacheScriptSound( "IRIS.DeploySonicAlarm" );
}

bool CWeaponSonic::Deploy( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if ( !pOwner )
		return false;

	SetModel( GetViewModel() );

	m_iSonicState = SONIC_SATCHEL_THROW;

	int iActivity;
	if ( CanAttachSonic() )
	{
		iActivity = ACT_SLAM_TRIPMINE_DRAW;
		m_iSonicState = SONIC_TRIPMINE_READY;
	}
	else
	{
		iActivity = ACT_SLAM_THROW_ND_DRAW;
		m_iSonicState = SONIC_SATCHEL_THROW;
	}

	SetWeaponIdleTime( gpGlobals->curtime );

	// Straight to DefaultDeploy: Beta 4b's sonic skips the SDK base's Deploy.
	return DefaultDeploy( (char *)GetViewModel(), (char *)GetWorldModel(), iActivity, (char *)GetAnimPrefix() );
}

bool CWeaponSonic::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	SetThink( NULL );
	return BaseClass::Holster( pSwitchingTo );
}

bool CWeaponSonic::Reload( void )
{
	WeaponIdle();
	return true;
}

void CWeaponSonic::ItemPostFrame( void )
{
	CBasePlayer *pOwner = ToBasePlayer( GetOwner() );
	if ( !pOwner )
		return;

	SonicThink();

	if ( ( pOwner->m_nButtons & IN_ATTACK2 ) && m_flNextSecondaryAttack <= gpGlobals->curtime )
		SecondaryAttack();
	else if ( !m_bNeedReload && ( pOwner->m_nButtons & IN_ATTACK ) && m_flNextPrimaryAttack <= gpGlobals->curtime )
		PrimaryAttack();
	else
		WeaponIdle();
}

void CWeaponSonic::PrimaryAttack( void )
{
	CBaseCombatCharacter *pOwner = GetOwner();
	if ( !pOwner || pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
		return;

	StartTripmineAttach();
}

// HL2's SLAMThink, which here only ever switches to tripmine mode.
void CWeaponSonic::SonicThink( void )
{
	if ( m_flWallSwitchTime > gpGlobals->curtime )
		return;

	CBaseCombatCharacter *pOwner = GetOwner();
	if ( !pOwner || pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 || m_iSonicState != SONIC_SATCHEL_THROW )
		return;

	m_iSonicState = SONIC_TRIPMINE_READY;
	SetWeaponIdleTime( gpGlobals->curtime );
	SendWeaponAnim( ACT_SLAM_THROW_TO_TRIPMINE_ND );
	m_flWallSwitchTime = gpGlobals->curtime + SequenceDuration();
	m_bNeedReload = false;
}

void CWeaponSonic::GetAttachTrace( float flRange, int iMask, trace_t &tr )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );

	const Vector vecSrc = pPlayer->EyePosition();
	Vector vecAiming;
	AngleVectors( pPlayer->GetAbsAngles(), &vecAiming );

	UTIL_TraceLine( vecSrc, vecSrc + vecAiming * flRange, iMask, pPlayer, COLLISION_GROUP_NONE, &tr );
}

bool CWeaponSonic::CanAttachSonic( void )
{
	if ( !ToBasePlayer( GetOwner() ) )
		return false;

	trace_t tr;
	GetAttachTrace( SONIC_CAN_ATTACH_RANGE, MASK_SOLID, tr );

	// Anything but a creature will do.
	return tr.fraction < 1.0f && !( tr.m_pEnt && tr.m_pEnt->MyCombatCharacterPointer() );
}

void CWeaponSonic::StartTripmineAttach( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	trace_t tr;
	GetAttachTrace( SONIC_ATTACH_RANGE, MASK_SOLID & ~CONTENTS_MONSTER, tr );

	if ( tr.fraction < 1.0f && tr.m_pEnt && !( tr.m_pEnt->GetFlags() & FL_CONVEYOR ) )
	{
		pPlayer->SetAnimation( PLAYER_ATTACK1 );
		SendWeaponAnim( ACT_SLAM_TRIPMINE_ATTACH );
		m_bNeedReload = true;
		m_bAttachTripmine = true;

		pPlayer->EmitSound( "IRIS.DeploySonicAlarm" );
	}

	m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	m_flNextSecondaryAttack = gpGlobals->curtime + SequenceDuration();
}

void CWeaponSonic::TripmineAttach( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( !pPlayer )
		return;

	m_bAttachTripmine = false;

	trace_t tr;
	GetAttachTrace( SONIC_ATTACH_RANGE, MASK_SOLID & ~CONTENTS_MONSTER, tr );

	if ( tr.fraction < 1.0f && tr.m_pEnt && !( tr.m_pEnt->GetFlags() & FL_CONVEYOR ) )
	{
#ifndef CLIENT_DLL
		QAngle angles;
		VectorAngles( tr.plane.normal, angles );
		angles.x += 90.0f;

		CHiddenSonicAlarm *pAlarm = static_cast<CHiddenSonicAlarm *>( CBaseEntity::Create( "npc_tripmine", tr.endpos, angles, NULL ) );
		if ( pAlarm )
			pAlarm->SetAlarmOwner( pPlayer );
#endif

		pPlayer->RemoveAmmo( 1, m_iPrimaryAmmoType );
	}
}

void CWeaponSonic::WeaponIdle( void )
{
	if ( !HasWeaponIdleTimeElapsed() )
		return;

	if ( m_bClearReload )
	{
		m_bNeedReload = false;
		m_bClearReload = false;
	}

	CBaseCombatCharacter *pOwner = GetOwner();
	if ( !pOwner )
		return;

	int iActivity = ACT_RESET;

	if ( m_bAttachTripmine )
	{
		TripmineAttach();
		iActivity = ACT_SLAM_TRIPMINE_ATTACH2;
	}
	else if ( pOwner->GetAmmoCount( m_iPrimaryAmmoType ) <= 0 )
	{
		// Out of alarms: throw the device away.
#ifndef CLIENT_DLL
		pOwner->Weapon_Drop( this );
		UTIL_Remove( this );
#endif
		return;
	}
	else if ( m_bNeedReload )
	{
		switch ( m_iSonicState )
		{
		case SONIC_TRIPMINE_READY:
			iActivity = ACT_SLAM_TRIPMINE_DRAW;
			break;
		case SONIC_SATCHEL_THROW:
			iActivity = ACT_SLAM_THROW_ND_DRAW;
			break;
		case SONIC_SATCHEL_ATTACH:
			iActivity = ACT_SLAM_STICKWALL_ND_DRAW;
			break;
		}
		m_bClearReload = true;
	}
	else if ( m_iSonicState == SONIC_TRIPMINE_READY )
	{
		m_flWallSwitchTime = 0.0f;
		iActivity = ACT_SLAM_TRIPMINE_IDLE;
	}

	SendWeaponAnim( iActivity );
}
