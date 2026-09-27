//========= Hidden: Source =====================================================//
//
// Purpose: The team and weapon menus' 3D marine preview (CSClassImagePanel).
//
//=============================================================================//

#ifndef HIDDEN_CLASSIMAGE_H
#define HIDDEN_CLASSIMAGE_H
#pragma once

// Draws the model into the first visible CSClassImagePanel; called after the HUD is painted.
void HiddenClassImage_PostRenderVGui( void );

// Which marine the preview shows (deviation: Beta 4b always showed the panel's model, skin 0). The
// support class gets its own model; the character picks the skin and body, as in game.
void HiddenClassImage_SetMarine( int iCharacter, int iClass );

#endif // HIDDEN_CLASSIMAGE_H
