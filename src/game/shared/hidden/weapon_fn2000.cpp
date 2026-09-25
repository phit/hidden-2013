//========= Hidden: Source =====================================================//
//
// Purpose: FN2000 assault rifle with its zoom scope. See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"
#include "in_buttons.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponFN2000 C_WeaponFN2000
#endif

#define FN2000_ZOOM_FOV			20
#define FN2000_ZOOM_IN_TIME		0.1f
#define FN2000_ZOOM_OUT_TIME	0.2f

class CWeaponFN2000 : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponFN2000, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponFN2000();

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );
	virtual void ItemPostFrame( void );
	virtual void ItemBusyFrame( void );

private:
	void CheckZoomToggle( void );
	void ToggleZoom( void );

	CNetworkVar( bool, m_bInZoom );
	CNetworkVar( float, m_flLastFire );

	CWeaponFN2000( const CWeaponFN2000 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponFN2000, DT_WeaponFN2000 )

BEGIN_NETWORK_TABLE( CWeaponFN2000, DT_WeaponFN2000 )
#ifdef CLIENT_DLL
	RecvPropBool( RECVINFO( m_bInZoom ) ),
	RecvPropTime( RECVINFO( m_flLastFire ) ),
#else
	SendPropBool( SENDINFO( m_bInZoom ) ),
	SendPropTime( SENDINFO( m_flLastFire ) ),
#endif
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponFN2000 )
#ifdef CLIENT_DLL
	DEFINE_PRED_FIELD( m_bInZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD_TOL( m_flLastFire, FIELD_FLOAT, FTYPEDESC_INSENDTABLE, TD_MSECTOLERANCE ),
#endif
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_fn2000, CWeaponFN2000 );
PRECACHE_WEAPON_REGISTER( weapon_fn2000 );

CWeaponFN2000::CWeaponFN2000()
{
	m_bInZoom = false;
	m_flLastFire = 0.0f;
}

void CWeaponFN2000::PrimaryAttack( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || !m_bDeployed )
		return;

	if ( CheckSafety() )
		return;

	// Zoomed shots are tighter, and the first shot after a second's pause is dead accurate.
	float flSpread = m_bInZoom ? 0.01f : 0.03f;
	if ( m_flLastFire + 1.0f < gpGlobals->curtime )
		flSpread = 0.0f;
	if ( !( pPlayer->GetFlags() & FL_ONGROUND ) )
		flSpread = 0.08f;

	FireAutomatic( flSpread );

	m_flLastFire = gpGlobals->curtime;
}

bool CWeaponFN2000::Reload( void )
{
	if ( !ReloadMagazine( "EjectFN2000Clip" ) )
		return false;

	if ( m_bInZoom )
		ToggleZoom();

	return true;
}

void CWeaponFN2000::WeaponIdle( void )
{
	IdleOrLowered();
}

bool CWeaponFN2000::Deploy( void )
{
	DeployLowered();
	m_flLastFire = gpGlobals->curtime - 1.0f;
	return BaseClass::Deploy();
}

bool CWeaponFN2000::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( !GetHiddenPlayerOwner() )
		return false;

	SetHolsterBodygroup( 0 );

	if ( m_bInZoom )
		ToggleZoom();

	return BaseClass::Holster( pSwitchingTo );
}

void CWeaponFN2000::ItemPostFrame( void )
{
	CheckZoomToggle();
	BaseClass::ItemPostFrame();
}

void CWeaponFN2000::ItemBusyFrame( void )
{
	CheckZoomToggle();
	BaseClass::ItemBusyFrame();
}

void CWeaponFN2000::CheckZoomToggle( void )
{
	CBasePlayer *pPlayer = ToBasePlayer( GetOwner() );
	if ( pPlayer && ( pPlayer->m_afButtonPressed & IN_ATTACK2 ) )
		ToggleZoom();
}

void CWeaponFN2000::ToggleZoom( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer || !m_bDeployed )
		return;

	if ( !m_bInZoom )
	{
		if ( !pPlayer->SetFOV( this, FN2000_ZOOM_FOV, FN2000_ZOOM_IN_TIME ) )
			return;

		m_bInZoom = true;
		pPlayer->SetZoom( true );
	}
	else
	{
		if ( !pPlayer->SetFOV( this, 0, FN2000_ZOOM_OUT_TIME ) )
			return;

		m_bInZoom = false;
		pPlayer->SetZoom( false );
	}
}
