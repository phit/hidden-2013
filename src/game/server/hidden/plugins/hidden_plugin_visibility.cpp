//========= Hidden: Source =====================================================//
//
// Purpose: Visibility (Paegus's hsm_visibility 1.2.4): the Hidden can show
//			itself, as a plant pot by default. See docs/spec/plugins.md.
//
//			Original: https://forums.alliedmods.net/showthread.php?p=1825373
//
//=============================================================================//

#include "cbase.h"
#include "hidden_plugins.h"
#include "hidden_player.h"
#include "filesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

#define VIS_DEFAULT_MODEL	"models/executive/hr_model_plantpot01.mdl"

extern ConVar mp_chattime;

static void VisModelChanged( IConVar *var, const char *pOldValue, float flOldValue );
static void VisChanged( IConVar *var, const char *pOldValue, float flOldValue );

static ConVar hsm_vis( "hsm_vis", "0", FCVAR_NOTIFY, "Enable or disable Hidden's visibility toggle.", true, 0.0f, true, 1.0f, VisChanged );
static ConVar hsm_vis_mintime( "hsm_vis_mintime", "0.2", FCVAR_NOTIFY, "Minimum number of seconds that hidden appears for when toggling visibility.", true, 0.1f, true, 2.0f );
static ConVar hsm_vis_model( "hsm_vis_model", VIS_DEFAULT_MODEL, FCVAR_NOTIFY, "Model file to use for visible hidden player.", VisModelChanged );

class CHiddenPluginVisibility : public CHiddenPlugin
{
public:
	CHiddenPluginVisibility()
	{
		m_bAllowed = false;
		m_flHideAllTime = 0.0f;
		m_szVisibleModel[0] = '\0';
		Reset();
	}

	virtual void LevelInit( void )
	{
		Reset();
		m_flHideAllTime = 0.0f;
		m_bAllowed = true;
		UpdateModel();
	}

	virtual void RoundStart( void )
	{
		m_flHideAllTime = 0.0f;
		m_bAllowed = true;
	}

	// Everyone goes back to normal just before the next round.
	virtual void RoundEnd( void )
	{
		m_flHideAllTime = gpGlobals->curtime + mp_chattime.GetFloat() - 0.5f;
	}

	virtual void Think( void )
	{
		if ( m_flHideAllTime > 0.0f && gpGlobals->curtime >= m_flHideAllTime )
		{
			m_flHideAllTime = 0.0f;
			HideAll();
			m_bAllowed = false;
		}

		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			if ( m_flLockEnd[i] > 0.0f && gpGlobals->curtime >= m_flLockEnd[i] )
			{
				m_flLockEnd[i] = 0.0f;
				CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
				if ( pPlayer && !m_bWantVisible[i] )
					Hide( pPlayer );
			}
		}
	}

	virtual void PlayerHurt( CHidden_Player *pVictim, const CTakeDamageInfo &info )
	{
		const int i = pVictim->entindex();
		if ( pVictim->GetHealth() <= 0 )
		{
			// Dying: the corpse takes its own model, so only forget the state.
			m_bWantVisible[i] = false;
			m_flLockEnd[i] = 0.0f;
			pVictim->SetRevealed( false );
			return;
		}

		if ( CanDisappear( pVictim ) )
			Hide( pVictim );
	}

	virtual void ClientDisconnect( CHidden_Player *pPlayer )
	{
		m_bWantVisible[pPlayer->entindex()] = false;
		m_flLockEnd[pPlayer->entindex()] = 0.0f;
	}

	void Appear( CHidden_Player *pPlayer )
	{
		if ( !CanAppear( pPlayer ) )
			return;

		const int i = pPlayer->entindex();
		m_bWantVisible[i] = true;
		m_flLockEnd[i] = gpGlobals->curtime + hsm_vis_mintime.GetFloat();

		if ( pPlayer->IsRevealed() )
			return;

		// Back to exactly this later.
		Q_strncpy( m_szHiddenModel[i], STRING( pPlayer->GetModelName() ), sizeof( m_szHiddenModel[i] ) );
		m_nHiddenSkin[i] = pPlayer->m_nSkin;
		m_nHiddenBody[i] = pPlayer->m_nBody;

		pPlayer->SetModel( m_szVisibleModel );
		pPlayer->m_nSkin = 0;
		pPlayer->m_nBody = 1;
		pPlayer->SetRevealed( true );
		KeepHull( pPlayer );

		CSingleUserRecipientFilter filter( pPlayer );
		CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Hidden.AuraOut" );

		UTIL_LogPrintf( "\"%s\" Appeared!\n", HiddenPlugins_LogName( pPlayer ) );
	}

	void Disappear( CHidden_Player *pPlayer )
	{
		if ( CanDisappear( pPlayer ) )
			Hide( pPlayer );
	}

