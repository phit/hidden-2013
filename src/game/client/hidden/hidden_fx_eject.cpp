//========= Hidden: Source =====================================================//
//
// Purpose: Spent shells and dropped magazines. The server dispatches the magazine
//			effects on reload; the viewmodels' AE_CLIENT_EFFECT_ATTACH events
//			dispatch the shells. See docs/spec/weapons.md.
//
//=============================================================================//

#include "cbase.h"
#include "c_te_effect_dispatch.h"
#include "c_te_legacytempents.h"
#include "tempent.h"
#include "igamesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

extern ConVar cl_ejectbrass;

ConVar cl_brasslife( "cl_brasslife", "90", 0, "Seconds ejected shell casings stay on the ground" );
ConVar cl_jimmeh( "cl_jimmeh", "0" );

// Hit sounds (CTempEnts::PlaySound)
#define TE_PISTOL_SHELL		2048
#define TE_SHOTGUN_SHELL	4096

enum HiddenShell_t
{
	HIDDEN_SHELL_57 = 0,
	HIDDEN_SHELL_556,
	HIDDEN_SHELL_9MM,
	HIDDEN_SHELL_12GAUGE,

	HIDDEN_SHELL_COUNT
};

enum HiddenClip_t
{
	HIDDEN_CLIP_PISTOL = 0,
	HIDDEN_CLIP_FN2000,
	HIDDEN_CLIP_P90,
	HIDDEN_CLIP_FN303,

	HIDDEN_CLIP_COUNT
};

static const char *s_pszShellModels[HIDDEN_SHELL_COUNT] =
{
	"models/Shells/shell_57.mdl",
	"models/Shells/shell_556.mdl",
	"models/Shells/shell_9mm.mdl",
	"models/Shells/shell_12gauge.mdl",
};

static const char *s_pszClipModels[HIDDEN_CLIP_COUNT] =
{
	"models/weapons/fnp9/w_fnp9mag.mdl",
	"models/weapons/f2000/w_f2000mag.mdl",
	"models/weapons/p90/w_p90mag.mdl",
	"models/weapons/fn303/w_fn303mag.mdl",
};

// cl_jimmeh swaps every shell and magazine for this.
#define JIMMY_MODEL "models/generic/jimmy.mdl"

//-----------------------------------------------------------------------------
// Loads the models with the level, like CTempEnts::LevelInit.
//-----------------------------------------------------------------------------
class CHiddenEjectModels : public CAutoGameSystem
{
public:
	CHiddenEjectModels() : CAutoGameSystem( "CHiddenEjectModels" ) { Clear(); }

	virtual void LevelInitPreEntity( void )
	{
		for ( int i = 0; i < HIDDEN_SHELL_COUNT; i++ )
			m_pShells[i] = (model_t *)engine->LoadModel( s_pszShellModels[i] );

		for ( int i = 0; i < HIDDEN_CLIP_COUNT; i++ )
			m_pClips[i] = (model_t *)engine->LoadModel( s_pszClipModels[i] );

		m_pJimmy = (model_t *)engine->LoadModel( JIMMY_MODEL );
	}

	virtual void LevelShutdownPostEntity( void ) { Clear(); }

	const model_t *GetShell( int nType ) const { return cl_jimmeh.GetBool() ? m_pJimmy : m_pShells[nType]; }
	const model_t *GetClip( int nType ) const { return cl_jimmeh.GetBool() ? m_pJimmy : m_pClips[nType]; }

private:
	void Clear( void )
	{
		memset( m_pShells, 0, sizeof( m_pShells ) );
		memset( m_pClips, 0, sizeof( m_pClips ) );
		m_pJimmy = NULL;
	}

	model_t *m_pShells[HIDDEN_SHELL_COUNT];
	model_t *m_pClips[HIDDEN_CLIP_COUNT];
	model_t *m_pJimmy;
};

static CHiddenEjectModels s_EjectModels;

