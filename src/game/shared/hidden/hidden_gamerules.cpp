//========= Hidden: Source =====================================================//
//
// Purpose: Hidden game rules: rounds, teams, Hidden selection and scoring.
//			See docs/spec/game-rules.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_gamerules.h"
#include "ammodef.h"

#ifndef CLIENT_DLL
	#include "team.h"
	#include "hidden_player.h"
	#include "weapon_hiddenbase.h"
	#include "hidden_cvars.h"
	#include "mapentities.h"
	#include "eventqueue.h"
	#include "checksum_crc.h"
	#include "viewport_panel_names.h"
	#include "gameinterface.h"
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

// Beta 4b's ammo types (docs/spec/weapons.md). Max carry counts magazines, not rounds: a reload
// always fills the clip and uses up one (CWeaponHiddenBase::FinishReload). Buckshot counts shells.
CAmmoDef *GetAmmoDef()
{
	static CAmmoDef def;
	static bool bInitted = false;

	if ( !bInitted )
	{
		bInitted = true;

		//				name				damage type						tracer					plr	npc	carry	force		flags
		def.AddAmmoType( "AMMO_GRENADE",	DMG_BLAST,						TRACER_LINE,			0,	0,	10,		75.0f,		0 );
		def.AddAmmoType( "AMMO_BULLETS",	DMG_BULLET,						TRACER_LINE_AND_WHIZ,	0,	0,	2,		2345.0f,	0 );
		def.AddAmmoType( "AMMO_556",		DMG_BULLET,						TRACER_LINE_AND_WHIZ,	0,	0,	2,		2345.0f,	0 );
		def.AddAmmoType( "AMMO_PISTOL",		DMG_BULLET,						TRACER_LINE,			0,	0,	3,		2000.0f,	0 );
		def.AddAmmoType( "AMMO_9MM",		DMG_BULLET,						TRACER_LINE,			0,	0,	3,		2000.0f,	0 );
		def.AddAmmoType( "AMMO_BUCKSHOT",	DMG_BULLET | DMG_BUCKSHOT,		TRACER_LINE,			0,	0,	38,		2600.0f,	0 );
		def.AddAmmoType( "XBowBolt",		DMG_BULLET | DMG_POISON,		TRACER_LINE,			0,	0,	4,		75.0f,		0 );	// 0x20002 as in Beta 4b
		def.AddAmmoType( "sonic",			DMG_BLAST,						TRACER_LINE,			0,	0,	10,		75.0f,		0 );
	}

	return &def;
}

#ifndef CLIENT_DLL
// Beta 4b's team names. Team 0 was also called "IRIS"; "Unassigned" reads better in logs.
static const char *s_HiddenTeamNames[] =
{
	"Unassigned",
	"Spectator",
	"IRIS",
	"Hidden",
};

// Beta 4b's CleanUpMap preserve list (docs/spec/game-rules.md): doors, rotating brushes and ambient
// sounds keep their state between rounds. Our own game rules proxy replaces sdk_gamerules.
static const char *s_HiddenPreserveEnts[] =
{
	"ai_network", "ai_hint", "ambient_generic", "hidden_gamerules", "team_manager", "player_manager",
	"env_soundscape", "env_soundscape_proxy", "env_soundscape_triggerable", "env_sun", "env_wind",
	"env_fog_controller", "func_brush", "func_door", "func_wall", "func_illusionary", "func_rotating",
	"infodecal", "info_projecteddecal", "info_node", "info_target", "info_node_hint",
	"info_marine_spawn", "info_hidden_spawn", "info_spectator", "info_map_parameters", "keyframe_rope",
	"move_rope", "info_ladder", "player", "hidden_bot", "point_viewcontrol", "scene_manager", "shadow_control",
	"sky_camera", "soundent", "trigger_soundscape", "viewmodel", "predicted_viewmodel", "worldspawn",
	"point_devshot_camera",
	"", // END Marker
};

extern bool FindInList( const char **pStrings, const char *pToFind );
extern ConVar mp_chattime;

