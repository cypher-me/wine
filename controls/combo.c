/*
 * Combo controls
 * 
 * Copyright 1993 Martin Ayotte
 * Copyright 1995 Bernd Schmidt
 * 
 */

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "windows.h"
#include "sysmetrics.h"
#include "combo.h"
#include "stackframe.h"
#include "user.h"
#include "win.h"
#include "graphics.h"
#include "listbox.h"
#include "dos_fs.h"
#include "drive.h"
#include "stddebug.h"
#include "debug.h"
#include "xmalloc.h"

 /*
  * Note: Combos are probably implemented in a different way by Windows.
  * Using a message spy for Windows, you can see some undocumented
  * messages being passed between ComboBox and ComboLBox.
  * I hope no programs rely on the implementation of combos.
  */

#define CBLMM_EDGE   4    /* distance inside box which is same as moving mouse
			     outside box, to trigger scrolling of CBL */

static HBITMAP hComboBit = 0;
static WORD CBitHeight, CBitWidth;

static int COMBO_Init()
{
  BITMAP bm;
  
  dprintf_combo(stddeb, "COMBO_Init\n");
  hComboBit = LoadBitmap(0, MAKEINTRESOURCE(OBM_COMBO));
  GetObject(hComboBit, sizeof(BITMAP), (LPSTR)&bm);
  CBitHeight = bm.bmHeight;
  CBitWidth = bm.bmWidth;
  return 0;
}

LPHEADCOMBO ComboGetStorageHeader(HWND hwnd)
{
  return (LPHEADCOMBO)GetWindowLong(hwnd,4);
}

LPHEADLIST ComboGetListHeader(HWND hwnd)
{
  return (LPHEADLIST)GetWindowLong(hwnd,0);
}

int CreateComboStruct(HWND hwnd, LONG style)
{
  LPHEADCOMBO lphc;

  lphc = (LPHEADCOMBO)xmalloc(sizeof(HEADCOMBO));
  SetWindowLong(hwnd,4,(LONG)lphc);
  lphc->hWndEdit = 0;
  lphc->hWndLBox = 0;
  lphc->dwState = 0;
  lphc->LastSel = -1;
  lphc->dwStyle = style;
  lphc->DropDownVisible = FALSE;
  return TRUE;
}

void ComboUpdateWindow(HWND hwnd, LPHEADLIST lphl, LPHEADCOMBO lphc, BOOL repaint)
{
  SetScrollRange(lphc->hWndLBox, SB_VERT, 0, ListMaxFirstVisible(lphl), TRUE);
  if (repaint && lphl->bRedrawFlag) {
    InvalidateRect(hwnd, NULL, TRUE);
  }
}

/***********************************************************************
 *           CBNCCreate
 */
static LRESULT CBNCCreate(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  CREATESTRUCT *createStruct;

  if (!hComboBit) COMBO_Init();

  createStruct = (CREATESTRUCT *)PTR_SEG_TO_LIN(lParam);
  createStruct->style &= ~(WS_VSCROLL | WS_HSCROLL);
  SetWindowLong(hwnd, GWL_STYLE, createStruct->style);

  dprintf_combo(stddeb,"ComboBox WM_NCCREATE!\n");
  return DefWindowProc(hwnd, WM_NCCREATE, wParam, lParam);

}

/***********************************************************************
 *           CBCreate
 */
