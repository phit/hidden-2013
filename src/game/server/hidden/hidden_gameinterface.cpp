//========= Hidden: Source =====================================================//
//
// Purpose: Mod-specific CServerGameClients and CServerGameDLL parts
//			(from hl2mp_gameinterface.cpp).
//
//=============================================================================//

#include "cbase.h"
#include "gameinterface.h"
#include "mapentities.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

void CServerGameClients::GetPlayerLimits( int& minplayers, int& maxplayers, int &defaultMaxPlayers ) const
{
	// Beta 4b allowed 2-10 with a default of 9 (one Hidden and eight marines). We allow 32 so
	// spectators and SourceTV fit; the game rules still spawn at most eight marines.
	minplayers = 2;
	maxplayers = 32;
	defaultMaxPlayers = 9;
}

void CServerGameDLL::LevelInit_ParseAllEntities( const char *pMapEntities )
{
}