// CRC of a file as the client sees it, for the material_check event.
static CRC32_t MaterialCRC( const char *pszPath )
{
	char szPath[MAX_PATH];
	Q_strncpy( szPath, pszPath, sizeof( szPath ) );
	Q_strlower( szPath );

	int iLength = 0;
	byte *pData = UTIL_LoadFileForMe( szPath, &iLength );
	if ( !pData )
		return 0;

	CRC32_t nCRC = CRC32_ProcessSingleBuffer( pData, iLength );
	UTIL_FreeFile( pData );
	return nCRC;
}

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

	m_nRoundState = ROUND_INTERMISSION;
	m_iMarineCount = 0;
	m_iHiddenCount = 0;
	m_bLastRoundAnnounced = false;
	m_bLevelChanged = false;
	m_nMaterialCRC = MaterialCRC( "materials/models/manor/mn_tapestry.vmt" );
	m_nMaterialDX7CRC = MaterialCRC( "materials/models/manor/mn_tapestry_dx7.vmt" );

	m_iSurvivalLeft = 0;

	if ( IsTutorial() )
	{
		// Beta 4b also limited tutorials to one player (gpGlobals->maxClients = 1); we leave the
		// engine's count alone.
		m_nRoundState = ROUND_TUTORIAL_CONFIG;
	}
	else
	{
		// The first round starts hdn_jointime seconds after the map loads.
		GoToIntermission();
		m_flIntermissionEnd = gpGlobals->curtime + hdn_jointime.GetFloat();
	}
#endif
}

CHiddenRules::~CHiddenRules()
{
}

const char *CHiddenRules::GetGameDescription( void )
{
	return "Hidden : Source";
}

bool CHiddenRules::ShouldCollide( int collisionGroup0, int collisionGroup1 )
{
	if ( collisionGroup0 > collisionGroup1 )
		V_swap( collisionGroup0, collisionGroup1 );

	// The Hidden walks over weapons, and traces without a group go through marine_clip.
	// HL2MP already keeps players' movement off weapons.
	if ( collisionGroup0 == COLLISION_GROUP_WEAPON && collisionGroup1 == HIDDEN_COLLISION_GROUP_HIDDEN )
		return false;
	if ( collisionGroup0 == COLLISION_GROUP_NONE && collisionGroup1 == HIDDEN_COLLISION_GROUP_MARINE_CLIP )
		return false;

	return BaseClass::ShouldCollide( collisionGroup0, collisionGroup1 );
}

float CHiddenRules::GetRoundTimeRemaining( void ) const
{
	if ( m_flRoundStart < 0.0f )
		return 0.0f;

	return MAX( 0.0f, m_iRoundDuration - ( gpGlobals->curtime - m_flRoundStart ) );
}

// Whole seconds left in the round, as Beta 4b counts them for OverRun's timers and the HUD.
int CHiddenRules::GetRoundTimerRemain( void ) const
{
	if ( m_flRoundStart < 0.0f )
		return 0;

	return m_iRoundDuration - RoundFloatToInt( gpGlobals->curtime - m_flRoundStart );
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
	{
		if ( pPlayer->GetTeamNumber() == TEAM_IRIS && pPlayer->IsAlive() )
		{
			m_iMarineCount--;
		}
		else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		{
			// If the last Hidden leaves, IRIS wins on the next think.
			m_iHiddenCount = MAX( 0, m_iHiddenCount - 1 );
			m_Selector.ForfeitHidden( pPlayer );
		}

		pPlayer->ReleaseCharacter();
	}

	BaseClass::ClientDisconnected( pClient );
}

void CHiddenRules::ClientSettingsChanged( CBasePlayer *pPlayer )
{
	// Skip CHL2MPRules: it swaps teams to match cl_playermodel, which puts anyone who changes
	// their name on team 2 (IRIS). Beta 4b's rules only had CTeamplayRules' name handling.
	CTeamplayRules::ClientSettingsChanged( pPlayer );
}

bool CHiddenRules::ClientCommand( CBaseEntity *pEdict, const CCommand &args )
{
	// In a tutorial, joining ends the intermission at once.
	if ( IsTutorial() && pEdict->IsPlayer() && FStrEq( args[0], "enter" ) )
		m_flIntermissionEnd = gpGlobals->curtime;

	return BaseClass::ClientCommand( pEdict, args );
}
#endif

