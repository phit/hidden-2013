//========= Hidden: Source =====================================================//
//
// Purpose: The client's copy of marine_clip. Solid with the server's model and collision group,
//			so client traces (movement prediction) find it and the shared rule in
//			PassServerEntityFilter stops marines at it and lets the Hidden through, as the
//			server does. Never drawn (EF_NODRAW). See docs/spec/map-entities.md.
//
//=============================================================================//

#include "cbase.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

class C_MarineClip : public C_BaseEntity
{
public:
	DECLARE_CLASS( C_MarineClip, C_BaseEntity );
	DECLARE_CLIENTCLASS();
};

IMPLEMENT_CLIENTCLASS_DT( C_MarineClip, DT_MarineClip, CMarineClip )
END_RECV_TABLE()
