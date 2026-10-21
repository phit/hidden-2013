//========= Hidden: Source =====================================================//
//
// Purpose: What Beta 4b's client mode did with game events: the marines' radio
//			calls, round sounds, the damage report, the new round's clean-up and the
//			material check. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "GameEventListener.h"
#include "engine/IEngineSound.h"
#include "hidden_shareddefs.h"
#include "hidden_gamerules.h"
#include "filesystem.h"
#include "physpropclientside.h"
#include "c_te_legacytempents.h"
#include "c_soundscape.h"
#include "c_hidden_player.h"
#include "c_playerresource.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHiddenClientEvents : public CAutoGameSystem, public CGameEventListener
{
public:
	CHiddenClientEvents() : CAutoGameSystem( "CHiddenClientEvents" ), m_bDamageReported( false ) {}

	virtual bool Init( void )
	{
		ListenForGameEvent( "iris_radio" );
		ListenForGameEvent( "game_round_restart" );
		ListenForGameEvent( "game_round_start" );
		ListenForGameEvent( "game_round_end" );
		ListenForGameEvent( "material_check" );
		ListenForGameEvent( "player_hurt" );
		ListenForGameEvent( "player_death" );
		return true;
	}

	virtual void LevelInitPreEntity( void ) { m_Damage.RemoveAll(); m_bDamageReported = false; }

	virtual void FireGameEvent( IGameEvent *event );

private:
	void MaterialCheck( C_BasePlayer *pLocal, IGameEvent *event );
	void AddDamage( int iAttacker, int iVictim, float flDamage );
	void DamageReport( C_BasePlayer *pLocal );

	// The hits the local player gave or took this round, summed per attacker and victim (user IDs).
	struct DamageEntry_t
	{
		int iAttacker;
		int iVictim;
		float flDamage;
		int iShots;
	};
	CUtlVector<DamageEntry_t> m_Damage;
	bool m_bDamageReported;

	void PlayLocalSound( const char *pszSound )
	{
		CLocalPlayerFilter filter;
		C_BaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, pszSound );
	}
};

static CHiddenClientEvents g_HiddenClientEvents;

void CHiddenClientEvents::FireGameEvent( IGameEvent *event )
{
	C_BasePlayer *pLocal = C_BasePlayer::GetLocalPlayer();
	if ( !pLocal )
		return;

	const char *pszName = event->GetName();

	// A new round starts clean: no decals, client-side props back where the map put them, no
	// temporary effects and no sounds left over from the last round.
	if ( FStrEq( pszName, "game_round_restart" ) )
	{
		engine->ClientCmd( "r_cleardecals\n" );
		C_PhysPropClientside::RecreateAll();
		tempents->Clear();
		enginesound->StopAllSounds( true );
		Soundscape_OnStopAllSounds();
		m_Damage.RemoveAll();
		return;
	}

	if ( FStrEq( pszName, "player_hurt" ) )
	{
		const int iAttacker = event->GetInt( "attacker" );
		const int iVictim = event->GetInt( "userid" );
		if ( iAttacker == pLocal->GetUserID() || iVictim == pLocal->GetUserID() )
			AddDamage( iAttacker, iVictim, event->GetFloat( "damage" ) );
		return;
	}

	if ( FStrEq( pszName, "player_death" ) )
	{
		if ( event->GetInt( "userid" ) == pLocal->GetUserID() )
			DamageReport( pLocal );
		return;
	}

	if ( FStrEq( pszName, "material_check" ) )
	{
		MaterialCheck( pLocal, event );
		return;
	}

	const bool bMarine = ( pLocal->GetTeamNumber() == TEAM_IRIS );

	if ( FStrEq( pszName, "game_round_start" ) )
	{
		if ( bMarine )
			PlayLocalSound( "IRIS.RoundStart" );
		return;
	}

	if ( FStrEq( pszName, "game_round_end" ) )
	{
		if ( bMarine && pLocal->IsAlive() )
			PlayLocalSound( "IRIS.RoundEnd" );

		// The survivors' report; the dead had theirs when they died.
		DamageReport( pLocal );
		m_bDamageReported = false;
		return;
	}

	// A radio call reaches every living player who isn't the Hidden.
	if ( !pLocal->IsAlive() || pLocal->GetTeamNumber() == TEAM_HIDDEN )
		return;

	switch ( event->GetInt( "message" ) )
	{
	case 0:	PlayLocalSound( "IRIS.AgentDown" ); break;
	case 1:	PlayLocalSound( "IRIS.EnemySighted" ); break;
	case 2:	PlayLocalSound( "IRIS.Affirmative" ); break;
	case 3:	PlayLocalSound( "IRIS.RequestingAmmo" ); break;
	case 4:
		PlayLocalSound( "IRIS.ReportIn" );

		// Everyone else answers a report-in by themselves.
		if ( event->GetInt( "userid" ) != pLocal->GetUserID() )
			engine->ClientCmd( "radio 10" );
		break;
	case 10: PlayLocalSound( "IRIS.ReportingIn" ); break;
	}
}

