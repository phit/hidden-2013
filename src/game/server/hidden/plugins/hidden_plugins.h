//========= Hidden: Source =====================================================//
//
// Purpose: Built-in versions of popular Hidden:SourceMod plugins, with their
//			cvars. See docs/spec/plugins.md.
//
//=============================================================================//

#ifndef HIDDEN_PLUGINS_H
#define HIDDEN_PLUGINS_H
#pragma once

class CHidden_Player;
class CTakeDamageInfo;
class CHiddenSonicAlarm;
class CRecipientFilter;

// One static instance per plugin. The hooks stand in for the SourceMod events and callbacks the
// plugins used; each plugin checks its own on/off cvar.
class CHiddenPlugin
{
public:
	CHiddenPlugin();

	virtual void LevelInit( void ) {}					// OnMapStart
	virtual void RoundStart( void ) {}					// game_round_start
	virtual void RoundEnd( void ) {}					// game_round_end
	virtual void Think( void ) {}						// every frame, for the plugins' timers
	virtual void ClientDisconnect( CHidden_Player *pPlayer ) {}

	// Before damage is applied to a player. The plugin may change info; returning false drops the
	// damage entirely (no health lost, no player_hurt, no log line).
	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info ) { return true; }

	// After a player lost health (player_hurt). The victim's health is already reduced.
	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info ) {}

	virtual void PlayerSay( CHidden_Player *pPlayer, const char *pszText ) {}	// player_say
	virtual void Radio( CHidden_Player *pPlayer, int iMessage ) {}				// iris_radio

	// A sonic alarm's beam was broken, by pBreaker if it was something (the plugins listened for the
	// alarm's sound). A plugin may take players out of filter, who hear it, or return false to keep
	// it quiet: no sound and nothing on the radar.
	virtual bool AlarmTriggered( CHiddenSonicAlarm *pAlarm, CBaseEntity *pBreaker, CRecipientFilter &filter ) { return true; }

	static CHiddenPlugin *s_pFirst;
	CHiddenPlugin *m_pNext;
};

bool HiddenPlugins_OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info );
void HiddenPlugins_PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info );
bool HiddenPlugins_AlarmTriggered( CHiddenSonicAlarm *pAlarm, CBaseEntity *pBreaker, CRecipientFilter &filter );

// SourceMod's PrintCenterText, PrintToChat and PrintToConsole for one player.
void HiddenPlugins_PrintCenter( CBasePlayer *pPlayer, const char *pszFormat, ... ) FMTFUNCTION( 2, 3 );
void HiddenPlugins_PrintChat( CBasePlayer *pPlayer, const char *pszFormat, ... ) FMTFUNCTION( 2, 3 );
void HiddenPlugins_PrintConsole( CBasePlayer *pPlayer, const char *pszFormat, ... ) FMTFUNCTION( 2, 3 );

// Takes a weapon away from a player (SourceMod's RemovePlayerItem and RemoveEdict). Returns false
// if they had none.
bool HiddenPlugins_RemoveWeapon( CBasePlayer *pPlayer, const char *pszWeapon );

// A player's log name, "name<userid><networkid><team>".
const char *HiddenPlugins_LogName( CBasePlayer *pPlayer );

#endif // HIDDEN_PLUGINS_H
