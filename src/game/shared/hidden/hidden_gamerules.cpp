//========= Hidden: Source =====================================================//
//
// Purpose: Hidden game rules: rounds, teams, Hidden selection and scoring.
//			See docs/spec/game-rules.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_gamerules.h"

#ifndef CLIENT_DLL
	#include "team.h"
	#include "hidden_player.h"
#endif

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

REGISTER_GAMERULES_CLASS( CHiddenRules );

BEGIN_NETWORK_TABLE_NOBASE( CHiddenRules, DT_HiddenRules )
#ifdef CLIENT_DLL
	RecvPropFloat( RECVINFO( m_flRoundStart ) ),
	RecvPropInt( RECVINFO( m_iRoundDuration ) ),
	RecvPropArray3( RECVINFO_ARRAY( m_bCharacterTaken ), RecvPropBool( RECVINFO( m_bCharacterTaken[0] ) ) ),
#else
	SendPropFloat( SENDINFO( m_flRoundStart ), 0, SPROP_NOSCALE ),
	SendPropInt( SENDINFO( m_iRoundDuration ), 16, SPROP_UNSIGNED ),
	SendPropArray3( SENDINFO_ARRAY3( m_bCharacterTaken ), SendPropBool( SENDINFO_ARRAY( m_bCharacterTaken ) ) ),
#endif
END_NETWORK_TABLE()

LINK_ENTITY_TO_CLASS( hidden_gamerules, CHiddenGameRulesProxy );
IMPLEMENT_NETWORKCLASS_ALIASED( HiddenGameRulesProxy, DT_HiddenGameRulesProxy )

#ifdef CLIENT_DLL
	void RecvProxy_HiddenRules( const RecvProp *pProp, void **pOut, void *pData, int objectID )
	{
		CHiddenRules *pRules = HiddenRules();
		Assert( pRules );
		*pOut = pRules;
	}

	BEGIN_RECV_TABLE( CHiddenGameRulesProxy, DT_HiddenGameRulesProxy )
		RecvPropDataTable( "hidden_gamerules_data", 0, 0, &REFERENCE_RECV_TABLE( DT_HiddenRules ), RecvProxy_HiddenRules )
	END_RECV_TABLE()
#else
	void *SendProxy_HiddenRules( const SendProp *pProp, const void *pStructBase, const void *pData, CSendProxyRecipients *pRecipients, int objectID )
	{
		CHiddenRules *pRules = HiddenRules();
		Assert( pRules );
		return pRules;
	}

	BEGIN_SEND_TABLE( CHiddenGameRulesProxy, DT_HiddenGameRulesProxy )
		SendPropDataTable( "hidden_gamerules_data", 0, &REFERENCE_SEND_TABLE( DT_HiddenRules ), SendProxy_HiddenRules )
	END_SEND_TABLE()
#endif

#ifndef CLIENT_DLL
// Beta 4b's team names. Team 0 was also called "IRIS"; "Unassigned" reads better in logs.
static const char *s_HiddenTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"IRIS",
	"Hidden",
};

// The map name's prefix (the text before the first '_') picks the game type.
static HiddenGameType_t GameTypeForMap( const char *pszMap )
{
	if ( !Q_strnicmp( pszMap, "ovr_", 4 ) )
		return HIDDEN_GAMETYPE_OVERRUN;
	if ( !Q_strnicmp( pszMap, "mtr_", 4 ) )
		return HIDDEN_GAMETYPE_MARINE_TUTORIAL;
	if ( !Q_strnicmp( pszMap, "htr_", 4 ) )
		return HIDDEN_GAMETYPE_HIDDEN_TUTORIAL;
	return HIDDEN_GAMETYPE_HIDDEN;
}
#endif

CHiddenRules::CHiddenRules()
{
	m_nGameType = HIDDEN_GAMETYPE_HIDDEN;
	m_flRoundStart = -1.0f;
	m_iRoundDuration = 0;
	for ( int i = 0; i < HIDDEN_NUM_CHARACTERS; i++ )
		m_bCharacterTaken.Set( i, false );

#ifndef CLIENT_DLL
	// CHL2MPRules created the teams as Combine and Rebels; rename them.
	for ( int i = 0; i < ARRAYSIZE( s_HiddenTeamNames ) && i < g_Teams.Count(); i++ )
		g_Teams[i]->Init( s_HiddenTeamNames[i], i );

	m_nGameType = GameTypeForMap( STRING( gpGlobals->mapname ) );
#endif
}

CHiddenRules::~CHiddenRules()
{
}

const char *CHiddenRules::GetGameDescription( void )
{
	return "Hidden : Source";
}

float CHiddenRules::GetRoundTimeRemaining( void ) const
{
	if ( m_flRoundStart < 0.0f )
		return 0.0f;

	return MAX( 0.0f, m_iRoundDuration - ( gpGlobals->curtime - m_flRoundStart ) );
}

bool CHiddenRules::IsCharacterTaken( int iCharacter ) const
{
	if ( iCharacter < 0 || iCharacter >= HIDDEN_NUM_CHARACTERS )
		return true;

	return m_bCharacterTaken[iCharacter];
}

#ifndef CLIENT_DLL
void CHiddenRules::CreateStandardEntities( void )
{
	// Skip CHL2MPRules, which creates its own proxy; ours carries the HL2MP table as its base.
	CTeamplayRules::CreateStandardEntities();

	CBaseEntity::Create( "hidden_gamerules", vec3_origin, vec3_angle );
}
#endif

#ifndef CLIENT_DLL
void CHiddenRules::SetCharacterTaken( int iCharacter, bool bTaken )
{
	if ( iCharacter >= 0 && iCharacter < HIDDEN_NUM_CHARACTERS )
		m_bCharacterTaken.Set( iCharacter, bTaken );
}

void CHiddenRules::ClientDisconnected( edict_t *pClient )
{
	CHidden_Player *pPlayer = ToHiddenPlayer( CBaseEntity::Instance( pClient ) );
	if ( pPlayer )
		pPlayer->ReleaseCharacter();

	BaseClass::ClientDisconnected( pClient );
}
#endif
