//========= Hidden: Source =====================================================//
//
// Purpose: Playable bots for both teams: the bot player, its locomotion, body
//			and vision, the marines' gun handling, and the hdn_bot_* commands and
//			quota. The behaviours are in hidden_nbot_behavior.cpp.
//			Not a Beta 4b feature: see docs/spec/bots.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_nbot.h"
#include "hidden_gamerules.h"
#include "hidden_corpse.h"
#include "NextBotManager.h"
#include "func_break.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar hdn_bot_difficulty( "hdn_bot_difficulty", "1", FCVAR_GAMEDLL, "Skill of new bots: 0 easy, 1 normal, 2 hard, 3 expert", true, 0.0f, true, 3.0f );
ConVar hdn_bot_quota( "hdn_bot_quota", "0", FCVAR_GAMEDLL, "Keep the server filled up to this many players with bots" );
ConVar hdn_bot_join_after_player( "hdn_bot_join_after_player", "1", FCVAR_GAMEDLL, "Only fill the quota once a human has joined" );
ConVar hdn_bot_forfeit_hidden( "hdn_bot_forfeit_hidden", "0", FCVAR_GAMEDLL, "New bots forfeit the Hidden selection (1), so humans play the Hidden" );
ConVar hdn_bot_auto_nav( "hdn_bot_auto_nav", "1", FCVAR_GAMEDLL, "Generate a navigation mesh when bots are wanted on a map without one (reloads the map)" );

//-----------------------------------------------------------------------------
// Nav mesh
//-----------------------------------------------------------------------------
void CHiddenNavMesh::AddWalkableSeeds( void )
{
	static const char *s_pszSpawns[] = { "info_marine_spawn", "info_hidden_spawn", "info_player_start" };

	for ( int i = 0; i < ARRAYSIZE( s_pszSpawns ); i++ )
	{
		for ( CBaseEntity *pSpawn = gEntList.FindEntityByClassname( NULL, s_pszSpawns[i] ); pSpawn;
			pSpawn = gEntList.FindEntityByClassname( pSpawn, s_pszSpawns[i] ) )
		{
			Vector pos = pSpawn->GetAbsOrigin();
			pos.x = SnapToGrid( pos.x );
			pos.y = SnapToGrid( pos.y );

			Vector normal;
			if ( FindGroundForNode( &pos, &normal ) )
				AddWalkableSeed( pos, normal );
		}
	}
}

//-----------------------------------------------------------------------------
// Locomotion
//-----------------------------------------------------------------------------
bool CHiddenBotLocomotion::IsAreaTraversable( const CNavArea *area ) const
{
	return !area->IsBlocked( GetBot()->GetEntity()->GetTeamNumber() );
}

bool CHiddenBotLocomotion::IsEntityTraversable( CBaseEntity *obstacle, TraverseWhenType when ) const
{
	// Players move or die, and held things and corpses are pushed aside.
	if ( obstacle && ( obstacle->IsPlayer() || dynamic_cast< CHiddenCorpse * >( obstacle ) ) )
		return true;

	if ( obstacle && obstacle->VPhysicsGetObject() && ( obstacle->VPhysicsGetObject()->GetGameFlags() & FVPHYSICS_PLAYER_HELD ) )
		return true;

	return PlayerLocomotion::IsEntityTraversable( obstacle, when );
}

//-----------------------------------------------------------------------------
// Body: how often the aim samples its target, so easier bots trail behind.
//-----------------------------------------------------------------------------
float CHiddenBotBody::GetHeadAimTrackingInterval( void ) const
{
	switch ( static_cast< CHiddenBot * >( GetBot() )->GetDifficulty() )
	{
	case CHiddenBot::EXPERT:	return 0.05f;
	case CHiddenBot::HARD:		return 0.1f;
	case CHiddenBot::NORMAL:	return 0.25f;
	default:					return 0.6f;
	}
}

//-----------------------------------------------------------------------------
// Vision
//-----------------------------------------------------------------------------
void CHiddenBotVision::CollectPotentiallyVisibleEntities( CUtlVector< CBaseEntity * > *potentiallyVisible )
{
	potentiallyVisible->RemoveAll();

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsConnected() && pPlayer->IsAlive() && !pPlayer->IsObserver() )
			potentiallyVisible->AddToTail( pPlayer );
	}
}