static LRESULT CBCreate(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST   lphl;
  LPHEADCOMBO  lphc;
  LONG         style = 0;
  LONG         cstyle = GetWindowLong(hwnd,GWL_STYLE);
  RECT         rect,lboxrect;
  char className[] = "COMBOLBOX";  /* Hack so that class names are > 0x10000 */
  char editName[] = "EDIT";

  /* translate combo into listbox styles */
  if (cstyle & CBS_OWNERDRAWFIXED) style |= LBS_OWNERDRAWFIXED;
  if (cstyle & CBS_OWNERDRAWVARIABLE) style |= LBS_OWNERDRAWVARIABLE;
  if (cstyle & CBS_SORT) style |= LBS_SORT;
  if (cstyle & CBS_HASSTRINGS) style |= LBS_HASSTRINGS;
  style |= LBS_NOTIFY;
  CreateListBoxStruct(hwnd, ODT_COMBOBOX, style, GetParent(hwnd));
  CreateComboStruct(hwnd,cstyle);
  lphl = ComboGetListHeader(hwnd);
  lphc = ComboGetStorageHeader(hwnd);

  GetClientRect(hwnd,&rect);
  GetWindowRect(hwnd,&lboxrect);
  /* FIXME: combos with edit controls are broken. */
  switch(cstyle & 3) {
   case CBS_SIMPLE:            /* edit control, list always visible  */
    dprintf_combo(stddeb,"CBS_SIMPLE\n");
    SetRectEmpty(&lphc->RectButton);
    lphc->LBoxTop = lphl->StdItemHeight;
    lphc->hWndEdit = CreateWindow(MAKE_SEGPTR(editName), (SEGPTR)0, 
				  WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | SS_LEFT,
				  0, 0, rect.right, lphl->StdItemHeight,
				  hwnd, (HMENU)1, WIN_GetWindowInstance(hwnd), 0L);
    break;
   case CBS_DROPDOWN:          /* edit control, dropdown listbox     */
    dprintf_combo(stddeb,"CBS_DROPDOWN\n");
    lphc->RectButton = rect;
    lphc->RectButton.left = lphc->RectButton.right - 6 - CBitWidth;
    lphc->RectButton.bottom = lphc->RectButton.top + lphl->StdItemHeight;
    lphc->LBoxTop = lphl->StdItemHeight;
    SetWindowPos(hwnd, 0, 0, 0, rect.right - rect.left + 2*SYSMETRICS_CXBORDER,
		 lphl->StdItemHeight + 2*SYSMETRICS_CYBORDER,
		 SWP_NOMOVE | SWP_NOZORDER);
    lphc->hWndEdit = CreateWindow(MAKE_SEGPTR(editName), (SEGPTR)0,
				  WS_CHILD | WS_CLIPCHILDREN | WS_VISIBLE | SS_LEFT,
				  0, 0, lphc->RectButton.left, lphl->StdItemHeight,
				  hwnd, (HMENU)1, WIN_GetWindowInstance(hwnd), 0L);
    break;
   case CBS_DROPDOWNLIST:      /* static control, dropdown listbox   */
    dprintf_combo(stddeb,"CBS_DROPDOWNLIST\n");
    lphc->RectButton = rect;
    lphc->RectButton.left = lphc->RectButton.right - 6 - CBitWidth;
    lphc->RectButton.bottom = lphc->RectButton.top + lphl->StdItemHeight;
    lphc->LBoxTop = lphl->StdItemHeight;
    SetWindowPos(hwnd, 0, 0, 0, rect.right - rect.left + 2*SYSMETRICS_CXBORDER,
		 lphl->StdItemHeight + 2*SYSMETRICS_CYBORDER,
		 SWP_NOMOVE | SWP_NOZORDER);
    break;
  }
  lboxrect.top += lphc->LBoxTop;
  /* FIXME: WinSight says these should be CHILD windows with the TOPMOST flag
   * set. Wine doesn't support TOPMOST, and simply setting the WS_CHILD
   * flag doesn't work. */
  lphc->hWndLBox = CreateWindow(MAKE_SEGPTR(className), (SEGPTR)0, 
				WS_POPUP | WS_BORDER | WS_VSCROLL,
				lboxrect.left, lboxrect.top,
				lboxrect.right - lboxrect.left, 
				lboxrect.bottom - lboxrect.top,
				0, 0, WIN_GetWindowInstance(hwnd),
				(SEGPTR)hwnd );
  ShowWindow(lphc->hWndLBox, SW_HIDE);
  dprintf_combo(stddeb,"Combo Creation LBox=%04x\n", lphc->hWndLBox);
  return 0;
}

/***********************************************************************
 *           CBDestroy
 */
static LRESULT CBDestroy(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);

  ListBoxResetContent(lphl);
  DestroyListBoxStruct(lphl);
  dprintf_combo(stddeb,"Combo WM_DESTROY %p !\n", lphl);
  return 0;
}

/***********************************************************************
 *           CBPaint
 */
