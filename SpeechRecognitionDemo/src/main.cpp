/*******************************************************************************

INTEL CORPORATION PROPRIETARY INFORMATION
This software is supplied under the terms of a license agreement or nondisclosure
agreement with Intel Corporation and may not be copied or disclosed except in
accordance with the terms of that agreement
Copyright(c) 2014 Intel Corporation. All Rights Reserved.

*******************************************************************************/
#include <Windows.h>
#include <WindowsX.h>
#include <commctrl.h>
#include <map>
#include <vector>
#include "resource.h"
#include "main.h"
#include "pxcmetadata.h"
#include "service/pxcsessionservice.h"
#include "speech_recognition.h"

#define ID_MODULEX   21000
#define ID_SOURCEX	 22000
#define ID_LANGUAGEX 23000

HINSTANCE    g_hInst = 0;
PXCSession  *g_session = 0;
HWND		 g_hwndEdit = 0;
WNDPROC		 g_oldProc = 0;
std::map<int, PXCAudioSource::DeviceInfo> g_devices;
std::map<int, pxcUID> g_modules;
/* Grammar & Vocabulary File Names */
pxcCHAR g_file[1024] = { 0 };
pxcCHAR v_file[1024] = { 0 };

pxcCHAR *LanguageToString(PXCSpeechRecognition::LanguageType language) {
    switch (language) {
    case PXCSpeechRecognition::LANGUAGE_US_ENGLISH:		return L"US English";
    case PXCSpeechRecognition::LANGUAGE_GB_ENGLISH:		return L"British English";
    case PXCSpeechRecognition::LANGUAGE_DE_GERMAN:		return L"Deutsch";
    case PXCSpeechRecognition::LANGUAGE_IT_ITALIAN:		return L"Italiano";
    case PXCSpeechRecognition::LANGUAGE_BR_PORTUGUESE:	return L"BR Português";
    case PXCSpeechRecognition::LANGUAGE_CN_CHINESE:		return L"中文";
    case PXCSpeechRecognition::LANGUAGE_FR_FRENCH:		return L"Français";
    case PXCSpeechRecognition::LANGUAGE_JP_JAPANESE:	return L"日本語";
    case PXCSpeechRecognition::LANGUAGE_US_SPANISH:		return L"US Español";
    case PXCSpeechRecognition::LANGUAGE_LA_SPANISH:		return L"LA Español";
    }
    return 0;
}

pxcCHAR *AlertToString(PXCSpeechRecognition::AlertType label) {
    switch (label) {
    case PXCSpeechRecognition::ALERT_SNR_LOW: return L"SNR_LOW";
    case PXCSpeechRecognition::ALERT_SPEECH_UNRECOGNIZABLE: return L"SPEECH_UNRECOGNIZABLE";
    case PXCSpeechRecognition::ALERT_VOLUME_HIGH: return L"VOLUME_HIGH";
    case PXCSpeechRecognition::ALERT_VOLUME_LOW: return L"VOLUME_LOW";
    case PXCSpeechRecognition::ALERT_SPEECH_BEGIN: return L"SPEECH_BEGIN";
    case PXCSpeechRecognition::ALERT_SPEECH_END: return L"SPEECH_END";
    case PXCSpeechRecognition::ALERT_RECOGNITION_ABORTED: return L"RECOGNITION_ABORTED";
    case PXCSpeechRecognition::ALERT_RECOGNITION_END: return L"RECOGNITION_END";
    }
    return L"Unknown";
}

pxcCHAR *NewCommand(void) {
    return L"[Enter New Command]";
}

void PrintStatus(HWND hWnd, pxcCHAR *string) {
    HWND hTreeView = GetDlgItem(hWnd, IDC_STATUS);
    TVINSERTSTRUCT tvis;
    memset(&tvis, 0, sizeof(tvis));
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    tvis.item.pszText = string;
    tvis.item.cchTextMax = (int)wcsnlen_s(string, 1024);
    TreeView_EnsureVisible(hTreeView, TreeView_InsertItem(hTreeView, &tvis));
}