private:
	void Reset( void )
	{
		for ( int i = 0; i <= MAX_PLAYERS; i++ )
		{
			m_bWantVisible[i] = false;
			m_flLockEnd[i] = 0.0f;
			m_szHiddenModel[i][0] = '\0';
			m_nHiddenSkin[i] = 0;
			m_nHiddenBody[i] = 0;
		}
	}

	bool IsLivingHidden( CHidden_Player *pPlayer ) const { return pPlayer && pPlayer->IsAlive() && pPlayer->GetTeamNumber() == TEAM_HIDDEN; }
	bool CanAppear( CHidden_Player *pPlayer ) const { return hsm_vis.GetBool() && m_bAllowed && IsLivingHidden( pPlayer ) && !m_bWantVisible[pPlayer->entindex()]; }
	bool CanDisappear( CHidden_Player *pPlayer ) const { return hsm_vis.GetBool() && m_bAllowed && IsLivingHidden( pPlayer ) && m_bWantVisible[pPlayer->entindex()]; }

	// Hides a Hidden once its minimum time is up.
	void Hide( CHidden_Player *pPlayer )
	{
		const int i = pPlayer->entindex();
		m_bWantVisible[i] = false;

		if ( m_flLockEnd[i] > 0.0f || !pPlayer->IsRevealed() )
			return;

		if ( m_szHiddenModel[i][0] )
			pPlayer->SetModel( m_szHiddenModel[i] );
		pPlayer->m_nSkin = m_nHiddenSkin[i];
		pPlayer->m_nBody = m_nHiddenBody[i];
		pPlayer->SetRevealed( false );
		KeepHull( pPlayer );

		CSingleUserRecipientFilter filter( pPlayer );
		CBaseEntity::EmitSound( filter, SOUND_FROM_LOCAL_PLAYER, "Hidden.AuraIn" );

		UTIL_LogPrintf( "\"%s\" Vanished!\n", HiddenPlugins_LogName( pPlayer ) );
	}

public:
	void HideAll( void )
	{
		for ( int i = 1; i <= gpGlobals->maxClients; i++ )
		{
			CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_PlayerByIndex( i ) );
			if ( pPlayer && pPlayer->IsRevealed() )
			{
				m_flLockEnd[i] = 0.0f;
				Hide( pPlayer );
			}
		}
	}

	void UpdateModel( void )
	{
		const char *pszModel = hsm_vis_model.GetString();
		if ( !filesystem->FileExists( pszModel, "GAME" ) )
		{
			Warning( "[VISTOG] Model \"%s\" does not exist in filesystem. Using default: \"%s\"\n", pszModel, VIS_DEFAULT_MODEL );
			pszModel = VIS_DEFAULT_MODEL;
		}

		Q_strncpy( m_szVisibleModel, pszModel, sizeof( m_szVisibleModel ) );
		CBaseEntity::PrecacheModel( m_szVisibleModel );
	}

private:
	// SetModel sizes the player to the model; players keep their own hull.
	void KeepHull( CHidden_Player *pPlayer )
	{
		pPlayer->SetCollisionBounds( pPlayer->GetPlayerMins(), pPlayer->GetPlayerMaxs() );
	}

	bool m_bAllowed;					// not between a round's end and the next
	float m_flHideAllTime;
	char m_szVisibleModel[MAX_PATH];
	bool m_bWantVisible[MAX_PLAYERS + 1];
	float m_flLockEnd[MAX_PLAYERS + 1];	// stays visible until then
	char m_szHiddenModel[MAX_PLAYERS + 1][MAX_PATH];
	int m_nHiddenSkin[MAX_PLAYERS + 1];
	int m_nHiddenBody[MAX_PLAYERS + 1];
};

static CHiddenPluginVisibility s_Visibility;

static void VisChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	if ( !hsm_vis.GetBool() )
		s_Visibility.HideAll();
}

static void VisModelChanged( IConVar *var, const char *pOldValue, float flOldValue )
{
	// Only with a map loaded; otherwise the next map's start picks it up.
	if ( gpGlobals->mapname != NULL_STRING )
		s_Visibility.UpdateModel();
}

static void CC_Visible( const CCommand &args )
{
	s_Visibility.Appear( ToHiddenPlayer( UTIL_GetCommandClient() ) );
}

static void CC_Invisible( const CCommand &args )
{
	CHidden_Player *pPlayer = ToHiddenPlayer( UTIL_GetCommandClient() );
	if ( pPlayer )
		s_Visibility.Disappear( pPlayer );
}

static ConCommand plus_visible( "+visible", CC_Visible, "Makes Hidden player visible using model defined by hsm_vis_model", FCVAR_GAMEDLL );
static ConCommand minus_visible( "-visible", CC_Invisible, "Makes Hidden player invisible again.", FCVAR_GAMEDLL );
