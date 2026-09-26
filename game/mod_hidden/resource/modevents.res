//=========== (C) Copyright 1999 Valve, L.L.C. All rights reserved. ===========
//
// The copyright to the contents herein is the property of Valve, L.L.C.
// The contents may be used and/or copied only with the written permission of
// Valve, L.L.C., or in accordance with the terms and conditions stipulated in
// the agreement/contract under which the contents have been supplied.
//=============================================================================

// No spaces in event names, max length 32
// All strings are case sensitive
//
// valid data key types are:
//   string : a zero terminated string
//   bool   : unsigned int, 1 bit
//   byte   : unsigned int, 8 bit
//   short  : signed int, 16 bit
//   long   : signed int, 32 bit
//   float  : float, 32 bit
//   local  : any data, but not networked to clients
//
// following key names are reserved:
//   local      : if set to 1, event is not networked to clients
//   unreliable : networked, but unreliable
//   suppress   : never fire this event
//   time	: firing server time
//   eventid	: holds the event ID

"modevents"
{
	"player_death"				// a game event, name may be 32 charaters long
	{
		"userid"	"short"   	// user ID who died				
		"attacker"	"short"	 	// user ID who killed
		"weapon"	"string" 	// weapon name killed used 
	}
	
	"teamplay_round_start"			// round restart
	{
		"full_reset"	"bool"		// is this a full reset of the map
	}
	
	"spec_target_updated"
	{
	}
	
	"achievement_earned"
	{
		"player"	"byte"		// entindex of the player
		"achievement"	"short"		// achievement ID
	}

	// Beta 4b's player_hurt adds the damage and whether the Hidden was involved (docs/spec/sounds.md).
	"player_hurt"
	{
		"userid"	"short"   	// user ID of the player hurt
		"attacker"	"short"	 	// user ID of the attacker, 0 for the world
		"health"	"byte"		// health left
		"damage"	"float"
		"hidden"	"bool"		// the attacker or the victim is the Hidden
	}

	// Hidden: Source (docs/spec/game-rules.md, client.md)
	"game_round_restart"			// players were reset for a new round
	{
	}

	"game_round_start"			// the round is live
	{
	}

	"game_round_end"
	{
	}

	"material_check"			// clients compare the cloak material with the server's
	{
		"vmt_CRC"	"long"
		"bump_CRC"	"long"
	}

	"player_location"			// a player entered a location_brush
	{
		"userid"	"short"
		"location"	"string"
	}

	"iris_radio"				// a marine used the radio
	{
		"userid"	"short"
		"message"	"short"
	}

	"alarm_trigger"				// a sonic alarm went off
	{
		"posx"		"float"
		"posy"		"float"
		"posz"		"float"
	}
}