#ifndef CLIENT_DLL
void CHiddenRules::FireSimpleEvent( const char *pszName )
{
	IGameEvent *pEvent = gameeventmanager->CreateEvent( pszName );
	if ( pEvent )
		gameeventmanager->FireEvent( pEvent );
}

bool CHiddenRules::HasTimeLimitPassed( void ) const
{
	const float flTimeLimit = mp_timelimit.GetFloat() * 60.0f;
	return flTimeLimit != 0.0f && gpGlobals->curtime >= flTimeLimit;
}

void CHiddenRules::GameThink( void )
{
	if ( !m_bLastRoundAnnounced && HasTimeLimitPassed() )
	{
		UTIL_ClientPrintAll( HUD_PRINTCENTER, "Last Round" );
		m_bLastRoundAnnounced = true;
	}
	if ( IRISWins() || HiddenWins() )
		m_nRoundState = ROUND_ENDING;
}

void CHiddenRules::Think( void )
{
	// Skip CHL2MPRules and CMultiplayRules: they end the map on their own time and frag limits.
	CGameRules::Think();

	switch ( m_nRoundState )
	{
	case ROUND_INTERMISSION:
		if ( gpGlobals->curtime >= m_flIntermissionEnd )
		{
			DevMsg( 1, "Intermission over, restarting round\n" );
			RestartRound();
		}
		break;

	case ROUND_STARTING:
		FireSimpleEvent( "game_round_start" );
		UTIL_RestartAmbientSounds();
		m_nRoundState = ROUND_MATERIAL_CHECK;
		break;

	case ROUND_MATERIAL_CHECK:
	{
		DevMsg( 1, "Checking Material CRCS, who's been a bad person?\n" );
		IGameEvent *pEvent = gameeventmanager->CreateEvent( "material_check" );
		if ( pEvent )
		{
			pEvent->SetInt( "vmt_CRC", (int)m_nMaterialCRC );
			pEvent->SetInt( "bump_CRC", (int)m_nMaterialDX7CRC );
			gameeventmanager->FireEvent( pEvent );
		}
		m_nRoundState = ROUND_ACTIVE;
		break;
	}

	case ROUND_ACTIVE:
		GameThink();
		break;

	case ROUND_SURVIVAL_START:
		// Tell the living who they are now; the countdown starts next frame.
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( !pPlayer || !pPlayer->IsAlive() )
				continue;

			if ( pPlayer->GetTeamNumber() == TEAM_IRIS )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "Survive" );
			else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
				ClientPrint( pPlayer, HUD_PRINTCENTER, "Eliminate" );
		}
		m_nRoundState = ROUND_SURVIVAL;
		break;

	case ROUND_SURVIVAL:
		GameThink();
		SurvivalThink();
		break;

	case ROUND_TUTORIAL_CONFIG:
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( 1 );
		if ( pPlayer )
			engine->ClientCommand( pPlayer->edict(), "exec tutorial.cfg" );
		GoToIntermission();
		break;
	}

	case ROUND_TUTORIAL:
		break;	// no win checks: a tutorial round never ends

	case ROUND_ENDING:
		FireSimpleEvent( "game_round_end" );
		GoToIntermission();
		break;

	case ROUND_GAME_OVER:
		if ( !m_bLevelChanged && gpGlobals->curtime >= m_flIntermissionEnd )
		{
			DevMsg( 1, "Intermission over, changing levels\n" );
			m_Selector.ClearHidden();
			m_bLevelChanged = true;
			ChangeLevel();
		}
		break;
	}
}

void CHiddenRules::GoToIntermission( void )
{
	m_flIntermissionEnd = gpGlobals->curtime + mp_chattime.GetInt();

	// Safety on: nobody can be hurt until the next round.
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || !pPlayer->IsAlive() )
			continue;

		pPlayer->SetSafe( true );

		CWeaponHiddenBase *pWeapon = dynamic_cast<CWeaponHiddenBase *>( pPlayer->GetActiveWeapon() );
		if ( pWeapon )
			pWeapon->SetSafe();
	}

	m_nRoundState = HasTimeLimitPassed() ? ROUND_GAME_OVER : ROUND_INTERMISSION;
}