bool CHiddenBotVision::IsIgnored( CBaseEntity *subject ) const
{
	CHiddenBot *me = static_cast< CHiddenBot * >( GetBot()->GetEntity() );
	if ( !me->IsEnemy( subject ) )
		return false;

	return subject->IsEffectActive( EF_NODRAW );
}

bool CHiddenBotVision::IsVisibleEntityNoticed( CBaseEntity *subject ) const
{
	CHiddenBot *me = static_cast< CHiddenBot * >( GetBot()->GetEntity() );
	if ( subject->GetTeamNumber() != TEAM_HIDDEN || me->GetTeamNumber() == TEAM_HIDDEN )
		return true;

	const float flRange = ( subject->GetAbsOrigin() - me->GetAbsOrigin() ).Length();
	if ( flRange < 100.0f )
		return true;

	const int iIndex = subject->entindex();
	if ( iIndex <= 0 || iIndex > MAX_PLAYERS )
		return false;

	const float flElapsed = clamp( gpGlobals->curtime - m_flLastNoticeCheck[iIndex], 0.0f, 1.0f );
	m_flLastNoticeCheck[iIndex] = gpGlobals->curtime;

	const float flSpeed = subject->GetAbsVelocity().Length();
	const float flSkill = 0.5f + 0.25f * me->GetDifficulty();

	// Keeping track of it once spotted is a chance too, so a moving Hidden flickers in and out of
	// view (and fire): about 3.5 times a second while it stands still, 1.5 at full speed.
	const CKnownEntity *known = GetKnown( subject );
	if ( known && known->IsVisibleRecently() && flRange < 900.0f )
	{
		const float flTrack = ( 3.5f - 2.0f * clamp( flSpeed / 220.0f, 0.0f, 1.0f ) ) * flSkill;
		return RandomFloat() < flTrack * flElapsed;
	}

	// Spotting it: about 0.3 a second at 600 units for a normal bot, twice that if it stands
	// still, none beyond 1200.
	float flPerSecond = clamp( ( 1200.0f - flRange ) / 1200.0f, 0.0f, 1.0f ) * 0.6f;
	if ( flSpeed < 30.0f )
		flPerSecond *= 2.0f;
	flPerSecond *= flSkill;

	return RandomFloat() < flPerSecond * flElapsed;
}

float CHiddenBotVision::GetMinRecognizeTime( void ) const
{
	switch ( static_cast< CHiddenBot * >( GetBot() )->GetDifficulty() )
	{
	case CHiddenBot::EXPERT:	return 0.2f;
	case CHiddenBot::HARD:		return 0.3f;
	case CHiddenBot::NORMAL:	return 0.5f;
	default:					return 1.0f;
	}
}

//-----------------------------------------------------------------------------
// Path cost
//-----------------------------------------------------------------------------
float CHiddenBotPathCost::operator()( CNavArea *area, CNavArea *fromArea, const CNavLadder *ladder, const CFuncElevator *elevator, float length ) const
{
	if ( !fromArea )
		return 0.0f;

	ILocomotion *mover = m_me->GetLocomotionInterface();
	if ( !mover->IsAreaTraversable( area ) )
		return -1.0f;

	float flDist;
	if ( ladder )
		flDist = ladder->m_length;
	else if ( length > 0.0f )
		flDist = length;
	else
		flDist = ( area->GetCenter() - fromArea->GetCenter() ).Length();

	float flCost = flDist + fromArea->GetCostSoFar();

	const float flDeltaZ = fromArea->ComputeAdjacentConnectionHeightChange( area );
	if ( flDeltaZ >= mover->GetStepHeight() )
	{
		if ( flDeltaZ >= mover->GetMaxJumpHeight() )
			return -1.0f;
		flCost += 5.0f * flDist;
	}
	else if ( flDeltaZ < -mover->GetDeathDropHeight() )
	{
		return -1.0f;
	}

	// Some randomness per bot so they don't all take the same corridor.
	flCost *= 1.0f + 0.3f * ( ( m_me->entindex() * 7 + area->GetID() * 13 ) % 10 ) / 10.0f;
	return flCost;
}

