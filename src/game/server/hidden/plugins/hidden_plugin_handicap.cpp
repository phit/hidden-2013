//========= Hidden: Source =====================================================//
//
// Purpose: HandiCap (Ice's Hdn_HandiCap 2.0.0): the Hidden handicaps itself
//			from a chat menu, with less damage or less health. See
//			docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?p=1100916
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "hidden_gamerules.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define HANDICAP_MENU_TIME	30

static ConVar hsm_handicap_enable( "hsm_handicap_enable", "0", FCVAR_NOTIFY, "Enable/disable handicap plugin", true, 0.0f, true, 1.0f );
static ConVar hsm_handicap_healthreduction( "hsm_handicap_healthreduction", "0.0", FCVAR_NOTIFY, "Used for reporting, can not be used to adjust anything", true, 0.0f, true, 75.0f );
static ConVar hsm_handicap_damagereduction( "hsm_handicap_damagereduction", "0.0", FCVAR_NOTIFY, "Used for reporting, can not be used to adjust anything", true, 0.0f, true, 90.0f );

// Damage reductions offered, in percent, in menu order.
static const int s_iDamageSteps[] = { 10, 25, 50, 75, 90 };
static const int s_iHealthSteps[] = { 25, 50, 75 };

class CHiddenPluginHandiCap : public CHiddenPlugin
{
public:
	CHiddenPluginHandiCap() { Reset(); }

	virtual void LevelInit( void ) { Reset(); }
	virtual void RoundStart( void ) { Reset(); }

	virtual void RoundEnd( void )
	{
		m_flDamageScale = 1.0f;
		m_bReduced = false;
		m_iPage = PAGE_MAIN;
		hsm_handicap_healthreduction.SetValue( 0.0f );
		hsm_handicap_damagereduction.SetValue( 0.0f );
	}

	// The Hidden's hits do only part of their damage. (The plugin gave the victim that part back in
	// player_hurt.)
	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
	{
		if ( IsActive() && m_flDamageScale < 1.0f )
		{
			CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
			if ( pAttacker && pAttacker != pVictim && pAttacker->GetTeamNumber() == TEAM_HIDDEN && !pAttacker->IsBot() )
				info.ScaleDamage( m_flDamageScale );
		}
		return true;
	}

	virtual void PlayerSay( CHidden_Player *pPlayer, const char *pszText )
	{
		if ( !IsActive() || pPlayer->GetTeamNumber() != TEAM_HIDDEN )
			return;

		if ( !Q_stricmp( pszText, "HandiCap" ) )
		{
			m_iPage = PAGE_MAIN;
			ShowMenu( pPlayer );
		}
		else if ( !Q_stricmp( pszText, "UnHandiCap" ) )
		{
			m_flDamageScale = 1.0f;
			m_bReduced = false;
			PrintAll( "[HandiCap] %s has disabled all of his HandiCaps!", pPlayer->GetPlayerName() );
			hsm_handicap_healthreduction.SetValue( 0.0f );
			hsm_handicap_damagereduction.SetValue( 0.0f );
		}
		else if ( !Q_strnicmp( pszText, "HandiCap d ", 11 ) )
		{
			const int iPercent = atoi( pszText + 11 );
			for ( int i = 0; i < ARRAYSIZE( s_iDamageSteps ); i++ )
			{
				if ( s_iDamageSteps[i] == iPercent )
					ReduceDamage( pPlayer, iPercent );
			}
		}
		else if ( !Q_strnicmp( pszText, "HandiCap h ", 11 ) )
		{
			const int iAmount = atoi( pszText + 11 );
			for ( int i = 0; i < ARRAYSIZE( s_iHealthSteps ); i++ )
			{
				if ( s_iHealthSteps[i] == iAmount )
					ReduceHealth( pPlayer, iAmount );
			}
		}
	}

	virtual void MenuSelect( CHidden_Player *pPlayer, int iItem )
	{
		if ( !IsActive() || pPlayer->GetTeamNumber() != TEAM_HIDDEN )
			return;

		switch ( m_iPage )
		{
		case PAGE_MAIN:
			if ( iItem == 1 )
			{
				m_iPage = PAGE_HEALTH;
			}
			else if ( iItem == 2 )
			{
				if ( !m_bReduced )
				{
					m_iPage = PAGE_DAMAGE;
				}
				else
				{
					m_bReduced = false;
					m_flDamageScale = 1.0f;
					hsm_handicap_damagereduction.SetValue( 0.0f );
					PrintAll( "[HandiCap] %s has set his damage to normal!", pPlayer->GetPlayerName() );
				}
			}
			break;

		case PAGE_DAMAGE:
			if ( iItem >= 1 && iItem <= ARRAYSIZE( s_iDamageSteps ) )
				ReduceDamage( pPlayer, s_iDamageSteps[iItem - 1] );
			else if ( iItem == ARRAYSIZE( s_iDamageSteps ) + 1 )
				m_iPage = PAGE_MAIN;
			break;

		case PAGE_HEALTH:
			if ( iItem >= 1 && iItem <= ARRAYSIZE( s_iHealthSteps ) )
				ReduceHealth( pPlayer, s_iHealthSteps[iItem - 1] );
			else if ( iItem == ARRAYSIZE( s_iHealthSteps ) + 1 )
				m_iPage = PAGE_MAIN;
			break;
		}

		// The menu comes back after every choice, as the plugin's did.
		ShowMenu( pPlayer );
	}

private:
	enum { PAGE_MAIN, PAGE_DAMAGE, PAGE_HEALTH };

