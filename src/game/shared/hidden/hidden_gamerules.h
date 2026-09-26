//========= Hidden: Source =====================================================//
//
// Purpose: Hidden game rules: rounds, teams, Hidden selection and scoring.
//			See docs/spec/game-rules.md.
//
//=============================================================================//

#ifndef HIDDEN_GAMERULES_H
#define HIDDEN_GAMERULES_H
#pragma once

#include "hl2mp_gamerules.h"
#include "hidden_shareddefs.h"

#ifndef CLIENT_DLL
	#include "hidden_selector.h"
#endif

#ifdef CLIENT_DLL
	#define CHiddenRules C_HiddenRules
	#define CHiddenGameRulesProxy C_HiddenGameRulesProxy
#endif

class CHiddenGameRulesProxy : public CHL2MPGameRulesProxy
{
public:
	DECLARE_CLASS( CHiddenGameRulesProxy, CHL2MPGameRulesProxy );
	DECLARE_NETWORKCLASS();
};

class CHiddenRules : public CHL2MPRules
{
public:
	DECLARE_CLASS( CHiddenRules, CHL2MPRules );

#ifdef CLIENT_DLL
	DECLARE_CLIENTCLASS_NOBASE();
#else
	DECLARE_SERVERCLASS_NOBASE();
#endif

	CHiddenRules();
	virtual ~CHiddenRules();

	virtual const char *GetGameDescription( void );
	virtual bool IsTeamplay( void ) { return true; }
	virtual bool ShouldCollide( int collisionGroup0, int collisionGroup1 );

	HiddenGameType_t GetGameType( void ) const { return m_nGameType; }
	bool IsTutorial( void ) const { return m_nGameType == HIDDEN_GAMETYPE_MARINE_TUTORIAL || m_nGameType == HIDDEN_GAMETYPE_HIDDEN_TUTORIAL; }
	float GetRoundTimeRemaining( void ) const;
	int GetRoundTimerRemain( void ) const;
	float GetRoundStart( void ) const { return m_flRoundStart; }
	bool IsCharacterTaken( int iCharacter ) const;

#ifndef CLIENT_DLL
	virtual void CreateStandardEntities( void );
	virtual bool FPlayerCanRespawn( CBasePlayer *pPlayer ) { return false; }
	virtual void ClientDisconnected( edict_t *pClient );
	virtual bool ClientCommand( CBaseEntity *pEdict, const CCommand &args );

	void SetCharacterTaken( int iCharacter, bool bTaken );

	// Round loop (docs/spec/game-rules.md)
	virtual void Think( void );
	virtual void GoToIntermission( void );
	virtual void PlayerKilled( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual void DeathNotice( CBasePlayer *pVictim, const CTakeDamageInfo &info );
	virtual bool FPlayerCanTakeDamage( CBasePlayer *pPlayer, CBaseEntity *pAttacker, const CTakeDamageInfo &info );
	virtual float FlPlayerFallDamage( CBasePlayer *pPlayer );

	// Chat reads "(IRIS) name : (location) text" (Beta 4b's Host_Say), not HL2MP's localized formats.
	virtual const char *GetChatFormat( bool bTeamOnly, CBasePlayer *pPlayer ) { return NULL; }
	virtual const char *GetChatPrefix( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual const char *GetChatLocation( bool bTeamOnly, CBasePlayer *pPlayer );
	virtual void RadiusDamage( const CTakeDamageInfo &info, const Vector &vecSrc, float flRadius, int iClassIgnore, CBaseEntity *pEntityIgnore );

	void RestartRound( void );
	void CleanUpMap( void );
	CHiddenSelector &GetSelector( void ) { return m_Selector; }

private:
	enum RoundState_t
	{
		ROUND_INTERMISSION,		// waiting for the next round
		ROUND_STARTING,			// players spawned, announce the start next frame
		ROUND_MATERIAL_CHECK,	// then send the cloak material CRCs
		ROUND_ACTIVE,			// playing; check for a winner every frame
		ROUND_ENDING,			// someone won
		ROUND_GAME_OVER,		// time limit reached, change level after the chat time
		ROUND_SURVIVAL_START,	// OverRun: the last marine was found; tell everyone
		ROUND_SURVIVAL,			// OverRun: playing, with the survivor's countdown
		ROUND_TUTORIAL_CONFIG,	// tutorials: exec tutorial.cfg, then the intermission
		ROUND_TUTORIAL,			// tutorials: playing; the round never ends
	};

	void FireSimpleEvent( const char *pszName );
	bool HasTimeLimitPassed( void ) const;
	void GameThink( void );
	bool IsRoundTimeUp( void );
	bool IRISWins( void );
	bool HiddenWins( void );

	// OverRun (docs/spec/game-modes.md)
	void OverRunPlayerKilled( CHidden_Player *pVictim, CBasePlayer *pScorer );
	void SurvivalThink( void );
	void RespawnHiddens( void );

	CHiddenSelector m_Selector;
	RoundState_t m_nRoundState;
	float m_flIntermissionEnd;
	int m_iMarineCount;		// living marines
	int m_iHiddenCount;		// players on the Hidden team
	bool m_bLastRoundAnnounced;
	bool m_bLevelChanged;
	CRC32_t m_nMaterialCRC;
	CRC32_t m_nMaterialDX7CRC;

	CUtlVector< CHandle<CHidden_Player> > m_SpawnQueue;	// OverRun: killed players waiting to come back as the Hidden
	CHandle<CHidden_Player> m_hSurvivor;					// OverRun: the last marine
	int m_iSurvivalLeft;									// OverRun: the survivor's countdown as last announced
#endif

protected:
	HiddenGameType_t m_nGameType;

	CNetworkVar( float, m_flRoundStart );		// -1 before the first round
	CNetworkVar( int, m_iRoundDuration );		// seconds
	CNetworkArray( bool, m_bCharacterTaken, HIDDEN_NUM_CHARACTERS );
};

inline CHiddenRules *HiddenRules()
{
	return static_cast<CHiddenRules *>( g_pGameRules );
}

#endif // HIDDEN_GAMERULES_H
