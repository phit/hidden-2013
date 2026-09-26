//========= Hidden: Source =====================================================//
//
// Purpose: What Beta 4b's client mode did with game events: the marines' radio
//			calls and round sounds. See docs/spec/client.md.
//
//=============================================================================//

#include "cbase.h"
#include "igamesystem.h"
#include "GameEventListener.h"
#include "engine/IEngineSound.h"
#include "hidden_shareddefs.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CHiddenClientEvents : public CAutoGameSystem, public CGameEventListener
{
public:
	CHiddenClientEvents() : CAutoGameSystem( "CHiddenClientEvents" ) {}

	virtual bool Init( void )
	{
		ListenForGameEvent( "iris_radio" );
		ListenForGameEvent( "game_round_start" );
		ListenForGameEvent( "game_round_end" );
		return true;
	}

	virtual void FireGameEvent( IGameEvent *event );

private:
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