void CHiddenRules::RestartRound( void )
{
	CHidden_Player *pHidden = m_Selector.SelectHidden( m_nGameType, hdn_hiddenrounds.GetInt() );
	if ( !pHidden )
	{
		GoToIntermission();	// not enough players; try again later
		return;
	}

	CleanUpMap();
	UTIL_ClientPrintAll( HUD_PRINTCENTER, "Round restarting..." );

	// Everyone who isn't spectating plays: the chosen player as the Hidden, the rest as marines.
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer || pPlayer->GetTeamNumber() == TEAM_SPECTATOR || pPlayer->GetTeamNumber() == TEAM_UNASSIGNED )
			continue;

		pPlayer->ChangeTeam( pPlayer == pHidden ? TEAM_HIDDEN : TEAM_IRIS );
		pPlayer->SetSpawnQueued( false );
		pPlayer->RemoveAllItems( true );
		pPlayer->ShowViewPortPanel( PANEL_SCOREBOARD, false );
	}

	// Spawn the Hidden and the first eight marines; everyone else watches.
	int iMarines = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
		if ( !pPlayer )
			continue;

		const int iTeam = pPlayer->GetTeamNumber();
		if ( iTeam == TEAM_HIDDEN || ( iTeam == TEAM_IRIS && iMarines++ < HIDDEN_MAX_MARINES ) )
		{
			if ( pPlayer->IsObserver() )
				pPlayer->StopObserverMode();
			pPlayer->State_Transition( STATE_ACTIVE );
			pPlayer->Spawn();
		}
		else if ( iTeam != TEAM_SPECTATOR )
		{
			pPlayer->BecomeObserver();
		}
	}

	m_iMarineCount = 0;
	m_iHiddenCount = 0;
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( !pPlayer )
			continue;

		if ( pPlayer->GetTeamNumber() == TEAM_IRIS && pPlayer->IsAlive() )
			m_iMarineCount++;
		else if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
			m_iHiddenCount++;
	}

	DevMsg( 1, "Fired roundrestart event\n" );
	FireSimpleEvent( "game_round_restart" );

	// OverRun starts without a respawn queue or a survivor; a tutorial round just plays.
	m_SpawnQueue.Purge();
	m_hSurvivor = NULL;
	m_nRoundState = IsTutorial() ? ROUND_TUTORIAL : ROUND_STARTING;
	m_iRoundDuration = mp_roundtime.GetInt();
	m_flRoundStart = gpGlobals->curtime;
}

void CHiddenRules::CleanUpMap( void )
{
	// As CHL2MPRules::CleanUpMap, with Beta 4b's preserve list.
	for ( CBaseEntity *pCur = gEntList.FirstEnt(); pCur; pCur = gEntList.NextEnt( pCur ) )
	{
		CBaseCombatWeapon *pWeapon = pCur->MyCombatWeaponPointer();
		if ( pWeapon )
		{
			if ( !pWeapon->GetOwner() )
				UTIL_Remove( pCur );
		}
		else if ( !FindInList( s_HiddenPreserveEnts, pCur->GetClassname() ) )
		{
			UTIL_Remove( pCur );
		}
	}

	gEntList.CleanupDeleteList();
	g_EventQueue.Clear();

	class CHiddenMapEntityFilter : public IMapEntityFilter
	{
	public:
		virtual bool ShouldCreateEntity( const char *pClassname )
		{
			if ( !FindInList( s_HiddenPreserveEnts, pClassname ) )
				return true;

			// Not created, so CreateNextEntity won't advance past it.
			if ( m_iIterator != g_MapEntityRefs.InvalidIndex() )
				m_iIterator = g_MapEntityRefs.Next( m_iIterator );
			return false;
		}

		virtual CBaseEntity *CreateNextEntity( const char *pClassname )
		{
			if ( m_iIterator == g_MapEntityRefs.InvalidIndex() )
			{
				Assert( false );
				return NULL;
			}

			CMapEntityRef &ref = g_MapEntityRefs[m_iIterator];
			m_iIterator = g_MapEntityRefs.Next( m_iIterator );

			// Reuse the entity's old edict slot if it's free, to keep its baseline.
			if ( ref.m_iEdict == -1 || engine->PEntityOfEntIndex( ref.m_iEdict ) )
				return CreateEntityByName( pClassname );
			return CreateEntityByName( pClassname, ref.m_iEdict );
		}

		int m_iIterator;
	};

	CHiddenMapEntityFilter filter;
	filter.m_iIterator = g_MapEntityRefs.Head();
	MapEntity_ParseAllEntities( engine->GetMapEntitiesString(), &filter, true );
}