static LRESULT CBPaint(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls;
  PAINTSTRUCT  ps;
  HBRUSH hBrush;
  HFONT  hOldFont;
  HDC  hdc;
  RECT rect;
  int height;
  
  hdc = BeginPaint(hwnd, &ps);

  if (hComboBit != 0) {
    GRAPH_DrawReliefRect(hdc, &lphc->RectButton, 2, 2, FALSE);
    GRAPH_DrawBitmap(hdc, hComboBit,
		     lphc->RectButton.left + 3,lphc->RectButton.top + 2,
		     0, 0, CBitWidth, CBitHeight );
  }
  if (!IsWindowVisible(hwnd) || !lphl->bRedrawFlag 
      || (lphc->dwStyle & 3) != CBS_DROPDOWNLIST) 
  {
    /* we don't want to draw an entry when there is an edit control */
    EndPaint(hwnd, &ps);
    return 0;
  }

  hOldFont = SelectObject(hdc, lphl->hFont);

#ifdef WINELIB32
  hBrush = SendMessage(lphl->hParent, WM_CTLCOLORLISTBOX, hdc, hwnd);
#else
  hBrush = SendMessage(lphl->hParent, WM_CTLCOLOR, hdc,
		       MAKELONG(hwnd, CTLCOLOR_LISTBOX));
#endif
  if (hBrush == 0) hBrush = GetStockObject(WHITE_BRUSH);

  GetClientRect(hwnd, &rect);
  rect.right -= (lphc->RectButton.right - lphc->RectButton.left);

  lpls = ListBoxGetItem(lphl,lphl->ItemFocused);
  if (lpls != NULL) {  
    height = lpls->mis.itemHeight;
    rect.bottom = rect.top + height;
    FillRect(hdc, &rect, hBrush);
    ListBoxDrawItem (hwnd, lphl, hdc, lpls, &rect, ODA_DRAWENTIRE, 0);
    if (GetFocus() == hwnd)
    ListBoxDrawItem (hwnd,lphl, hdc, lpls, &rect, ODA_FOCUS, ODS_FOCUS);
  }
  else FillRect(hdc, &rect, hBrush);
  SelectObject(hdc,hOldFont);
  EndPaint(hwnd, &ps);
  return 0;
}

/***********************************************************************
 *           CBGetDlgCode
 */
static LRESULT CBGetDlgCode(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  return DLGC_WANTARROWS | DLGC_WANTCHARS;
}

/***********************************************************************
 *           CBLButtonDown
 */
static LRESULT CBLButtonDown(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);
  SendMessage(hwnd,CB_SHOWDROPDOWN,!lphc->DropDownVisible,0);
  return 0;
}

/***********************************************************************
 *           CBKeyDown
 */
static LRESULT CBKeyDown(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  WORD       newFocused = lphl->ItemFocused;

  switch(wParam) {
  case VK_HOME:
    newFocused = 0;
    break;
  case VK_END:
    newFocused = lphl->ItemsCount - 1;
    break;
  case VK_UP:
    if (newFocused > 0) newFocused--;
    break;
  case VK_DOWN:
    newFocused++;
    break;
  default:
    return 0;
  }

  if (newFocused >= lphl->ItemsCount)
    newFocused = lphl->ItemsCount - 1;
  
  ListBoxSetCurSel(lphl, newFocused);
  ListBoxSendNotification(lphl, CBN_SELCHANGE);

  lphl->ItemFocused = newFocused;
  ListBoxScrollToFocus(lphl);
/*  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);*/
  InvalidateRect(hwnd, NULL, TRUE);

  return 0;
}

/***********************************************************************
 *           CBChar
 */
static LRESULT CBChar(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  WORD       newFocused;

  newFocused = ListBoxFindNextMatch(lphl, wParam);
  if (newFocused == (WORD)LB_ERR) return 0;

  if (newFocused >= lphl->ItemsCount)
    newFocused = lphl->ItemsCount - 1;
  
  ListBoxSetCurSel(lphl, newFocused);
  ListBoxSendNotification(lphl, CBN_SELCHANGE);
  lphl->ItemFocused = newFocused;
  ListBoxScrollToFocus(lphl);
  
/*  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);*/
  InvalidateRect(hwnd, NULL, TRUE);

  return 0;
}

/***********************************************************************
 *           CBKillFocus
 */
static LRESULT CBKillFocus(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  return 0;
}

/***********************************************************************
 *           CBSetFocus
 */
static LRESULT CBSetFocus(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  return 0;
}

/***********************************************************************
 *           CBResetContent
 */