void PrintConsole(HWND hWnd, pxcCHAR *string) {
    HWND hTreeView = GetDlgItem(hWnd, IDC_CONSOLE2);
    TVINSERTSTRUCT tvis;
    memset(&tvis, 0, sizeof(tvis));
    tvis.hInsertAfter = TVI_LAST;
    tvis.item.mask = TVIF_TEXT;
    tvis.item.pszText = string;
    tvis.item.cchTextMax = (int)wcsnlen_s(string, 1024);
    TreeView_EnsureVisible(hTreeView, TreeView_InsertItem(hTreeView, &tvis));
}

static int GetCheckedItem(HMENU menu) {
    for (int i = 0; i < GetMenuItemCount(menu); i++)
        if (GetMenuState(menu, i, MF_BYPOSITION)&MF_CHECKED) return i;
    return 0;
}

static void PopulateSource(HMENU menu) {
    DeleteMenu(menu, 0, MF_BYPOSITION);
    HMENU menu1 = CreatePopupMenu();

    g_devices.clear();
    PXCAudioSource *source = g_session->CreateAudioSource();
    if (source) {
        source->ScanDevices();

        for (int i = 0;; i++) {
            PXCAudioSource::DeviceInfo dinfo = {};
            if (source->QueryDeviceInfo(i, &dinfo) < PXC_STATUS_NO_ERROR) break;
            AppendMenu(menu1, MF_STRING, ID_SOURCEX + i, dinfo.name);
            g_devices[i] = dinfo;
        }

        source->Release();
    }

    CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), 0, MF_BYPOSITION);
    InsertMenu(menu, 0, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Source");
}

static void PopulateModules(HMENU menu) {
    DeleteMenu(menu, 1, MF_BYPOSITION);

    g_modules.clear();
    PXCSession::ImplDesc desc = {}, desc1;
    desc.cuids[0] = PXCSpeechRecognition::CUID;
    HMENU menu1 = CreatePopupMenu();
    int i;
    for (i = 0;; i++) {
        if (g_session->QueryImpl(&desc, i, &desc1) < PXC_STATUS_NO_ERROR) break;
        AppendMenu(menu1, MF_STRING, ID_MODULEX + i, desc1.friendlyName);
        g_modules[i] = desc1.iuid;
    }
    CheckMenuRadioItem(menu1, 0, i, 0, MF_BYPOSITION);
    InsertMenu(menu, 1, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Module");
}

static void PopulateLanguage(HMENU menu) {
    DeleteMenu(menu, 2, MF_BYPOSITION);

    HMENU menu1 = CreatePopupMenu();
    PXCSession::ImplDesc desc, desc1;
    memset(&desc, 0, sizeof(desc));
    desc.cuids[0] = PXCSpeechRecognition::CUID;
    unsigned int checkedItem = 0;
    if (g_session->QueryImpl(&desc, GetCheckedItem(GetSubMenu(menu, 1)) /*ID_MODULE*/, &desc1) >= PXC_STATUS_NO_ERROR) {
        PXCSpeechRecognition *vrec;
        if (g_session->CreateImpl<PXCSpeechRecognition>(&desc1, &vrec) >= PXC_STATUS_NO_ERROR) {
            for (int k = 0;; k++) {
                PXCSpeechRecognition::ProfileInfo pinfo;
                if (vrec->QueryProfile(k, &pinfo) < PXC_STATUS_NO_ERROR) break;
                AppendMenu(menu1, MF_STRING, ID_LANGUAGEX + k, LanguageToString(pinfo.language));
                if (pinfo.language == PXCSpeechRecognition::LANGUAGE_US_ENGLISH) checkedItem = k;
            }
            vrec->Release();
        }
    }
    CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), checkedItem, MF_BYPOSITION);
    InsertMenu(menu, 2, MF_BYPOSITION | MF_POPUP, (UINT_PTR)menu1, L"Language");
}

