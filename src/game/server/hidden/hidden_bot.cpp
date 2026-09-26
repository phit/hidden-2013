//========= Hidden: Source =====================================================//
//
// Purpose: Beta 4b's test bots (CSDKBot, the 2006 SDK template's sdk_bot_temp.cpp):
//			bot_add_test (Beta 4b's bot_add) and the marine tutorial's "Subject 617". Each frame a bot runs
//			forward, turns away from walls, now and then strafes or backs up, and
//			stops for good once it's hurt; the bot_* cvars make bots mimic a
//			player, fire or send commands. The playable bots are bot_add (bot/hidden_nbot.cpp).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_player.h"
#include "in_buttons.h"
#include "movehelper_server.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

ConVar bot_forcefireweapon( "bot_forcefireweapon", "", 0, "Force bots with the specified weapon to fire." );
ConVar bot_forceattack2( "bot_forceattack2", "0", 0, "When firing, use attack2." );
ConVar bot_forceattackon( "bot_forceattackon", "0", 0, "When firing, don't tap fire, hold it down." );
ConVar bot_flipout( "bot_flipout", "0", 0, "When on, all bots fire their guns." );
ConVar bot_changeclass( "bot_changeclass", "0", 0, "Force all bots to change to the specified class." );
static ConVar bot_mimic_yaw_offset( "bot_mimic_yaw_offset", "0", 0, "Offsets the bot yaw." );
ConVar bot_sendcmd( "bot_sendcmd", "", 0, "Forces bots to send the specified command." );
ConVar bot_crouch( "bot_crouch", "0", 0, "Bot crouches" );

// bot_mimic itself is NextBot's (NextBotPlayerBody.cpp), with the same meaning.
static ConVarRef bot_mimic( "bot_mimic" );

// CSDKBot's members, kept per player slot instead of in a player subclass.
struct HiddenBot_t
{
	bool	m_bBackwards;
	float	m_flNextTurnTime;
	bool	m_bLastTurnToRight;
	float	m_flNextStrafeTime;
	float	m_flSideMove;
	QAngle	m_ForwardAngle;
	QAngle	m_LastAngles;
};

static HiddenBot_t s_Bots[MAX_PLAYERS + 1];

static void BotCommand( CHidden_Player *pBot, const char *pszFormat, ... )
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

static CHidden_Player *CreateBot( const char *pszName, bool bFrozen )
{
	// Not reported as a fake client, so a dedicated server with only test bots doesn't hibernate.
	edict_t *pEdict = engine->CreateFakeClientEx( pszName, false );
	CHidden_Player *pBot = pEdict ? ToHiddenPlayer( CBaseEntity::Instance( pEdict ) ) : NULL;
	if ( !pBot )
	{
		Msg( "Failed to create Bot.\n" );
		return NULL;
	}

	pBot->ClearFlags();
	pBot->AddFlag( FL_CLIENT | FL_FAKECLIENT );
	if ( bFrozen )
		pBot->AddEFlags( EFL_BOT_FROZEN );

	Q_memset( &s_Bots[pBot->entindex()], 0, sizeof( HiddenBot_t ) );
	return pBot;
}

// Beta 4b's bot_add; that name now adds the playable bots.
CON_COMMAND_F( bot_add_test, "Add a test bot: bot_add_test [-count n] [-frozen]", FCVAR_GAMEDLL | FCVAR_CHEAT )
{
	static int s_iBotNumber = 0;

	int iCount = clamp( args.FindArgInt( "-count", 1 ), 1, 16 );
	const bool bFrozen = !!args.FindArg( "-frozen" );

	while ( --iCount >= 0 )
	{
		char szName[32];
		Q_snprintf( szName, sizeof( szName ), "Bot%02d", ++s_iBotNumber );

		CHidden_Player *pBot = CreateBot( szName, bFrozen );
		if ( !pBot )
			return;

		// Beta 4b's bots joined unassigned; ours pick a random class, character and loadout and join
		// the marines, so the round loop can use them.
		BotCommand( pBot, "changeclass %d", random->RandomInt( 0, HIDDEN_CLASS_COUNT - 1 ) );
		BotCommand( pBot, "changemarine %d", HIDDEN_NUM_CHARACTERS );	// first free character
		BotCommand( pBot, "primary %d", HIDDEN_LOADOUT_RANDOM );
		BotCommand( pBot, "secondary %d", HIDDEN_LOADOUT_RANDOM );
		BotCommand( pBot, "equip %d", HIDDEN_LOADOUT_RANDOM );
		BotCommand( pBot, "enter" );
	}
}

