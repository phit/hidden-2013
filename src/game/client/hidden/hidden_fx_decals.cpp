//========= Hidden: Source =====================================================//
//
// Purpose: Keeps the model versions of Beta 4b's decals loaded. The decal materials name them in
//			$modelmaterial, but SDK 2013's engine takes no reference on them, so they were
//			unloaded and model decals (FN303 paint, blood on props) drew as the missing
//			texture, with "bad reference count 0 when being bound" in the console.
//
//=============================================================================//

#include "cbase.h"
#include "clienteffectprecachesystem.h"

// memdbgon must be the last include file in a .cpp file!!!
#include "tier0/memdbgon.h"

CLIENTEFFECT_REGISTER_BEGIN( PrecacheHiddenModelDecals )
CLIENTEFFECT_MATERIAL( "decals/fn303splashmodel" )
CLIENTEFFECT_MATERIAL( "decals/bloodsplashmodel" )
CLIENTEFFECT_REGISTER_END()