static void PromptNewCommand(HWND hWnd) {
    TVINSERTSTRUCT tvis;
    tvis.item.mask = TVIF_TEXT;
    pxcCHAR line[256] = { 0 };
    tvis.item.pszText = line;
    tvis.item.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
    for (HTREEITEM item = TreeView_GetRoot(hWnd); item; item = TreeView_GetNextSibling(hWnd, item)) {
        tvis.item.hItem = item;
        TreeView_GetItem(hWnd, &tvis.item);
        if (!wcscmp(tvis.item.pszText, NewCommand())) return;
    }
    wcscpy_s<sizeof(line) / sizeof(pxcCHAR)>(line, NewCommand());
    tvis.hParent = 0;
    tvis.hInsertAfter = TVI_LAST;
    TreeView_InsertItem(hWnd, &tvis);
}

static void TrimSpace(pxcCHAR *line, int nchars) {
    while (line[0] == L' ') wcscpy_s(line, nchars, line + 1);
    for (int i = 0; i < nchars; i++) {
        if (line[i]) continue;
        for (int j = i - 1; j >= 0; j--) {
            if (line[j] != L' ') break;
            line[j] = 0;
        }
        break;
    }
}

static void TrimScore(pxcCHAR *line, int nchars) {
    pxcCHAR *p = wcschr(line, L'[');
    if (p) *p = 0;
    TrimSpace(line, nchars);
}

void ClearScores(HWND hWnd) {
    HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
    TVINSERTSTRUCT tvis;
    tvis.item.mask = TVIF_TEXT;
    pxcCHAR line[256] = { 0 };
    tvis.item.pszText = line;
    tvis.item.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
    for (HTREEITEM item = TreeView_GetRoot(hwndConsole); item; item = TreeView_GetNextSibling(hwndConsole, item)) {
        tvis.item.hItem = item;
        TreeView_GetItem(hwndConsole, &tvis.item);
        TrimScore(line, tvis.item.cchTextMax);
        if (line[0]) TreeView_SetItem(hwndConsole, &tvis.item);
    }
}

void SetScore(HWND hWnd, int label, pxcI32 confidence) {
    HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
    TVITEM tvi;
    tvi.mask = TVIF_TEXT;
    pxcCHAR line[256] = { 0 };
    tvi.pszText = line;
    tvi.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
    for (HTREEITEM item = TreeView_GetRoot(hwndConsole); item; item = TreeView_GetNextSibling(hwndConsole, item)) {
        tvi.hItem = item;
        TreeView_GetItem(hwndConsole, &tvi);
        TrimScore(line, tvi.cchTextMax);
        if (!line[0]) continue;
        if (label--) continue;
        swprintf_s(line, tvi.cchTextMax, L"%s [%d%%]", line, confidence);
        TreeView_SetItem(hwndConsole, &tvi);
        break;
    }
}

bool IsCommandControl(HWND hWnd) {
    return (GetMenuState(GetMenu(hWnd), ID_CMDCTRL, MF_BYCOMMAND)&MF_CHECKED) != 0;
}

std::vector<std::wstring> GetCommands(HWND hWnd) {
    HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
    TVITEM tvi;
    tvi.mask = TVIF_TEXT;
    pxcCHAR line[256];
    tvi.pszText = line;
    tvi.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
    std::vector<std::wstring> cmds;
    for (HTREEITEM item = TreeView_GetRoot(hwndConsole); item; item = TreeView_GetNextSibling(hwndConsole, item)) {
        tvi.hItem = item;
        TreeView_GetItem(hwndConsole, &tvi);
        TrimScore(line, tvi.cchTextMax);
        if (line[0]) cmds.push_back(std::wstring(line));
    }
    return cmds;
}

PXCAudioSource::DeviceInfo GetAudioSource(HWND hWnd) {
    HMENU menu1 = GetSubMenu(GetMenu(hWnd), 0);	// ID_SOURCE
    int item = GetCheckedItem(menu1);
    return g_devices[item];
}

pxcUID GetModule(HWND hWnd) {
    HMENU menu1 = GetSubMenu(GetMenu(hWnd), 1);	// ID_MODULE
    int item = GetCheckedItem(menu1);
    return g_modules[item];
}

