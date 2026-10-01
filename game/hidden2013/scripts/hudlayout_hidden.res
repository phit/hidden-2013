// SDK 2013 HUD elements that Beta 4b's scripts/HudLayout.res predates, positioned as in SDK 2013's
// HL2MP HudLayout.res. Loaded after Beta 4b's layout (clientmode_shared.cpp, baseviewport.cpp).

"Resource/HudLayout_hidden.res"
{
	// The numbered menu (plugin menus such as HandiCap's). Beta 4b's scheme has none of the fonts
	// and colours SDK 2013's menu takes by default, so it drew nothing.
	HudMenu
	{
		"fieldName"		"HudMenu"
		"visible"		"1"
		"enabled"		"1"
		"wide"			"640"
		"tall"			"480"
		"TextFont"		"HudSelectionText"
		"ItemFont"		"HudSelectionText"
		"ItemFontPulsing"	"HudSelectionText"
		"MenuColor"		"BrightFg"
		"MenuItemColor"	"Orange"
		"MenuBoxColor"	"0 0 0 128"
	}
	// The kill feed. SDK 2013's sizes itself to the whole screen and draws from its right edge, so
	// Beta 4b's xpos ("r640", for a 628-wide panel) put the notices off-screen on widescreens.
	HudDeathNotice
	{
		"fieldName"		"HudDeathNotice"
		"visible"		"1"
		"enabled"		"1"
		"xpos"			"0"
		"ypos"			"12"
		"wide"			"628"
		"tall"			"468"
		"MaxDeathNotices"	"4"
		"LineHeight"	"22"
		"RightJustify"	"1"
		"TextFont"		"Default"
	}
	AchievementNotificationPanel
	{
		"fieldName"		"AchievementNotificationPanel"
		"visible"		"1"
		"enabled"		"1"
		"xpos"			"0"
		"ypos"			"180"
		"wide"			"f10"
		"tall"			"100"
	}
	CHudVote
	{
		"fieldName"		"CHudVote"
		"xpos"			"0"
		"ypos"			"0"
		"wide"			"640"
		"tall"			"480"
		"visible"		"1"
		"enabled"		"1"
		"bgcolor_override"	"0 0 0 0"
		"PaintBackgroundType"	"0"
	}
	HUDAutoAim
	{
		"fieldName"	"HUDAutoAim"
		"visible"	"1"
		"enabled"	"1"
		"wide"		"640"
		"tall"		"480"
	}
	HudCommentary
	{
		"fieldName"	"HudCommentary"
		"xpos"		"c-190"
		"ypos"		"350"
		"wide"		"380"
		"tall"		"40"
		"visible"	"1"
		"enabled"	"1"
		"PaintBackgroundType"	"2"
		"bar_xpos"		"50"
		"bar_ypos"		"20"
		"bar_height"	"8"
		"bar_width"		"320"
		"speaker_xpos"	"50"
		"speaker_ypos"	"8"
		"count_xpos_from_right"	"10"
		"count_ypos"	"8"
		"icon_texture"	"vgui/hud/icon_commentary"
		"icon_xpos"		"0"
		"icon_ypos"		"0"
		"icon_width"	"40"
		"icon_height"	"40"
	}
	HudHDRDemo
	{
		"fieldName"	"HudHDRDemo"
		"xpos"		"0"
		"ypos"		"0"
		"wide"		"640"
		"tall"		"480"
		"visible"	"1"
		"enabled"	"1"
		"Alpha"		"255"
		"PaintBackgroundType"	"2"
		"BorderColor"	"0 0 0 255"
		"BorderLeft"	"16"
		"BorderRight"	"16"
		"BorderTop"		"16"
		"BorderBottom"	"64"
		"BorderCenter"	"0"
		"TextColor"		"255 255 255 255"
		"LeftTitleY"	"422"
		"RightTitleY"	"422"
	}
	HudHintKeyDisplay
	{
		"fieldName"	"HudHintKeyDisplay"
		"visible"	"0"
		"enabled"	"1"
		"xpos"		"r120"
		"ypos"		"r340"
		"wide"		"100"
		"tall"		"200"
		"text_xpos"	"8"
		"text_ypos"	"8"
		"text_xgap"	"8"
		"text_ygap"	"8"
		"TextColor"	"255 170 0 220"
		"PaintBackgroundType"	"2"
	}
}
