/*
 * Copyright (C) 2017 FIX94
 *
 * This software may be modified and distributed under the terms
 * of the MIT license.  See the LICENSE file for details.
 */
#include <string.h>
#include <stdarg.h>
#include <stdlib.h>
#include <malloc.h>
#include <stdio.h>
#include <vector>
#include <string>
#include "dynamic_libs/os_functions.h"
#include "dynamic_libs/sys_functions.h"
#include "dynamic_libs/padscore_functions.h"
#include "dynamic_libs/vpad_functions.h"
#include "system/memory.h"
#include "utils/utils.h"
#include "common/common.h"

static const char *verChar = "WiiU Video Mode Changer v1.0 by FIX94";

static unsigned int getButtonsDown();

#define OSScreenEnable(enable) OSScreenEnableEx(0, enable); OSScreenEnableEx(1, enable);
#define OSScreenClearBuffer(tmp) OSScreenClearBufferEx(0, tmp); OSScreenClearBufferEx(1, tmp);
#define OSScreenPutFont(x, y, buf) OSScreenPutFontEx(0, x, y, buf); OSScreenPutFontEx(1, x, y, buf);
#define OSScreenFlipBuffers() OSScreenFlipBuffersEx(0); OSScreenFlipBuffersEx(1);

const char *outPortStr[] = { "HDMI", "Component", "Composite/S-Video", "Composite/SCART" };
const char *outResStr[] = { "720p (fallback?)", "576i PAL50", "480i", "480p", "720p", "720p 3D?", "1080i", "1080p", "720p (again?)", "720p (No Signal from Component?)", "480i PAL60", "720p (yet again?)", "720p (50Hz, glitchy gamepad)", "1080i (50Hz, glitchy gamepad)", "1080p (50Hz, glitchy gamepad)" };