int  GetLanguage(HWND hWnd) {
    HMENU menu1 = GetSubMenu(GetMenu(hWnd), 2);	// ID_LANGUAGE
    int item = GetCheckedItem(menu1);
    return item;
}

void FillCommandListConsole(HWND hWnd, pxcCHAR *filename) {
    FILE *gfile;
    _wfopen_s(&gfile, filename, L"rb");
    if (!gfile)
        return;

    TreeView_DeleteAllItems(GetDlgItem(hWnd, IDC_STATUS));
    TreeView_DeleteAllItems(GetDlgItem(hWnd, IDC_CONSOLE2));

    HWND hwndConsole = GetDlgItem(hWnd, IDC_CONSOLE2);
    PromptNewCommand(hwndConsole);

    const size_t maxLength = 1024;
    char line[maxLength];
    memset(line, 0, maxLength);
    pxcCHAR wline[maxLength];
    memset(wline, 0, maxLength);
    for (int linenum = 0; !feof(gfile); linenum++)
    {
        if (!fgets(line, maxLength, gfile))
            break;
        size_t lineSize = strnlen_s(line, maxLength);
        while (isspace(line[lineSize - 1]))
            line[--lineSize] = '\0';
        MultiByteToWideChar(CP_UTF8, 0, line, (lineSize + 1) * sizeof(char), wline, maxLength);

        TVINSERTSTRUCT tvis;
        tvis.item.mask = TVIF_TEXT;
        tvis.item.pszText = wline;
        tvis.item.cchTextMax = sizeof(wline) / sizeof(pxcCHAR);
        tvis.hParent = 0;
        tvis.hInsertAfter = TVI_LAST;
        TreeView_InsertItem(hwndConsole, &tvis);
    }
    fclose(gfile);
}

static void GetGrammarFile(void) {
    OPENFILENAME ofn;
    memset(&ofn, 0, sizeof(ofn));
    ofn.lStructSize = sizeof(ofn);
    ofn.lpstrFilter = L"JSGF files\0*.jsgf\0LIST files\0*.list\0All Files\0*.*\0\0";
    ofn.lpstrFile = g_file; g_file[0] = 0;
    ofn.nMaxFile = sizeof(g_file) / sizeof(pxcCHAR);
    ofn.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileName(&ofn)) g_file[0] = 0;
}

static void GetVocabularyFile(void) {
    OPENFILENAME ofn1;
    memset(&ofn1, 0, sizeof(ofn1));
    ofn1.lStructSize = sizeof(ofn1);
    ofn1.lpstrFilter = L"Text files\0*.txt\0All Files\0*.*\0\0";
    ofn1.lpstrFile = v_file; v_file[0] = 0;
    ofn1.nMaxFile = sizeof(v_file) / sizeof(pxcCHAR);
    ofn1.Flags = OFN_FILEMUSTEXIST | OFN_PATHMUSTEXIST | OFN_EXPLORER;
    if (!GetOpenFileName(&ofn1)) v_file[0] = 0;
}

// Work around treeview label editing bug
static LRESULT APIENTRY EditLabelControlProc(HWND hwndEdit, UINT uMsg, WPARAM wParam, LPARAM lParam) {
    if (uMsg == WM_GETDLGCODE) return DLGC_WANTALLKEYS;
    return CallWindowProc(g_oldProc, hwndEdit, uMsg, wParam, lParam);
}