bool CHiddenRules::IsRoundTimeUp( void )
{
	if ( m_flRoundStart >= 0.0f && RoundFloatToInt( gpGlobals->curtime - m_flRoundStart ) < m_iRoundDuration )
		return false;

	// Time's up: the Hidden loses their place and every surviving marine scores.
	DevMsg( 1, "round timer ended\n" );
	m_Selector.ForfeitHidden( m_Selector.GetCurrentHidden() );

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
		if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS )
			pPlayer->IncrementFragCount( 1 );
	}
	return true;
}

bool CHiddenRules::IRISWins( void )
{
	if ( m_nGameType == HIDDEN_GAMETYPE_OVERRUN )
	{
		CHidden_Player *pSurvivor = m_hSurvivor.Get();
		if ( !pSurvivor )
		{
			RespawnHiddens();
		}
		else if ( pSurvivor->IsAlive() && m_iSurvivalLeft < 1 )
		{
			// The survivor made it, and becomes the next Hidden.
			char szMessage[128];
			Q_snprintf( szMessage, sizeof( szMessage ), "%s Survives!", pSurvivor->GetPlayerName() );
			UTIL_ClientPrintAll( HUD_PRINTCENTER, szMessage );
			m_Selector.NewHidden( pSurvivor );
			pSurvivor->IncrementFragCount( 2 );
			pSurvivor->AddWeighting( 4900 );
			return true;
		}
	}

	if ( m_iHiddenCount > 0 && !IsRoundTimeUp() )
		return false;

	UTIL_ClientPrintAll( HUD_PRINTCENTER, "I.R.I.S. Wins" );
	return true;
}

bool CHiddenRules::HiddenWins( void )
{
	if ( m_iMarineCount > 0 )
		return false;

	UTIL_ClientPrintAll( HUD_PRINTCENTER, m_nGameType == HIDDEN_GAMETYPE_OVERRUN ? "I.R.I.S. Eliminated" : "Hidden Wins" );
	return true;
}

// Killed players come back as the Hidden, until one marine is left: then that marine has to
// survive hdn_survivaltime seconds.
void CHiddenRules::OverRunPlayerKilled( CHidden_Player *pVictim, CBasePlayer *pScorer )
{
	const int iTeam = pVictim->GetTeamNumber();
	if ( iTeam == TEAM_HIDDEN )
		m_iHiddenCount++;	// they come back

	if ( m_hSurvivor.Get() )
		return;

	int iDelay = hdn_deathwait.GetInt();
	if ( pScorer == pVictim && iTeam == TEAM_IRIS )
		iDelay *= 4;	// marines who kill themselves wait longer

	pVictim->SetSpawnTimer( GetRoundTimerRemain(), iDelay );
	m_SpawnQueue.AddToTail( pVictim );

	if ( m_iMarineCount != 1 )
		return;

	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
		if ( pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_IRIS && !pPlayer->IsSpawnQueued() )
		{
			m_hSurvivor = pPlayer;
			break;
		}
	}

	CHidden_Player *pSurvivor = m_hSurvivor.Get();
	if ( !pSurvivor )
		return;

	const int iSurvivalTime = hdn_survivaltime.GetInt();
	m_iSurvivalLeft = GetRoundTimerRemain() - iSurvivalTime;
	pSurvivor->SetSpawnTimer( GetRoundTimerRemain(), iSurvivalTime );
	pSurvivor->IncrementFragCount( 1 );
	pSurvivor->AddWeighting( 100 );
	m_nRoundState = ROUND_SURVIVAL_START;
}