//-----------------------------------------------------------------------------
// The bot
//-----------------------------------------------------------------------------
LINK_ENTITY_TO_CLASS( hidden_bot, CHiddenBot );

CBasePlayer *CHiddenBot::AllocatePlayerEntity( edict_t *edict, const char *playerName )
{
	CBasePlayer::s_PlayerEdict = edict;
	return static_cast< CBasePlayer * >( CreateEntityByName( "hidden_bot" ) );
}

CHiddenBot::CHiddenBot()
{
	m_body = new CHiddenBotBody( this );
	m_locomotor = new CHiddenBotLocomotion( this );
	m_vision = new CHiddenBotVision( this );
	ALLOCATE_INTENTION_INTERFACE( CHiddenBot );

	m_difficulty = (DifficultyType)hdn_bot_difficulty.GetInt();
	m_bLoner = RandomFloat() < 0.35f;
	m_bFireToggle = false;
}

CHiddenBot::~CHiddenBot()
{
	DEALLOCATE_INTENTION_INTERFACE;

	delete m_vision;
	delete m_locomotor;
	delete m_body;
}

void CHiddenBot::Spawn( void )
{
	BaseClass::Spawn();
	m_bFireToggle = false;
}

bool CHiddenBot::IsEnemy( const CBaseEntity *them ) const
{
	if ( !them || !IsOnPlayingTeam() )
		return false;

	const int iTheirTeam = them->GetTeamNumber();
	return ( iTheirTeam == TEAM_IRIS || iTheirTeam == TEAM_HIDDEN ) && iTheirTeam != GetTeamNumber();
}

bool CHiddenBot::IsLineOfFireClear( const Vector &where )
{
	trace_t tr;
	UTIL_TraceLine( EyePosition(), where, MASK_SOLID_BRUSHONLY, this, COLLISION_GROUP_NONE, &tr );
	return !tr.DidHit();
}

static bool HasAmmo( CHidden_Player *pPlayer, CBaseCombatWeapon *pWeapon )
{
	if ( !pWeapon )
		return false;
	if ( pWeapon->UsesClipsForAmmo1() && pWeapon->Clip1() > 0 )
		return true;
	return pWeapon->GetPrimaryAmmoType() >= 0 && pPlayer->GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) > 0;
}

// Primary, then pistol; the sonic alarm and anything else don't count as guns.
void CHiddenBot::EquipBestGun( void )
{
	static const char *s_pszGuns[] = { "weapon_fn2000", "weapon_p90", "weapon_shotgun", "weapon_fn303", "weapon_pistol", "weapon_pistol2" };

	for ( int i = 0; i < ARRAYSIZE( s_pszGuns ); i++ )
	{
		CBaseCombatWeapon *pWeapon = Weapon_OwnsThisType( s_pszGuns[i] );
		if ( !HasAmmo( this, pWeapon ) )
			continue;

		if ( GetActiveWeapon() != pWeapon )
			Weapon_Switch( pWeapon );
		return;
	}
}

bool CHiddenBot::IsCloseRangeGun( void ) const
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	return pWeapon && FClassnameIs( pWeapon, "weapon_shotgun" );
}

void CHiddenBot::ReloadIfNeeded( bool bInCombat )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( !pWeapon || !pWeapon->UsesClipsForAmmo1() || pWeapon->m_bInReload )
		return;
	if ( pWeapon->GetPrimaryAmmoType() < 0 || GetAmmoCount( pWeapon->GetPrimaryAmmoType() ) <= 0 )
		return;

	// Empty, or low while nothing's going on (a reload throws the rest of the magazine away).
	if ( pWeapon->Clip1() <= 0 || ( !bInCombat && pWeapon->Clip1() < pWeapon->GetMaxClip1() / 3 ) )
		PressReloadButton();
}