extern "C" int Menu_Main(void)
{
	InitOSFunctionPointers();
	InitSysFunctionPointers();
	InitPadScoreFunctionPointers();
	InitVPadFunctionPointers();

	int screen_buf0_size, screen_buf1_size;
	uint8_t *screenBuffer;
	int redraw = 1;

	unsigned int avm_handle;
	OSDynLoad_Acquire("avm.rpl", &avm_handle);

	int (*AVMSetTVVideoRegion)(int r3, int r4, int r5);
	int (*AVMSetTVOutPort)(int r3, int r4);
	int (*AVMSetTVScanResolution)(int r3);
	int (*AVMDebugIsNTSC)();
	int (*AVMGetCurrentPort)(int *port);

	OSDynLoad_FindExport(avm_handle, 0, "AVMSetTVVideoRegion", &AVMSetTVVideoRegion);
	OSDynLoad_FindExport(avm_handle, 0, "AVMSetTVOutPort", &AVMSetTVOutPort);
	OSDynLoad_FindExport(avm_handle, 0, "AVMSetTVScanResolution", &AVMSetTVScanResolution);
	OSDynLoad_FindExport(avm_handle, 0, "AVMDebugIsNTSC", &AVMDebugIsNTSC);
	OSDynLoad_FindExport(avm_handle, 0, "AVMGetCurrentPort", &AVMGetCurrentPort);

	bool isNTSC = !!AVMDebugIsNTSC();
	bool wantNTSC = isNTSC;

	int outPort = 0;
	AVMGetCurrentPort(&outPort);
	if(outPort > 3) outPort = 0;
	int wantPort = outPort;

	//int outRes; //still need to get resolution somehow...
	int wantRes = 2; //default 480i
	if(outPort == 0) wantRes = 4; //720p from HDMI
	else if(outPort == 1) wantRes = 3; //480p from Component
	else if(outPort == 3) wantRes = 10; //480i PAL60 from Composite/SCART

	memoryInitialize();

	OSScreenInit();
	screen_buf0_size = OSScreenGetBufferSizeEx(0);
	screen_buf1_size = OSScreenGetBufferSizeEx(1);
	screenBuffer = (uint8_t*)MEMBucket_alloc(screen_buf0_size+screen_buf1_size, 0x100);
	OSScreenSetBufferEx(0, (void*)(screenBuffer));
	OSScreenSetBufferEx(1, (void*)(screenBuffer + screen_buf0_size));
	OSScreenEnable(1);

	KPADInit();
	WPADEnableURCC(1);

	//garbage read
	getButtonsDown();

	int curSel = 0;
	bool applyChanges = false;
	while(1)
	{
		usleep(25000);
		unsigned int btnDown = getButtonsDown();

		if( btnDown & VPAD_BUTTON_HOME )
			break;

		if( btnDown & VPAD_BUTTON_RIGHT )
		{
			if(curSel == 0) //NTSC/PAL
			{
				wantNTSC = !wantNTSC;
				if(wantNTSC)
				{
					//no Composite/SCART
					if(wantPort == 3)
					{
						wantPort = 2; //Composite/S-Video
						wantRes = 2; //480i
					}
				}
				else //want PAL
				{
					//no Composite/S-Video
					if(wantPort == 2)
					{
						wantPort = 3; //Composite/SCART
						wantRes = 10; //480i PAL60
					}
				}
			}
			else if(curSel == 1) //Port
			{
				wantPort++;
				if(wantPort > 3)
					wantPort = 0;
				if(wantNTSC)
				{
					//no Composite/SCART
					if(wantPort == 3)
						wantPort = 0;
				}
				else //want PAL
				{
					//no Composite/S-Video
					if(wantPort == 2)
						wantPort = 3;
				}
				//Set default res for port
				if(wantPort == 0) wantRes = 4; //720p from HDMI
				else if(wantPort == 1) wantRes = 3; //480p from Component
				else if(wantPort == 2) wantRes = 2; //480i from Composite/S-Video
				else if(wantPort == 3) wantRes = 10; //480i PAL60 from Composite/SCART
			}
			else if(curSel == 2) //Resolution
			{
				wantRes++;
				if(wantRes > 14)
					wantRes = 1;
			}
			redraw = 1;
		}

		if( btnDown & VPAD_BUTTON_LEFT )
		{
			if(curSel == 0) //NTSC/PAL
			{
				wantNTSC = !wantNTSC;
				if(wantNTSC)
				{
					//no Composite/SCART
					if(wantPort == 3)
					{
						wantPort = 2; //Composite/S-Video
						wantRes = 2; //480i
					}
				}
				else //want PAL
				{
					//no Composite/S-Video
					if(wantPort == 2)
					{
						wantPort = 3; //Composite/SCART
						wantRes = 10; //480i PAL60
					}
				}
			}
			else if(curSel == 1) //Port
			{
				wantPort--;
				if(wantPort < 0)
					wantPort = 3;
				if(wantNTSC)
				{
					//no Composite/SCART
					if(wantPort == 3)
						wantPort = 2;
				}
				else //want PAL
				{
					//no Composite/S-Video
					if(wantPort == 2)
						wantPort = 1;
				}
				//Set default res for port
				if(wantPort == 0) wantRes = 4; //720p from HDMI
				else if(wantPort == 1) wantRes = 3; //480p from Component
				else if(wantPort == 2) wantRes = 2; //480i from Composite/S-Video
				else if(wantPort == 3) wantRes = 10; //480i PAL60 from Composite/SCART
			}
			else if(curSel == 2) //Resolution
			{
				wantRes--;
				if(wantRes < 0)
					wantRes = 14;
			}
			redraw = 1;
		}

		if( btnDown & VPAD_BUTTON_DOWN )
		{
			curSel++;
			if(curSel > 2) curSel = 0;
			redraw = 1;
		}

		if( btnDown & VPAD_BUTTON_UP )
		{
			curSel--;
			if(curSel < 0) curSel = 2;
			redraw = 1;
		}

		if( btnDown & VPAD_BUTTON_A )
		{
			applyChanges = true;
			break;
		}

		if( redraw )
		{
			OSScreenClearBuffer(0);
			OSScreenPutFont(0, 0, verChar);
			char printStr[256];
			sprintf(printStr,"%s Video Region: %s", (curSel==0)?">":" ", wantNTSC ? "NTSC" : "PAL");
			OSScreenPutFont(0, 1, printStr);
			sprintf(printStr,"%s Output Port: %s", (curSel==1)?">":" ", outPortStr[wantPort]);
			OSScreenPutFont(0, 2, printStr);
			sprintf(printStr,"%s Output Resolution: %s", (curSel==2)?">":" ", outResStr[wantRes]);
			OSScreenPutFont(0, 3, printStr);
			OSScreenPutFont(0, 4, "Press A to apply settings.");
			OSScreenPutFont(0, 5, "Press Home to exit without changes.");
			OSScreenFlipBuffers();
			redraw = 0;
		}
	}

	OSScreenClearBuffer(0);
	OSScreenFlipBuffers();

	OSScreenEnable(0);
	MEMBucket_free(screenBuffer);

	memoryRelease();

	if(applyChanges)
	{
		if((isNTSC && !wantNTSC) || (!isNTSC && wantNTSC))
			AVMSetTVVideoRegion(wantNTSC ? 1 : 2, wantPort, wantRes);
		else if(outPort != wantPort)
			AVMSetTVOutPort(wantPort, wantRes);
		else //only set resolution
			AVMSetTVScanResolution(wantRes);
	}
	return EXIT_SUCCESS;
}

