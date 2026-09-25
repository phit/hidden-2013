//========= Hidden: Source =====================================================//
//
// Purpose: Client side of the Hidden player.
//
//=============================================================================//

#include "cbase.h"
#include "c_hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

LINK_ENTITY_TO_CLASS( player, C_Hidden_Player );

IMPLEMENT_CLIENTCLASS_DT( C_Hidden_Player, DT_Hidden_Player, CHidden_Player )
	RecvPropInt( RECVINFO( m_iPlayerClass ) ),
	RecvPropInt( RECVINFO( m_iCharacter ) ),
	RecvPropInt( RECVINFO( m_iPrimary ) ),
	RecvPropInt( RECVINFO( m_iSecondary ) ),
	RecvPropInt( RECVINFO( m_iEquipment ) ),
	RecvPropBool( RECVINFO( m_bNoHidden ) ),
	RecvPropInt( RECVINFO( m_iWeighting ) ),
END_RECV_TABLE()

C_Hidden_Player::C_Hidden_Player()
{
	m_iPlayerClass = HIDDEN_CLASS_NONE;
	m_iCharacter = -1;
	m_iPrimary = HIDDEN_LOADOUT_NONE;
	m_iSecondary = HIDDEN_LOADOUT_NONE;
	m_iEquipment = HIDDEN_LOADOUT_NONE;
	m_bNoHidden = false;
	m_iWeighting = 0;
}

C_Hidden_Player *C_Hidden_Player::GetLocalHiddenPlayer( void )
{
	return ToHiddenPlayer( C_BasePlayer::GetLocalPlayer() );
}