// Hold the trigger on the automatics, tap everything else.
void CHiddenBot::FireGun( void )
{
	CBaseCombatWeapon *pWeapon = GetActiveWeapon();
	if ( !pWeapon )
		return;

	if ( FClassnameIs( pWeapon, "weapon_fn2000" ) || FClassnameIs( pWeapon, "weapon_p90" ) )
	{
		PressFireButton( 0.15f );
		return;
	}

	m_bFireToggle = !m_bFireToggle;
	if ( m_bFireToggle )
		PressFireButton();
	else
		ReleaseFireButton();
}

//-----------------------------------------------------------------------------
// Adding and removing bots
//-----------------------------------------------------------------------------
static const char *s_pszBotNames[] =
{
	"Ash", "Bishop", "Crowe", "Dietrich", "Ellis", "Frost", "Graves", "Hicks", "Irving", "Jonas",
	"Kowalski", "Lang", "Mercer", "Novak", "Ortega", "Pryce", "Quinn", "Reyes", "Stone", "Vance",
};

static void BotClientCommand( CHiddenBot *pBot, const char *pszFormat, ... )
{
	char szCommand[128];
	va_list args;
	va_start( args, pszFormat );
	Q_vsnprintf( szCommand, sizeof( szCommand ), pszFormat, args );
	va_end( args );

	CCommand command;
	command.Tokenize( szCommand );
	pBot->ClientCommand( command );
}

static bool IsNameTaken( const char *pszName )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && !Q_stricmp( pPlayer->GetPlayerName(), pszName ) )
			return true;
	}
	return false;
}

static void CheckNavMesh( void )
{
	if ( TheNavMesh->IsLoaded() || TheNavMesh->IsGenerating() )
		return;

	if ( !hdn_bot_auto_nav.GetBool() )
	{
		Warning( "Bots: %s has no navigation mesh, so bots can't find their way. Run nav_generate.\n", STRING( gpGlobals->mapname ) );
		return;
	}

	UTIL_ClientPrintAll( HUD_PRINTTALK, "Generating a navigation mesh for the bots; the map restarts when it's done.\n" );
	Msg( "Bots: generating a navigation mesh for %s\n", STRING( gpGlobals->mapname ) );
	TheNavMesh->BeginGeneration();
}

static CHiddenBot *AddBot( int iTeamPref )
{
	CheckNavMesh();

	char szName[64];
	int iTries = 0;
	do
	{
		Q_snprintf( szName, sizeof( szName ), "%s", s_pszBotNames[RandomInt( 0, ARRAYSIZE( s_pszBotNames ) - 1 )] );
		if ( ++iTries > 20 )
			Q_snprintf( szName, sizeof( szName ), "%s %d", s_pszBotNames[RandomInt( 0, ARRAYSIZE( s_pszBotNames ) - 1 )], RandomInt( 2, 99 ) );
	} while ( IsNameTaken( szName ) && iTries < 40 );

	CHiddenBot *pBot = NextBotCreatePlayerBot< CHiddenBot >( szName );
	if ( !pBot )
		return NULL;

	// As a player would: a class, the first free character, a loadout, then in.
	const bool bForfeit = ( iTeamPref == TEAM_IRIS ) || ( iTeamPref == TEAM_UNASSIGNED && hdn_bot_forfeit_hidden.GetBool() );
	BotClientCommand( pBot, "choosehidden %d", bForfeit ? 1 : 0 );
	BotClientCommand( pBot, "changeclass %d", RandomInt( 0, HIDDEN_CLASS_COUNT - 1 ) );
	BotClientCommand( pBot, "changemarine %d", HIDDEN_NUM_CHARACTERS );
	BotClientCommand( pBot, "primary %d", HIDDEN_LOADOUT_RANDOM );
	BotClientCommand( pBot, "secondary %d", HIDDEN_LOADOUT_RANDOM );
	BotClientCommand( pBot, "equip %d", HIDDEN_LOADOUT_RANDOM );
	BotClientCommand( pBot, "enter" );
	return pBot;
}

static int CountBots( void )
{
	int iCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		if ( ToHiddenBot( UTIL_PlayerByIndex( i ) ) )
			iCount++;
	}
	return iCount;
}

