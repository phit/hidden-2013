//========= Hidden: Source =====================================================//
//
// Purpose: hdn_ammocrate, a crate marines open with +use to refill their reserve
//			ammo. See docs/spec/map-entities.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define AMMOCRATE_MODEL			"models/generic/iris_ammobox.mdl"
#define AMMOCRATE_CLOSE_DELAY	2.0f	// after opening

class CHDNAmmoCrate : public CBaseAnimating
{
public:
	DECLARE_CLASS( CHDNAmmoCrate, CBaseAnimating );
	DECLARE_DATADESC();

	virtual void Spawn( void );
	virtual void Precache( void );
	virtual void Think( void );
	virtual void Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value );
	virtual int ObjectCaps( void ) { return ( BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_IMPULSE_USE | FCAP_DONT_SAVE; }

private:
	void PlaySequence( const char *pszSequence );
	void GiveAmmo( CHidden_Player *pPlayer );

	float m_flCloseTime;
};

LINK_ENTITY_TO_CLASS( hdn_ammocrate, CHDNAmmoCrate );

BEGIN_DATADESC( CHDNAmmoCrate )
	DEFINE_FIELD( m_flCloseTime, FIELD_TIME ),
END_DATADESC()

void CHDNAmmoCrate::Spawn( void )
{
	Precache();
	SetModel( AMMOCRATE_MODEL );
	SetSolid( SOLID_VPHYSICS );
}

void CHDNAmmoCrate::Precache( void )
{
	PrecacheModel( AMMOCRATE_MODEL );
}

void CHDNAmmoCrate::PlaySequence( const char *pszSequence )
{
	m_flAnimTime = gpGlobals->curtime;
	m_flPlaybackRate = 0.0f;
	SetCycle( 0.0f );
	ResetSequence( LookupSequence( pszSequence ) );
}

void CHDNAmmoCrate::Think( void )
{
	StudioFrameAdvance();
	DispatchAnimEvents( this );
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Close 2 s after opening, then go idle once the lid is down.
	if ( m_flCloseTime < gpGlobals->curtime && GetSequence() == LookupSequence( "open" ) )
	{
		PlaySequence( "close" );
	}
	else if ( GetSequence() == LookupSequence( "close" ) && IsSequenceFinished() )
	{
		PlaySequence( "idle" );
	}
}

void CHDNAmmoCrate::Use( CBaseEntity *pActivator, CBaseEntity *pCaller, USE_TYPE useType, float value )
{
	if ( !pActivator || !pActivator->IsPlayer() )
		return;

	// Busy until it's closed again.
	if ( GetSequence() == LookupSequence( "open" ) || GetSequence() == LookupSequence( "close" ) )
		return;

	PlaySequence( "open" );
	SetNextThink( gpGlobals->curtime + 0.1f );

	// Anyone can open it, but only marines get ammo.
	CHidden_Player *pPlayer = ToHiddenPlayer( pActivator );
	if ( pPlayer && pPlayer->GetTeamNumber() == TEAM_IRIS )
		GiveAmmo( pPlayer );

	m_flCloseTime = gpGlobals->curtime + AMMOCRATE_CLOSE_DELAY;
}

void CHDNAmmoCrate::GiveAmmo( CHidden_Player *pPlayer )
{
	switch ( pPlayer->GetSecondary() )
	{
	case HIDDEN_SECONDARY_PISTOL:	pPlayer->CBasePlayer::GiveAmmo( 20, "AMMO_PISTOL" ); break;
	case HIDDEN_SECONDARY_PISTOL2:	pPlayer->CBasePlayer::GiveAmmo( 16, "AMMO_9MM" ); break;
	}

	switch ( pPlayer->GetPrimary() )
	{
	case HIDDEN_PRIMARY_FN2000:		pPlayer->CBasePlayer::GiveAmmo( 30, "AMMO_556" ); break;
	case HIDDEN_PRIMARY_P90:		pPlayer->CBasePlayer::GiveAmmo( 50, "AMMO_BULLETS" ); break;
	case HIDDEN_PRIMARY_SHOTGUN:	pPlayer->CBasePlayer::GiveAmmo( 8, "AMMO_BUCKSHOT" ); break;
	case HIDDEN_PRIMARY_FN303:		pPlayer->CBasePlayer::GiveAmmo( 15, "XBowBolt" ); break;
	}
}