//-----------------------------------------------------------------------------
// A spent shell: CS:S's CSEjectBrass as it was in 2006. m_fFlags carries the speed
// from the viewmodel event.
//-----------------------------------------------------------------------------
static void EjectShell( const CEffectData &data, int nType )
{
	if ( !cl_ejectbrass.GetBool() )
		return;

	const model_t *pModel = s_EjectModels.GetShell( nType );
	if ( !pModel )
		return;

	Vector vecForward, vecRight, vecUp;
	AngleVectors( data.m_vAngles, &vecForward, &vecRight, &vecUp );

	Vector vecVelocity = vecForward * data.m_fFlags * random->RandomFloat( 1.2f, 2.8f ) +
						 vecUp * random->RandomFloat( -10.0f, 10.0f ) +
						 vecRight * random->RandomFloat( -20.0f, 20.0f );

	C_BasePlayer *pShooter = C_BasePlayer::GetLocalPlayer();
	if ( pShooter )
		vecVelocity += pShooter->GetAbsVelocity();

	const QAngle angles = pShooter ? pShooter->EyeAngles() : vec3_angle;
	C_LocalTempEntity *pTemp = tempents->SpawnTempModel( pModel, data.m_vOrigin, angles, vecVelocity, cl_brasslife.GetFloat(), 0 );
	if ( !pTemp )
		return;

	// Beta 4b gives the 5.56 the pistol shell's sound.
	pTemp->hitSound = ( nType == HIDDEN_SHELL_12GAUGE ) ? TE_SHOTGUN_SHELL : TE_PISTOL_SHELL;
	pTemp->SetGravity( 0.4f );
	pTemp->flags = FTENT_FADEOUT | FTENT_GRAVITY | FTENT_COLLIDEALL | FTENT_HITSOUND | FTENT_ROTATE;

	pTemp->m_vecTempEntAngVelocity[0] = random->RandomFloat( -256.0f, 256.0f );
	pTemp->m_vecTempEntAngVelocity[1] = random->RandomFloat( -256.0f, 256.0f );
	pTemp->m_vecTempEntAngVelocity[2] = 0.0f;
}

//-----------------------------------------------------------------------------
// A dropped magazine: HL2's EjectBrass, falling from the player's origin, and
// lasting as long as a shell.
//-----------------------------------------------------------------------------
static void EjectClip( const CEffectData &data, int nType )
{
	if ( !cl_ejectbrass.GetBool() )
		return;

	const model_t *pModel = s_EjectModels.GetClip( nType );
	if ( !pModel )
		return;

	Vector vecDir;
	AngleVectors( data.m_vAngles, &vecDir );
	vecDir *= random->RandomFloat( 150.0f, 200.0f );

	const Vector vecVelocity( vecDir[0] + random->RandomFloat( -64.0f, 64.0f ),
							  vecDir[1] + random->RandomFloat( -64.0f, 64.0f ),
							  vecDir[2] + random->RandomFloat( 0.0f, 64.0f ) );

	C_LocalTempEntity *pTemp = tempents->SpawnTempModel( pModel, data.m_vOrigin, data.m_vAngles, vecVelocity, cl_brasslife.GetFloat(),
		FTENT_COLLIDEWORLD | FTENT_FADEOUT | FTENT_GRAVITY | FTENT_ROTATE );
	if ( !pTemp )
		return;

	pTemp->m_vecTempEntAngVelocity[0] = random->RandomFloat( -1024.0f, 1024.0f );
	pTemp->m_vecTempEntAngVelocity[1] = random->RandomFloat( -1024.0f, 1024.0f );
	pTemp->m_vecTempEntAngVelocity[2] = random->RandomFloat( -1024.0f, 1024.0f );
}

#define DECLARE_EJECT_EFFECT( name, func, type ) \
	static void name##_Callback( const CEffectData &data ) { func( data, type ); } \
	DECLARE_CLIENT_EFFECT( #name, name##_Callback );

DECLARE_EJECT_EFFECT( EjectBrass_57, EjectShell, HIDDEN_SHELL_57 )
DECLARE_EJECT_EFFECT( EjectBrass_556, EjectShell, HIDDEN_SHELL_556 )
DECLARE_EJECT_EFFECT( EjectBrass_9mm, EjectShell, HIDDEN_SHELL_9MM )
DECLARE_EJECT_EFFECT( EjectBrass_12Gauge, EjectShell, HIDDEN_SHELL_12GAUGE )

DECLARE_EJECT_EFFECT( EjectPistolClip, EjectClip, HIDDEN_CLIP_PISTOL )
DECLARE_EJECT_EFFECT( EjectFN2000Clip, EjectClip, HIDDEN_CLIP_FN2000 )
DECLARE_EJECT_EFFECT( EjectFNP90Clip, EjectClip, HIDDEN_CLIP_P90 )
DECLARE_EJECT_EFFECT( EjectFN303Clip, EjectClip, HIDDEN_CLIP_FN303 )