static void KickBot( CHiddenBot *pBot )
{
	engine->ServerCommand( UTIL_VarArgs( "kickid %d\n", pBot->GetUserID() ) );
}

// Deviation: Beta 4b's bot_add made the SDK template's test bots (now bot_add_test, hidden_bot.cpp).
CON_COMMAND_F( bot_add, "Add bots: bot_add [count] [iris]; 'iris' bots never become the Hidden", FCVAR_GAMEDLL )
{
	if ( !HiddenIsCommandIssuedByServerAdmin( "bot_add" ) )
		return;

	int iCount = 1;
	int iTeamPref = TEAM_UNASSIGNED;
	for ( int i = 1; i < args.ArgC(); i++ )
	{
		if ( !Q_stricmp( args[i], "iris" ) || !Q_stricmp( args[i], "marine" ) )
			iTeamPref = TEAM_IRIS;
		else if ( atoi( args[i] ) > 0 )
			iCount = clamp( atoi( args[i] ), 1, 32 );
	}

	while ( iCount-- > 0 )
	{
		if ( !AddBot( iTeamPref ) )
			break;
	}

	// Don't let the quota take them straight back out.
	if ( hdn_bot_quota.GetInt() > 0 && hdn_bot_quota.GetInt() < CountBots() )
		hdn_bot_quota.SetValue( CountBots() );
}

CON_COMMAND_F( bot_kick, "Remove a bot by name, or all of them; 'bot_kick all' also sets hdn_bot_quota to 0", FCVAR_GAMEDLL )
{
	if ( !HiddenIsCommandIssuedByServerAdmin( "bot_kick" ) )
		return;

	// No name: kick them all but keep the quota (nav_generate issues a bare bot_kick before the reload).
	const char *pszWho = args.ArgC() > 1 ? args[1] : "all";
	const bool bAll = !Q_stricmp( pszWho, "all" );
	if ( bAll && args.ArgC() > 1 )
		hdn_bot_quota.SetValue( 0 );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHiddenBot *pBot = ToHiddenBot( UTIL_PlayerByIndex( i ) );
		if ( pBot && ( bAll || !Q_stricmp( pBot->GetPlayerName(), pszWho ) ) )
			KickBot( pBot );
	}
}

//-----------------------------------------------------------------------------
// hdn_bot_quota: add or remove a bot a second until the player count matches.
//-----------------------------------------------------------------------------
class CHiddenBotQuota : public CAutoGameSystemPerFrame
{
public:
	CHiddenBotQuota() : CAutoGameSystemPerFrame( "CHiddenBotQuota" ), m_flNextCheck( 0.0f ) {}

	virtual void LevelInitPostEntity( void ) { m_flNextCheck = gpGlobals->curtime + 3.0f; }

	virtual void FrameUpdatePostEntityThink( void )
	{
		if ( gpGlobals->curtime < m_flNextCheck || !g_pGameRules )
			return;
		m_flNextCheck = gpGlobals->curtime + 1.0f;

		const int iQuota = MIN( hdn_bot_quota.GetInt(), gpGlobals->maxClients );
		if ( iQuota <= 0 && CountBots() == 0 )
			return;

		int iHumans = 0, iBots = 0;
		CHiddenBot *pLastBot = NULL;
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || !pPlayer->IsConnected() || pPlayer->IsHLTV() || pPlayer->IsReplay() )
				continue;

			CHiddenBot *pBot = ToHiddenBot( pPlayer );
			if ( pBot )
			{
				iBots++;
				pLastBot = pBot;
			}
			else if ( !( pPlayer->GetFlags() & FL_FAKECLIENT ) )
			{
				iHumans++;
			}
		}

		if ( iQuota <= 0 )
			return;

		if ( hdn_bot_join_after_player.GetBool() && iHumans == 0 )
		{
			if ( pLastBot )
				KickBot( pLastBot );
			return;
		}

		if ( iHumans + iBots < iQuota )
			AddBot( TEAM_UNASSIGNED );
		else if ( iHumans + iBots > iQuota && pLastBot )
			KickBot( pLastBot );
	}

private:
	float m_flNextCheck;
};

static CHiddenBotQuota s_HiddenBotQuota;
