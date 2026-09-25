//========= Hidden: Source =====================================================//
//
// Purpose: The Hidden player: marine class and character, loadout, team setup
//			and Hidden selection state. See docs/spec/teams-classes.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_player.h"
#include "hidden_gamerules.h"
#include "hidden_cvars.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HIDDEN_MODEL_MARINE			"models/player/iris.mdl"
#define HIDDEN_MODEL_MARINE_SUPPORT	"models/player/iris_supply.mdl"
#define HIDDEN_MODEL_HIDDEN			"models/manor/mn_fixture1.mdl"	// the cloaked Hidden, see docs/spec/client.md
#define HIDDEN_MODEL_HIDDEN_RAGDOLL	"models/player/hidden.mdl"

#define HIDDEN_MARINE_SPEED			180.0f
#define HIDDEN_HIDDEN_SPEED			220.0f
#define HIDDEN_HIDDEN_FOV			110

LINK_ENTITY_TO_CLASS( player, CHidden_Player );

LINK_ENTITY_TO_CLASS( info_marine_spawn, CPointEntity );
LINK_ENTITY_TO_CLASS( info_hidden_spawn, CPointEntity );

// The last spawn point used per team; spawning walks them round-robin, as in Beta 4b.
static EHANDLE g_hLastMarineSpawn;
static EHANDLE g_hLastHiddenSpawn;

IMPLEMENT_SERVERCLASS_ST( CHidden_Player, DT_Hidden_Player )
	SendPropInt( SENDINFO( m_iPlayerClass ), 3 ),
	SendPropInt( SENDINFO( m_iCharacter ), 5 ),
	SendPropInt( SENDINFO( m_iPrimary ), 5 ),
	SendPropInt( SENDINFO( m_iSecondary ), 5 ),
	SendPropInt( SENDINFO( m_iEquipment ), 5 ),
	SendPropBool( SENDINFO( m_bNoHidden ) ),
	SendPropInt( SENDINFO( m_iWeighting ), 16 ),
	SendPropBool( SENDINFO( m_bSafety ) ),
	SendPropBool( SENDINFO( m_bZoom ) ),
	SendPropInt( SENDINFO( m_iShotsFired ), 8, SPROP_UNSIGNED ),
END_SEND_TABLE()

BEGIN_DATADESC( CHidden_Player )
END_DATADESC()

CHidden_Player::CHidden_Player()
{
	m_iPlayerClass = HIDDEN_CLASS_NONE;
	m_iCharacter = -1;
	m_iPrimary = HIDDEN_LOADOUT_NONE;
	m_iSecondary = HIDDEN_LOADOUT_NONE;
	m_iEquipment = HIDDEN_LOADOUT_NONE;
	m_bNoHidden = false;
	m_iWeighting = 0;
	m_bReadyToPlay = false;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;
	m_bHadHidden = false;
}

void CHidden_Player::Precache( void )
{
	BaseClass::Precache();

	PrecacheModel( HIDDEN_MODEL_MARINE );
	PrecacheModel( HIDDEN_MODEL_MARINE_SUPPORT );
	PrecacheModel( HIDDEN_MODEL_HIDDEN );
	PrecacheModel( HIDDEN_MODEL_HIDDEN_RAGDOLL );
}

void CHidden_Player::InitialSpawn( void )
{
	BaseClass::InitialSpawn();

	// New players watch until they pick a class and character and the next round starts.
	// Setting the team first also stops CHL2MP_Player::Spawn from picking one.
	ChangeTeam( TEAM_SPECTATOR );
}

void CHidden_Player::Spawn( void )
{
	m_iWeighting = 0;
	m_bSafety = false;
	m_bZoom = false;
	m_iShotsFired = 0;

	const int iTeam = GetTeamNumber();
	const bool bMarine = ( iTeam == TEAM_IRIS && HasValidCharacter() && m_iPlayerClass != HIDDEN_CLASS_NONE );
	const bool bPlaying = m_bReadyToPlay && ( bMarine || iTeam == TEAM_HIDDEN );

	// The model decides the hull, so set it before the base spawn. Players who sit out still need a
	// valid model: the client's player code expects one.
	if ( bPlaying && iTeam == TEAM_HIDDEN )
		SetModel( HIDDEN_MODEL_HIDDEN );
	else if ( bPlaying && m_iPlayerClass == HIDDEN_CLASS_SUPPORT )
		SetModel( HIDDEN_MODEL_MARINE_SUPPORT );
	else
		SetModel( HIDDEN_MODEL_MARINE );

	BaseClass::Spawn();

	if ( iTeam == TEAM_SPECTATOR )
		return;	// CBasePlayer::Spawn already started observer mode

	if ( !bPlaying )
	{
		BecomeObserver();
		return;
	}

	// Drop what CHL2MP_Player::Spawn gave out and hand out our own.
	RemoveAllItems( false );
	EquipSuit();

	if ( iTeam == TEAM_HIDDEN )
		SetupHidden();
	else
		SetupMarine();
}

