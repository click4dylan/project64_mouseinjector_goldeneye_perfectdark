//==========================================================================
// Mouse Injector Plugin
//==========================================================================
// Copyright (C) 2018 Carnivorous
// All rights reserved.
//
// Mouse Injector is free software; you can redistribute it and/or modify it
// under the terms of the GNU General Public License as published by the Free
// Software Foundation; either version 2 of the License, or (at your option)
// any later version.
//
// This program is distributed in the hope that it will be useful, but
// WITHOUT ANY WARRANTY; without even the implied warranty of MERCHANTABILITY
// or FITNESS FOR A PARTICULAR PURPOSE. See the GNU General Public License
// for more details.
//
// You should have received a copy of the GNU General Public License
// along with this program; if not, visit http://www.gnu.org/licenses/gpl-2.0.html
//==========================================================================
#include <math.h>
#include "../global.h"
#include "../device.h"
#include "../maindll.h"
#include "game.h"
#include "memory.h"

#define GUNAIMLIMIT 14.12940025 // 0x41621206
#define CROSSHAIRLIMIT 18.76135635 // 0x41961742
#define GUNRECOILXLIMIT 756.1247559 // 0x443D07FC
#define GUNRECOILYLIMIT 57.63883972 // 0x42668E2C
#define BIKEXROTATIONLIMIT 6.282184601 // 0x40C907A8
#define BIKEROLLLIMIT 0.7852724195 // 0xBF49079D/0x3F49079D
#define BIKESPEEDLIMIT 0.5 // 0x3F000000
// PERFECT DARK ADDRESSES - OFFSET ADDRESSES BELOW (REQUIRES PLAYERBASE/BIKEBASE TO USE)
#define PD_crouchflag 0x801BB74C - 0x801BB6A0
#define PD_deathflag 0x801BB778 - 0x801BB6A0
#define PD_camx 0x801BB7E4 - 0x801BB6A0
#define PD_camy 0x801BB7F4 - 0x801BB6A0
#define PD_fov 0x801BCEE8 - 0x801BB6A0
#define PD_crosshairx 0x801BCD08 - 0x801BB6A0
#define PD_crosshairy 0x801BCD0C - 0x801BB6A0
#define PD_grabflag 0x801BB850 - 0x801BB6A0
#define PD_thirdperson 0x801BB6A0 - 0x801BB6A0
#define PD_gunrx 0x801BC374 - 0x801BB6A0
#define PD_gunry 0x801BC378 - 0x801BB6A0
#define PD_gunlx 0x801BCB18 - 0x801BB6A0
#define PD_gunly 0x801BCB1C - 0x801BB6A0
#define PD_aimingflag 0x801BB7C0 - 0x801BB6A0
#define PD_gunrstate 0x801BC2DC - 0x801BB6A0
#define PD_gunlstate 0x801BCA80 - 0x801BB6A0
#define PD_gunrxrecoil 0x801BBE98 - 0x801BB6A0
#define PD_gunryrecoil 0x801BBE9C - 0x801BB6A0
#define PD_gunlxrecoil 0x801BC63C - 0x801BB6A0
#define PD_gunlyrecoil 0x801BC640 - 0x801BB6A0
#define PD_currentweapon 0x801BCC28 - 0x801BB6A0
#define PD_bikeroll 0x8052D69C - 0x8052D64C
#define PD_bikespeed 0x8052D694 - 0x8052D64C
// STATIC ADDRESSES BELOW
#define JOANNADATA(X) (unsigned int)EMU_ReadInt(0x8009A024, X * 0x4) // player pointer address (0x4 offset for each players)
#define PD_menu(X) 0x80070750 + X * 0x4 // player menu flag (0 = PD is in menu) (0x4 offset for each players)
#define PD_camera 0x8009A26C // camera flag (1 = gameplay, 2 & 3 = ???, 4 = multiplayer sweep, 5 = gameover screen, 6 = cutscene mode, 7 = force player to move: extraction's dark room)
#define PD_pause 0x80084014 // menu flag (1 = PD is paused)
#define PD_menuitem 0x800739F8 // menu item flag (used to check if PD is running)
#define PD_mppause 0x800ACBA2 // used to check if multiplayer match is paused
#define PD_defaultfov 0x802EAA5C // field of view default
#define PD_defaultfovzoom 0x802EACFC // field of view default for zoom
#define PD_introcounter 0x800624C4 // counter for intro