void CHiddenRules::SurvivalThink( void )
{
	CHidden_Player *pSurvivor = m_hSurvivor.Get();
	if ( !pSurvivor || !pSurvivor->IsAlive() )
		return;

	const int iLeft = GetRoundTimerRemain() - pSurvivor->GetSpawnTime();
	if ( iLeft == m_iSurvivalLeft )
		return;

	m_iSurvivalLeft = iLeft;
	if ( iLeft < 6 || iLeft % 5 == 0 )
	{
		char szMessage[128];
		Q_snprintf( szMessage, sizeof( szMessage ), "Survival/Hunt Time : %d Seconds(s)", iLeft );
		UTIL_ClientPrintAll( HUD_PRINTTALK, szMessage );
	}
}

void CHiddenRules::RespawnHiddens( void )
{
	bool bWaiting = false;
	for ( int i = 0; i < m_SpawnQueue.Count(); i++ )
	{
		CHidden_Player *pPlayer = m_SpawnQueue[i].Get();
		if ( !pPlayer || !pPlayer->IsSpawnQueued() )
			continue;

		const int iLeft = MAX( 0, GetRoundTimerRemain() - pPlayer->GetSpawnTime() );

		char szMessage[128];
		Q_snprintf( szMessage, sizeof( szMessage ), "Spawning In : %d Second(s)", iLeft );
		ClientPrint( pPlayer, HUD_PRINTCENTER, szMessage );

		if ( iLeft > 0 )
		{
			bWaiting = true;
			continue;
		}

		pPlayer->SetSpawnQueued( false );
		pPlayer->ChangeTeam( TEAM_HIDDEN );
		if ( pPlayer->IsObserver() )
			pPlayer->StopObserverMode();
		pPlayer->State_Transition( STATE_ACTIVE );
		pPlayer->Spawn();
	}

	if ( !bWaiting )
		m_SpawnQueue.Purge();
}

void CHiddenRules::PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	DeathNotice( pVictim, info );

	CBasePlayer *pScorer = ToBasePlayer( GetDeathScorer( info.GetAttacker(), info.GetInflictor() ) );
	CHidden_Player *pVictimHidden = ToHiddenPlayer( pVictim );

	pVictim->IncrementDeathCount( 1 );
	if ( pVictimHidden )
		pVictimHidden->AddWeighting( -20 );
	FireTargets( "game_playerdie", pVictim, pVictim, USE_TOGGLE, 0 );

	const bool bVictimHidden = ( pVictim->GetTeamNumber() == TEAM_HIDDEN );
	if ( bVictimHidden )
		m_iHiddenCount--;
	else if ( pVictim->GetTeamNumber() == TEAM_IRIS )
		m_iMarineCount--;

	if ( !pScorer || pScorer == pVictim )
	{
		// Suicide or killed by the world.
		pVictim->IncrementFragCount( -1 );
		if ( bVictimHidden )
		{
			DevMsg( 1, pScorer ? "Hidden Suicided\n" : "Hidden killed by world\n" );
			m_Selector.ForfeitHidden( pVictimHidden );
		}
	}
	else
	{
		CHidden_Player *pScorerHidden = ToHiddenPlayer( pScorer );
		if ( bVictimHidden )
		{
			// A marine killed the Hidden.
			DevMsg( 1, "Hidden killed\n" );
			pScorer->IncrementFragCount( 2 );
			if ( pScorerHidden )
				pScorerHidden->AddWeighting( 50 );
			m_Selector.NewHidden( pScorerHidden );
		}
		else
		{
			// The Hidden scores for a marine; a marine loses a point for a teamkill.
			pScorer->IncrementFragCount( pScorer->GetTeamNumber() == TEAM_HIDDEN ? 1 : -1 );
		}

		pScorer->AllowImmediateDecalPainting();
		FireTargets( "game_playerkill", pScorer, pScorer, USE_TOGGLE, 0 );
	}

	if ( m_nGameType == HIDDEN_GAMETYPE_OVERRUN && pVictimHidden )
		OverRunPlayerKilled( pVictimHidden, pScorer );

	DevMsg( 1, "Marines Left:%i\n", m_iMarineCount );
	DevMsg( 1, "Hiddens Left:%i\n", m_iHiddenCount );
}