void CHidden_Player::SetupMarine( void )
{
	m_nSkin = m_iCharacter;
	m_nBody = m_iCharacter;
	SetBodygroup( 2, 2 );
	SetMaxSpeed( HIDDEN_MARINE_SPEED );
	SetFOV( this, 0 );

	GiveMarineLoadout();
}

void CHidden_Player::SetupHidden( void )
{
	m_nSkin = 1;
	m_nBody = 1;
	SetMaxSpeed( HIDDEN_HIDDEN_SPEED );
	SetFOV( this, HIDDEN_HIDDEN_FOV );

	GiveHiddenLoadout();
}

void CHidden_Player::GiveMarineLoadout( void )
{
	// Nothing chosen yet, or "9": pick at random.
	if ( m_iPrimary < 0 || m_iPrimary >= HIDDEN_PRIMARY_COUNT )
		m_iPrimary = random->RandomInt( 0, HIDDEN_PRIMARY_COUNT - 1 );
	if ( m_iSecondary < 0 || m_iSecondary >= HIDDEN_SECONDARY_COUNT )
		m_iSecondary = random->RandomInt( 0, HIDDEN_SECONDARY_COUNT - 1 );
	if ( m_iEquipment < 0 || m_iEquipment >= HIDDEN_EQUIPMENT_COUNT )
		m_iEquipment = random->RandomInt( 0, HIDDEN_EQUIPMENT_COUNT - 1 );

	// Support marines can't carry the FN2000 or the shotgun.
	if ( m_iPlayerClass == HIDDEN_CLASS_SUPPORT &&
		( m_iPrimary == HIDDEN_PRIMARY_FN2000 || m_iPrimary == HIDDEN_PRIMARY_SHOTGUN ) )
	{
		m_iPrimary = HIDDEN_PRIMARY_P90;
	}

	// Ammo is given in rounds and clamped to the ammo type's magazine count (docs/spec/weapons.md).
	switch ( m_iSecondary )
	{
	case HIDDEN_SECONDARY_PISTOL:
		GiveNamedItem( "weapon_pistol" );
		CBasePlayer::GiveAmmo( 48, "AMMO_PISTOL" );
		break;
	case HIDDEN_SECONDARY_PISTOL2:
		GiveNamedItem( "weapon_pistol2" );
		CBasePlayer::GiveAmmo( 48, "AMMO_9MM" );
		break;
	}

	switch ( m_iEquipment )
	{
	case HIDDEN_EQUIPMENT_SONIC:
		GiveNamedItem( "weapon_sonic" );
		break;
	}

	if ( m_iPlayerClass == HIDDEN_CLASS_SUPPORT )
	{
		if ( m_iEquipment != HIDDEN_EQUIPMENT_SONIC )
			GiveNamedItem( "weapon_sonic" );
		CBasePlayer::GiveAmmo( m_iEquipment == HIDDEN_EQUIPMENT_SONIC ? 3 : 2, "sonic" );
	}

	switch ( m_iPrimary )
	{
	case HIDDEN_PRIMARY_FN2000:
		GiveNamedItem( "weapon_fn2000" );
		CBasePlayer::GiveAmmo( 60, "AMMO_556" );
		break;
	case HIDDEN_PRIMARY_P90:
		GiveNamedItem( "weapon_p90" );
		CBasePlayer::GiveAmmo( 100, "AMMO_BULLETS" );
		break;
	case HIDDEN_PRIMARY_SHOTGUN:
		GiveNamedItem( "weapon_shotgun" );
		CBasePlayer::GiveAmmo( 24, "AMMO_BUCKSHOT" );
		break;
	case HIDDEN_PRIMARY_FN303:
		GiveNamedItem( "weapon_fn303" );
		CBasePlayer::GiveAmmo( 45, "XBowBolt" );
		break;
	}
}

void CHidden_Player::GiveHiddenLoadout( void )
{
	// Pipe bombs: none in OverRun; three without hdn_limitbombs; otherwise half the marines (rounded
	// up, as Beta 4b's FPU rounding mode does), less one.
	int iBombs = 3;
	if ( HiddenRules()->GetGameType() == HIDDEN_GAMETYPE_OVERRUN )
	{
		iBombs = 0;
	}
	else if ( hdn_limitbombs.GetBool() )
	{
		CTeam *pIRIS = GetGlobalTeam( TEAM_IRIS );
		const int iMarines = pIRIS ? pIRIS->GetNumPlayers() : 0;
		iBombs = (int)ceilf( iMarines * 0.5f ) - 1;
	}

	if ( iBombs > 0 )
	{
		GiveNamedItem( "weapon_grenade" );
		CBasePlayer::GiveAmmo( iBombs - 1, "AMMO_GRENADE" );	// the weapon itself holds one
	}

	GiveNamedItem( "weapon_knife" );
}

void CHidden_Player::BecomeObserver( void )
{
	if ( IsObserver() )
		return;

	RemoveAllItems( true );
	State_Transition( STATE_OBSERVER_MODE );
}

