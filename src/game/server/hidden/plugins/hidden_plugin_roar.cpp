//========= Hidden: Source =====================================================//
//
// Purpose: Beta 5 Hidden Roar (Paegus's hsm_roar 1.0.1): the Hidden cries out
//			when hurt. See docs/spec/plugins.md.
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define ROAR_MIN_DELAY		4.5f
#define ROAR_MAX_DELAY		9.5f

static ConVar hsm_roar( "hsm_roar", "0", FCVAR_NOTIFY, "Does the hidden cry out when hurt? 0: Disable, 1: Enable", true, 0.0f, true, 1.0f );
static ConVar hsm_roar_303( "hsm_roar_303", "0", FCVAR_NOTIFY, "Does the hidden only cry out on 303 hits? 0: All hits, 1: Only initial 303 hit", true, 0.0f, true, 1.0f );

class CHiddenPluginRoar : public CHiddenPlugin
{
public:
	CHiddenPluginRoar() { m_flNextRoar = 0.0f; }

	virtual void LevelInit( void )
	{
		m_flNextRoar = 0.0f;

		for ( int i = 1; i <= 8; i++ )
			CBaseEntity::PrecacheSound( UTIL_VarArgs( "player/hidden/voice/617-303pain%02i.mp3", i ) );
		for ( int i = 1; i <= 6; i++ )
			CBaseEntity::PrecacheSound( UTIL_VarArgs( "player/hidden/voice/617-pain%02i.mp3", i ) );
	}

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		if ( !hsm_roar.GetBool() || pVictim->GetTeamNumber() != TEAM_HIDDEN || pVictim->GetHealth() <= 0 || gpGlobals->curtime < m_flNextRoar )
			return;

		// The plugin told the FN303's bolt (3 damage) and its stun (0.3 every 0.2 s) apart by the damage.
		const bool bBolt = info.GetInflictor() && FClassnameIs( info.GetInflictor(), "fn303_bolt" );
		const bool bStun = ( info.GetDamageType() == DMG_DIRECT );

		char szSound[64];
		if ( bBolt )
			Q_snprintf( szSound, sizeof( szSound ), "player/hidden/voice/617-303pain%02i.mp3", random->RandomInt( 1, 8 ) );
		else if ( hsm_roar_303.GetBool() )
			return;
		else if ( bStun )
			Q_snprintf( szSound, sizeof( szSound ), "player/hidden/voice/617-303pain%02i.mp3", random->RandomInt( 1, 8 ) );
		else
			Q_snprintf( szSound, sizeof( szSound ), "player/hidden/voice/617-pain%02i.mp3", random->RandomInt( 1, 6 ) );

		const Vector vecEyes = pVictim->EyePosition();
		CPASAttenuationFilter filter( pVictim, SNDLVL_90dB );
		EmitSound_t params;
		params.m_pSoundName = szSound;
		params.m_SoundLevel = SNDLVL_90dB;
		params.m_nChannel = CHAN_AUTO;
		params.m_flVolume = VOL_NORM;
		params.m_pOrigin = &vecEyes;
		CBaseEntity::EmitSound( filter, pVictim->entindex(), params );

		// The plugin's timer was RandomFloat( 9.5, 4.5 ).
		m_flNextRoar = gpGlobals->curtime + random->RandomFloat( ROAR_MIN_DELAY, ROAR_MAX_DELAY );
	}

private:
	float m_flNextRoar;
};

static CHiddenPluginRoar s_Roar;
