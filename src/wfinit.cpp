/********************************************************************

   wfinit.c

   Windows File System Initialization Routines

   Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License.

********************************************************************/

#include <stdlib.h>

#include "winfile.h"
#include "lfn.h"
#include "wnetcaps.h"  // WNetGetCaps()
#include "wfdpi.h"     // Add DPI awareness header

#include <ole2.h>
#include <shlobj.h>
#include "resize.h"

#include "dbg.h"

TCHAR szNTlanman[] = TEXT("ntlanman.dll");
TCHAR szHelv[] = TEXT("Segoe UI");

HBITMAP hbmSave;

DWORD RGBToBGR(DWORD rgb);
VOID BoilThatDustSpec(WCHAR* pStart, BOOL bLoadIt);
VOID DoRunEquals(PINT pnCmdShow);
VOID GetSavedWindow(LPWSTR szBuf, PWINDOW pwin);
VOID GetSettings(VOID);

#define MENU_STRING_SIZ 80
#define PROFILE_STRING_SIZ 300

INT GetHeightFromPointsString(LPTSTR szPoints) {
    HDC hdc;
    INT height;

    hdc = GetDC(NULL);
    height = MulDiv(-atoi(szPoints), GetDeviceCaps(hdc, LOGPIXELSY), 72);
    ReleaseDC(NULL, hdc);

    return height;
}

VOID BiasMenu(HMENU hMenu, UINT Bias) {
    UINT pos, id, count;
    HMENU hSubMenu;
    TCHAR szMenuString[MENU_STRING_SIZ];

    count = GetMenuItemCount(hMenu);

    if (count == (UINT)-1)
        return;

    for (pos = 0; pos < count; pos++) {
        id = GetMenuItemID(hMenu, pos);

        if (id == (UINT)-1) {
            // must be a popup, recurse and update all ID's here
            if (hSubMenu = GetSubMenu(hMenu, pos))
                BiasMenu(hSubMenu, Bias);
        } else if (id) {
            // replace the item that was there with a new
            // one with the id adjusted

            // makes sure id range is 0=99 first; really should assert or throw an exception
            id %= 100;

            GetMenuString(hMenu, pos, szMenuString, COUNTOF(szMenuString), MF_BYPOSITION);
            DeleteMenu(hMenu, pos, MF_BYPOSITION);
            InsertMenu(hMenu, pos, MF_BYPOSITION | MF_STRING, id + Bias, szMenuString);
        }
    }
}

