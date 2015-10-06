/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2013-2014 Intel Corporation. All Rights Reserved.

*******************************************************************************/
#pragma once
#include <Windows.h>
#include <vector>
#include "pxcspeechrecognition.h"
#include "pxcsession.h"

extern PXCSession  *g_session;
/* Grammar & Vocabulary File Names */
extern pxcCHAR g_file[1024];
extern pxcCHAR v_file[1024];

pxcCHAR *LanguageToString(PXCSpeechRecognition::LanguageType);
pxcCHAR *AlertToString(PXCSpeechRecognition::AlertType);
pxcCHAR *NewCommand(void);
void PrintStatus(HWND, pxcCHAR *);
void PrintConsole(HWND, pxcCHAR *);
void ClearScores(HWND);
void SetScore(HWND, int, pxcI32);
bool IsCommandControl(HWND);
std::vector<std::wstring> GetCommands(HWND);
PXCAudioSource::DeviceInfo GetAudioSource(HWND);
pxcUID GetModule(HWND);
int  GetLanguage(HWND);
void FillCommandListConsole(HWND, pxcCHAR *);
