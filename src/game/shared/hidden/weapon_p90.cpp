//========= Hidden: Source =====================================================//
//
// Purpose: P90 submachine gun. See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "weapon_hiddenbase.h"
#include "hidden_player_shared.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#ifdef CLIENT_DLL
	#define CWeaponP90 C_WeaponP90
#endif

class CWeaponP90 : public CWeaponHiddenBase
{
public:
	DECLARE_CLASS( CWeaponP90, CWeaponHiddenBase );
	DECLARE_NETWORKCLASS();
	DECLARE_PREDICTABLE();

	CWeaponP90() {}

	virtual void PrimaryAttack( void );
	virtual bool Reload( void );
	virtual void WeaponIdle( void );
	virtual bool Deploy( void );
	virtual bool Holster( CBaseCombatWeapon *pSwitchingTo = NULL );

private:
	CWeaponP90( const CWeaponP90 & );
};

IMPLEMENT_NETWORKCLASS_ALIASED( WeaponP90, DT_WeaponP90 )

BEGIN_NETWORK_TABLE( CWeaponP90, DT_WeaponP90 )
END_NETWORK_TABLE()

BEGIN_PREDICTION_DATA( CWeaponP90 )
END_PREDICTION_DATA()

LINK_ENTITY_TO_CLASS( weapon_p90, CWeaponP90 );
PRECACHE_WEAPON_REGISTER( weapon_p90 );

void CWeaponP90::PrimaryAttack( void )
{
	CHidden_Player *pPlayer = GetHiddenPlayerOwner();
	if ( !pPlayer )
		return;

	if ( CheckSafety() || !m_bDeployed )
		return;

	FireAutomatic( ( pPlayer->GetFlags() & FL_ONGROUND ) ? 0.03f : 0.08f );
}

bool CWeaponP90::Reload( void )
{
	return ReloadMagazine( "EjectFNP90Clip" );
}

void CWeaponP90::WeaponIdle( void )
{
	IdleOrLowered();
}

bool CWeaponP90::Deploy( void )
{
	DeployLowered();
	return BaseClass::Deploy();
}

bool CWeaponP90::Holster( CBaseCombatWeapon *pSwitchingTo )
{
	if ( !GetHiddenPlayerOwner() )
		return false;

	SetHolsterBodygroup( 0 );
	return BaseClass::Holster( pSwitchingTo );
}