void CHiddenRules::DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info )
{
	// The Hidden's kills stay out of the kill feed unless hdn_deathnotices is 1; they're only logged.
	CBaseEntity *pAttacker = info.GetAttacker();
	if ( pAttacker && pAttacker != pVictim && pAttacker->GetTeamNumber() == TEAM_HIDDEN && !hdn_deathnotices.GetBool() )
	{
		CBasePlayer *pKiller = ToBasePlayer( pAttacker );
		if ( pKiller && pKiller->GetTeam() && pVictim->GetTeam() )
		{
			UTIL_LogPrintf( "\"%s<%i><%s><%s>\" killed \"%s<%i><%s><%s>\"\n",
				pKiller->GetPlayerName(), pKiller->GetUserID(), pKiller->GetNetworkIDString(), pKiller->GetTeam()->GetName(),
				pVictim->GetPlayerName(), pVictim->GetUserID(), pVictim->GetNetworkIDString(), pVictim->GetTeam()->GetName() );
		}
		return;
	}

	BaseClass::DeathNotice( pVictim, info );
}

// Beta 4b's CGameRules::CanHavePlayerItem: each team picks up only its own weapons, and a marine
// only the primary and pistol of their loadout (another of the same kind tops up its ammo).
bool CHiddenRules::CanHavePlayerItem( CBasePlayer *pPlayer, CBaseCombatWeapon *pItem )
{
	static const char *s_pszPrimaries[HIDDEN_PRIMARY_COUNT] = { "weapon_fn2000", "weapon_p90", "weapon_shotgun", "weapon_fn303" };
	static const char *s_pszSecondaries[HIDDEN_SECONDARY_COUNT] = { "weapon_pistol", "weapon_pistol2" };

	const char *pszName = pItem->GetName();

	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
	{
		for ( int i = 0; i < HIDDEN_PRIMARY_COUNT; i++ )
		{
			if ( !Q_stricmp( pszName, s_pszPrimaries[i] ) )
				return false;
		}
		for ( int i = 0; i < HIDDEN_SECONDARY_COUNT; i++ )
		{
			if ( !Q_stricmp( pszName, s_pszSecondaries[i] ) )
				return false;
		}
		if ( !Q_stricmp( pszName, "weapon_sonic" ) )
			return false;
	}
	else
	{
		if ( !Q_stricmp( pszName, "weapon_knife" ) || !Q_stricmp( pszName, "weapon_grenade" ) )
			return false;

		CHidden_Player *pHiddenPlayer = ToHiddenPlayer( pPlayer );
		if ( pHiddenPlayer )
		{
			for ( int i = 0; i < HIDDEN_PRIMARY_COUNT; i++ )
			{
				if ( i != pHiddenPlayer->GetPrimary() && !Q_stricmp( pszName, s_pszPrimaries[i] ) )
					return false;
			}
			for ( int i = 0; i < HIDDEN_SECONDARY_COUNT; i++ )
			{
				if ( i != pHiddenPlayer->GetSecondary() && !Q_stricmp( pszName, s_pszSecondaries[i] ) )
					return false;
			}
		}
	}

	return BaseClass::CanHavePlayerItem( pPlayer, pItem );
}

bool CHiddenRules::FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker, const CTakeDamageInfo &info )
{
	if ( pAttacker && pAttacker->IsPlayer() )
	{
		CHidden_Player *pVictim = ToHiddenPlayer( pPlayer );
		if ( pVictim && pVictim->GetSafe() )
			return false;

		if ( pAttacker->GetTeamNumber() == pPlayer->GetTeamNumber() && !friendlyfire.GetBool() )
			return false;
	}

	return CTeamplayRules::FPlayerCanTakeDamage( pPlayer, pAttacker, info );
}