/* General Input Code */

static unsigned int wpadToVpad(unsigned int buttons)
{
	unsigned int conv_buttons = 0;

	if(buttons & WPAD_BUTTON_LEFT)
		conv_buttons |= VPAD_BUTTON_LEFT;

	if(buttons & WPAD_BUTTON_RIGHT)
		conv_buttons |= VPAD_BUTTON_RIGHT;

	if(buttons & WPAD_BUTTON_DOWN)
		conv_buttons |= VPAD_BUTTON_DOWN;

	if(buttons & WPAD_BUTTON_UP)
		conv_buttons |= VPAD_BUTTON_UP;

	if(buttons & WPAD_BUTTON_PLUS)
		conv_buttons |= VPAD_BUTTON_PLUS;

	if(buttons & WPAD_BUTTON_2)
		conv_buttons |= VPAD_BUTTON_X;

	if(buttons & WPAD_BUTTON_1)
		conv_buttons |= VPAD_BUTTON_Y;

	if(buttons & WPAD_BUTTON_B)
		conv_buttons |= VPAD_BUTTON_B;

	if(buttons & WPAD_BUTTON_A)
		conv_buttons |= VPAD_BUTTON_A;

	if(buttons & WPAD_BUTTON_MINUS)
		conv_buttons |= VPAD_BUTTON_MINUS;

	if(buttons & WPAD_BUTTON_HOME)
		conv_buttons |= VPAD_BUTTON_HOME;

	return conv_buttons;
}

static unsigned int wpadClassicToVpad(unsigned int buttons)
{
	unsigned int conv_buttons = 0;

	if(buttons & WPAD_CLASSIC_BUTTON_LEFT)
		conv_buttons |= VPAD_BUTTON_LEFT;

	if(buttons & WPAD_CLASSIC_BUTTON_RIGHT)
		conv_buttons |= VPAD_BUTTON_RIGHT;

	if(buttons & WPAD_CLASSIC_BUTTON_DOWN)
		conv_buttons |= VPAD_BUTTON_DOWN;

	if(buttons & WPAD_CLASSIC_BUTTON_UP)
		conv_buttons |= VPAD_BUTTON_UP;

	if(buttons & WPAD_CLASSIC_BUTTON_PLUS)
		conv_buttons |= VPAD_BUTTON_PLUS;

	if(buttons & WPAD_CLASSIC_BUTTON_X)
		conv_buttons |= VPAD_BUTTON_X;

	if(buttons & WPAD_CLASSIC_BUTTON_Y)
		conv_buttons |= VPAD_BUTTON_Y;

	if(buttons & WPAD_CLASSIC_BUTTON_B)
		conv_buttons |= VPAD_BUTTON_B;

	if(buttons & WPAD_CLASSIC_BUTTON_A)
		conv_buttons |= VPAD_BUTTON_A;

	if(buttons & WPAD_CLASSIC_BUTTON_MINUS)
		conv_buttons |= VPAD_BUTTON_MINUS;

	if(buttons & WPAD_CLASSIC_BUTTON_HOME)
		conv_buttons |= VPAD_BUTTON_HOME;

	if(buttons & WPAD_CLASSIC_BUTTON_ZR)
		conv_buttons |= VPAD_BUTTON_ZR;

	if(buttons & WPAD_CLASSIC_BUTTON_ZL)
		conv_buttons |= VPAD_BUTTON_ZL;

	if(buttons & WPAD_CLASSIC_BUTTON_R)
		conv_buttons |= VPAD_BUTTON_R;

	if(buttons & WPAD_CLASSIC_BUTTON_L)
		conv_buttons |= VPAD_BUTTON_L;

	return conv_buttons;
}

static unsigned int getButtonsDown()
{
	unsigned int btnDown = 0;

	s32 vpadError = -1;
	VPADData vpad;
	VPADRead(0, &vpad, 1, &vpadError);
	if(vpadError == 0)
		btnDown |= vpad.btns_d;

	int i;
	for(i = 0; i < 4; i++)
	{
		u32 controller_type;
		if(WPADProbe(i, &controller_type) != 0)
			continue;
		KPADData kpadData;
		KPADRead(i, &kpadData, 1);
		if(kpadData.device_type <= 1)
			btnDown |= wpadToVpad(kpadData.btns_d);
		else
			btnDown |= wpadClassicToVpad(kpadData.classic.btns_d);
	}

	return btnDown;
}