// The marine tutorial's Hidden (TutorialBotPutInServer): an unfrozen bot on team 3, so it roams its cell.
CHidden_Player *HiddenTutorialBotPutInServer( void )
{
	CHidden_Player *pBot = CreateBot( "Subject 617", false );
	if ( !pBot )
		return NULL;

	pBot->SetReadyToPlay( true );
	pBot->ChangeTeam( TEAM_HIDDEN );
	return pBot;
}

static bool Bot_RunMimicCommand( CUserCmd &cmd )
{
	if ( bot_mimic.GetInt() <= 0 || bot_mimic.GetInt() > gpGlobals->maxClients )
		return false;

	CBasePlayer *pPlayer = UTIL_PlayerByIndex( bot_mimic.GetInt() );
	if ( !pPlayer || !pPlayer->GetLastUserCommand() )
		return false;

	cmd = *pPlayer->GetLastUserCommand();
	cmd.viewangles[YAW] += bot_mimic_yaw_offset.GetFloat();

	if ( bot_crouch.GetInt() )
		cmd.buttons |= IN_DUCK;

	return true;
}

// Simulates a single frame of movement for a bot.
static void RunPlayerMove( CHidden_Player *pBot, CUserCmd &cmd, float flFrameTime )
{
	// Store off the globals, they're going to get whacked.
	const float flOldFrameTime = gpGlobals->frametime;
	const float flOldCurTime = gpGlobals->curtime;

	pBot->SetTimeBase( gpGlobals->curtime + gpGlobals->frametime - flFrameTime );

	MoveHelperServer()->SetHost( pBot );
	pBot->PlayerRunCommand( &cmd, MoveHelperServer() );

	pBot->SetLastUserCommand( cmd );

	// Clear out any fixangle that has been set.
	pBot->pl.fixangle = FIXANGLE_NONE;

	gpGlobals->frametime = flOldFrameTime;
	gpGlobals->curtime = flOldCurTime;
}

static void Bot_UpdateStrafing( HiddenBot_t &bot, CUserCmd &cmd )
{
	if ( gpGlobals->curtime < bot.m_flNextStrafeTime )
		return;

	bot.m_flNextStrafeTime = gpGlobals->curtime + 1.0f;

	if ( random->RandomInt( 0, 5 ) == 0 )
		bot.m_flSideMove = -600.0f + 1200.0f * random->RandomFloat( 0, 2 );
	else
		bot.m_flSideMove = 0;
	cmd.sidemove = bot.m_flSideMove;

	bot.m_bBackwards = ( random->RandomInt( 0, 20 ) == 0 );
}

// Keep the heading while the way ahead is clear; otherwise, or every 2 seconds, turn 15 degrees at a
// time until it is.
static void Bot_UpdateDirection( CHidden_Player *pBot, HiddenBot_t &bot )
{
	float flAngleDelta = 15.0f;
	int iMaxTries = (int)( 360.0f / flAngleDelta );

	if ( bot.m_bLastTurnToRight )
		flAngleDelta = -flAngleDelta;

	QAngle angle = pBot->GetLocalAngles();

	while ( --iMaxTries >= 0 )
	{
		Vector vecForward;
		AngleVectors( angle, &vecForward );

		const Vector vecSrc = pBot->GetLocalOrigin() + Vector( 0, 0, 36 );
		const Vector vecEnd = vecSrc + vecForward * 10;

		trace_t trace;
		UTIL_TraceHull( vecSrc, vecEnd, VEC_HULL_MIN_SCALED( pBot ), VEC_HULL_MAX_SCALED( pBot ),
			MASK_PLAYERSOLID, pBot, COLLISION_GROUP_NONE, &trace );

		if ( trace.fraction == 1.0f && gpGlobals->curtime < bot.m_flNextTurnTime )
			break;

		angle.y += flAngleDelta;

		if ( angle.y > 180 )
			angle.y -= 360;
		else if ( angle.y < -180 )
			angle.y += 360;

		bot.m_flNextTurnTime = gpGlobals->curtime + 2.0f;
		bot.m_bLastTurnToRight = ( random->RandomInt( 0, 1 ) == 0 );

		bot.m_ForwardAngle = angle;
		bot.m_LastAngles = angle;
	}

	pBot->SetLocalAngles( angle );
}