static LRESULT CBResetContent(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);

  ListBoxResetContent(lphl);
  ComboUpdateWindow(hwnd, lphl, lphc, TRUE);
  return 0;
}

/***********************************************************************
 *           CBDir
 */
static LRESULT CBDir(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  WORD wRet;
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);

  wRet = ListBoxDirectory(lphl, wParam, (LPSTR)PTR_SEG_TO_LIN(lParam));
  ComboUpdateWindow(hwnd, lphl, lphc, TRUE);
  return wRet;
}

/***********************************************************************
 *           CBInsertString
 */
static LRESULT CBInsertString(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  WORD  wRet;
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);

  if (lphl->HasStrings)
    wRet = ListBoxInsertString(lphl, wParam, (LPSTR)PTR_SEG_TO_LIN(lParam));
  else
    wRet = ListBoxInsertString(lphl, wParam, (LPSTR)lParam);

  ComboUpdateWindow(hwnd, lphl, lphc, TRUE);
  return wRet;
}

/***********************************************************************
 *           CBAddString
 */
static LRESULT CBAddString(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  WORD  wRet;
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);

  if (lphl->HasStrings)
    wRet = ListBoxAddString(lphl, (LPSTR)PTR_SEG_TO_LIN(lParam));
  else
    wRet = ListBoxAddString(lphl, (LPSTR)lParam);

  ComboUpdateWindow(hwnd, lphl, lphc, TRUE);
  return wRet;
}

/***********************************************************************
 *           CBDeleteString
 */
static LRESULT CBDeleteString(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);
  LONG lRet = ListBoxDeleteString(lphl,wParam);
  
  ComboUpdateWindow(hwnd, lphl, lphc, TRUE);
  return lRet;
}

/***********************************************************************
 *           CBSelectString
 */
static LRESULT CBSelectString(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  WORD  wRet;

  wRet = ListBoxFindString(lphl, wParam, (SEGPTR)lParam);

  /* XXX add functionality here */

  return 0;
}

/***********************************************************************
 *           CBFindString
 */
static LRESULT CBFindString(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return ListBoxFindString(lphl, wParam, (SEGPTR)lParam);
}

/***********************************************************************
 *           CBGetCount
 */
static LRESULT CBGetCount(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return lphl->ItemsCount;
}

/***********************************************************************
 *           CBSetCurSel
 */
static LRESULT CBSetCurSel(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  WORD  wRet;

  wRet = ListBoxSetCurSel(lphl, wParam);

  dprintf_combo(stddeb,"CBSetCurSel: hwnd %04x wp %x lp %lx wRet %d\n",
		hwnd,wParam,lParam,wRet);
/*  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);*/
  InvalidateRect(hwnd, NULL, TRUE);

  return wRet;
}

/***********************************************************************
 *           CBGetCurSel
 */
static LRESULT CBGetCurSel(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return lphl->ItemFocused;
}

/***********************************************************************
 *           CBGetItemHeight
 */
static LRESULT CBGetItemHeight(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  LPLISTSTRUCT lpls = ListBoxGetItem (lphl, wParam);

  if (lpls == NULL) return LB_ERR;
  return lpls->mis.itemHeight;
}

/***********************************************************************
 *           CBSetItemHeight
 */
static LRESULT CBSetItemHeight(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return ListBoxSetItemHeight(lphl, wParam, lParam);
}

/***********************************************************************
 *           CBSetRedraw
 */
static LRESULT CBSetRedraw(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  lphl->bRedrawFlag = wParam;
  return 0;
}

/***********************************************************************
 *           CBSetFont
 */
static LRESULT CBSetFont(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST  lphl = ComboGetListHeader(hwnd);

  if (wParam == 0)
    lphl->hFont = GetStockObject(SYSTEM_FONT);
  else
    lphl->hFont = (HFONT)wParam;

  return 0;
}

/***********************************************************************
 *           CBGetLBTextLen
 */
static LRESULT CBGetLBTextLen(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST   lphl = ComboGetListHeader(hwnd);
  LPLISTSTRUCT lpls = ListBoxGetItem(lphl,wParam);

  if (lpls == NULL || !lphl->HasStrings) return LB_ERR;
  return strlen(lpls->itemText);
}

/***********************************************************************
 *           CBGetLBText
 */
