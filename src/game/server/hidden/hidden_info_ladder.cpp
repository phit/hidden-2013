//========= Hidden: Source =====================================================//
//
// Purpose: info_ladder, the data-only entity vbsp writes for every func_ladder
//			brush in the old maps (Beta 4b's SDK had it; SDK 2013 doesn't). The
//			ladder itself is the brush; this only keeps its bounds, e.g. for
//			building ladders into the bots' nav mesh.
//
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class CInfoLadder : public CBaseEntity
{
public:
	DECLARE_CLASS( CInfoLadder, CBaseEntity );

	CInfoLadder() : m_vecMins( vec3_origin ), m_vecMaxs( vec3_origin ) {}

	virtual bool KeyValue( const char *szKeyName, const char *szValue );

private:
	Vector m_vecMins;
	Vector m_vecMaxs;
};

LINK_ENTITY_TO_CLASS( info_ladder, CInfoLadder );

bool CInfoLadder::KeyValue( const char *szKeyName, const char *szValue )
{
	static const char *s_pszKeys[] = { "mins.x", "mins.y", "mins.z", "maxs.x", "maxs.y", "maxs.z" };

	for ( int i = 0; i < ARRAYSIZE( s_pszKeys ); i++ )
	{
		if ( !Q_stricmp( szKeyName, s_pszKeys[i] ) )
		{
			Vector &vec = ( i < 3 ) ? m_vecMins : m_vecMaxs;
			vec[i % 3] = atof( szValue );
			SetCollisionBounds( m_vecMins, m_vecMaxs );
			break;
		}
	}

	return BaseClass::KeyValue( szKeyName, szValue );
}
