//========= Hidden: Source =====================================================//
//
// Purpose: The marines' semi-automatic pistols, the Five-seven (weapon_pistol) and
//			the FN P9 (weapon_pistol2). See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponHiddenPistol C_WeaponHiddenPistol
	#define CWeaponPistol C_WeaponPistol
	#define CWeaponPistol2 C_WeaponPistol2
#endif

#define PISTOL_FASTEST_REFIRE_TIME	0.1f

// HL2's pistol: firing as fast as the trigger is pulled, but no faster than every 0.1 s. Beta 4b
// also tracks HL2's accuracy penalty, but never uses it; it's left out here.
class CWeaponHiddenPistol : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponHiddenPistol, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponHiddenPistol();

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void ItemPostFrame( void );

protected:
	// The Five-seven's numbers; the FN P9 overrides them.
	virtual float GetSpread( bool bOnGround ) const { return bOnGround ? 0.03f : 0.06f; }
	virtual float GetDryFireDelay( void ) const { return 0.2f; }

private:
	CNetworkVar( float, m_flSoonestPrimaryAttack );

	CWeaponHiddenPistol( const CWeaponHiddenPistol & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponHiddenPistol, DT_WeaponHiddenPistol )

BEGIN_NETWORK_TABLE( CWeaponHiddenPistol, DT_WeaponHiddenPistol )
#ifdef CLIENT_DLL
	RecvPropTime( RECVINFO( m_flSoonestPrimaryAttack ) ),
#else
	SendPropTime( SENDINFO( m_flSoonestPrimaryAttack ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponHiddenPistol )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD_TOL( m_flSoonestPrimaryAttack, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
END_PREDICTION_DATA()

CWeaponHiddenPistol::CWeaponHiddenPistol()
{
	m_flSoonestPrimaryAttack = 0.0f;
}

void CWeaponHiddenPistol::PrimaryAttack( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || !m_bDeployed )
		return;

	if ( pPlayer->GetSafe() )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		return;
	}

	m_flSoonestPrimaryAttack = gpGlobals->curtime + PISTOL_FASTEST_REFIRE_TIME;

	pPlayer->SetShotsFired( pPlayer->GetShotsFired() + 1 );

	if ( m_iClip1 <= 0 )
	{
		if ( m_bFireOnEmpty )
		{
			PlayEmptySound();
			m_flNextPrimaryAttack = gpGlobals->curtime + 0.2f;
		}
		return;
	}

	SendWeaponAnim( ACT_VM_PRIMARYATTACK );
	m_iClip1--;
	pPlayer->SetAnimation( PLAYER_ATTACK1 );

	const float flFireDuration = SequenceDuration();

	FireHiddenBullets( pPlayer->EyeAngles() + pPlayer->GetPunchAngle(), GetSpread( ( pPlayer->GetFlags() & FL_ONGROUND ) != 0 ) );
	pPlayer->DoMuzzleFlash();

	m_flNextPrimaryAttack = m_flNextSecondaryAttack = gpGlobals->curtime + flFireDuration + 0.1f;

	CheckAmmoDepleted();
	SetWeaponIdleTime( gpGlobals->curtime + 5.0f );
}

void CWeaponHiddenPistol::ItemPostFrame( void )
{
	BaseClass::ItemPostFrame();

	if ( m_bInReload )
		return;

	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || pPlayer->GetSafe() )
		return;

	const bool bAttack = ( pPlayer->m_nButtons & IN_ATTACK ) != 0;

	// Letting go of the trigger allows the next shot as soon as the refire time is up.
	if ( !bAttack && m_flSoonestPrimaryAttack < gpGlobals->curtime )
	{
		m_flNextPrimaryAttack = gpGlobals->curtime - 0.1f;
	}
	else if ( bAttack && m_flNextPrimaryAttack < gpGlobals->curtime && m_iClip1 <= 0 )
	{
		WeaponSound( EMPTY );
		SendWeaponAnim( ACT_VM_DRYFIRE );
		m_flSoonestPrimaryAttack = gpGlobals->curtime + GetDryFireDelay();
		m_flNextPrimaryAttack = gpGlobals->curtime + SequenceDuration();
	}
}

bool CWeaponHiddenPistol::Reload( void )
{
	return ReloadMagazine( "EjectPistolClip" );
}

void CWeaponHiddenPistol::WeaponIdle( void )
{
	IdleOrLowered();
}

bool CWeaponHiddenPistol::Deploy( void )
{
	DeployLowered();
	return BaseClass::Deploy();
}

bool CWeaponHiddenPistol::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( !GetHiddenPlayerOwner() )
		return false;

	SetHolsterBodygroup( 2 );
	return BaseClass::Holster( pSwitchingTo );
}

//-----------------------------------------------------------------------------
// Five-seven
//-----------------------------------------------------------------------------
class CWeaponPistol : public CWeaponHiddenPistol
{
public:
	DECLARE_CLASS( CWeaponPistol, CWeaponHiddenPistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponPistol() {}

private:
	CWeaponPistol( const CWeaponPistol & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPistol, DT_WeaponPistol )

BEGIN_NETWORK_TABLE( CWeaponPistol, DT_WeaponPistol )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponPistol )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_pistol, CWeaponPistol );
PRECACHE_WEAPON_REGISTER( weapon_pistol );

//-----------------------------------------------------------------------------
// FN P9
//-----------------------------------------------------------------------------
class CWeaponPistol2 : public CWeaponHiddenPistol
{
public:
	DECLARE_CLASS( CWeaponPistol2, CWeaponHiddenPistol );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponPistol2() {}

protected:
	virtual float GetSpread( bool bOnGround ) const { return bOnGround ? 0.04f : 0.09f; }
	virtual float GetDryFireDelay( void ) const { return 0.1f; }

private:
	CWeaponPistol2( const CWeaponPistol2 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponPistol2, DT_WeaponPistol2 )

BEGIN_NETWORK_TABLE( CWeaponPistol2, DT_WeaponPistol2 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponPistol2 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_pistol2, CWeaponPistol2 );
PRECACHE_WEAPON_REGISTER( weapon_pistol2 );