static LRESULT CBGetLBText(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return ListBoxGetText(lphl, wParam, (LPSTR)PTR_SEG_TO_LIN(lParam));
}

/***********************************************************************
 *           CBGetItemData
 */
static LRESULT CBGetItemData(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return ListBoxGetItemData(lphl, wParam);
}

/***********************************************************************
 *           CBSetItemData
 */
static LRESULT CBSetItemData(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST lphl = ComboGetListHeader(hwnd);
  return ListBoxSetItemData(lphl, wParam, lParam);
}

/***********************************************************************
 *           CBShowDropDown
 */
static LRESULT CBShowDropDown(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADCOMBO lphc = ComboGetStorageHeader(hwnd);
  RECT rect;
  
  if ((lphc->dwStyle & 3) == CBS_SIMPLE) return LB_ERR;
  
  wParam = !!wParam;
  if (wParam != lphc->DropDownVisible) {
    lphc->DropDownVisible = wParam;
    GetWindowRect(hwnd,&rect);
    SetWindowPos(lphc->hWndLBox, 0, rect.left, rect.top+lphc->LBoxTop, 0, 0,
		 SWP_NOSIZE | (wParam ? SWP_SHOWWINDOW : SWP_HIDEWINDOW));
    if (!wParam) SetFocus(hwnd);
  }
  return 0;
}


/***********************************************************************
 *           ComboWndProc
 */