VOID InitExtensions() {
    TCHAR szBuf[PROFILE_STRING_SIZ];
    TCHAR szPath[MAXPATHLEN];
    LPTSTR p;
    HMODULE hMod;
    FM_EXT_PROC fp;
    HMENU hMenu;
    INT iMenuBase;
    HMENU hMenuFrame;
    INT iMenuOffset = 0;

    hMenuFrame = GetMenu(hwndFrame);

    ASSERT(!bSecMenuDeleted);
    iMenuBase = MapIDMToMenuPos(IDM_EXTENSIONS);

    GetPrivateProfileString(szAddons, NULL, szNULL, szBuf, COUNTOF(szBuf), szTheINIFile);

    for (p = szBuf; *p && iNumExtensions < MAX_EXTENSIONS; p += lstrlen(p) + 1) {
        GetPrivateProfileString(szAddons, p, szNULL, szPath, COUNTOF(szPath), szTheINIFile);

        hMod = LoadLibrary(szPath);

        if (hMod) {
            fp = (FM_EXT_PROC)GetProcAddress(hMod, FM_EXT_PROC_ENTRYW);

            if (fp) {
                UINT bias;
                FMS_LOADW lsW;

                bias = ((IDM_EXTENSIONS + iNumExtensions + 1) * 100);

                // We are now going to bias each menu, since extensions
                // don't know about each other and may clash IDM_xx if
                // we don't.

                // Our system is as follow:  IDMs 100 - 699 are reserved
                // for us (menus 0 - 5 inclusive).  Thus, IDMs
                // 700 - 1699 is reserved for extensions.
                // This is why we added 1 in the above line to compute
                // NOTE: IDMs 0000-0099 are not used for menu 0.

                lsW.wMenuDelta = bias;

                if ((*fp)(hwndFrame, FMEVENT_LOAD, (LPARAM)&lsW)) {
                    if (lsW.dwSize != sizeof(FMS_LOADW))
                        goto LoadFail;

                    hMenu = lsW.hMenu;

                    extensions[iNumExtensions].ExtProc = fp;
                    extensions[iNumExtensions].Delta = bias;
                    extensions[iNumExtensions].hModule = hMod;
                    extensions[iNumExtensions].hMenu = hMenu;
                    extensions[iNumExtensions].bUnicode = TRUE;
                    extensions[iNumExtensions].hbmButtons = NULL;
                    extensions[iNumExtensions].idBitmap = 0;
                    extensions[iNumExtensions].iStartBmp = 0;
                    extensions[iNumExtensions].bRestored = FALSE;

                    if (hMenu) {
                        BiasMenu(hMenu, bias);

                        InsertMenuW(
                            hMenuFrame, iMenuBase + iMenuOffset, MF_BYPOSITION | MF_POPUP, (UINT_PTR)hMenu,
                            lsW.szMenuName);
                        iMenuOffset++;
                    }

                    iNumExtensions++;

                } else {
                    goto LoadFail;
                }
            } else {
            LoadFail:
                FreeLibrary(hMod);
            }
        }
    }
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  GetSettings() -                                                         */
/*                                                                          */
/*--------------------------------------------------------------------------*/

VOID GetSettings() {
    TCHAR szTemp[128];
    INT size;
    INT weight;

    INT bfCharset;

    /* Get the flags out of the INI file. */
    bMinOnRun = GetPrivateProfileInt(szSettings, szMinOnRun, bMinOnRun, szTheINIFile);
    bIndexOnLaunch = GetPrivateProfileInt(szSettings, szIndexOnLaunch, bIndexOnLaunch, szTheINIFile);
    bIndexHiddenSystem = GetPrivateProfileInt(szSettings, szIndexHiddenSystem, bIndexHiddenSystem, szTheINIFile);
    wTextAttribs = (WORD)GetPrivateProfileInt(szSettings, szLowerCase, wTextAttribs, szTheINIFile);
    bStatusBar = GetPrivateProfileInt(szSettings, szStatusBar, bStatusBar, szTheINIFile);
    bDisableVisualStyles = GetPrivateProfileInt(szSettings, szDisableVisualStyles, bDisableVisualStyles, szTheINIFile);
    bMirrorContent = GetPrivateProfileInt(szSettings, szMirrorContent, DefaultLayoutRTL(), szTheINIFile);

    bDriveBar = GetPrivateProfileInt(szSettings, szDriveBar, bDriveBar, szTheINIFile);

    bNewWinOnConnect = GetPrivateProfileInt(szSettings, szNewWinOnNetConnect, bNewWinOnConnect, szTheINIFile);
    bConfirmDelete = GetPrivateProfileInt(szSettings, szConfirmDelete, bConfirmDelete, szTheINIFile);
    bConfirmSubDel = GetPrivateProfileInt(szSettings, szConfirmSubDel, bConfirmSubDel, szTheINIFile);
    bConfirmReplace = GetPrivateProfileInt(szSettings, szConfirmReplace, bConfirmReplace, szTheINIFile);
    bConfirmMouse = GetPrivateProfileInt(szSettings, szConfirmMouse, bConfirmMouse, szTheINIFile);
    bConfirmFormat = GetPrivateProfileInt(szSettings, szConfirmFormat, bConfirmFormat, szTheINIFile);
    bConfirmReadOnly = GetPrivateProfileInt(szSettings, szConfirmReadOnly, bConfirmReadOnly, szTheINIFile);
    uChangeNotifyTime = GetPrivateProfileInt(szSettings, szChangeNotifyTime, uChangeNotifyTime, szTheINIFile);
    bSaveSettings = GetPrivateProfileInt(szSettings, szSaveSettings, bSaveSettings, szTheINIFile);
    bScrollOnExpand = GetPrivateProfileInt(szSettings, szScrollOnExpand, bScrollOnExpand, szTheINIFile);
    weight = GetPrivateProfileInt(szSettings, szFaceWeight, 400, szTheINIFile);

    GetPrivateProfileString(szSettings, szSize, TEXT("9"), szTemp, COUNTOF(szTemp), szTheINIFile);

    size = GetHeightFromPointsString(szTemp);

    GetPrivateProfileString(szSettings, szFace, szHelv, szTemp, COUNTOF(szTemp), szTheINIFile);

    bfCharset = ANSI_CHARSET;

    hFont = CreateFont(
        size, 0, 0, 0, weight, wTextAttribs & TA_ITALIC, 0, 0, bfCharset, OUT_DEFAULT_PRECIS, CLIP_DEFAULT_PRECIS,
        DEFAULT_QUALITY, DEFAULT_PITCH | FF_SWISS, szTemp);
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  GetInternational() -                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

VOID GetInternational() {
    GetLocaleInfoW(lcid, LOCALE_STHOUSAND, (LPWSTR)szComma, COUNTOF(szComma));
    GetLocaleInfoW(lcid, LOCALE_SDECIMAL, (LPWSTR)szDecimal, COUNTOF(szDecimal));
}

INT GetDriveOffset(DRIVE drive) {
    if (IsRemoteDrive(drive)) {
        if (aDriveInfo[drive].bRemembered)
            return 5;
        else
            return 4;
    }

    if (IsRemovableDrive(drive))
        return 1;

    if (IsRamDrive(drive))
        return 3;

    if (IsCDRomDrive(drive))
        return 0;

    return 2;
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  InitMenus() -                                                           */
/*                                                                          */
/*--------------------------------------------------------------------------*/

VOID InitMenus() {
    HMENU hMenu;

    hMenu = GetMenu(hwndFrame);

    if (bStatusBar)
        CheckMenuItem(hMenu, IDM_STATUSBAR, MF_BYCOMMAND | MF_CHECKED);
    if (bMinOnRun)
        CheckMenuItem(hMenu, IDM_MINONRUN, MF_BYCOMMAND | MF_CHECKED);
    if (bIndexOnLaunch)
        CheckMenuItem(hMenu, IDM_INDEXONLAUNCH, MF_BYCOMMAND | MF_CHECKED);

    if (bSaveSettings)
        CheckMenuItem(hMenu, IDM_SAVESETTINGS, MF_BYCOMMAND | MF_CHECKED);

    if (bDriveBar)
        CheckMenuItem(hMenu, IDM_DRIVEBAR, MF_BYCOMMAND | MF_CHECKED);

    if (bNewWinOnConnect)
        CheckMenuItem(hMenu, IDM_NEWWINONCONNECT, MF_BYCOMMAND | MF_CHECKED);

    //
    // Init menus after the window/menu has been created
    //
    InitExtensions();

    //
    // Redraw the menu bar since it's already displayed
    //
    DrawMenuBar(hwndFrame);
}

// maps all IDM_* values, even those of submenus, into a top level menu position;
// File menu is position 0 except when maximized in which it is position 1;
// when the security menu is missing (due to not loading acledit.dll),
// the menus to the right of security are shifted left by one.
UINT MapIDMToMenuPos(UINT idm) {
    UINT pos;

    if (idm < 100) {
        // idm values < 100 are just the top level menu IDM_ value (e.g., IDM_FILE)
        pos = idm;
    } else {
        // these are the built in or extension menu IDM_ values; IDM_OPEN is 101 and thus pos will be 0 (IDM_FILE)
        pos = idm / 100 - 1;
    }

    // if maximized, menu position shifted one to the right
    HWND hwndActive;
    hwndActive = (HWND)SendMessage(hwndMDIClient, WM_MDIGETACTIVE, 0, 0L);
    if (hwndActive && GetWindowLongPtr(hwndActive, GWL_STYLE) & WS_MAXIMIZE)
        pos++;

    // if pos >= IDM_EXTENSIONS, subtract one if security menu missing
    if (pos >= IDM_EXTENSIONS && bSecMenuDeleted) {
        pos--;
    }

    return pos;
}

// opposite of the above, but only works for top level menu positions
UINT MapMenuPosToIDM(UINT pos) {
    UINT idm = pos;

    // if maximized, idm is one position to the left
    HWND hwndActive;
    hwndActive = (HWND)SendMessage(hwndMDIClient, WM_MDIGETACTIVE, 0, 0L);
    if (hwndActive && GetWindowLongPtr(hwndActive, GWL_STYLE) & WS_MAXIMIZE)
        idm--;

    // if pos >= IDM_SECURITY, add one if security menu missing
    if (idm >= IDM_SECURITY && bSecMenuDeleted) {
        idm++;
    }

    if (idm >= IDM_EXTENSIONS + (UINT)iNumExtensions) {
        idm += MAX_EXTENSIONS - (UINT)iNumExtensions;
    }

    return idm;
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  BoilThatDustSpec() -                                                    */
/*                                                                          */
/*--------------------------------------------------------------------------*/

/* Parses the command line (if any) passed into WINFILE and exec's any tokens
 * it may contain.
 */

VOID BoilThatDustSpec(TCHAR* pStart, BOOL bLoadIt) {
    TCHAR* pEnd;
    DWORD ret;
    BOOL bFinished;

    if (*pStart == CHAR_NULL)
        return;

    bFinished = FALSE;
    while (!bFinished) {
        pEnd = pStart;
        while ((*pEnd) && (*pEnd != CHAR_SPACE) && (*pEnd != CHAR_COMMA))
            ++pEnd;

        if (*pEnd == CHAR_NULL)
            bFinished = TRUE;
        else
            *pEnd = CHAR_NULL;

        ret = ExecProgram(pStart, szNULL, NULL, bLoadIt, FALSE);
        if (ret)
            MyMessageBox(NULL, IDS_EXECERRTITLE, ret, MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL);

        pStart = pEnd + 1;
    }
}

/////////////////////////////////////////////////////////////////////
//
// Name:     GetSavedWindow
//
// Synopsis:
//
// IN        szBuf  buffer to parse out all save win info (NULL = default)
// OUT       pwin   pwin filled with szBuf fields.
//
// Return:
//
//
// Assumes:
//
// Effects:
//
//
// Notes:
//
/////////////////////////////////////////////////////////////////////

VOID GetSavedWindow(LPWSTR szBuf, PWINDOW pwin) {
    PINT pint;
    INT count;

    //
    // defaults
    //
    // (CW_USEDEFAULT must be type casted since it is defined
    // as an int.
    //
    pwin->rc.right = pwin->rc.left = (LONG)CW_USEDEFAULT;
    pwin->pt.x = pwin->pt.y = pwin->rc.top = pwin->rc.bottom = 0;
    pwin->sw = SW_SHOWNORMAL;
    pwin->dwSort = IDD_NAME;
    pwin->dwView = VIEW_NAMEONLY;
    pwin->dwAttribs = ATTR_DEFAULT;
    pwin->nSplit = 0;

    pwin->szDir[0] = CHAR_NULL;

    if (!szBuf)
        return;

    //
    // BUGBUG: (LATER)
    // This assumes the members of the point structure are
    // the same size as INT (sizeof(LONG) == sizeof(INT) == sizeof(UINT))!
    //

    count = 0;
    pint = (PINT)&pwin->rc;  // start by filling the rect

    while (*szBuf && count < 11) {
        *pint++ = atoi(szBuf);  // advance to next field

        while (*szBuf && *szBuf != CHAR_COMMA)
            szBuf++;

        while (*szBuf && *szBuf == CHAR_COMMA)
            szBuf++;

        count++;
    }

    lstrcpy(pwin->szDir, szBuf);  // this is the directory
}

BOOL CheckDirExists(LPWSTR szDir) {
    BOOL bRet = FALSE;

    if (IsNetDrive(DRIVEID(szDir)) == 2) {
        CheckDrive(hwndFrame, DRIVEID(szDir), FUNC_SETDRIVE);
        return TRUE;
    }

    if (IsValidDisk(DRIVEID(szDir)))
        bRet = SetCurrentDirectory(szDir);

    return bRet;
}

BOOL CreateSavedWindows(LPCWSTR pszInitialDir) {
    WCHAR buf[2 * MAXPATHLEN + 7 * 7], key[10];
    WINDOW win;
    LPTSTR FilePart;
    DWORD SizeAvailable;
    DWORD CharsCopied;

    //
    // since win.szDir is bigger.
    //
    WCHAR szDir[2 * MAXPATHLEN];

    INT nDirNum;
    HWND hwnd;
    INT iNumTrees;

    //
    // Initialize window geometry to use system default
    //
    win.rc.left = CW_USEDEFAULT;
    win.rc.top = 0;
    win.rc.right = win.rc.left + CW_USEDEFAULT;
    win.rc.bottom = 0;
    win.nSplit = -1;

    win.dwView = dwNewView;
    win.dwSort = dwNewSort;
    win.dwAttribs = dwNewAttribs;

    //
    // make sure this thing exists so we don't hit drives that don't
    // exist any more
    //
    nDirNum = 1;
    iNumTrees = 0;

    do {
        wsprintf(key, szDirKeyFormat, nDirNum++);

        GetPrivateProfileString(szSettings, key, szNULL, buf, COUNTOF(buf), szTheINIFile);

        if (*buf) {
            GetSavedWindow(buf, &win);

            if (pszInitialDir == NULL) {
                //
                // Winfile won't retain any relative paths in the INI file, but if
                // one was provided externally, convert it into a full path
                //

                SizeAvailable = COUNTOF(szDir);
                CharsCopied = GetFullPathName(win.szDir, SizeAvailable, szDir, &FilePart);
                if (CharsCopied == 0 || CharsCopied >= SizeAvailable || ISUNCPATH(szDir)) {
                    continue;
                }
                lstrcpy(win.szDir, szDir);

                //
                // clean off some junk so we can do this test
                //
                StripFilespec(szDir);
                StripBackslash(szDir);

                if (!CheckDirExists(szDir)) {
                    continue;
                }

                dwNewView = win.dwView;
                dwNewSort = win.dwSort;
                dwNewAttribs = win.dwAttribs;

                hwnd = CreateTreeWindow(
                    win.szDir, win.rc.left, win.rc.top, win.rc.right - win.rc.left, win.rc.bottom - win.rc.top,
                    win.nSplit);

                if (!hwnd) {
                    continue;
                }

                iNumTrees++;

                //
                // keep track of this for now...
                //
                if (IsIconic(hwnd)) {
                    SetWindowPos(hwnd, NULL, win.pt.x, win.pt.y, 0, 0, SWP_NOSIZE | SWP_NOZORDER | SWP_NOACTIVATE);
                }

                ShowWindow(hwnd, win.sw);
            }
        }

    } while (*buf);

    //
    //  If the user requested to open the program with a specific directory,
    //  open it
    //

    if (pszInitialDir != NULL) {
        SizeAvailable = COUNTOF(buf) - (DWORD)wcslen(szStarDotStar) - 1;
        CharsCopied = GetFullPathName(pszInitialDir, SizeAvailable, buf, &FilePart);
        if (CharsCopied > 0 && CharsCopied < SizeAvailable && !ISUNCPATH(buf) && CheckDirExists(buf)) {
            AddBackslash(buf);
            lstrcat(buf, szStarDotStar);

            //
            // use the settings of the most recent window as defaults
            //

            dwNewView = win.dwView;
            dwNewSort = win.dwSort;
            dwNewAttribs = win.dwAttribs;

            hwnd = CreateTreeWindow(
                buf, win.rc.left, win.rc.top, win.rc.right - win.rc.left, win.rc.bottom - win.rc.top, win.nSplit);

            if (!hwnd) {
                return FALSE;
            }

            //
            // Default to maximized since the user requested to open a single
            // directory
            //
            ShowWindow(hwnd, SW_MAXIMIZE);

            iNumTrees++;
        }
    }

    //
    // if nothing was saved or specified, create a tree for the current drive
    //
    if (!iNumTrees) {
        lstrcpy(buf, szOriginalDirPath);
        AddBackslash(buf);
        lstrcat(buf, szStarDotStar);

        //
        // default to split window
        //
        hwnd = CreateTreeWindow(buf, CW_USEDEFAULT, 0, CW_USEDEFAULT, 0, -1);

        if (!hwnd)
            return FALSE;

        iNumTrees++;
    }

    return TRUE;
}

// void  GetTextStuff(HDC hdc)
//
// this computes all the globals that are dependent on the
// currently selected font
//
// in:
//      hdc     DC with font selected into it
//

VOID GetTextStuff(HDC hdc) {
    SIZE size;
    TEXTMETRIC tm;

    GetTextExtentPoint32(hdc, TEXT("W"), 1, &size);

    dxText = size.cx;
    dyText = size.cy;

    //
    // the old method of GetTextExtent("M") was not good enough for the
    // drive bar because in Arial 8 (the default font), a lowercase 'w'
    // is actually wider than an uppercase 'M', which causes drive W in
    // the drive bar to clip.  rather than play around with a couple of
    // sample letters and taking the max, or fudging the width, we just
    // get the metrics and use the real max character width.
    //
    // to avoid the tree window indents looking too wide, we use the old
    // 'M' extent everywhere besides the drive bar, though.
    //
    GetTextMetrics(hdc, &tm);

    //
    // these are all dependent on the text metrics
    //
    dxDrive = dxDriveBitmap + tm.tmMaxCharWidth + (4 * dyBorderx2);
    dyDrive = max(dyDriveBitmap + (4 * dyBorderx2), dyText);
    dyFileName = max(dyText, dyFolder);  //  + dyBorder;

    // Add additional spacing for high-DPI displays
    if (g_scale > 1.0f) {
        dxDrive += ScaleByDpi(4);
        dyDrive += ScaleByDpi(2);
    }
}

UINT FillDocType(PPDOCBUCKET ppDoc, LPCWSTR pszSection, LPCWSTR pszDefault) {
    LPWSTR pszDocuments = NULL;
    LPWSTR p;
    LPWSTR p2;
    UINT uLen = 0;

    UINT uRetval = 0;

    do {
        uLen += 32;

        if (pszDocuments)
            LocalFree((HLOCAL)pszDocuments);

        pszDocuments = (LPWSTR)LocalAlloc(LMEM_FIXED, uLen * sizeof(WCHAR));

        if (!pszDocuments) {
            return 0;
        }

    } while (GetProfileString(szWindows, pszSection, pszDefault, pszDocuments, uLen) == (DWORD)uLen - 2);

    //
    // Parse through string, searching for blanks
    //
    for (p = pszDocuments; *p; p++) {
        if (CHAR_SPACE == *p) {
            *p = CHAR_NULL;
        }
    }

    for (p2 = pszDocuments; p2 < p; p2++) {
        if (*p2) {
            if (DocInsert(ppDoc, p2, NULL) == 1)
                uRetval++;

            while (*p2 && p2 != p)
                p2++;
        }
    }

    LocalFree((HLOCAL)pszDocuments);

    return uRetval;
}

/*--------------------------------------------------------------------------*/
/*                                                                          */
/*  InitFileManager() -                                                     */
/*                                                                          */
/*--------------------------------------------------------------------------*/

BOOL InitFileManager(HINSTANCE hInstance, LPWSTR lpCmdLine, INT nCmdShow) {
    INT i;
    HDC hdcScreen;

    //
    // 2*MAXPATHLEN since may have huge filter.
    //
    WCHAR szBuffer[2 * MAXPATHLEN];

    HCURSOR hcurArrow;
    WNDCLASS wndClass;
    WINDOW win;
    HWND hwnd;
    HANDLE hOld;
    RECT rcT, rcS;
    WCHAR szTemp[80];
    DWORD Ignore;

    HANDLE hThread;
    DWORD dwRetval;
    DWORD dwExStyle = 0L;
    LPWSTR pszInitialDir = NULL;

    // Initialize DPI awareness support
    InitDPIAwareness();

    hThread = GetCurrentThread();

    SetThreadPriority(hThread, THREAD_PRIORITY_ABOVE_NORMAL);

    InitializeCriticalSection(&CriticalSectionPath);

    // ProfStart();

    //
    // Preserve this instance's module handle
    //
    hAppInstance = hInstance;

    // setup ini file location
    lstrcpy(szTheINIFile, szBaseINIFile);
    dwRetval = GetEnvironmentVariable(TEXT("APPDATA"), szBuffer, MAXPATHLEN);
    if (dwRetval > 0 && dwRetval <= (DWORD)(MAXPATHLEN - lstrlen(szRoamINIPath) - 1 - lstrlen(szBaseINIFile) - 1)) {
        wsprintf(szTheINIFile, TEXT("%s%s"), szBuffer, szRoamINIPath);
        if (CreateDirectory(szTheINIFile, NULL) || GetLastError() == ERROR_ALREADY_EXISTS) {
            wsprintf(szTheINIFile, TEXT("%s%s\\%s"), szBuffer, szRoamINIPath, szBaseINIFile);
        } else {
            wsprintf(szTheINIFile, TEXT("%s\\%s"), szBuffer, szBaseINIFile);
        }
    }

    // e.g., UILanguage=zh-CN; UI language defaults to OS set language or English if that language is not supported.
    GetPrivateProfileString(szSettings, szUILanguage, szNULL, szTemp, COUNTOF(szTemp), szTheINIFile);
    if (szTemp[0]) {
        LCID lcidUI = WFLocaleNameToLCID(szTemp, 0);
        if (lcidUI != 0) {
            SetThreadUILanguage((LANGID)lcidUI);

            // update to current local used for dispaly
            SetThreadLocale(lcidUI);
        }
    }

    lcid = GetThreadLocale();

    //
    // Constructors for info system.
    // Must always do since these never fail and we don't test for
    // non-initialization during destruction (if FindWindow returns valid)
    //
    M_Info();

    M_Type();
    M_Space();
    M_NetCon();
    M_VolInfo();

    //
    // Bounce errors to us, not fs.
    //
    SetErrorMode(1);

    for (i = 0; i < 26; i++) {
        I_Space(i);
    }

    if (OleInitialize(0) != NOERROR)
        return FALSE;

    //
    // Remember the current directory.
    //
    GetCurrentDirectory(COUNTOF(szOriginalDirPath), szOriginalDirPath);

    if (*lpCmdLine) {
        LPWSTR lpArgs;

        //
        //  Note this isn't just finding the next argument, it's NULL
        //  terminating lpCmdLine at the point of the next argument
        //
        lpArgs = pszNextComponent(lpCmdLine);
        lpCmdLine = pszRemoveSurroundingQuotes(lpCmdLine);

        if (WFIsDir(lpCmdLine)) {
            pszInitialDir = lpCmdLine;
        } else {
            nCmdShow = SW_SHOWMINNOACTIVE;

            dwRetval = ExecProgram(lpCmdLine, lpArgs, NULL, FALSE, FALSE);
            if (dwRetval != 0) {
                MyMessageBox(NULL, IDS_EXECERRTITLE, dwRetval, MB_OK | MB_ICONEXCLAMATION | MB_SYSTEMMODAL);
            }
        }
    }

    //
    // Read WINFILE.INI and set the appropriate variables.
    //
    GetSettings();

    //
    // If the user specified an initial directory on the command line, that
    // directory will be opened, and save settings is disabled by default.
    //
    if (pszInitialDir != NULL) {
        bSaveSettings = FALSE;
    }

    dwExStyle = MainWindowExStyle();

    dyBorder = ScaledSystemMetric(SM_CYBORDER);
    dyBorderx2 = dyBorder * 2;
    dxFrame = ScaledSystemMetric(SM_CXFRAME) - dyBorder;

    dxDriveBitmap = ScaleByDpi(DRIVES_WIDTH);
    dyDriveBitmap = ScaleByDpi(DRIVES_HEIGHT);
    dxFolder = ScaleByDpi(FILES_WIDTH);
    dyFolder = ScaleByDpi(FILES_HEIGHT);

    // Update icon size variables based on DPI
    dyIcon = ScaleByDpi(32);
    dxIcon = ScaleByDpi(32);

    dxButtonSep = ScaleByDpi(8);
    dxButton = ScaleByDpi(24);
    dyButton = ScaleByDpi(22);
    dxDriveList = ScaleByDpi(205);
    dyDriveItem = ScaleByDpi(17);

    PngStartup();

    hicoTree = LoadIcon(hAppInstance, (LPTSTR)MAKEINTRESOURCE(TREEICON));
    hicoTreeDir = LoadIcon(hAppInstance, (LPTSTR)MAKEINTRESOURCE(TREEDIRICON));
    hicoDir = LoadIcon(hAppInstance, (LPTSTR)MAKEINTRESOURCE(DIRICON));

    chFirstDrive = CHAR_a;

    // now build the parameters based on the font we will be using

    hdcScreen = GetDC(NULL);

    hOld = SelectObject(hdcScreen, hFont);
    GetTextStuff(hdcScreen);
    if (hOld)
        SelectObject(hdcScreen, hOld);

    dxClickRect = max(GetSystemMetrics(SM_CXDOUBLECLK) / 2, 2 * dxText);
    dyClickRect = max(GetSystemMetrics(SM_CYDOUBLECLK) / 2, dyText);

    GetPrivateProfileString(szSettings, szDriveListFace, szHelv, szTemp, COUNTOF(szTemp), szTheINIFile);

    hfontDriveList = CreateFont(
        GetHeightFromPointsString(TEXT("8")), 0, 0, 0, 400, 0, 0, 0, ANSI_CHARSET, OUT_DEFAULT_PRECIS,
        CLIP_DEFAULT_PRECIS, DEFAULT_QUALITY, VARIABLE_PITCH | FF_SWISS, szTemp);

    ReleaseDC(NULL, hdcScreen);

    /* Load the accelerator table. */
    hAccel = LoadAccelerators(hInstance, (LPTSTR)MAKEINTRESOURCE(WFACCELTABLE));

    wHelpMessage = RegisterWindowMessage(TEXT("ShellHelp"));
    wBrowseMessage = RegisterWindowMessage(TEXT("commdlg_help"));

    hhkMsgFilter = SetWindowsHook(WH_MSGFILTER, MessageFilter);

    hcurArrow = LoadCursor(NULL, IDC_ARROW);

    wndClass.lpszClassName = szFrameClass;
    wndClass.style = 0L;
    wndClass.lpfnWndProc = FrameWndProc;
    wndClass.cbClsExtra = 0;
    wndClass.cbWndExtra = 0;
    wndClass.hInstance = hInstance;
    wndClass.hIcon = LoadIcon(hInstance, (LPTSTR)MAKEINTRESOURCE(APPICON));
    wndClass.hCursor = hcurArrow;
    wndClass.hbrBackground = (HBRUSH)(COLOR_APPWORKSPACE + 1);  // COLOR_WINDOW+1;
    wndClass.lpszMenuName = (LPTSTR)MAKEINTRESOURCE(FRAMEMENU);

    if (!RegisterClass(&wndClass)) {
        return FALSE;
    }

    wndClass.lpszClassName = szTreeClass;

    wndClass.style = 0;  // WS_CLIPCHILDREN;  //CS_VREDRAW | CS_HREDRAW;

    wndClass.lpfnWndProc = TreeWndProc;
    // wndClass.cbClsExtra     = 0;

    wndClass.cbWndExtra = GWL_LASTFOCUS + sizeof(LONG_PTR);

    // wndClass.hInstance      = hInstance;
    wndClass.hIcon = NULL;
    wndClass.hCursor = LoadCursor(hInstance, (LPTSTR)MAKEINTRESOURCE(SPLITCURSOR));
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    wndClass.lpszMenuName = NULL;

    if (!RegisterClass(&wndClass)) {
        return FALSE;
    }

    wndClass.lpszClassName = szDrivesClass;
    wndClass.style = CS_HREDRAW | CS_VREDRAW | CS_DBLCLKS;
    wndClass.lpfnWndProc = DrivesWndProc;
    // wndClass.cbClsExtra     = 0;
    wndClass.cbWndExtra = sizeof(LONG_PTR) +  // GWL_CURDRIVEIND
        sizeof(LONG_PTR) +                    // GWL_CURDRIVEFOCUS
        sizeof(LONG_PTR);                     // GWL_LPSTRVOLUME

    // wndClass.hInstance      = hInstance;
    // wndClass.hIcon          = NULL;
    wndClass.hCursor = hcurArrow;
    wndClass.hbrBackground = (HBRUSH)(COLOR_BTNFACE + 1);
    // wndClass.lpszMenuName   = NULL;

    if (!RegisterClass(&wndClass)) {
        return FALSE;
    }

    wndClass.lpszClassName = szTreeControlClass;
    wndClass.style = CS_DBLCLKS;
    wndClass.lpfnWndProc = TreeControlWndProc;
    // wndClass.cbClsExtra     = 0;
    wndClass.cbWndExtra = 2 * sizeof(LONG_PTR);  // GWL_READLEVEL, GWL_XTREEMAX
                                                 // wndClass.hInstance      = hInstance;
                                                 // wndClass.hIcon          = NULL;
    wndClass.hCursor = hcurArrow;
    wndClass.hbrBackground = NULL;
    // wndClass.lpszMenuName   = NULL;

    if (!RegisterClass(&wndClass)) {
        return FALSE;
    }

    wndClass.lpszClassName = szDirClass;
    wndClass.style = 0;  // CS_VREDRAW | CS_HREDRAW;
    wndClass.lpfnWndProc = DirWndProc;
    // wndClass.cbClsExtra     = 0;
    wndClass.cbWndExtra = GWL_OLEDROP + sizeof(LONG_PTR);
    // wndClass.hInstance      = hInstance;
    wndClass.hIcon = NULL;
    // wndClass.hCursor        = hcurArrow;
    wndClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
    // wndClass.lpszMenuName   = NULL;

    if (!RegisterClass(&wndClass)) {
        return FALSE;
    }

    wndClass.lpszClassName = szSearchClass;
    wndClass.style = 0L;
    wndClass.lpfnWndProc = SearchWndProc;
    // wndClass.cbClsExtra     = 0;
    wndClass.cbWndExtra = GWL_LASTFOCUS + sizeof(LONG_PTR);

    // wndClass.hInstance      = hInstance;
    wndClass.hIcon = LoadIcon(hInstance, (LPTSTR)MAKEINTRESOURCE(DIRICON));
    // wndClass.hCursor        = NULL;
    wndClass.hbrBackground = NULL;
    // wndClass.lpszMenuName   = NULL;

    if (!RegisterClass(&wndClass)) {
        return FALSE;
    }

    if (!ResizeDialogInitialize(hInstance)) {
        return FALSE;
    }

    if (!LoadString(hInstance, IDS_WINFILE, szTitle, COUNTOF(szTitle))) {
        return FALSE;
    }

    GetPrivateProfileString(szSettings, szWindow, szNULL, szBuffer, COUNTOF(szBuffer), szTheINIFile);
    GetSavedWindow(szBuffer, &win);

    // Check that at least some portion of the window is visible;
    // set to screen size if not

    // OLD: SystemParametersInfo(SPI_GETWORKAREA, 0, (PVOID)&rcT, 0);
    rcT.left = GetSystemMetrics(SM_XVIRTUALSCREEN);
    rcT.top = GetSystemMetrics(SM_YVIRTUALSCREEN);
    rcT.right = rcT.left + GetSystemMetrics(SM_CXVIRTUALSCREEN);
    rcT.bottom = rcT.top + GetSystemMetrics(SM_CYVIRTUALSCREEN);

    // right and bottom are width and height, so convert to coordinates

    // NOTE: in the cold startup case (no value in winfile.ini), the coordinates are
    // left: CW_USEDEFAULT, top: 0, right: CW_USEDEFAULT, bottom: 0.

    win.rc.right += win.rc.left;
    win.rc.bottom += win.rc.top;

    if (!IntersectRect(&rcS, &rcT, &win.rc)) {
        // window off virtual screen or initial case; reset to defaults
        win.rc.right = win.rc.left = (LONG)CW_USEDEFAULT;
        win.rc.top = win.rc.bottom = 0;

        // compenstate as above so the conversion below still results in the defaults
        win.rc.right += win.rc.left;
        win.rc.bottom += win.rc.top;
    }

    // Now convert back again

    win.rc.right -= win.rc.left;
    win.rc.bottom -= win.rc.top;

    // We need to know about all reparse tags
    hNtdll = GetModuleHandle(NTDLL_DLL);
    if (hNtdll) {
        pfnRtlSetProcessPlaceholderCompatibilityMode = (RtlSetProcessPlaceholderCompatibilityMode_t)GetProcAddress(
            hNtdll, "RtlSetProcessPlaceholderCompatibilityMode");
        if (pfnRtlSetProcessPlaceholderCompatibilityMode)
            pfnRtlSetProcessPlaceholderCompatibilityMode(PHCM_EXPOSE_PLACEHOLDERS);
    }

    //
    // Check to see if another copy of winfile is running.  If
    // so, either bring it to the foreground, or restore it to
    // a window (from an icon).
    //
    {
        HWND hwndPrev;
        HWND hwnd;

        hwndPrev = NULL;  // FindWindow (szFrameClass, NULL);

        // Check if developer mode is available in Windows10
        OSVERSIONINFO osversion;
        osversion.dwOSVersionInfoSize = sizeof(OSVERSIONINFO);
#pragma warning(push)
#pragma warning(disable : 4996)
        GetVersionEx(&osversion);
#pragma warning(pop)
        if (osversion.dwMajorVersion >= 10 && osversion.dwBuildNumber >= 14972)
            bDeveloperModeAvailable = TRUE;

        if (hwndPrev != NULL) {
            //  For Win32, this will accomplish almost the same effect as the
            //  above code does for Win 3.0/3.1   [stevecat]

            hwnd = GetLastActivePopup(hwndPrev);

            if (IsIconic(hwndPrev)) {
                ShowWindow(hwndPrev, SW_RESTORE);
                SetForegroundWindow(hwndPrev);
            } else {
                SetForegroundWindow(hwnd);
            }

            return FALSE;
        }
    }

    if (!CreateWindowEx(
            dwExStyle, szFrameClass, szTitle, WS_OVERLAPPEDWINDOW | WS_CLIPCHILDREN, win.rc.left, win.rc.top,
            win.rc.right, win.rc.bottom, NULL, NULL, hInstance, NULL)) {
        return FALSE;
    }

    // Now, we need to start the Change Notify thread, so that any changes
    // that happen will be notified.  This thread will create handles for
    // each viewed directory and wait on them.  If any of the handles are
    // awakened, then the appropriate directory will be sent a message to
    // refresh itself.  Another thread will also be created to watch for
    // network connections/disconnections.  This will be done via the
    // registry.  !! LATER !!

    InitializeWatchList();

    bUpdateRun = TRUE;
    //
    // Now create our worker thread and two events to communicate by
    //
    hEventUpdate = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEventUpdatePartial = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEventNetLoad = CreateEvent(NULL, TRUE, FALSE, NULL);
    hEventAcledit = CreateEvent(NULL, TRUE, FALSE, NULL);

    hThreadUpdate = CreateThread(NULL, 0L, UpdateInit, NULL, 0L, &Ignore);

    if (!hEventUpdate || !hEventUpdatePartial || !hThreadUpdate || !hEventNetLoad || !hEventAcledit) {
        LoadFailMessage();

        bUpdateRun = FALSE;

        return FALSE;
    }

    SetThreadPriority(hThreadUpdate, THREAD_PRIORITY_BELOW_NORMAL);

    //
    // Reset simple drive info, and don't let anyone block
    //
    ResetDriveInfo();
    SetEvent(hEventUpdatePartial);

    nFloppies = 0;
    for (i = 0; i < cDrives; i++) {
        if (IsRemovableDrive(rgiDrive[i])) {
            //
            // Avoid Phantom B: problems.
            //
            if ((nFloppies == 1) && (i > 1))
                nFloppies = 2;

            nFloppies++;
        }
    }

    //
    // Turn off redraw for hwndDriveList since we will display it later
    // (avoid flicker)
    //
    SendMessage(hwndDriveList, WM_SETREDRAW, 0, 0l);

    //
    // support forced min or max
    //
    if (nCmdShow == SW_SHOW || nCmdShow == SW_SHOWNORMAL && win.sw != SW_SHOWMINIMIZED) {
        nCmdShow = win.sw;
    }

    ShowWindow(hwndFrame, nCmdShow);

    if (bDriveBar) {
        //
        // Update the drive bar
        //
        InvalidateRect(hwndDriveBar, NULL, TRUE);
    }
    UpdateWindow(hwndFrame);

    //
    // Party time...
    // Slow stuff here
    //

    LoadString(hInstance, IDS_DIRSREAD, szDirsRead, COUNTOF(szDirsRead));
    LoadString(hInstance, IDS_BYTES, szBytes, COUNTOF(szBytes));
    LoadString(hInstance, IDS_SBYTES, szSBytes, COUNTOF(szSBytes));

    //
    // Read the International constants out of WIN.INI.
    //
    GetInternational();

    if (ppProgBucket = DocConstruct()) {
        FillDocType(ppProgBucket, L"Programs", szDefPrograms);
    }

    BuildDocumentStringWorker();

    if (!InitDirRead()) {
        LoadFailMessage();
        return FALSE;
    }

    //
    // Now draw drive list box
    //
    SendMessage(hwndDriveList, WM_SETREDRAW, (WPARAM)TRUE, 0l);
    InvalidateRect(hwndDriveList, NULL, TRUE);

    //
    // Init Menus fast stuff
    //
    // Init the Disk menu
    //
    InitMenus();

    if (!CreateSavedWindows(pszInitialDir)) {
        return FALSE;
    }

    ShowWindow(hwndMDIClient, SW_NORMAL);

    //
    // Kick start the background net read
    //
    // Note: We must avoid deadlock situations by kick starting the network
    // here.  There is a possible deadlock if you attempt a WAITNET() before
    // hEventUpdate is first set.  This can occur in the wfYield() step
    // of the background read below.
    //
    SetEvent(hEventUpdate);

    // now refresh all tree windows (start background tree read)
    //
    // since the tree reads happen in the background the user can
    // change the Z order by activating windows once the read
    // starts.  to avoid missing a window we must restart the
    // search through the MDI child list, checking to see if the
    // tree has been read yet (if there are any items in the
    // list box).  if it has not been read yet we start the read

    hwnd = GetWindow(hwndMDIClient, GW_CHILD);

    while (hwnd) {
        HWND hwndTree;

        if ((hwndTree = HasTreeWindow(hwnd)) &&
            (INT)SendMessage(GetDlgItem(hwndTree, IDCW_TREELISTBOX), LB_GETCOUNT, 0, 0L) == 0) {
            SendMessage(hwndTree, TC_SETDRIVE, MAKEWORD(FALSE, 0), 0L);
            hwnd = GetWindow(hwndMDIClient, GW_CHILD);
        } else {
            hwnd = GetWindow(hwnd, GW_HWNDNEXT);
        }
    }

    SetThreadPriority(hThread, THREAD_PRIORITY_NORMAL);

    if (bIndexOnLaunch) {
        StartBuildingDirectoryTrie();
    }

    return TRUE;
}

//--------------------------------------------------------------------------
//
//  FreeFileManager() -
//
//--------------------------------------------------------------------------

VOID FreeFileManager() {
    if (hThreadUpdate && bUpdateRun) {
        UpdateWaitQuit();
        CloseHandle(hThreadUpdate);
    }

    DeleteCriticalSection(&CriticalSectionPath);

#define CLOSEHANDLE(handle) \
    if (handle)             \
    CloseHandle(handle)

    CLOSEHANDLE(hEventNetLoad);
    CLOSEHANDLE(hEventAcledit);
    CLOSEHANDLE(hEventUpdate);
    CLOSEHANDLE(hEventUpdatePartial);

    DestroyWatchList();
    DestroyDirRead();

    D_Info();

    D_Type();
    D_Space();
    D_NetCon();
    D_VolInfo();

    DocDestruct(ppDocBucket);
    DocDestruct(ppProgBucket);

    PngShutdown();

    if (hFont)
        DeleteObject(hFont);

    if (hfontDriveList)
        DeleteObject(hfontDriveList);

    if (hNtshrui)
        FreeLibrary(hNtshrui);

    if (hMPR)
        FreeLibrary(hMPR);

    if (hVersion)
        FreeLibrary(hVersion);

    OleUninitialize();

#undef CLOSEHANDLE
}

/////////////////////////////////////////////////////////////////////
//
// Name:     LoadFailMessage
//
// Synopsis: Puts up message on load failure.
//
// IN        VOID
//
// Return:   VOID
//
//
// Assumes:
//
// Effects:
//
//
// Notes:
//
/////////////////////////////////////////////////////////////////////

VOID LoadFailMessage(VOID) {
    WCHAR szMessage[MAXMESSAGELEN];

    szMessage[0] = 0;

    LoadString(hAppInstance, IDS_INITUPDATEFAIL, szMessage, COUNTOF(szMessage));

    FormatError(FALSE, szMessage, COUNTOF(szMessage), GetLastError());

    LoadString(hAppInstance, IDS_INITUPDATEFAILTITLE, szTitle, COUNTOF(szTitle));

    MessageBox(hwndFrame, szMessage, szTitle, MB_ICONEXCLAMATION | MB_OK | MB_APPLMODAL);

    return;
}
