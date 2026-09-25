//========= Hidden: Source =====================================================//
//
// Purpose: Player movement. Beta 4b's CSDKGameMovement is the SDK's CGameMovement,
//			so ladders are brush ladders (CONTENTS_LADDER); HL2's func_useableladder
//			did nothing. See docs/spec/hidden-abilities.md.
//
//=============================================================================//

#include "cbase.h"
#include "hl_gamemovement.h"
#include "hidden_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHiddenGameMovement : public CHL2GameMovement
{
	typedef CHL2GameMovement BaseClass;
public:
	// Brush ladders, as in the SDK, instead of HL2's useable ladders.
	virtual bool LadderMove( void );
	virtual void FullLadderMove( void ) { CGameMovement::FullLadderMove(); }
	virtual bool OnLadder( trace_t &trace ) { return CGameMovement::OnLadder( trace ); }
	virtual int GetCheckInterval( IntervalType_t type ) { return CGameMovement::GetCheckInterval( type ); }
};

bool CHiddenGameMovement::LadderMove( void )
{
	if ( !CGameMovement::LadderMove() )
		return false;

	// Marines put their weapon away while they hold on to a ladder, every tick they're on it;
	// CWeaponHiddenBase::ItemPostFrame brings it back out once they're off.
	if ( player->GetTeamNumber() == TEAM_IRIS )
	{
		CBaseCombatWeapon *pWeapon = player->GetActiveWeapon();
		if ( pWeapon )
			pWeapon->Holster( NULL );
	}

	return true;
}

// Expose our interface in place of HL2's.
static CHiddenGameMovement g_GameMovement;
IGameMovement *g_pGameMovement = ( IGameMovement * )&g_GameMovement;

EXPOSE_SINGLE_INTERFACE_GLOBALVAR( CGameMovement, IGameMovement, INTERFACENAME_GAMEMOVEMENT, g_GameMovement );
