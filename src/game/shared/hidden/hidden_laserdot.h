//========= Hidden: Source =====================================================//
//
// Purpose: The laser sight's dot (Beta 4b's CLaserDot, HL2MP's RPG dot with its own
//			sprite). See docs/spec/hidden-abilities.md.
//
//=============================================================================//

#ifndef HIDDEN_LASERDOT_H
#define HIDDEN_LASERDOT_H
#pragma once

#ifdef CLIENT_DLL
	#define CHiddenLaserDot C_HiddenLaserDot
	#include "materialsystem/imaterial.h"
#endif

class CHiddenLaserDot : public CBaseEntity
{
public:
	DECLARE_CLASS( CHiddenLaserDot, CBaseEntity );
	DECLARE_NETWORKCLASS();

#ifndef CLIENT_DLL
	DECLARE_DATADESC();

	static CHiddenLaserDot *Create( const Vector &vecOrigin, CBaseEntity *pOwner );

	// The server keeps the dot where the owner aims, so it's sent with the right PVS.
	void SetLaserPosition( const Vector &vecOrigin, const Vector &vecNormal );

	virtual int ObjectCaps( void ) { return ( BaseClass::ObjectCaps() & ~FCAP_ACROSS_TRANSITION ) | FCAP_DONT_SAVE; }

private:
	Vector m_vecSurfaceNormal;
#else
	virtual void OnDataChanged( DataUpdateType_t updateType );
	virtual bool ShouldDraw( void ) { return !IsEffectActive( EF_NODRAW ); }
	virtual int DrawModel( int flags );
	virtual RenderGroup_t GetRenderGroup( void ) { return RENDER_GROUP_TRANSLUCENT_ENTITY; }
	virtual bool IsTransparent( void ) { return true; }

private:
	CMaterialReference m_hSpriteMaterial;
#endif
};

#endif // HIDDEN_LASERDOT_H