static INT_PTR CALLBACK DialogProc(HWND hwndDlg, UINT message, WPARAM wParam, LPARAM lParam) {
    HMENU menu = GetMenu(hwndDlg);
    HMENU menu1;

    switch (message) {
    case WM_INITDIALOG:
        PopulateModules(menu);
        PopulateSource(menu);
        CheckMenuRadioItem(GetSubMenu(menu, 3), 0, 1, 1, MF_BYPOSITION);
        PopulateLanguage(menu);
        PostMessage(hwndDlg, WM_COMMAND, ID_DICTATION, 0);
        return TRUE;
    case WM_COMMAND:
        menu1 = GetSubMenu(menu, 0);
        if (LOWORD(wParam) >= ID_SOURCEX && LOWORD(wParam) < ID_SOURCEX + GetMenuItemCount(menu1)) {
            CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), LOWORD(wParam) - ID_SOURCEX, MF_BYPOSITION);
            return TRUE;
        }
        menu1 = GetSubMenu(menu, 1);
        if (LOWORD(wParam) >= ID_MODULEX && LOWORD(wParam) < ID_MODULEX + GetMenuItemCount(menu1)) {
            CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), LOWORD(wParam) - ID_MODULEX, MF_BYPOSITION);
            PopulateLanguage(menu);
            return TRUE;
        }
        menu1 = GetSubMenu(menu, 2);
        if (LOWORD(wParam) >= ID_LANGUAGEX && LOWORD(wParam) < ID_LANGUAGEX + GetMenuItemCount(menu1)) {
            CheckMenuRadioItem(menu1, 0, GetMenuItemCount(menu1), LOWORD(wParam) - ID_LANGUAGEX, MF_BYPOSITION);
            return TRUE;
        }
        menu1 = GetSubMenu(menu, 3);
        switch (LOWORD(wParam)) {
        case IDCANCEL:
            if (!Stop(hwndDlg)) {
                PostMessage(hwndDlg, WM_COMMAND, IDCANCEL, 0);
            }
            else {
                DestroyWindow(hwndDlg);
                PostQuitMessage(0);
            }
            return TRUE;
        case ID_CMDCTRL:
            TreeView_DeleteAllItems(GetDlgItem(hwndDlg, IDC_STATUS));
            CheckMenuRadioItem(menu1, ID_CMDCTRL, ID_DICTATION, ID_CMDCTRL, MF_BYCOMMAND);
            TreeView_DeleteAllItems(GetDlgItem(hwndDlg, IDC_CONSOLE2));
            PromptNewCommand(GetDlgItem(hwndDlg, IDC_CONSOLE2));
            SetWindowText(GetDlgItem(hwndDlg, IDC_MODE), L"Command Control:");
            EnableMenuItem(menu, ID_SETVOCLIST, MF_DISABLED);
            EnableMenuItem(menu, ID_SETGRAMMAR, MF_ENABLED);
            return TRUE;
        case ID_DICTATION:
            TreeView_DeleteAllItems(GetDlgItem(hwndDlg, IDC_STATUS));
            CheckMenuRadioItem(menu1, ID_CMDCTRL, ID_DICTATION, ID_DICTATION, MF_BYCOMMAND);
            TreeView_DeleteAllItems(GetDlgItem(hwndDlg, IDC_CONSOLE2));
            SetWindowText(GetDlgItem(hwndDlg, IDC_MODE), L"Dictation:");
            EnableMenuItem(menu, ID_SETGRAMMAR, MF_DISABLED);
            EnableMenuItem(menu, ID_SETVOCLIST, MF_ENABLED);
            return TRUE;
        case IDC_START:
            Start(hwndDlg);
            Button_Enable(GetDlgItem(hwndDlg, IDC_START), false);
            Button_Enable(GetDlgItem(hwndDlg, IDC_STOP), true);
            for (int i = 0; i < GetMenuItemCount(menu); i++)
                EnableMenuItem(menu, i, MF_BYPOSITION | MF_GRAYED);
            DrawMenuBar(hwndDlg);
            return TRUE;
        case IDC_STOP:
            if (!Stop(hwndDlg)) {
                PostMessage(hwndDlg, WM_COMMAND, IDC_STOP, 0);
            }
            else {
                for (int i = 0; i < GetMenuItemCount(menu); i++)
                    EnableMenuItem(menu, i, MF_BYPOSITION | MF_ENABLED);
                DrawMenuBar(hwndDlg);

                Button_Enable(GetDlgItem(hwndDlg, IDC_START), true);
                Button_Enable(GetDlgItem(hwndDlg, IDC_STOP), false);
            }
            return TRUE;
        case ID_SETVOCLIST:
            GetVocabularyFile();
            if (v_file && v_file[0] != '\0') {
                CheckMenuItem(menu, ID_SETVOCLIST, MF_CHECKED);
            }
            else {
                CheckMenuItem(menu, ID_SETVOCLIST, MF_UNCHECKED);
            }
            return TRUE;
        case ID_SETGRAMMAR:
            TreeView_DeleteAllItems(GetDlgItem(hwndDlg, IDC_STATUS));
            TreeView_DeleteAllItems(GetDlgItem(hwndDlg, IDC_CONSOLE2));
            GetGrammarFile();
            if (g_file && g_file[0] != '\0') {
                CheckMenuItem(menu, ID_SETGRAMMAR, MF_CHECKED);
            }
            else {
                CheckMenuItem(menu, ID_SETGRAMMAR, MF_UNCHECKED);
                PromptNewCommand(GetDlgItem(hwndDlg, IDC_CONSOLE2));
            }
            return TRUE;
        }
        break;
    case WM_NOTIFY:
        if (IsRunning()) break;	// Editing only when not running
        if (!IsCommandControl(hwndDlg)) break; // Editing only for Command & Control
        if (LOWORD(wParam) == IDC_CONSOLE2) {
            HWND hwndConsole = GetDlgItem(hwndDlg, IDC_CONSOLE2);
            switch (((LPNMHDR)lParam)->code) {
            case TVN_BEGINLABELEDIT:
                g_hwndEdit = TreeView_GetEditControl(hwndConsole);
                g_oldProc = (WNDPROC)SetWindowLong(g_hwndEdit, GWLP_WNDPROC, (LONG)EditLabelControlProc); // TreeViewBug
                break;
            case TVN_ENDLABELEDIT:
            {
                pxcCHAR line[256] = { 0 };
                TVITEM tvi;
                tvi.mask = TVIF_TEXT;
                tvi.hItem = TreeView_GetSelection(hwndConsole);
                tvi.pszText = line;
                tvi.cchTextMax = sizeof(line) / sizeof(pxcCHAR);
                GetWindowText(g_hwndEdit, tvi.pszText, tvi.cchTextMax);
                TreeView_SetItem(hwndConsole, &tvi);
                PromptNewCommand(hwndConsole);
                TreeView_Select(hwndConsole, TreeView_GetNextSibling(hwndConsole, tvi.hItem), TVGN_CARET);
                SetWindowLong(g_hwndEdit, GWLP_WNDPROC, (LONG)g_oldProc);
                g_hwndEdit = 0;
            }
                break;
            case TVN_KEYDOWN:
                if (((LPNMTVKEYDOWN)lParam)->wVKey == VK_DELETE) {
                    HTREEITEM selected = TreeView_GetSelection(hwndConsole);
                    if (selected) TreeView_DeleteItem(hwndConsole, selected);
                    PromptNewCommand(hwndConsole);
                }
                break;
            }
        }
        break;
    }
    return FALSE;
}

#pragma warning(disable:4706) /* assignment within conditional */
int APIENTRY wWinMain(HINSTANCE hInstance, HINSTANCE, LPTSTR, int) {
    InitCommonControls();
    g_hInst = hInstance;

    g_session = PXCSession::CreateInstance();
    if (!g_session) {
        MessageBoxW(0, L"Failed to create an SDK session", L"Voice Recognition", MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    HWND hWnd = CreateDialogW(hInstance, MAKEINTRESOURCE(IDD_MAIN), 0, DialogProc);
    if (!hWnd)  {
        MessageBoxW(0, L"Failed to create a window", L"Voice Recognition", MB_ICONEXCLAMATION | MB_OK);
        return 1;
    }

    MSG msg;
    for (int sts; (sts = GetMessageW(&msg, 0, 0, 0));) {
        if (sts == -1) break;
        if (IsDialogMessage(hWnd, &msg)) continue;
        TranslateMessage(&msg);
        DispatchMessage(&msg);
    }

    g_session->Release();
    return (int)msg.wParam;
}