// Beta 4b's CSDKGameRules::RadiusDamage, the SDK template's: linear falloff, blocked only by brushes,
// no damage from outside the water to anyone fully under it. Every player hurt also gets a shockwave.
void CHiddenRules::RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrcIn, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore )
{
	const float flFalloff = flRadius ? info.GetDamage() / flRadius : 1.0f;
	const bool bInWater = ( UTIL_PointContents( vecSrcIn ) & MASK_WATER ) != 0;

	Vector vecSrc = vecSrcIn;
	vecSrc.z += 1.0f;	// in case the grenade is lying on the ground

	for ( CEntitySphereQuery sphere( vecSrc, flRadius ); sphere.GetCurrentEntity(); sphere.NextEntity() )
	{
		CBaseEntity *pEntity = sphere.GetCurrentEntity();
		if ( pEntity == pEntityIgnore || pEntity->m_takedamage == DAMAGE_NO )
			continue;

		if ( iClassIgnore != CLASS_NONE && pEntity->Classify() == iClassIgnore )
			continue;

		// Blasts don't travel into the water.
		if ( !bInWater && pEntity->GetWaterLevel() == 3 )
			continue;

		const Vector vecSpot = pEntity->BodyTarget( vecSrc, true );

		trace_t tr;
		UTIL_TraceLine( vecSrc, vecSpot, MASK_SOLID_BRUSHONLY, info.GetInflictor(), COLLISION_GROUP_NONE, &tr );
		if ( tr.startsolid )
		{
			tr.endpos = vecSrc;
			tr.fraction = 0.0f;
		}

		if ( tr.fraction != 1.0f && tr.m_pEnt != pEntity )
			continue;

		Vector vecToTarget = tr.endpos - vecSrc;
		const float flDamage = info.GetDamage() - vecToTarget.Length() * flFalloff;
		if ( flDamage <= 0.0f )
			continue;

		CTakeDamageInfo adjustedInfo = info;
		adjustedInfo.SetDamage( flDamage );

		VectorNormalize( vecToTarget );

		if ( adjustedInfo.GetDamagePosition() == vec3_origin || adjustedInfo.GetDamageForce() == vec3_origin )
		{
			CalculateExplosiveDamageForce( &adjustedInfo, vecToTarget, vecSrc, 1.5f );
		}
		else
		{
			// The force passed in is the most there is; it falls off like the damage.
			adjustedInfo.SetDamageForce( vecToTarget * ( adjustedInfo.GetDamageForce().Length() * flFalloff ) );
			adjustedInfo.SetDamagePosition( vecSrc );
		}

		pEntity->TakeDamage( adjustedInfo );
		pEntity->TraceAttackToTriggers( adjustedInfo, vecSrc, tr.endpos, vecToTarget );

		CHidden_Player *pPlayer = ToHiddenPlayer( pEntity );
		if ( pPlayer )
		{
			DevMsg( 1, "Shockwave - sdkgamerules\n" );
			pPlayer->Shockwave( flDamage / 12.0f, 5.0f );
		}
	}
}

float CHiddenRules::FlPlayerFallDamage( CBasePlayer *pPlayer )
{
	// The Hidden takes no fall damage; marines use the HL2 formula.
	if ( pPlayer->GetTeamNumber() == TEAM_HIDDEN )
		return 0.0f;

	pPlayer->m_Local.m_flFallVelocity -= PLAYER_MAX_SAFE_FALL_SPEED;
	return pPlayer->m_Local.m_flFallVelocity * DAMAGE_FOR_FALL_SPEED;
}

const char *CHiddenRules::GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer )
{
	if ( !pPlayer )
		return "";

	if ( !pPlayer->IsAlive() )
		return "(Dead)";

	switch ( pPlayer->GetTeamNumber() )
	{
	case TEAM_HIDDEN:	return "(Hidden)";
	case TEAM_IRIS:		return "(IRIS)";
	default:			return "";
	}
}

const char *CHiddenRules::GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer )
{
	// Only marines give their location away, in team and public chat alike.
	if ( !pPlayer || pPlayer->GetTeamNumber() != TEAM_IRIS )
		return "";

	return ToHiddenPlayer( pPlayer )->GetCurrentLocation();
}
#endif