static unsigned int playerbase[4] = {0, 0, 0, 0}; // current player's joannadata address
static unsigned int bikebase[4][2] = {{0, 0}, {0, 0}, {0, 0}, {0, 0}}; // hoverbike's address and player's last grab state (used to find the exact moment when the player hops on a bike)
static int xstick[4] = {0, 0, 0, 0}, ystick[4] = {0, 0, 0, 0}, usingstick[4] = {0, 0, 0, 0}; // for camspy/slayer controls
static float xmenu[4] = {0, 0, 0, 0}, ymenu[4] = {0, 0, 0, 0}; // for pd radial nav function
static int radialmenudirection[4][4] = {{0, 0, 0, 0}, {0, 0, 0, 0}}; // used to override c buttons if user is interacting with a radial menu
static int safetoduck[4] = {1, 1, 1, 1}, safetostand[4] = {0, 0, 0, 0}, crouchstance[4] = {0, 0, 0, 0}; // used for crouch toggle changes (limits tick-tocking)
static float crosshairposx[4], crosshairposy[4], aimx[4], aimy[4];
static int gunrcenter[4], gunlcenter[4];

static int PD_Status(void);
static void PD_DetectMap(void);
static void PD_Inject(void);
static void PD_Crouch(const int player);
#define PD_ResetCrouchToggle(X) safetoduck[X] = 1, safetostand[X] = 0, crouchstance[X] = 0 // reset crouch toggle bind
#define PD_ResetXYStick(X) xstick[X] = 0, ystick[X] = 0, usingstick[X] = 0 // reset x/y stick array
#define PD_ResetRadialMenuBtns(X) for(int direction = 0; direction < 4; direction++) radialmenudirection[X][direction] = 0 // reset direction buttons
static void PD_AimMode(const int player, const int aimingflag, const float fov);
static void PD_CamspySlayer(const int player, const int camspyflag, const float sensitivity);
static void PD_RadialMenuNav(const int player);
static void PD_Controller(void);
static void PD_InjectHacks(void);
static void PD_Quit(void);

static const GAMEDRIVER GAMEDRIVER_INTERFACE =
{
	"Perfect Dark",
	PD_Status,
	PD_Inject,
	PD_Quit
};

const GAMEDRIVER *GAME_PERFECTDARK = &GAMEDRIVER_INTERFACE;

