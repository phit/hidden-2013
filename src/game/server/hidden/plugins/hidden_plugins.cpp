//========= Hidden: Source =====================================================//
//
// Purpose: Runs the built-in plugins' hooks (docs/spec/plugins.md).
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "igameevents.h"
#include "GameEventListener.h"
#include "team.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CHiddenPlugin *CHiddenPlugin::s_pFirst = NULL;

CHiddenPlugin::CHiddenPlugin()
{
	m_pNext = s_pFirst;
	s_pFirst = this;
}

#define FOR_EACH_HIDDEN_PLUGIN( p ) for ( CHiddenPlugin *p = CHiddenPlugin::s_pFirst; p; p = p->m_pNext )

bool HiddenPlugins_OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
{
	FOR_EACH_HIDDEN_PLUGIN( pPlugin )
	{
		if ( !pPlugin->OnTakeDamage( pVictim, info ) )
			return false;
	}
	return true;
}

void HiddenPlugins_PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
{
	FOR_EACH_HIDDEN_PLUGIN( pPlugin )
		pPlugin->PlayerHurt( pVictim, info );
}

bool HiddenPlugins_AlarmTriggered( CHiddenSonicAlarm *pAlarm, CBaseEntity *pBreaker, CRecipientFilter &filter )
{
	FOR_EACH_HIDDEN_PLUGIN( pPlugin )
	{
		if ( !pPlugin->AlarmTriggered( pAlarm, pBreaker, filter ) )
			return false;
	}
	return true;
}

class CHiddenPluginSystem : public CAutoGameSystemPerFrame, public CGameEventListener
{
public:
	CHiddenPluginSystem() : CAutoGameSystemPerFrame( "CHiddenPluginSystem" ) {}

	virtual void LevelInitPostEntity( void )
	{
		ListenForGameEvent( "game_round_start" );
		ListenForGameEvent( "game_round_end" );
		ListenForGameEvent( "player_say" );
		ListenForGameEvent( "iris_radio" );
		ListenForGameEvent( "player_disconnect" );

		FOR_EACH_HIDDEN_PLUGIN( pPlugin )
			pPlugin->LevelInit();
	}

	virtual void LevelShutdownPreEntity( void )
	{
		StopListeningForAllEvents();
	}

	virtual void FrameUpdatePostEntityThink( void )
	{
		FOR_EACH_HIDDEN_PLUGIN( pPlugin )
			pPlugin->Think();
	}

	virtual void FireGameEvent( IGameEvent *pEvent )
	{
		const char *pszName = pEvent->GetName();
		if ( !Q_strcmp( pszName, "game_round_start" ) )
		{
			FOR_EACH_HIDDEN_PLUGIN( pPlugin )
				pPlugin->RoundStart();
			return;
		}

		if ( !Q_strcmp( pszName, "game_round_end" ) )
		{
			FOR_EACH_HIDDEN_PLUGIN( pPlugin )
				pPlugin->RoundEnd();
			return;
		}

		CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByUserId( pEvent->GetInt( "userid" ) ) );
		if ( !pPlayer )
			return;

		if ( !Q_strcmp( pszName, "player_say" ) )
		{
			FOR_EACH_HIDDEN_PLUGIN( pPlugin )
				pPlugin->PlayerSay( pPlayer, pEvent->GetString( "text" ) );
		}
		else if ( !Q_strcmp( pszName, "iris_radio" ) )
		{
			FOR_EACH_HIDDEN_PLUGIN( pPlugin )
				pPlugin->Radio( pPlayer, pEvent->GetInt( "message" ) );
		}
		else if ( !Q_strcmp( pszName, "player_disconnect" ) )
		{
			FOR_EACH_HIDDEN_PLUGIN( pPlugin )
				pPlugin->ClientDisconnect( pPlayer );
		}
	}
};

static CHiddenPluginSystem s_HiddenPluginSystem;

static void HiddenPlugins_Print( CBasePlayer *pPlayer, int iDest, const char *pszFormat, va_list args )
{
	char szText[512];
	Q_vsnprintf( szText, sizeof( szText ), pszFormat, args );

	if ( iDest == HUD_PRINTCONSOLE )
		Q_strncat( szText, "\n", sizeof( szText ) );

	ClientPrint( pPlayer, iDest, szText );
}

void HiddenPlugins_PrintCenter( CBasePlayer *pPlayer, const char *pszFormat, ... )
{
	va_list args;
	va_start( args, pszFormat );
	HiddenPlugins_Print( pPlayer, HUD_PRINTCENTER, pszFormat, args );
	va_end( args );
}

void HiddenPlugins_PrintChat( CBasePlayer *pPlayer, const char *pszFormat, ... )
{
	va_list args;
	va_start( args, pszFormat );
	HiddenPlugins_Print( pPlayer, HUD_PRINTTALK, pszFormat, args );
	va_end( args );
}

void HiddenPlugins_PrintConsole( CBasePlayer *pPlayer, const char *pszFormat, ... )
{
	va_list args;
	va_start( args, pszFormat );
	HiddenPlugins_Print( pPlayer, HUD_PRINTCONSOLE, pszFormat, args );
	va_end( args );
}

const char *HiddenPlugins_LogName( CBasePlayer *pPlayer )
{
	// A few buffers in turn, so one log line can name two players.
	static char s_szNames[4][128];
	static int s_iNext = 0;
	char *pszName = s_szNames[s_iNext];
	s_iNext = ( s_iNext + 1 ) % ARRAYSIZE( s_szNames );

	Q_snprintf( pszName, sizeof( s_szNames[0] ), "%s<%i><%s><%s>", pPlayer->GetPlayerName(), pPlayer->GetUserID(),
		pPlayer->GetNetworkIDString(), pPlayer->GetTeam() ? pPlayer->GetTeam()->GetName() : "" );
	return pszName;
}

bool HiddenPlugins_RemoveWeapon( CBasePlayer *pPlayer, const char *pszWeapon )
{
	CBaseCombatWeapon *pWeapon = pPlayer->Weapon_OwnsThisType( pszWeapon );
	if ( !pWeapon )
		return false;

	pPlayer->RemovePlayerItem( pWeapon );
	UTIL_Remove( pWeapon );
	return true;
}
