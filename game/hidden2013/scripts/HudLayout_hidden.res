// SDK 2013 HUD elements that Beta 4b's scripts/HudLayout.res predates, positioned as in SDK 2013's
// HL2MP HudLayout.res. Loaded after Beta 4b's layout (clientmode_shared.cpp, baseviewport.cpp).

"Resource/HudLayout_hidden.res"
{
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