//==========================================================================
// Purpose: returns a value, which is then used to check what game is running in game.c
// Q: What is happening here?
// A: We look up some static addresses and if the values are within the expected ranges the program can assume that the game is currently running
//==========================================================================
static int PD_Status(void)
{
	const int pd_menu = EMU_ReadInt(PD_menu(0), 0), pd_camera = EMU_ReadInt(PD_camera, 0), pd_pause = EMU_ReadInt(PD_pause, 0), pd_romcheck = EMU_ReadInt(PD_menuitem, 0);
	if(pd_menu >= 0 && pd_menu <= 1 && pd_camera >= 0 && pd_camera <= 7 && pd_pause >= 0 && pd_pause <= 1 && pd_romcheck == 0x04010000)
		return 1;
	return 0;
}
//==========================================================================
// Purpose: searches for the current map memory locations
// Changes Globals: playerbase, safetoduck, safetostand, crouchstance, xmenu, ymenu, bikebase
//==========================================================================
static void PD_DetectMap(void)
{
	for(int player = PLAYER1; player < ALLPLAYERS; player++)
	{
		if(playerbase[player] != JOANNADATA(player) && (EMU_ReadInt(PD_camera, 0) == 1 || EMU_ReadInt(PD_camera, 0) == 3 || EMU_ReadInt(PD_camera, 0) == 4 || EMU_ReadInt(PD_camera, 0) == 6))
		{
			playerbase[player] = JOANNADATA(player);
			PD_ResetCrouchToggle(player); // reset crouch toggle on new map
			xmenu[player] = 0, ymenu[player] = 0; // reset menu direction
		}
		if(EMU_ReadInt(playerbase[player], PD_grabflag) == 3 && bikebase[player][1] != 3) // player has just now hopped on a bike, search for bike pointer
		{
			DEV_Sleep(20); // wait before we search for the bike pointer
			const unsigned int bikepointers[15] = {0x8052D5DC, 0x8054B1E8, 0x80552D8C, 0x8052A4FC, 0x8054ECDC, 0x80556DF4, 0x80501438, 0x8050EED4, 0x805142A4, 0x804CA4D4, 0x804D10D4, 0x803FF4F0, 0x803FF6A4, 0x803FF7A4, 0x803FF7BC}; // common bike pointers
			for(int i = 0; i < 32; i++)
			{
				for(int j = 0; j < 15; j++)
				{
					unsigned int bikeptr = EMU_ReadInt(bikepointers[j], 0) + 0x6C;
					if(EMU_ReadFloat(bikeptr, 0) <= BIKEXROTATIONLIMIT && EMU_ReadFloat(bikeptr, 0) >= 0 && EMU_ReadFloat(bikeptr, PD_bikeroll) <= BIKEROLLLIMIT && EMU_ReadFloat(bikeptr, PD_bikeroll) >= -BIKEROLLLIMIT && EMU_ReadFloat(bikeptr, PD_bikespeed) <= BIKESPEEDLIMIT && EMU_ReadFloat(bikeptr, PD_bikespeed) >= -BIKESPEEDLIMIT) // if bike base is valid
					{
						const float camx = -(EMU_ReadFloat(playerbase[player], PD_camx)) / (360 / BIKEXROTATIONLIMIT) + BIKEXROTATIONLIMIT; // player's rotation converted to bike rotation range
						if(roundf(EMU_ReadFloat(bikeptr, 0) * 100) / 100 != roundf(camx * 100) / 100) // bike rotation does not match player's rotation, keep searching
							continue;
						bikebase[player][0] = bikeptr; // success, we found it
						break;
					}
				}
				if(bikebase[player][0]) // bike base found, leave the while loop
					break;
				else
					DEV_Sleep(16); // wait one frame before we search again for the bike pointer
			}
		}
		else if(bikebase[player][1] != 3)
			bikebase[player][0] = 0;
		bikebase[player][1] = EMU_ReadInt(playerbase[player], PD_grabflag); // remember player's last grab state (used to find the exact moment when the player hops on a bike)
	}
}
//==========================================================================
// Purpose: calculate mouse movement and inject into current game
// Changes Globals: safetoduck, safetostand, crouchstance
//==========================================================================
static void PD_Inject(void)
{
	PD_DetectMap();
	if(!playerbase[PLAYER1]) // hacks can only be injected at boot sequence before code blocks are cached, so inject until player has spawned
		PD_InjectHacks();
	for(int player = PLAYER1; player < ALLPLAYERS; player++)
	{
		if(PROFILE[player].SETTINGS[CONFIG] == DISABLED) // bypass disabled players
			continue;
		const int camera = EMU_ReadInt(PD_camera, 0);
		const int dead = EMU_ReadInt(playerbase[player], PD_deathflag);
		const int menu = EMU_ReadInt(PD_menu(player), 0);
		const int pause = EMU_ReadInt(PD_pause, 0);
		const int mppause = EMU_ReadInt(PD_mppause, 0);
		const int aimingflag = EMU_ReadInt(playerbase[player], PD_aimingflag);
		const int grabflag = EMU_ReadInt(playerbase[player], PD_grabflag);
		const int thirdperson = EMU_ReadInt(playerbase[player], PD_thirdperson);
		const int cursoraimingflag = PROFILE[player].SETTINGS[PDAIMMODE] && aimingflag ? 1 : 0;
		const float fov = EMU_ReadFloat(playerbase[player], PD_fov);
		const float basefov = fov >= 60.0f ? (float)overridefov : 60.0f;
		const float mouseaccel = PROFILE[player].SETTINGS[ACCELERATION] ? sqrt(DEVICE[player].XPOS * DEVICE[player].XPOS + DEVICE[player].YPOS * DEVICE[player].YPOS) / TICKRATE / 12.0f * PROFILE[player].SETTINGS[ACCELERATION] : 0;
		const float sensitivity = PROFILE[player].SETTINGS[SENSITIVITY] / 40.0f * fmax(mouseaccel, 1);
		const float gunsensitivity = sensitivity * (PROFILE[player].SETTINGS[CROSSHAIR] / 2.5f);
		float camx = EMU_ReadFloat(playerbase[player], PD_camx), camy = EMU_ReadFloat(playerbase[player], PD_camy), bikex = EMU_ReadFloat(bikebase[player][0], 0), bikeroll = EMU_ReadFloat(bikebase[player][0], PD_bikeroll);
		if(camx >= 0 && camx <= 360 && camy >= -90 && camy <= 90 && fov >= 1 && fov <= 120 && dead == 0 && menu == 1 && pause == 0 && mppause < 16777476 && camera == 1 && (grabflag == 0 || grabflag == 4 || grabflag == 3 && bikebase[player][0] && bikex <= BIKEXROTATIONLIMIT && bikex >= 0 && bikeroll <= BIKEROLLLIMIT && bikeroll >= -BIKEROLLLIMIT)) // if safe to inject
		{
			if(thirdperson == 1 || thirdperson == 2) // if player is using the slayer/camspy, translate mouse input to analog stick and continue to next player
			{
				PD_CamspySlayer(player, thirdperson == 2, sensitivity);
				continue;
			}
			PD_AimMode(player, cursoraimingflag, fov / basefov);
			if(grabflag != 3) // if player is on foot
			{
				PD_Crouch(player);
				if(!cursoraimingflag) // if not aiming (or pdaimmode is off)
					camx += DEVICE[player].XPOS / 10.0f * sensitivity * (fov / basefov); // regular mouselook calculation
				else
					camx += aimx[player] * (fov / basefov); // scroll screen with aimx/aimy
				if(camx < 0)
					camx += 360;
				else if(camx >= 360)
					camx -= 360;
				EMU_WriteFloat(playerbase[player], PD_camx, camx);
			}
			else // if player is riding hoverbike (and hoverbike address is valid)
			{
				PD_ResetCrouchToggle(player);
				if(!cursoraimingflag)
				{
					bikex -= DEVICE[player].XPOS / 10.0f * sensitivity / (360 / BIKEXROTATIONLIMIT) * (fov / basefov);
					bikeroll += DEVICE[player].XPOS / 10.0f * sensitivity * (fov / basefov) / 100;
				}
				else
				{
					bikex -= aimx[player] / (360 / BIKEXROTATIONLIMIT) * (fov / basefov);
					bikeroll += aimx[player] * sensitivity * (fov / basefov) / 100;
				}
				if(bikex < 0)
					bikex += BIKEXROTATIONLIMIT;
				else if(bikex >= BIKEXROTATIONLIMIT)
					bikex -= BIKEXROTATIONLIMIT;
				bikeroll = ClampFloat(bikeroll, -BIKEROLLLIMIT, BIKEROLLLIMIT);
				EMU_WriteFloat(bikebase[player][0], 0, bikex);
				EMU_WriteFloat(bikebase[player][0], PD_bikeroll, bikeroll);
			}
			if(!cursoraimingflag)
				camy += (!PROFILE[player].SETTINGS[INVERTPITCH] ? -DEVICE[player].YPOS : DEVICE[player].YPOS) / 10.0f * sensitivity * (fov / basefov);
			else
				camy += -aimy[player] * (fov / basefov);
			camy = ClampFloat(camy, -90, 90);
			EMU_WriteFloat(playerbase[player], PD_camy, camy);
			if(PROFILE[player].SETTINGS[CROSSHAIR] && !cursoraimingflag) // if crosshair movement is enabled and player isn't aiming (don't calculate weapon movement while the player is in aim mode)
			{
				float gunx = EMU_ReadFloat(playerbase[player], PD_gunrx), crosshairx = EMU_ReadFloat(playerbase[player], PD_crosshairx); // after camera x and y have been calculated and injected, calculate the gun/reload/crosshair movement
				gunx += DEVICE[player].XPOS / (!aimingflag ? 10.0f : 40.0f) * gunsensitivity * (fov / basefov) * 0.05f;
				crosshairx += DEVICE[player].XPOS / (!aimingflag ? 10.0f : 40.0f) * gunsensitivity * (fov / 4 / (basefov / 4)) * 0.05f;
				if(aimingflag) // emulate cursor moving back to the center
					gunx /= emuoverclock ? 1.03f : 1.07f, crosshairx /= emuoverclock ? 1.03f : 1.07f;
				gunx = ClampFloat(gunx, -GUNAIMLIMIT, GUNAIMLIMIT);
				crosshairx = ClampFloat(crosshairx, -CROSSHAIRLIMIT, CROSSHAIRLIMIT);
				EMU_WriteFloat(playerbase[player], PD_gunrx, gunx);
				EMU_WriteFloat(playerbase[player], PD_gunrxrecoil, crosshairx * (GUNRECOILXLIMIT / CROSSHAIRLIMIT));
				EMU_WriteFloat(playerbase[player], PD_gunlx, gunx);
				EMU_WriteFloat(playerbase[player], PD_gunlxrecoil, crosshairx * (GUNRECOILXLIMIT / CROSSHAIRLIMIT));
				EMU_WriteFloat(playerbase[player], PD_crosshairx, crosshairx);
				if(camy > -90 && camy < 90) // only allow player's gun to pitch within a valid range
				{
					float guny = EMU_ReadFloat(playerbase[player], PD_gunry), crosshairy = EMU_ReadFloat(playerbase[player], PD_crosshairy);
					guny += (!PROFILE[player].SETTINGS[INVERTPITCH] ? DEVICE[player].YPOS : -DEVICE[player].YPOS) / (!aimingflag ? 10.0f : 40.0f) * gunsensitivity * (fov / basefov) * 0.075f;
					crosshairy += (!PROFILE[player].SETTINGS[INVERTPITCH] ? DEVICE[player].YPOS : -DEVICE[player].YPOS) / (!aimingflag ? 10.0f : 40.0f) * gunsensitivity * (fov / 4 / (basefov / 4)) * 0.1f;
					if(aimingflag)
						guny /= emuoverclock ? 1.15f : 1.35f, crosshairy /= emuoverclock ? 1.15f : 1.35f;
					guny = ClampFloat(guny, -GUNAIMLIMIT, GUNAIMLIMIT);
					crosshairy = ClampFloat(crosshairy, -CROSSHAIRLIMIT, CROSSHAIRLIMIT);
					EMU_WriteFloat(playerbase[player], PD_gunry, guny);
					EMU_WriteFloat(playerbase[player], PD_gunryrecoil, crosshairy * (GUNRECOILYLIMIT / CROSSHAIRLIMIT));
					EMU_WriteFloat(playerbase[player], PD_gunly, guny);
					EMU_WriteFloat(playerbase[player], PD_gunlyrecoil, crosshairy * (GUNRECOILYLIMIT / CROSSHAIRLIMIT));
					EMU_WriteFloat(playerbase[player], PD_crosshairy, crosshairy);
				}
			}
		}
		if(!dead && camera == 1 && !menu) // player is alive, in first person and is in menu
			PD_RadialMenuNav(player); // check if player is in radial menu and translate mouse input to C buttons
		else if(dead || camera != 1) // player is dead or not in first person camera
			PD_ResetCrouchToggle(player); // reset crouch toggle
	}
	PD_Controller(); // set controller data
}
//==========================================================================
// Purpose: crouching function for Perfect Dark (2 = stand, 0 = duck)
// Changes Globals: safetoduck, crouchstance, safetostand
//==========================================================================
static void PD_Crouch(const int player)
{
	int crouchheld = DEVICE[player].BUTTONPRIM[CROUCH] || DEVICE[player].BUTTONSEC[CROUCH];
	if(PROFILE[player].SETTINGS[CROUCHTOGGLE]) // check and change player stance
	{
		if(safetoduck[player] && crouchheld) // standing to ducking
			safetoduck[player] = 0, crouchstance[player] = 1;
		if(!safetoduck[player] && !crouchheld) // crouch is no longer being held, ready to stand
			safetostand[player] = 1;
		if(safetostand[player] && crouchheld) // standing up
			safetoduck[player] = 1, crouchstance[player] = 0;
		if(safetostand[player] && safetoduck[player] && !crouchheld) // crouch key not active, ready to change toggle
			safetostand[player] = 0;
		crouchheld = crouchstance[player];
	}
	if(crouchheld) // user is holding down the crouch button
		EMU_WriteInt(playerbase[player], PD_crouchflag, 0); // set in-game crouch flag to lowest point
	else // player isn't holding down crouch
		EMU_WriteInt(playerbase[player], PD_crouchflag, 2); // force standing flag value (the side effect to this brute force method is it causes a speed glitch while crawling in tunnels)
}
//==========================================================================
// Purpose: replicate the original aiming system, uses aimx/y to move screen when crosshair is on border of screen
// Changes Globals: crosshairposx, crosshairposy, gunrcenter, gunlcenter, aimx, aimy
//==========================================================================
static void PD_AimMode(const int player, const int aimingflag, const float fov)
{
	const float crosshairx = EMU_ReadFloat(playerbase[player], PD_crosshairx), crosshairy = EMU_ReadFloat(playerbase[player], PD_crosshairy);
	const int gunrreload = EMU_ReadInt(playerbase[player], PD_gunrstate) == 1, gunlreload = EMU_ReadInt(playerbase[player], PD_gunlstate) == 1, unarmed = EMU_ReadInt(playerbase[player], PD_currentweapon) < 2;
	const float threshold = 0.72f, speed = 475.f, sensitivity = 100.f, centertime = 60.f;
	if(aimingflag) // if player is aiming
	{
		const float mouseaccel = PROFILE[player].SETTINGS[ACCELERATION] ? sqrt(DEVICE[player].XPOS * DEVICE[player].XPOS + DEVICE[player].YPOS * DEVICE[player].YPOS) / TICKRATE / 12.0f * PROFILE[player].SETTINGS[ACCELERATION] : 0;
		crosshairposx[player] += DEVICE[player].XPOS / 10.0f * (PROFILE[player].SETTINGS[SENSITIVITY] / sensitivity) * fmax(mouseaccel, 1); // calculate the crosshair position
		crosshairposy[player] += (!PROFILE[player].SETTINGS[INVERTPITCH] ? DEVICE[player].YPOS : -DEVICE[player].YPOS) / 10.0f * (PROFILE[player].SETTINGS[SENSITIVITY] / sensitivity) * fmax(mouseaccel, 1);
		crosshairposx[player] = ClampFloat(crosshairposx[player], -CROSSHAIRLIMIT, CROSSHAIRLIMIT); // apply clamp then inject
		crosshairposy[player] = ClampFloat(crosshairposy[player], -CROSSHAIRLIMIT, CROSSHAIRLIMIT);
		EMU_WriteFloat(playerbase[player], PD_crosshairx, crosshairposx[player]);
		EMU_WriteFloat(playerbase[player], PD_crosshairy, crosshairposy[player]);
		if(unarmed || gunrreload) // if unarmed or reloading right weapon, remove from gunrcenter
			gunrcenter[player] -= emuoverclock ? 1 : 2;
		else if(gunrcenter[player] < (int)centertime) // increase gunrcenter over time until it equals centertime
			gunrcenter[player] += emuoverclock ? 1 : 2;
		if(gunlreload) // if reloading left weapon, remove from gunlcenter
			gunlcenter[player] -= emuoverclock ? 1 : 2;
		else if(gunlcenter[player] < (int)centertime)
			gunlcenter[player] += emuoverclock ? 1 : 2;
		if(gunrcenter[player] < 0)
			gunrcenter[player] = 0;
		if(gunlcenter[player] < 0)
			gunlcenter[player] = 0;
		EMU_WriteFloat(playerbase[player], PD_gunrx, (gunrcenter[player] / centertime) * (crosshairposx[player] * 0.75f) + fov - 1); // calculate and inject the gun angles
		EMU_WriteFloat(playerbase[player], PD_gunrxrecoil, crosshairposx[player] * (GUNRECOILXLIMIT / CROSSHAIRLIMIT)); // set the recoil to the correct rotation (if we don't, then the recoil is always z axis aligned)
		EMU_WriteFloat(playerbase[player], PD_gunry, (gunrcenter[player] / centertime) * (crosshairposy[player] * 0.66f) + fov - 1);
		EMU_WriteFloat(playerbase[player], PD_gunryrecoil, crosshairposy[player] * (GUNRECOILYLIMIT / CROSSHAIRLIMIT));
		EMU_WriteFloat(playerbase[player], PD_gunlx, (gunlcenter[player] / centertime) * (crosshairposx[player] * 0.75f) + fov - 1);
		EMU_WriteFloat(playerbase[player], PD_gunlxrecoil, crosshairposx[player] * (GUNRECOILXLIMIT / CROSSHAIRLIMIT));
		EMU_WriteFloat(playerbase[player], PD_gunly, (gunlcenter[player] / centertime) * (crosshairposy[player] * 0.66f) + fov - 1);
		EMU_WriteFloat(playerbase[player], PD_gunlyrecoil, crosshairposy[player] * (GUNRECOILYLIMIT / CROSSHAIRLIMIT));
		if(crosshairx > 0 && crosshairx / CROSSHAIRLIMIT > threshold) // if crosshair is within threshold of the border then calculate a linear scrolling speed and enable mouselook
			aimx[player] = (crosshairx / CROSSHAIRLIMIT - threshold) * speed * TIMESTEP;
		else if(crosshairx < 0 && crosshairx / CROSSHAIRLIMIT < -threshold)
			aimx[player] = (crosshairx / CROSSHAIRLIMIT + threshold) * speed * TIMESTEP;
		else
			aimx[player] = 0;
		if(crosshairy > 0 && crosshairy / CROSSHAIRLIMIT > threshold)
			aimy[player] = (crosshairy / CROSSHAIRLIMIT - threshold) * speed * TIMESTEP;
		else if(crosshairy < 0 && crosshairy / CROSSHAIRLIMIT < -threshold)
			aimy[player] = (crosshairy / CROSSHAIRLIMIT + threshold) * speed * TIMESTEP;
		else
			aimy[player] = 0;
	}
	else // player is not aiming so reset crosshairposxy and gunrlcenter
		crosshairposx[player] = crosshairx, crosshairposy[player] = crosshairy, gunrcenter[player] = 0, gunlcenter[player] = 0;
}
//==========================================================================
// Purpose: translate mouse to analog stick
// Changes Globals: xstick, ystick, usingstick
//==========================================================================
static void PD_CamspySlayer(const int player, const int camspyflag, const float sensitivity)
{
	if(camspyflag)
	{
		if(!DEVICE[player].BUTTONPRIM[AIM] && !DEVICE[player].BUTTONSEC[AIM]) // camspy
		{
			xstick[player] = (int)((!PROFILE[player].SETTINGS[INVERTPITCH] ? -DEVICE[player].YPOS : DEVICE[player].YPOS) * sensitivity * 8.0f);
			ystick[player] = (int)(DEVICE[player].XPOS * sensitivity * 16.0f);
		}
		else // camspy (aiming mode)
		{
			xstick[player] = (int)((!PROFILE[player].SETTINGS[INVERTPITCH] ? DEVICE[player].YPOS : -DEVICE[player].YPOS) * sensitivity * 15.0f);
			ystick[player] = (int)(DEVICE[player].XPOS * sensitivity * 15.0f);
		}
	}
	else // slayer
	{
		xstick[player] = (int)((!PROFILE[player].SETTINGS[INVERTPITCH] ? DEVICE[player].YPOS : -DEVICE[player].YPOS) * sensitivity * 17.0f);
		ystick[player] = (int)(DEVICE[player].XPOS * sensitivity * 17.0f);
	}
	xstick[player] = ClampInt(xstick[player], -127, 127);
	ystick[player] = ClampInt(ystick[player], -127, 127);
	usingstick[player] = 1;
}
//==========================================================================
// Purpose: translate mouse to weapon radial menu
// Changes Globals: xmenu, ymenu, radialmenudirection
//==========================================================================
static void PD_RadialMenuNav(const int player)
{
	const float max = 19, threshold = 13, diagonalthres = 0.75;
	if((DEVICE[player].BUTTONPRIM[ACCEPT] || DEVICE[player].BUTTONSEC[ACCEPT]) && !DEVICE[player].BUTTONPRIM[FIRE] && !DEVICE[player].BUTTONSEC[FIRE]) // if a button is held (reject if fire is pressed so aimx/y can be reset back to center)
	{
		xmenu[player] += DEVICE[player].XPOS / 10.0f * PROFILE[player].SETTINGS[SENSITIVITY] / 40.0f;
		ymenu[player] += DEVICE[player].YPOS / 10.0f * PROFILE[player].SETTINGS[SENSITIVITY] / 40.0f;
		xmenu[player] = ClampFloat(xmenu[player], -max, max);
		ymenu[player] = ClampFloat(ymenu[player], -max, max);
		if(ymenu[player] < -threshold) // c-up
			radialmenudirection[player][FORWARDS] = 1;
		else if(ymenu[player] > threshold) // c-down
			radialmenudirection[player][BACKWARDS] = 1;
		if(xmenu[player] < -threshold) // c-left
			radialmenudirection[player][STRAFELEFT] = 1;
		else if(xmenu[player] > threshold) // c-right
			radialmenudirection[player][STRAFERIGHT] = 1;
		if(xmenu[player] < -threshold && ymenu[player] < -threshold && (-xmenu[player] + -ymenu[player]) / (max * 2) > diagonalthres) // c-up-left
			radialmenudirection[player][FORWARDS] = 1, radialmenudirection[player][STRAFELEFT] = 1;
		else if(xmenu[player] > threshold && ymenu[player] < -threshold && (xmenu[player] + -ymenu[player]) / (max * 2) > diagonalthres) // c-up-right
			radialmenudirection[player][FORWARDS] = 1, radialmenudirection[player][STRAFERIGHT] = 1;
		else if(xmenu[player] < -threshold && ymenu[player] > threshold && (-xmenu[player] + ymenu[player]) / (max * 2) > diagonalthres) // c-down-left
			radialmenudirection[player][BACKWARDS] = 1, radialmenudirection[player][STRAFELEFT] = 1;
		else if(xmenu[player] > threshold && ymenu[player] > threshold && (xmenu[player] + ymenu[player]) / (max * 2) > diagonalthres) // c-down-right
			radialmenudirection[player][BACKWARDS] = 1, radialmenudirection[player][STRAFERIGHT] = 1;
	}
	else
		xmenu[player] = 0, ymenu[player] = 0;
}
//==========================================================================
// Purpose: calculate and send emulator key combo
// Changes Globals: xstick, ystick, usingstick, radialmenudirection
//==========================================================================
static void PD_Controller(void)
{
	for(int player = PLAYER1; player < ALLPLAYERS; player++)
	{
		CONTROLLER[player].U_CBUTTON = DEVICE[player].BUTTONPRIM[FORWARDS] || DEVICE[player].BUTTONSEC[FORWARDS] || radialmenudirection[player][FORWARDS];
		CONTROLLER[player].D_CBUTTON = DEVICE[player].BUTTONPRIM[BACKWARDS] || DEVICE[player].BUTTONSEC[BACKWARDS] || radialmenudirection[player][BACKWARDS];
		CONTROLLER[player].L_CBUTTON = DEVICE[player].BUTTONPRIM[STRAFELEFT] || DEVICE[player].BUTTONSEC[STRAFELEFT] || radialmenudirection[player][STRAFELEFT];
		CONTROLLER[player].R_CBUTTON = DEVICE[player].BUTTONPRIM[STRAFERIGHT] || DEVICE[player].BUTTONSEC[STRAFERIGHT] || radialmenudirection[player][STRAFERIGHT];
		CONTROLLER[player].Z_TRIG = DEVICE[player].BUTTONPRIM[FIRE] || DEVICE[player].BUTTONSEC[FIRE] || DEVICE[player].BUTTONPRIM[PREVIOUSWEAPON] || DEVICE[player].BUTTONSEC[PREVIOUSWEAPON];
		CONTROLLER[player].R_TRIG = DEVICE[player].BUTTONPRIM[AIM] || DEVICE[player].BUTTONSEC[AIM];
		CONTROLLER[player].A_BUTTON = DEVICE[player].BUTTONPRIM[ACCEPT] || DEVICE[player].BUTTONSEC[ACCEPT] || DEVICE[player].BUTTONPRIM[PREVIOUSWEAPON] || DEVICE[player].BUTTONSEC[PREVIOUSWEAPON] || DEVICE[player].BUTTONPRIM[NEXTWEAPON] || DEVICE[player].BUTTONSEC[NEXTWEAPON];
		CONTROLLER[player].B_BUTTON = DEVICE[player].BUTTONPRIM[CANCEL] || DEVICE[player].BUTTONSEC[CANCEL];
		CONTROLLER[player].START_BUTTON = DEVICE[player].BUTTONPRIM[START] || DEVICE[player].BUTTONSEC[START];
		if(!usingstick[player]) // player is not using the camspy/slayer
		{
			DEVICE[player].ARROW[0] = (DEVICE[player].BUTTONPRIM[UP] || DEVICE[player].BUTTONSEC[UP]) ? 127 : 0;
			DEVICE[player].ARROW[1] = (DEVICE[player].BUTTONPRIM[DOWN] || DEVICE[player].BUTTONSEC[DOWN]) ? -127 : 0;
			DEVICE[player].ARROW[2] = (DEVICE[player].BUTTONPRIM[LEFT] || DEVICE[player].BUTTONSEC[LEFT]) ? -127 : 0;
			DEVICE[player].ARROW[3] = (DEVICE[player].BUTTONPRIM[RIGHT] || DEVICE[player].BUTTONSEC[RIGHT]) ? 127 : 0;
			CONTROLLER[player].X_AXIS = DEVICE[player].ARROW[0] + DEVICE[player].ARROW[1];
			CONTROLLER[player].Y_AXIS = DEVICE[player].ARROW[2] + DEVICE[player].ARROW[3];
		}
		else // player is using camspy/slayer
		{
			CONTROLLER[player].X_AXIS = xstick[player];
			CONTROLLER[player].Y_AXIS = ystick[player];
		}
		PD_ResetXYStick(player); // reset x/y stick
		PD_ResetRadialMenuBtns(player); // reset radialmenudirection
	}
}
//==========================================================================
// Purpose: inject hacks into rom before code has been cached
//==========================================================================
static void PD_InjectHacks(void)
{
	const int addressarray[33] = {0x802C07B8, 0x802C07BC, 0x802C07EC, 0x802C07F0, 0x802C07FC, 0x802C0800, 0x802C0808, 0x802C0820, 0x802C0824, 0x802C082C, 0x802C0830, 0x803C7968, 0x803C796C, 0x803C7970, 0x803C7974, 0x803C7978, 0x803C797C, 0x803C7980, 0x803C7984, 0x803C7988, 0x803C798C, 0x803C7990, 0x803C7994, 0x803C7998, 0x803C799C, 0x803C79A0, 0x803C79A4, 0x803C79A8, 0x803C79AC, 0x803C79B0, 0x803C79B4, 0x803C79B8, 0x803C79BC}, codearray[33] = {0x0BC69E5A, 0x8EA10120, 0x0BC69E5F, 0x263107A4, 0x0BC69E63, 0x4614C500, 0x46120682, 0x0BC69E67, 0x26100004, 0x0BC69E6B, 0x4614C500, 0x54200003, 0x00000000, 0xE6B21668, 0xE6A8166C, 0x0BC281F0, 0x8EA10120, 0x50200001, 0xE6380530, 0x0BC281FD, 0x8EA10120, 0x50200001, 0xE6340534, 0x0BC28201, 0x8EA10120, 0x50200001, 0xE6380530, 0x0BC2820A, 0x8EA10120, 0x50200001, 0xE6340534, 0x0BC2820D, 0x00000000}; // add branch to crosshair code so cursor aiming mode is absolute (without jitter)
	for(int index = 0; index < 33; index++) // inject code array
		EMU_WriteInt(addressarray[index], 0, codearray[index]);
	if(overridefov != 60) // override default fov
	{
		float newfov = overridefov;
		unsigned int unsignedinteger = *(unsigned int *)(float *)(&newfov);
		EMU_WriteInt(PD_defaultfov, 0, 0x3C010000 + (short)(unsignedinteger / 0x10000));
		EMU_WriteInt(PD_defaultfovzoom, 0, 0x3C010000 + (short)(unsignedinteger / 0x10000));
	}
	if(CONTROLLER[PLAYER1].Z_TRIG && CONTROLLER[PLAYER1].R_TRIG) // skip intros if holding down fire + aim
		EMU_WriteInt(PD_introcounter, 0, 0x00001000);
}
//==========================================================================
// Purpose: run when emulator closes rom
// Changes Globals: playerbase, bikebase, safetoduck, safetostand, crouchstance, xmenu, ymenu
//==========================================================================
static void PD_Quit(void)
{
	for(int player = PLAYER1; player < ALLPLAYERS; player++)
	{
		playerbase[player] = 0;
		bikebase[player][0] = 0;
		bikebase[player][1] = 0;
		PD_ResetCrouchToggle(player);
		xmenu[player] = 0, ymenu[player] = 0;
	}
}