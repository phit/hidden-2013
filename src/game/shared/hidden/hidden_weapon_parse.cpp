//========= Hidden: Source =====================================================//
//
// Purpose: Weapon script data: the Beta 4b keys on top of the HL2MP ones.
//			See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include <KeyValues.h>
#include "hidden_weapon_parse.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

FileWeaponInfo_t *CreateWeaponInfo()
{
	return new CHiddenWeaponInfo;
}

CHiddenWeaponInfo::CHiddenWeaponInfo()
{
	m_iDamage = 0;
	m_iBullets = 0;
	m_flCycleTime = 0.0f;
	m_szAnimExtension[0] = '\0';
}

void CHiddenWeaponInfo::Parse( KeyValues *pKeyValuesData, const char *szWeaponName )
{
	BaseClass::Parse( pKeyValuesData, szWeaponName );

	// Defaults as in Beta 4b's CSDKWeaponInfo::Parse.
	m_iDamage = pKeyValuesData->GetInt( "Damage", 42 );
	m_iBullets = pKeyValuesData->GetInt( "Bullets", 1 );
	m_flCycleTime = pKeyValuesData->GetFloat( "CycleTime", 0.15f );
	Q_strncpy( m_szAnimExtension, pKeyValuesData->GetString( "PlayerAnimationExtension", "mp5" ), sizeof( m_szAnimExtension ) );
}