	// The plugin switched itself off on OverRun (ovr_) maps.
	bool IsActive( void ) const
	{
		return hsm_handicap_enable.GetBool() && Q_strnicmp( STRING( gpGlobals->mapname ), "ovr", 3 );
	}

	void Reset( void )
	{
		m_flDamageScale = 1.0f;
		m_bReduced = false;
		m_iPage = PAGE_MAIN;
		hsm_handicap_healthreduction.SetValue( 0.0f );
		hsm_handicap_damagereduction.SetValue( 0.0f );
	}

	void ReduceDamage( CHidden_Player *pPlayer, int iPercent )
	{
		m_flDamageScale = 1.0f - iPercent / 100.0f;
		m_bReduced = true;
		hsm_handicap_damagereduction.SetValue( iPercent );
		PrintAll( "[HandiCap] %s has reduced his damage to %d percent!", pPlayer->GetPlayerName(), 100 - iPercent );
	}

	void ReduceHealth( CHidden_Player *pPlayer, int iAmount )
	{
		if ( pPlayer->GetHealth() <= iAmount )
		{
			HiddenPlugins_PrintChat( pPlayer, "[HandiCap] You cannot reduce your health to 0!" );
			return;
		}

		pPlayer->SetHealth( pPlayer->GetHealth() - iAmount );
		hsm_handicap_healthreduction.SetValue( iAmount );
		PrintAll( "[HandiCap] %s has reduced his health by %d!", pPlayer->GetPlayerName(), iAmount );
	}

	void ShowMenu( CHidden_Player *pPlayer )
	{
		char szMenu[512];
		Q_strncpy( szMenu, "HandiCap Menu\nClick a button\nto handicap yourself\n \n", sizeof( szMenu ) );
		int iSlots = 0;

		switch ( m_iPage )
		{
		case PAGE_MAIN:
			// The plugin's Drug item needed its sm_drug plugin, which isn't built in.
			Q_strncat( szMenu, "1. Reduce Health\n", sizeof( szMenu ) );
			Q_strncat( szMenu, m_bReduced ? "2. Normalize damage\n" : "2. Reduce damage\n", sizeof( szMenu ) );
			iSlots = ( 1 << 0 ) | ( 1 << 1 );
			break;

		case PAGE_DAMAGE:
			for ( int i = 0; i < ARRAYSIZE( s_iDamageSteps ); i++ )
			{
				Q_strncat( szMenu, CFmtStr( "%d. %d%%\n", i + 1, s_iDamageSteps[i] ), sizeof( szMenu ) );
				iSlots |= 1 << i;
			}
			Q_strncat( szMenu, CFmtStr( "%d. Back\n", ARRAYSIZE( s_iDamageSteps ) + 1 ), sizeof( szMenu ) );
			iSlots |= 1 << ARRAYSIZE( s_iDamageSteps );
			break;

		case PAGE_HEALTH:
			for ( int i = 0; i < ARRAYSIZE( s_iHealthSteps ); i++ )
			{
				Q_strncat( szMenu, CFmtStr( "%d. Reduce by %d\n", i + 1, s_iHealthSteps[i] ), sizeof( szMenu ) );
				iSlots |= 1 << i;
			}
			Q_strncat( szMenu, CFmtStr( "%d. Back\n", ARRAYSIZE( s_iHealthSteps ) + 1 ), sizeof( szMenu ) );
			iSlots |= 1 << ARRAYSIZE( s_iHealthSteps );
			break;
		}

		HiddenPlugins_ShowMenu( this, pPlayer, iSlots, HANDICAP_MENU_TIME, szMenu );
	}

	static void PrintAll( const char *pszFormat, ... )
	{
		char szText[256];
		va_list args;
		va_start( args, pszFormat );
		Q_vsnprintf( szText, sizeof( szText ), pszFormat, args );
		va_end( args );

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
			if ( pPlayer )
				HiddenPlugins_PrintChat( pPlayer, "%s", szText );
		}
	}

	float m_flDamageScale;
	bool m_bReduced;
	int m_iPage;
};

static CHiddenPluginHandiCap s_HandiCap;
