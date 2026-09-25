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
	RecvPropBool( RECVINFO( m_bSafety ) ),
	RecvPropBool( RECVINFO( m_bZoom ) ),
	RecvPropInt( RECVINFO( m_iShotsFired ) ),
	RecvPropBool( RECVINFO( m_bStunned ) ),
	RecvPropFloat( RECVINFO( m_flBlur ) ),
	RecvPropBool( RECVINFO( m_bLAM ) ),
	RecvPropBool( RECVINFO( m_bNightVision ) ),
END_RECV_TABLE()

// Weapons change these in predicted code.
BEGIN_PREDICTION_DATA( C_Hidden_Player )
	DEFINE_PRED_FIELD( m_bZoom, FIELD_BOOLEAN, FTYPEDESC_INSENDTABLE ),
	DEFINE_PRED_FIELD( m_iShotsFired, FIELD_INTEGER, FTYPEDESC_INSENDTABLE ),
END_PREDICTION_DATA()

C_Hidden_Player::C_Hidden_Player()
{
	m_iPlayerClass = HIDDEN_CLASS_NONE;
	m_iCharacter = -1;
	m_iPrimary = HIDDEN_LOADOUT_NONE;
	m_iSecondary = HIDDEN_LOADOUT_NONE;
	m_iEquipment = HIDDEN_LOADOUT_NONE;
	m_bNoHidden = false;
	m_iWeighting = 0;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;
	m_bStunned = false;
	m_flBlur = 0.0f;
	m_bLAM = false;
	m_bNightVision = false;
}

C_Hidden_Player *C_Hidden_Player::GetLocalHiddenPlayer( void )
{
	return ToHiddenPlayer( C_BasePlayer::GetLocalPlayer() );
}