void CHidden_Player::ChangeTeam( int iTeam )
{
	// Skip CHL2MP_Player::ChangeTeam: it kills players who switch teams and picks HL2 models.
	CHL2_Player::ChangeTeam( iTeam );

	if ( iTeam == TEAM_SPECTATOR )
		BecomeObserver();
}

void CHidden_Player::PlayerDeathThink( void )
{
	BaseClass::PlayerDeathThink();

	// No respawning during a round: the dead watch until the next one.
	if ( !IsObserver() && gpGlobals->curtime > GetDeathTime() + DEATH_ANIMATION_TIME )
		BecomeObserver();
}

CBaseEntity *CHidden_Player::EntSelectSpawnPoint( void )
{
	const bool bHidden = ( GetTeamNumber() == TEAM_HIDDEN );
	const char *pszSpawn = bHidden ? "info_hidden_spawn" : "info_marine_spawn";
	EHANDLE &hLast = bHidden ? g_hLastHiddenSpawn : g_hLastMarineSpawn;

	// Beta 4b walks the spawn points in order and doesn't check whether they're occupied.
	CBaseEntity *pSpot = gEntList.FindEntityByClassname( hLast.Get(), pszSpawn );
	if ( !pSpot )
		pSpot = gEntList.FindEntityByClassname( NULL, pszSpawn );

	if ( !pSpot )
		return BaseClass::EntSelectSpawnPoint();

	hLast = pSpot;
	return pSpot;
}

void CHidden_Player::AddWeighting( int iAmount )
{
	m_iWeighting += iAmount;
	DevMsg( 1, "weighting : %i\n", m_iWeighting.Get() );
}

bool CHidden_Player::TakeCharacter( int iCharacter )
{
	CHiddenRules *pRules = HiddenRules();

	if ( iCharacter == m_iCharacter )
		return true;

	// 9 takes the first free slot.
	if ( iCharacter == HIDDEN_NUM_CHARACTERS )
	{
		for ( int i = 0; i < HIDDEN_NUM_CHARACTERS; i++ )
		{
			if ( !pRules->IsCharacterTaken( i ) )
			{
				iCharacter = i;
				break;
			}
		}
	}

	if ( pRules->IsCharacterTaken( iCharacter ) )
		return false;

	ReleaseCharacter();
	pRules->SetCharacterTaken( iCharacter, true );
	m_iCharacter = iCharacter;
	return true;
}

void CHidden_Player::ReleaseCharacter( void )
{
	if ( HasValidCharacter() )
		HiddenRules()->SetCharacterTaken( m_iCharacter, false );

	m_iCharacter = -1;
}

bool CHidden_Player::ClientCommand( const CCommand &args )
{
	const char *pszCmd = args[0];
	const int iArg = ( args.ArgC() > 1 ) ? atoi( args[1] ) : 0;

	if ( FStrEq( pszCmd, "changeclass" ) )
	{
		m_iPlayerClass = ( iArg >= 0 && iArg < HIDDEN_CLASS_COUNT ) ? iArg : HIDDEN_CLASS_NONE;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "changemarine" ) )
	{
		if ( iArg == -2 )
			return true;	// keep the current character

		if ( !TakeCharacter( iArg ) )
		{
			ClientPrint( this, HUD_PRINTCENTER, "That marine is already taken" );
			return true;
		}

		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "primary" ) )
	{
		m_iPrimary = iArg;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "secondary" ) )
	{
		m_iSecondary = iArg;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "equip" ) )
	{
		m_iEquipment = iArg;
		m_bReadyToPlay = true;
		return true;
	}
	else if ( FStrEq( pszCmd, "enter" ) )
	{
		if ( !HasValidCharacter() )
		{
			m_bReadyToPlay = false;
			ClientPrint( this, HUD_PRINTCENTER, "You will not spawn, use classmenu to pick a valid player" );
			return true;
		}

		// Spectators join IRIS; the next round decides who is the Hidden.
		if ( GetTeamNumber() == TEAM_SPECTATOR || GetTeamNumber() == TEAM_UNASSIGNED )
			ChangeTeam( TEAM_IRIS );
		return true;
	}
	else if ( FStrEq( pszCmd, "choosehidden" ) )
	{
		m_bNoHidden = ( iArg != 0 );
		DevMsg( 1, m_bNoHidden ? "I DON'T Want to be the Hidden\n" : "I Do Want to be the Hidden\n" );
		return true;
	}
	else if ( FStrEq( pszCmd, "spectate" ) )
	{
		ReleaseCharacter();
		m_bReadyToPlay = false;
		HandleCommand_JoinTeam( TEAM_SPECTATOR );
		return true;
	}
	else if ( FStrEq( pszCmd, "jointeam" ) || FStrEq( pszCmd, "joingame" ) || FStrEq( pszCmd, "vguicancel" ) )
	{
		return true;	// the game rules pick teams
	}

	return BaseClass::ClientCommand( args );
}
