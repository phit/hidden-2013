//========= Hidden: Source =====================================================//
//
// Purpose: bot_add, Beta 4b's test bot: a fake client that picks a random class,
//			character and loadout and joins the marines. It has no AI; real bots
//			come later (docs/spec/bots.md).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

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

CON_COMMAND_F( bot_add, "Add a test bot that joins the marines.", FCVAR_GAMEDLL | FCVAR_CHEAT )
{
	static int s_iBotNumber = 0;

	char szName[32];
	Q_snprintf( szName, sizeof( szName ), "Bot%02d", ++s_iBotNumber );

	// Not reported as a fake client, so a dedicated server with only test bots doesn't hibernate.
	edict_t *pEdict = engine->CreateFakeClientEx( szName, false );
	CHidden_Player *pBot = pEdict ? ToHiddenPlayer( CBaseEntity::Instance( pEdict ) ) : NULL;
	if ( !pBot )
	{
		Warning( "bot_add: couldn't create a bot\n" );
		return;
	}

	pBot->ClearFlags();
	pBot->AddFlag( FL_CLIENT | FL_FAKECLIENT );

	BotCommand( pBot, "changeclass %d", random->RandomInt( 0, HIDDEN_CLASS_COUNT - 1 ) );
	BotCommand( pBot, "changemarine %d", HIDDEN_NUM_CHARACTERS );	// first free character
	BotCommand( pBot, "primary %d", HIDDEN_LOADOUT_RANDOM );
	BotCommand( pBot, "secondary %d", HIDDEN_LOADOUT_RANDOM );
	BotCommand( pBot, "equip %d", HIDDEN_LOADOUT_RANDOM );
	BotCommand( pBot, "enter" );
}

// The marine tutorial's Hidden (TutorialBotPutInServer): a fake client with no AI yet.
CHidden_Player *HiddenTutorialBotPutInServer( void )
{
	edict_t *pEdict = engine->CreateFakeClientEx( "Subject 617", false );
	CHidden_Player *pBot = pEdict ? ToHiddenPlayer( CBaseEntity::Instance( pEdict ) ) : NULL;
	if ( !pBot )
	{
		Msg( "Failed to create Bot.\n" );
		return NULL;
	}

	pBot->ClearFlags();
	pBot->AddFlag( FL_CLIENT | FL_FAKECLIENT );
	pBot->SetReadyToPlay( true );
	pBot->ChangeTeam( TEAM_HIDDEN );
	return pBot;
}
