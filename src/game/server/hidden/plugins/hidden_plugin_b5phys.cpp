//========= Hidden: Source =====================================================//
//
// Purpose: Beta 5 Physics (Paegus's hsm_b5phys 1.1.0): the Hidden's physics hits
//			of 100 to 500 damage take 75, unless the victim has no more than that,
//			and can shake screens. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?t=78945
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "shake.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define B5PHYS_MIN_DAMAGE		100		// less isn't an instant physics kill
#define B5PHYS_MAX_DAMAGE		500		// more is a pigstick
#define B5PHYS_MAX_HURT			75
#define B5PHYS_BOUNCE_TIME		0.2f	// a rebounding prop hits again within this
#define B5PHYS_SHAKE_AMPLITUDE	16.0f
#define B5PHYS_SHAKE_FREQUENCY	64.0f
#define B5PHYS_SHAKE_DURATION	0.75f

static ConVar hsm_b5phys( "hsm_b5phys", "0", FCVAR_NOTIFY, "Cap the Hidden's physics hits at 75 damage, as Beta 5 did? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_b5physics_shaker( "hsm_b5physics_shaker", "2", 0, "Impact shaker mode. 0: Off, 1: On for target only, 2: Everyone scaled to range, 3: On for EVERYONE, EVERYWHERE in the universer!", true, 0.0f, true, 3.0f );
static ConVar hsm_b5physics_range( "hsm_b5physics_range", "2000", 0, "Impact shaker range scaler if hsm_b5phys_shaker is 2.", true, 1.0f, false, 0.0f );

class CHiddenPluginB5Phys : public CHiddenPlugin
{
public:
	CHiddenPluginB5Phys() { m_flBounceEnd = 0.0f; }

	virtual void LevelInit( void )
	{
		m_flBounceEnd = 0.0f;
	}

	virtual bool OnTakeDamage( CHidden_Player *pVictim, CTakeDamageInfo &info )
	{
		if ( !hsm_b5phys.GetBool() )
			return true;

		// The plugin read the damage from player_hurt, as whole health points.
		const int iDamage = (int)info.GetDamage();
		const int iHealth = pVictim->GetHealth();

		// Just after a capped hit, anything that would kill anyone is dropped: the prop bouncing back
		// would otherwise finish the victim off.
		if ( gpGlobals->curtime < m_flBounceEnd && iHealth - iDamage <= 0 )
			return false;

		CBasePlayer *pAttacker = ToBasePlayer( info.GetAttacker() );
		if ( !pAttacker || pAttacker->GetTeamNumber() != TEAM_HIDDEN )
			return true;

		if ( iDamage < B5PHYS_MIN_DAMAGE || iDamage > B5PHYS_MAX_DAMAGE )
			return true;

		Shake( pVictim );

		// Victims with 75 health or less still take the full hit, so it can gib them.
		if ( iHealth > B5PHYS_MAX_HURT )
		{
			info.SetDamage( B5PHYS_MAX_HURT );
			m_flBounceEnd = gpGlobals->curtime + B5PHYS_BOUNCE_TIME;
		}

		return true;
	}

private:
	void Shake( CHidden_Player *pVictim )
	{
		switch ( hsm_b5physics_shaker.GetInt() )
		{
		case 1:
			{
				CSingleUserRecipientFilter filter( pVictim );
				SendShake( filter, B5PHYS_SHAKE_AMPLITUDE );
			}
			break;

		case 2:
			// Living players, stronger the closer they are.
			for ( int i = 1; i <= gpGlobals->maxClients; i++ )
			{
				CBasePlayer *pPlayer = UTIL_PlayerByIndex( i );
				if ( !pPlayer || !pPlayer->IsAlive() )
					continue;

				const float flRange = MAX( 1.0f, pPlayer->GetAbsOrigin().DistTo( pVictim->GetAbsOrigin() ) );
				CSingleUserRecipientFilter filter( pPlayer );
				SendShake( filter, hsm_b5physics_range.GetFloat() / flRange );
			}
			break;

		case 3:
			{
				CReliableBroadcastRecipientFilter filter;
				SendShake( filter, B5PHYS_SHAKE_AMPLITUDE );
			}
			break;
		}
	}

	// The plugin's env_Shake: a Shake user message, amplitude capped at 100.
	void SendShake( IRecipientFilter &filter, float flAmplitude )
	{
		UserMessageBegin( filter, "Shake" );
			WRITE_BYTE( SHAKE_START );
			WRITE_FLOAT( MIN( flAmplitude, 100.0f ) );
			WRITE_FLOAT( B5PHYS_SHAKE_FREQUENCY );
			WRITE_FLOAT( B5PHYS_SHAKE_DURATION );
		MessageEnd();
	}

	float m_flBounceEnd;
};

static CHiddenPluginB5Phys s_B5Phys;