static void Bot_FlipOut( CHidden_Player *pBot, HiddenBot_t &bot, CUserCmd &cmd )
{
	if ( bot_flipout.GetInt() <= 0 || !pBot->IsAlive() )
		return;

	if ( bot_forceattackon.GetBool() || RandomFloat( 0.0f, 1.0f ) > 0.5f )
		cmd.buttons |= bot_forceattack2.GetBool() ? IN_ATTACK2 : IN_ATTACK;

	if ( bot_flipout.GetInt() >= 2 )
	{
		bot.m_LastAngles += RandomAngle( -1, 1 );

		for ( int i = 0; i < 2; i++ )
		{
			if ( fabs( bot.m_LastAngles[i] - bot.m_ForwardAngle[i] ) > 15.0f )
			{
				if ( bot.m_LastAngles[i] > bot.m_ForwardAngle[i] )
					bot.m_LastAngles[i] = bot.m_ForwardAngle[i] + 15;
				else
					bot.m_LastAngles[i] = bot.m_ForwardAngle[i] - 15;
			}
		}

		bot.m_LastAngles[2] = 0;

		pBot->SetLocalAngles( bot.m_LastAngles );
	}
}

static void Bot_HandleSendCmd( CHidden_Player *pBot )
{
	if ( bot_sendcmd.GetString()[0] )
	{
		CCommand command;
		command.Tokenize( bot_sendcmd.GetString() );
		pBot->ClientCommand( command );

		bot_sendcmd.SetValue( "" );
	}
}

// If bots are being forced to fire a weapon, see if this one has it.
static void Bot_ForceFireWeapon( CHidden_Player *pBot, CUserCmd &cmd )
{
	CBaseCombatWeapon *pWeapon = pBot->Weapon_OwnsThisType( bot_forcefireweapon.GetString() );
	if ( !pWeapon )
		return;

	if ( pBot->GetActiveWeapon() != pWeapon )
	{
		pBot->Weapon_Switch( pWeapon );
	}
	else if ( bot_forceattackon.GetBool() || RandomFloat( 0.0f, 1.0f ) > 0.5f )
	{
		// Some weapons require releases, so randomise firing.
		cmd.buttons |= bot_forceattack2.GetBool() ? IN_ATTACK2 : IN_ATTACK;
	}
}

static void Bot_SetForwardMovement( CHidden_Player *pBot, HiddenBot_t &bot, CUserCmd &cmd )
{
	if ( pBot->IsEFlagSet( EFL_BOT_FROZEN ) )
		return;

	if ( pBot->m_iHealth == 100 )
	{
		cmd.forwardmove = 600 * ( bot.m_bBackwards ? -1 : 1 );
		if ( bot.m_flSideMove != 0.0f )
			cmd.forwardmove *= random->RandomFloat( 0.1f, 1.0f );
	}
	else
	{
		cmd.forwardmove = 0;	// stop when shot
	}
}

static void Bot_HandleRespawn( CHidden_Player *pBot, CUserCmd &cmd )
{
	// Try hitting the buttons occasionally.
	if ( !pBot->IsAlive() && random->RandomInt( 0, 100 ) > 80 )
	{
		if ( random->RandomInt( 0, 1 ) == 0 )
			cmd.buttons |= IN_JUMP;
		else
			cmd.buttons = 0;
	}
}

// Run one bot's AI for one frame.
static void Bot_Think( CHidden_Player *pBot )
{
	HiddenBot_t &bot = s_Bots[pBot->entindex()];

	// Make sure we stay being a bot.
	pBot->AddFlag( FL_FAKECLIENT );

	CUserCmd cmd;
	Q_memset( &cmd, 0, sizeof( cmd ) );

	if ( !Bot_RunMimicCommand( cmd ) )
	{
		cmd.sidemove = bot.m_flSideMove;

		if ( pBot->IsAlive() && pBot->GetSolid() == SOLID_BBOX )
		{
			Bot_SetForwardMovement( pBot, bot, cmd );

			// Only turn if not hurt.
			if ( !pBot->IsEFlagSet( EFL_BOT_FROZEN ) && pBot->m_iHealth == 100 )
			{
				Bot_UpdateDirection( pBot, bot );
				Bot_UpdateStrafing( bot, cmd );
			}

			Bot_ForceFireWeapon( pBot, cmd );
			Bot_HandleSendCmd( pBot );
		}
		else
		{
			Bot_HandleRespawn( pBot, cmd );
		}

		Bot_FlipOut( pBot, bot, cmd );

		cmd.viewangles = pBot->GetLocalAngles();
		cmd.upmove = 0;
		cmd.impulse = 0;
	}

	RunPlayerMove( pBot, cmd, gpGlobals->frametime );
}

// Every frame, from GameStartFrame.
void Bot_RunAll( void )
{
	for ( int i = 1; i <= gpGlobals->maxClients; i++ )
	{
		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
		// NextBots (bot_add, bot/hidden_nbot.cpp) run themselves.
		if ( pPlayer && ( pPlayer->GetFlags() & FL_FAKECLIENT ) && !pPlayer->IsHLTV() && !pPlayer->IsReplay() && !pPlayer->MyNextBotPointer() )
			Bot_Think( pPlayer );
	}
}
