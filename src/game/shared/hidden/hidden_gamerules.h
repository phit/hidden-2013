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

	HiddenGameType_t GetGameType( void ) const { return m_nGameType; }
	float GetRoundTimeRemaining( void ) const;
	bool IsCharacterTaken( int iCharacter ) const;

#ifndef CLIENT_DLL
	virtual void CreateStandardEntities( void );
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