void CHiddenClientEvents::AddDamage( int iAttacker, int iVictim, float flDamage )
{
	DevMsg( 1, "damage done : %f\n", flDamage );

	for ( int i = 0; i < m_Damage.Count(); i++ )
	{
		if ( m_Damage[i].iAttacker == iAttacker && m_Damage[i].iVictim == iVictim )
		{
			m_Damage[i].flDamage += flDamage;
			m_Damage[i].iShots++;
			return;
		}
	}

	DamageEntry_t entry = { iAttacker, iVictim, flDamage, 1 };
	m_Damage.AddToTail( entry );
}

static const char *DamageReportName( int iUserID )
{
	// Deviation: Beta 4b printed ERRORNAME for the world (falls).
	const int iPlayer = engine->GetPlayerForUserID( iUserID );
	return ( iPlayer && g_PR ) ? g_PR->GetPlayerName( iPlayer ) : "world";
}

// Beta 4b's damage report, in the console once a round: when the local player dies, or when the
// round ends for the survivors. Nothing when they neither hurt anyone nor got hurt.
void CHiddenClientEvents::DamageReport( C_BasePlayer *pLocal )
{
	if ( m_bDamageReported || !m_Damage.Count() )
		return;

	const int iLocal = pLocal->GetUserID();

	Msg( "Damage List - \n\n" );
	for ( int i = 0; i < m_Damage.Count(); i++ )
	{
		if ( m_Damage[i].iAttacker == iLocal )
			Msg( "You did %f to %s in %i shots\n", m_Damage[i].flDamage, DamageReportName( m_Damage[i].iVictim ), m_Damage[i].iShots );
	}
	Msg( "\n-------------------------------\n\n" );
	for ( int i = 0; i < m_Damage.Count(); i++ )
	{
		if ( m_Damage[i].iVictim == iLocal )
			Msg( "%s did %i to you in %i shots\n", DamageReportName( m_Damage[i].iAttacker ), (int)m_Damage[i].flDamage, m_Damage[i].iShots );
	}
	Msg( "\n-------------------------------\n\n" );

	C_Hidden_Player *pHidden = ToHiddenPlayer( pLocal );
	Msg( "Weighting : %i\n", pHidden ? pHidden->GetWeighting() : 0 );

	m_Damage.RemoveAll();
	m_bDamageReported = true;
}

// Beta 4b's anti-cheat: the server sends the CRCs of its cloak materials at every round start. A
// client whose copies differ, who plays the Hidden on a model without the cloak model's geoff and
// jim attachments, or who has no cloak texture, says so, and the server disconnects it.
void CHiddenClientEvents::MaterialCheck( C_BasePlayer *pLocal, IGameEvent *event )
{
	if ( (int)GetMaterialCRC( "materials/models/manor/mn_tapestry.vmt" ) != event->GetInt( "vmt_CRC" ) ||
		 (int)GetMaterialCRC( "materials/models/manor/mn_tapestry_dx7.vmt" ) != event->GetInt( "bump_CRC" ) )
	{
		Msg( "material mismatch, we reckon you're cheating!\n" );
		engine->ClientCmd( "materialCRC" );
	}

	if ( pLocal->GetTeamNumber() == TEAM_HIDDEN && pLocal->IsAlive() &&
		 ( !pLocal->LookupAttachment( "geoff" ) || !pLocal->LookupAttachment( "jim" ) ) )
	{
		Msg( "bad player model\n" );
		engine->ClientCmd( "materialCRC" );
	}

	if ( !g_pFullFileSystem->FileExists( "materials/models/manor/mn_tapestry.vtf" ) )
	{
		Msg( "missing player materials\n" );
		engine->ClientCmd( "materialCRC" );
	}
}