LRESULT ComboBoxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) {	
     case WM_NCCREATE: return CBNCCreate(hwnd, wParam, lParam);
     case WM_CREATE: return CBCreate(hwnd, wParam, lParam);
     case WM_DESTROY: return CBDestroy(hwnd, wParam, lParam);
     case WM_GETDLGCODE: return CBGetDlgCode(hwnd, wParam, lParam);
     case WM_KEYDOWN: return CBKeyDown(hwnd, wParam, lParam);
     case WM_CHAR: return CBChar(hwnd, wParam, lParam);
     case WM_SETFONT: return CBSetFont(hwnd, wParam, lParam);
     case WM_SETREDRAW: return CBSetRedraw(hwnd, wParam, lParam);
     case WM_PAINT: return CBPaint(hwnd, wParam, lParam);
     case WM_LBUTTONDOWN: return CBLButtonDown(hwnd, wParam, lParam);
     case WM_SETFOCUS: return CBSetFocus(hwnd, wParam, lParam);
     case WM_KILLFOCUS: return CBKillFocus(hwnd, wParam, lParam);
     case CB_RESETCONTENT: return CBResetContent(hwnd, wParam, lParam);
     case CB_DIR: return CBDir(hwnd, wParam, lParam);
     case CB_ADDSTRING: return CBAddString(hwnd, wParam, lParam);
     case CB_INSERTSTRING: return CBInsertString(hwnd, wParam, lParam);
     case CB_DELETESTRING: return CBDeleteString(hwnd, wParam, lParam);
     case CB_FINDSTRING: return CBFindString(hwnd, wParam, lParam);
     case CB_GETCOUNT: return CBGetCount(hwnd, wParam, lParam);
     case CB_GETCURSEL: return CBGetCurSel(hwnd, wParam, lParam);
     case CB_GETITEMDATA: return CBGetItemData(hwnd, wParam, lParam);
     case CB_GETITEMHEIGHT: return CBGetItemHeight(hwnd, wParam, lParam);
     case CB_GETLBTEXT: return CBGetLBText(hwnd, wParam, lParam);
     case CB_GETLBTEXTLEN: return CBGetLBTextLen(hwnd, wParam, lParam);
     case CB_SELECTSTRING: return CBSelectString(hwnd, wParam, lParam);
     case CB_SETITEMDATA: return CBSetItemData(hwnd, wParam, lParam);
     case CB_SETCURSEL: return CBSetCurSel(hwnd, wParam, lParam);
     case CB_SETITEMHEIGHT: return CBSetItemHeight(hwnd, wParam, lParam);
     case CB_SHOWDROPDOWN: return CBShowDropDown(hwnd, wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/*--------------------------------------------------------------------*/
/* ComboLBox code starts here */

HWND CLBoxGetCombo(HWND hwnd)
{
#ifdef WINELIB32
  return (HWND)GetWindowLong(hwnd,0);
#else
  return (HWND)GetWindowWord(hwnd,0);
#endif
}

LPHEADLIST CLBoxGetListHeader(HWND hwnd)
{
  return ComboGetListHeader(CLBoxGetCombo(hwnd));
}

/***********************************************************************
 *           CBLCreate
 */
static LRESULT CBLCreate( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  CREATESTRUCT *createStruct = (CREATESTRUCT *)PTR_SEG_TO_LIN(lParam);
#ifdef WINELIB32
  SetWindowLong(hwnd,0,(LONG)createStruct->lpCreateParams);
#else
  SetWindowWord(hwnd,0,LOWORD(createStruct->lpCreateParams));
#endif
  return 0;
}

/***********************************************************************
 *           CBLGetDlgCode
 */
static LRESULT CBLGetDlgCode( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  return DLGC_WANTARROWS | DLGC_WANTCHARS;
}

/***********************************************************************
 *           CBLKeyDown
 */
static LRESULT CBLKeyDown( HWND hwnd, WPARAM wParam, LPARAM lParam ) 
{
  LPHEADLIST lphl = CLBoxGetListHeader(hwnd);
  WORD newFocused = lphl->ItemFocused;

  switch(wParam) {
  case VK_HOME:
    newFocused = 0;
    break;
  case VK_END:
    newFocused = lphl->ItemsCount - 1;
    break;
  case VK_UP:
    if (newFocused > 0) newFocused--;
    break;
  case VK_DOWN:
    newFocused++;
    break;
  case VK_PRIOR:
    if (newFocused > lphl->ItemsVisible) {
      newFocused -= lphl->ItemsVisible;
    } else {
      newFocused = 0;
    }
    break;
  case VK_NEXT:
    newFocused += lphl->ItemsVisible;
    break;
  default:
    return 0;
  }

  if (newFocused >= lphl->ItemsCount)
    newFocused = lphl->ItemsCount - 1;
  
  ListBoxSetCurSel(lphl, newFocused);
  ListBoxSendNotification(lphl, CBN_SELCHANGE);

  lphl->ItemFocused = newFocused;
  ListBoxScrollToFocus(lphl);
  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
  InvalidateRect(hwnd, NULL, TRUE);  
  return 0;
}

/***********************************************************************
 *           CBLChar
 */
static LRESULT CBLChar( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  return 0;
}

/***********************************************************************
 *           CBLPaint
 */
static LRESULT CBLPaint( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  LPHEADLIST   lphl = CLBoxGetListHeader(hwnd);
  LPLISTSTRUCT lpls;
  PAINTSTRUCT  ps;
  HBRUSH       hBrush;
  HFONT        hOldFont;
  HWND  combohwnd = CLBoxGetCombo(hwnd);
  HDC 	hdc;
  RECT 	rect;
  int   i, top, height;

  top = 0;
  hdc = BeginPaint( hwnd, &ps );

  if (!IsWindowVisible(hwnd) || !lphl->bRedrawFlag) {
    EndPaint(hwnd, &ps);
    return 0;
  }

  hOldFont = SelectObject(hdc, lphl->hFont);
#ifdef WINELIB32
  hBrush = SendMessage(lphl->hParent, WM_CTLCOLORLISTBOX, hdc, hwnd);
#else
  hBrush = SendMessage(lphl->hParent, WM_CTLCOLOR, hdc,
		       MAKELONG(hwnd, CTLCOLOR_LISTBOX));
#endif

  if (hBrush == 0) hBrush = GetStockObject(WHITE_BRUSH);

  GetClientRect(hwnd, &rect);
  FillRect(hdc, &rect, hBrush);

  lpls = lphl->lpFirst;

  lphl->ItemsVisible = 0;
  for(i = 0; i < lphl->ItemsCount; i++) {
    if (lpls == NULL) break;

    if (i >= lphl->FirstVisible) {
      height = lpls->mis.itemHeight;
      /* must have enough room to draw entire item */
      if (top > (rect.bottom-height+1)) break;

      lpls->itemRect.top    = top;
      lpls->itemRect.bottom = top + height;
      lpls->itemRect.left   = rect.left;
      lpls->itemRect.right  = rect.right;

      dprintf_listbox(stddeb,"drawing item: %d %d %d %d %d\n",
                      rect.left,top,rect.right,top+height,lpls->itemState);
      if (lphl->OwnerDrawn) {
	ListBoxDrawItem (combohwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_DRAWENTIRE, 0);
	if (lpls->itemState)
	  ListBoxDrawItem (combohwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_SELECT, ODS_SELECTED);
      } else {
	ListBoxDrawItem (combohwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_DRAWENTIRE, 
			 lpls->itemState);
      }
      if ((lphl->ItemFocused == i) && GetFocus() == hwnd)
	ListBoxDrawItem (combohwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_FOCUS, ODS_FOCUS);

      top += height;
      lphl->ItemsVisible++;
    }

    lpls = lpls->lpNext;
  }
  SetScrollRange(hwnd, SB_VERT, 0, ListMaxFirstVisible(lphl), TRUE);
  SelectObject(hdc,hOldFont);
  EndPaint( hwnd, &ps );
  return 0;

}

/***********************************************************************
 *           CBLKillFocus
 */
static LRESULT CBLKillFocus( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
/*  SendMessage(CLBoxGetCombo(hwnd),CB_SHOWDROPDOWN,0,0);*/
  return 0;
}

/***********************************************************************
 *           CBLActivate
 */
static LRESULT CBLActivate( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  if (wParam == WA_INACTIVE)
    SendMessage(CLBoxGetCombo(hwnd),CB_SHOWDROPDOWN,0,0);
  return 0;
}

/***********************************************************************
 *           CBLLButtonDown
 */
static LRESULT CBLLButtonDown( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  LPHEADLIST lphl = CLBoxGetListHeader(hwnd);
  int        y;
  RECT       rectsel;

  SetFocus(hwnd);
  SetCapture(hwnd);

  lphl->PrevFocused = lphl->ItemFocused;

  y = ListBoxFindMouse(lphl, LOWORD(lParam), HIWORD(lParam));
  if (y == -1)
    return 0;

  ListBoxSetCurSel(lphl, y);
  ListBoxGetItemRect(lphl, y, &rectsel);

  InvalidateRect(hwnd, NULL, TRUE);
  return 0;
}

/***********************************************************************
 *           CBLLButtonUp
 */
static LRESULT CBLLButtonUp( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  LPHEADLIST lphl = CLBoxGetListHeader(hwnd);

  if (GetCapture() == hwnd) ReleaseCapture();

  if(!lphl)
     {
      fprintf(stdnimp,"CBLLButtonUp: CLBoxGetListHeader returned NULL!\n");
     }
  else if (lphl->PrevFocused != lphl->ItemFocused) 
          {
      		SendMessage(CLBoxGetCombo(hwnd),CB_SETCURSEL,lphl->ItemFocused,0);
      		ListBoxSendNotification(lphl, CBN_SELCHANGE);
     	  }

  SendMessage(CLBoxGetCombo(hwnd),CB_SHOWDROPDOWN,0,0);

  return 0;
}

/***********************************************************************
 *           CBLMouseMove
 */
static LRESULT CBLMouseMove( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  LPHEADLIST lphl = CLBoxGetListHeader(hwnd);
  short y;
  WORD wRet;
  RECT rect, rectsel;

  y = SHIWORD(lParam);
  wRet = ListBoxFindMouse(lphl, LOWORD(lParam), HIWORD(lParam));
  ListBoxGetItemRect(lphl, wRet, &rectsel);
  GetClientRect(hwnd, &rect);

  dprintf_combo(stddeb,"CBLMouseMove: hwnd %04x wp %x lp %lx  y %d  if %d wret %d %d,%d-%d,%d\n",
hwnd,wParam,lParam,y,lphl->ItemFocused,wRet,rectsel.left,rectsel.top,rectsel.right,rectsel.bottom);
  
  if ((wParam & MK_LBUTTON) != 0) {
    if (y < CBLMM_EDGE) {
      if (lphl->FirstVisible > 0) {
	lphl->FirstVisible--;
	SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
	ListBoxSetCurSel(lphl, wRet);
	InvalidateRect(hwnd, NULL, TRUE);
	return 0;
      }
    }
    else if (y >= (rect.bottom-CBLMM_EDGE)) {
      if (lphl->FirstVisible < ListMaxFirstVisible(lphl)) {
	lphl->FirstVisible++;
	SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
	ListBoxSetCurSel(lphl, wRet);
	InvalidateRect(hwnd, NULL, TRUE);
	return 0;
      }
    }
    else {
      if ((short) wRet == lphl->ItemFocused) return 0;
      ListBoxSetCurSel(lphl, wRet);
      InvalidateRect(hwnd, NULL, TRUE);
    }
  }

  return 0;
}

/***********************************************************************
 *           CBLVScroll
 */
static LRESULT CBLVScroll( HWND hwnd, WPARAM wParam, LPARAM lParam )
{
  LPHEADLIST lphl = CLBoxGetListHeader(hwnd);
  int  y;

  y = lphl->FirstVisible;

  switch(wParam) {
  case SB_LINEUP:
    if (lphl->FirstVisible > 0)
      lphl->FirstVisible--;
    break;

  case SB_LINEDOWN:
    lphl->FirstVisible++;
    break;

  case SB_PAGEUP:
    if (lphl->FirstVisible > lphl->ItemsVisible) {
      lphl->FirstVisible -= lphl->ItemsVisible;
    } else {
      lphl->FirstVisible = 0;
    }
    break;

  case SB_PAGEDOWN:
    lphl->FirstVisible += lphl->ItemsVisible;
    break;

  case SB_THUMBTRACK:
    lphl->FirstVisible = LOWORD(lParam);
    break;
  }

  if (lphl->FirstVisible > ListMaxFirstVisible(lphl))
    lphl->FirstVisible = ListMaxFirstVisible(lphl);

  if (y != lphl->FirstVisible) {
    SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
    InvalidateRect(hwnd, NULL, TRUE);
  }

  return 0;
}

/***********************************************************************
 *           ComboLBoxWndProc
 */
LRESULT ComboLBoxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
    switch(message) {	
     case WM_CREATE: return CBLCreate(hwnd, wParam, lParam);
     case WM_GETDLGCODE: return CBLGetDlgCode(hwnd, wParam, lParam);
     case WM_KEYDOWN: return CBLKeyDown(hwnd, wParam, lParam);
     case WM_CHAR: return CBLChar(hwnd, wParam, lParam);
     case WM_PAINT: return CBLPaint(hwnd, wParam, lParam);
     case WM_KILLFOCUS: return CBLKillFocus(hwnd, wParam, lParam);
     case WM_ACTIVATE: return CBLActivate(hwnd, wParam, lParam);
     case WM_LBUTTONDOWN: return CBLLButtonDown(hwnd, wParam, lParam);
     case WM_LBUTTONUP: return CBLLButtonUp(hwnd, wParam, lParam);
     case WM_MOUSEMOVE: return CBLMouseMove(hwnd, wParam, lParam);
     case WM_VSCROLL: return CBLVScroll(hwnd, wParam, lParam);
    }
    return DefWindowProc(hwnd, message, wParam, lParam);
}

/************************************************************************
 * 			       	DlgDirSelectComboBox	[USER.194]
 */
BOOL DlgDirSelectComboBox(HWND hDlg, LPSTR lpStr, INT nIDLBox)
{
	fprintf(stdnimp,"DlgDirSelectComboBox(%04x, '%s', %d) \n",
				hDlg, lpStr, nIDLBox);
	return TRUE;
}


/************************************************************************
 * 					DlgDirListComboBox     [USER.195]
 */
INT DlgDirListComboBox( HWND hDlg, SEGPTR path, INT idCBox,
                        INT idStatic, UINT wType )
{
    INT ret = 0;

    dprintf_combo( stddeb,"DlgDirListComboBox(%04x,%08lx,%d,%d,%04X) \n",
                   hDlg, (DWORD)path, idCBox, idStatic, wType );

    if (idCBox)
    {
        SendDlgItemMessage( hDlg, idCBox, CB_RESETCONTENT, 0, 0 );
        ret = (SendDlgItemMessage( hDlg, idCBox, CB_DIR, wType, path ) >= 0);
    }
    if (idStatic)
    {
        char temp[256];
        int drive = DRIVE_GetCurrentDrive();
        strcpy( temp, "A:\\" );
        temp[0] += drive;
        lstrcpyn( temp+3, DRIVE_GetDosCwd(drive), 253 );
        SendDlgItemMessage( hDlg, idStatic, WM_SETTEXT,
                            0, (LPARAM)MAKE_SEGPTR(temp) );
    } 
    return ret;
}
