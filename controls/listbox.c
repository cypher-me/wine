/*
 * Listbox controls
 * 
 * Copyright  Martin Ayotte, 1993
 *            Constantine Sapuntzakis, 1995
 * 	      Alex Korobka, 1995 
 * 
 */

 /*
  * FIXME: 
  * - check if multi-column listboxes work
  * - implement more messages and styles 
  * - implement caret for LBS_EXTENDEDSEL
  */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include "windows.h"
#include "user.h"
#include "win.h"
#include "gdi.h"
#include "msdos.h"
#include "listbox.h"
#include "dos_fs.h"
#include "drive.h"
#include "file.h"
#include "stackframe.h"
#include "stddebug.h"
#include "debug.h"
#include "xmalloc.h"

#if 0
#define LIST_HEAP_ALLOC(lphl,f,size) ((int)HEAP_Alloc(&lphl->Heap,f,size) & 0xffff)
#define LIST_HEAP_FREE(lphl,handle) (HEAP_Free(&lphl->Heap,LIST_HEAP_ADDR(lphl,handle)))
#define LIST_HEAP_ADDR(lphl,handle) \
    ((void *)((handle) ? ((handle) | ((int)lphl->Heap & 0xffff0000)) : 0))
#else
/* FIXME: shouldn't each listbox have its own heap? */
#if 0
#define LIST_HEAP_ALLOC(lphl,f,size) USER_HEAP_ALLOC(size)
#define LIST_HEAP_FREE(lphl,handle)  USER_HEAP_FREE(handle)
#define LIST_HEAP_ADDR(lphl,handle)  USER_HEAP_LIN_ADDR(handle)
#define LIST_HEAP_SEG_ADDR(lphl,handle) USER_HEAP_SEG_ADDR(handle)
#else
/* Something like this maybe ? */
#define LIST_HEAP_ALLOC(lphl,f,size) \
            LOCAL_Alloc( lphl->HeapSel, LMEM_FIXED, (size) )
#if 0
#define LIST_HEAP_REALLOC(handle,size) \
            LOCAL_ReAlloc( USER_HeapSel, (handle), (size), LMEM_FIXED )
#endif
#define LIST_HEAP_FREE(lphl,handle) \
            LOCAL_Free( lphl->HeapSel, (handle) )
#define LIST_HEAP_ADDR(lphl,handle)  \
            ((handle) ? PTR_SEG_OFF_TO_LIN(lphl->HeapSel, (handle)) : NULL)
#define LIST_HEAP_SEG_ADDR(lphl,handle)  \
            ((handle) ? MAKELONG((handle), lphl->HeapSel) : 0)
#endif
#endif

#define LIST_HEAP_SIZE 0x10000

#define LBMM_EDGE   4    /* distance inside box which is same as moving mouse
			    outside box, to trigger scrolling of LB */

static void ListBoxInitialize(LPHEADLIST lphl)
{
  lphl->lpFirst        = NULL;
  lphl->ItemsCount     = 0;
  lphl->ItemsVisible   = 0;
  lphl->FirstVisible   = 0;
  lphl->ColumnsVisible = 1;
  lphl->ItemsPerColumn = 0;
  lphl->ItemFocused    = -1;
  lphl->PrevFocused    = -1;
}

void CreateListBoxStruct(HWND hwnd, WORD CtlType, LONG styles, HWND parent)
{
  LPHEADLIST lphl;
  HDC         hdc;

  lphl = (LPHEADLIST)xmalloc(sizeof(HEADLIST));
  SetWindowLong(hwnd, 0, (LONG)lphl);
  ListBoxInitialize(lphl);
  lphl->DrawCtlType    = CtlType;
  lphl->CtlID          = GetWindowWord(hwnd,GWW_ID);
  lphl->bRedrawFlag    = TRUE;
  lphl->iNumStops      = 0;
  lphl->TabStops       = NULL;
  lphl->hFont          = GetStockObject(SYSTEM_FONT);
  lphl->hSelf          = hwnd;  
  if (CtlType==ODT_COMBOBOX)              /* use the "faked" style for COMBOLBOX */
                                          /* LBS_SORT instead CBS_SORT e.g.      */
    lphl->dwStyle   = MAKELONG(LOWORD(styles),HIWORD(GetWindowLong(hwnd,GWL_STYLE)));
  else
    lphl->dwStyle   = GetWindowLong(hwnd,GWL_STYLE); /* use original style dword */
  lphl->hParent        = parent;
  lphl->StdItemHeight  = 15; /* FIXME: should get the font height */
  lphl->OwnerDrawn     = styles & (LBS_OWNERDRAWFIXED | LBS_OWNERDRAWVARIABLE);
  lphl->HasStrings     = (styles & LBS_HASSTRINGS) || !lphl->OwnerDrawn;

  /* create dummy hdc to set text height */
  if ((hdc = GetDC(0)))
  {
      TEXTMETRIC tm;
      GetTextMetrics( hdc, &tm );
      lphl->StdItemHeight = tm.tmHeight;
      dprintf_listbox(stddeb,"CreateListBoxStruct:  font height %d\n",
                      lphl->StdItemHeight);
      ReleaseDC( 0, hdc );
  }

  if (lphl->OwnerDrawn) {
    LISTSTRUCT dummyls;
    
    lphl->hDrawItemStruct = USER_HEAP_ALLOC(sizeof(DRAWITEMSTRUCT));
    lphl->needMeasure = TRUE;
    dummyls.mis.CtlType    = lphl->DrawCtlType;
    dummyls.mis.CtlID      = lphl->CtlID;
    dummyls.mis.itemID     = -1;
    dummyls.mis.itemWidth  = 0; /* ignored */
    dummyls.mis.itemData   = 0;

    ListBoxAskMeasure(lphl,&dummyls);
  } else {
    lphl->hDrawItemStruct = 0;
  }

#if 0
  HeapHandle = GlobalAlloc(GMEM_FIXED, LIST_HEAP_SIZE);
  HeapBase = GlobalLock(HeapHandle);
  HEAP_Init(&lphl->Heap, HeapBase, LIST_HEAP_SIZE);
#endif
/* WINELIBS list boxes do not operate on local heaps */
#ifndef WINELIB
  lphl->HeapSel = GlobalAlloc(GMEM_FIXED,LIST_HEAP_SIZE);
  LocalInit( lphl->HeapSel, 0, LIST_HEAP_SIZE-1);
#else
  lphl->HeapSel = 0;
#endif
}

void DestroyListBoxStruct(LPHEADLIST lphl)
{
  if (lphl->hDrawItemStruct)
    USER_HEAP_FREE(lphl->hDrawItemStruct);

  /* XXX need to free lphl->Heap */
  GlobalFree(lphl->HeapSel);
  free(lphl);
}

static LPHEADLIST ListBoxGetStorageHeader(HWND hwnd)
{
    return (LPHEADLIST)GetWindowLong(hwnd,0);
}

/* Send notification "code" as part of a WM_COMMAND-message if hwnd
   has the LBS_NOTIFY style */
void ListBoxSendNotification(LPHEADLIST lphl, WORD code)
{
  if (lphl->dwStyle & LBS_NOTIFY)
#ifdef WINELIB32
    SendMessage(lphl->hParent, WM_COMMAND,
		MAKEWPARAM(lphl->CtlID,code), (LPARAM)lphl->hSelf);
#else
    SendMessage(lphl->hParent, WM_COMMAND,
		lphl->CtlID, MAKELONG(lphl->hSelf, code));
#endif
}


/* get the maximum value of lphl->FirstVisible */
int ListMaxFirstVisible(LPHEADLIST lphl)
{
    int m = lphl->ItemsCount-lphl->ItemsVisible;
    return (m < 0) ? 0 : m;
}


void ListBoxUpdateWindow(HWND hwnd, LPHEADLIST lphl, BOOL repaint)
{
  SetScrollRange(hwnd, SB_VERT, 0, ListMaxFirstVisible(lphl), TRUE);
  if (lphl->ItemsPerColumn != 0) {
    SetScrollRange(hwnd, SB_HORZ, 1, lphl->ItemsVisible /
		   lphl->ItemsPerColumn + 1, TRUE);
  }
  if (repaint && lphl->bRedrawFlag) {
    InvalidateRect(hwnd, NULL, TRUE);
  }
}

/* Returns: 0 if nothing needs to be changed */
/*          1 if FirstVisible changed */

int ListBoxScrollToFocus(LPHEADLIST lphl)
{
  short       end;

  if (lphl->ItemsCount == 0) return 0;
  if (lphl->ItemFocused == -1) return 0;

  end = lphl->FirstVisible + lphl->ItemsVisible - 1;

  if (lphl->ItemFocused < lphl->FirstVisible ) {
    lphl->FirstVisible = lphl->ItemFocused;
    return 1;
  } else {
    if (lphl->ItemFocused > end) {
      WORD maxFirstVisible = ListMaxFirstVisible(lphl);

      lphl->FirstVisible = lphl->ItemFocused;
      
      if (lphl->FirstVisible > maxFirstVisible) {
	lphl->FirstVisible = maxFirstVisible;
      }
      return 1;
    }
  } 
  return 0;
}


LPLISTSTRUCT ListBoxGetItem(LPHEADLIST lphl, UINT uIndex)
{
  LPLISTSTRUCT lpls;
  UINT         Count = 0;

  if (uIndex >= lphl->ItemsCount) return NULL;

  lpls = lphl->lpFirst;
  while (Count++ < uIndex) lpls = lpls->lpNext;
  return lpls;
}


void ListBoxDrawItem (HWND hwnd, LPHEADLIST lphl, HDC hdc, LPLISTSTRUCT lpls, 
		      RECT *rect, WORD itemAction, WORD itemState)
{
  if (lphl->OwnerDrawn) {
    DRAWITEMSTRUCT   *dis = USER_HEAP_LIN_ADDR(lphl->hDrawItemStruct);

    dis->CtlID    = lpls->mis.CtlID;
    dis->CtlType  = lpls->mis.CtlType;
    dis->itemID   = lpls->mis.itemID;
    dis->hDC      = hdc;
    dis->hwndItem = hwnd;
    dis->itemData = lpls->mis.itemData;
    dis->itemAction = itemAction;
    dis->itemState  = itemState;
    dis->rcItem     = *rect;
    SendMessage(lphl->hParent, WM_DRAWITEM,
		0, (LPARAM)USER_HEAP_SEG_ADDR(lphl->hDrawItemStruct));
  } else {
    if (itemAction == ODA_DRAWENTIRE || itemAction == ODA_SELECT) {
      int 	OldBkMode;
      DWORD 	dwOldTextColor = 0;

      OldBkMode = SetBkMode(hdc, TRANSPARENT);

      if (itemState != 0) {
	dwOldTextColor = SetTextColor(hdc, 0x00FFFFFFL);
	FillRect(hdc, rect, GetStockObject(BLACK_BRUSH));
      }

      if (lphl->dwStyle & LBS_USETABSTOPS) {
	TabbedTextOut(hdc, rect->left + 5, rect->top + 2, 
		      (char *)lpls->itemText, strlen((char *)lpls->itemText), 
		      lphl->iNumStops, lphl->TabStops, 0);
      } else {
	TextOut(hdc, rect->left + 5, rect->top + 2,
		(char *)lpls->itemText, strlen((char *)lpls->itemText));
      }

      if (itemState != 0) {
	SetTextColor(hdc, dwOldTextColor);
      }
      
      SetBkMode(hdc, OldBkMode);
    } else DrawFocusRect(hdc, rect);
  }

  return;
}


int ListBoxFindMouse(LPHEADLIST lphl, int X, int Y)
{
  LPLISTSTRUCT lpls = lphl->lpFirst;
  int          i, j;
  POINT        point;
  
  point.x = X; point.y = Y;
  if (lphl->ItemsCount == 0) return LB_ERR;

  for(i = 0; i < lphl->FirstVisible; i++) {
    if (lpls == NULL) return LB_ERR;
    lpls = lpls->lpNext;
  }
  for(j = 0; j < lphl->ItemsVisible; i++, j++) {
    if (lpls == NULL) return LB_ERR;
    if (PtInRect(&lpls->itemRect,point)) {
      return i;
    }
    lpls = lpls->lpNext;
  }
  dprintf_listbox(stddeb,"ListBoxFindMouse: not found\n");
  return LB_ERR;
}


void ListBoxAskMeasure(LPHEADLIST lphl, LPLISTSTRUCT lpls)  
{
  HANDLE hTemp = USER_HEAP_ALLOC( sizeof(MEASUREITEMSTRUCT) );
  MEASUREITEMSTRUCT *lpmeasure = (MEASUREITEMSTRUCT *) USER_HEAP_LIN_ADDR(hTemp);

  if (lpmeasure == NULL) {
    fprintf(stdnimp,"ListBoxAskMeasure() out of memory !\n");
    return;
  }
 
  *lpmeasure = lpls->mis;
  lpmeasure->itemHeight = lphl->StdItemHeight;
  SendMessage(lphl->hParent, WM_MEASUREITEM, 0, (LPARAM)USER_HEAP_SEG_ADDR(hTemp));

  if (lphl->dwStyle & LBS_OWNERDRAWFIXED) {
    lphl->StdItemHeight = lpmeasure->itemHeight;
    lpls->mis.itemHeight = lpmeasure->itemHeight;
  }

  USER_HEAP_FREE(hTemp);			
}

/* -------------------- strings and item data ---------------------- */

LPLISTSTRUCT ListBoxCreateItem(LPHEADLIST lphl, int id)
{
  LPLISTSTRUCT lplsnew = (LPLISTSTRUCT)malloc(sizeof(LISTSTRUCT));

  if (lplsnew == NULL) return NULL;
  
  lplsnew->itemState      = 0;
  lplsnew->mis.CtlType    = lphl->DrawCtlType;
  lplsnew->mis.CtlID      = lphl->CtlID;
  lplsnew->mis.itemID     = id;
  lplsnew->mis.itemHeight = lphl->StdItemHeight;
  lplsnew->mis.itemWidth  = 0; /* ignored */
  lplsnew->mis.itemData   = 0;
  SetRect(&lplsnew->itemRect, 0, 0, 0, 0);

  return lplsnew;
}


int ListBoxInsertString(LPHEADLIST lphl, UINT uIndex, LPCSTR newstr)
{
  LPLISTSTRUCT *lppls, lplsnew, lpls;
  HANDLE       hStr;
  LPSTR	str;
  UINT	Count;
    
  dprintf_listbox(stddeb,"ListBoxInsertString(%d, %p);\n", uIndex, newstr);
    
  if (!newstr) return -1;

  if (uIndex == (UINT)-1)
    uIndex = lphl->ItemsCount;

  lppls = &lphl->lpFirst;
  for(Count = 0; Count < uIndex; Count++) {
    if (*lppls == NULL) return LB_ERR;
    lppls = (LPLISTSTRUCT *) &(*lppls)->lpNext;
  }
    
  lplsnew = ListBoxCreateItem(lphl, Count);
  
  if (lplsnew == NULL) {
    fprintf(stdnimp,"ListBoxInsertString() out of memory !\n");
    return LB_ERRSPACE;
  }

  lplsnew->lpNext = *lppls;
  *lppls = lplsnew;
  lphl->ItemsCount++;
  
  hStr = 0;
  if (lphl->HasStrings) {
    dprintf_listbox(stddeb,"  string: %s\n", newstr);
    hStr = LIST_HEAP_ALLOC(lphl, LMEM_MOVEABLE, strlen(newstr) + 1);
    str = (LPSTR)LIST_HEAP_ADDR(lphl, hStr);
    if (str == NULL) return LB_ERRSPACE;
    strcpy(str, newstr);
    lplsnew->itemText = str;
    /* I'm not so sure about the next one */
    lplsnew->mis.itemData = 0;
  } else {
    lplsnew->itemText = NULL;
    lplsnew->mis.itemData = (DWORD)newstr;
  }

  lplsnew->mis.itemID = uIndex;
  lplsnew->hData = hStr;
  
  /* adjust the itemID field of the following entries */
  for(lpls = lplsnew->lpNext; lpls != NULL; lpls = lpls->lpNext) {
      lpls->mis.itemID++;
  }
 
  if (lphl->needMeasure) {
    ListBoxAskMeasure(lphl, lplsnew);
  }

  dprintf_listbox(stddeb,"ListBoxInsertString // count=%d\n", lphl->ItemsCount);
  return uIndex;
}


int ListBoxAddString(LPHEADLIST lphl, LPCSTR newstr)
{
    UINT pos = (UINT) -1;
    
    if (lphl->HasStrings && (lphl->dwStyle & LBS_SORT)) {
	LPLISTSTRUCT lpls = lphl->lpFirst;
	for (pos = 0; lpls != NULL; lpls = lpls->lpNext, pos++)
	    if (strcmp(lpls->itemText, newstr) >= 0)
		break;
    }
    return ListBoxInsertString(lphl, pos, newstr);
}


int ListBoxGetText(LPHEADLIST lphl, UINT uIndex, LPSTR OutStr)
{
  LPLISTSTRUCT lpls;

  if (!OutStr) {
    dprintf_listbox(stddeb, "ListBoxGetText // OutStr==NULL\n");
    return 0;
  }
  *OutStr = '\0';
  lpls = ListBoxGetItem (lphl, uIndex);
  if (lpls == NULL) return LB_ERR;

  if (!lphl->HasStrings) {
    *((long *)OutStr) = lpls->mis.itemData;
    return 4;
  }
	
  strcpy(OutStr, lpls->itemText);
  return strlen(OutStr);
}


DWORD ListBoxGetItemData(LPHEADLIST lphl, UINT uIndex)
{
  LPLISTSTRUCT lpls;

  lpls = ListBoxGetItem (lphl, uIndex);
  if (lpls == NULL) return LB_ERR;
  return lpls->mis.itemData;
}


int ListBoxSetItemData(LPHEADLIST lphl, UINT uIndex, DWORD ItemData)
{
  LPLISTSTRUCT lpls = ListBoxGetItem(lphl, uIndex);

  if (lpls == NULL) return LB_ERR;
  lpls->mis.itemData = ItemData;
  return 1;
}


int ListBoxDeleteString(LPHEADLIST lphl, UINT uIndex)
{
  LPLISTSTRUCT lpls, lpls2;
  UINT	Count;

  if (uIndex >= lphl->ItemsCount) return LB_ERR;

  lpls = lphl->lpFirst;
  if (lpls == NULL) return LB_ERR;

  if (uIndex == 0)
    lphl->lpFirst = lpls->lpNext;
  else {
    LPLISTSTRUCT lpls2 = NULL;
    for(Count = 0; Count < uIndex; Count++) {
      if (lpls->lpNext == NULL) return LB_ERR;

      lpls2 = lpls;
      lpls = (LPLISTSTRUCT)lpls->lpNext;
    }
    lpls2->lpNext = lpls->lpNext;
  }

  /* adjust the itemID field of the following entries */
  for(lpls2 = lpls->lpNext; lpls2 != NULL; lpls2 = lpls2->lpNext) {
      lpls2->mis.itemID--;
  }
 
  lphl->ItemsCount--;

  if (lpls->hData != 0) LIST_HEAP_FREE(lphl, lpls->hData);
  free(lpls);
  
  return lphl->ItemsCount;
}


int ListBoxFindString(LPHEADLIST lphl, UINT nFirst, SEGPTR MatchStr)
{
  LPLISTSTRUCT lpls;
  UINT	       Count;
  UINT         First = nFirst + 1;
  LPSTR        lpMatchStr = (LPSTR)MatchStr;

  if (First > lphl->ItemsCount) return LB_ERR;

  if (lphl->HasStrings) lpMatchStr = PTR_SEG_TO_LIN(MatchStr);
  
  lpls = ListBoxGetItem(lphl, First);
  Count = 0;
  while(lpls != NULL) {
    if (lphl->HasStrings) {
      if (strstr(lpls->itemText, lpMatchStr) == lpls->itemText) return Count;
    } else if (lphl->dwStyle & LBS_SORT) {
      /* XXX Do a compare item */
    }
    else
      if (lpls->mis.itemData == (DWORD)lpMatchStr) return Count;

    lpls = lpls->lpNext;
    Count++;
  }

  /* Start over at top */
  Count = 0;
  lpls = lphl->lpFirst;

  while (Count < First) {
    if (lphl->HasStrings) {
      if (strstr(lpls->itemText, lpMatchStr) == lpls->itemText) return Count;
    } else if (lphl->dwStyle & LBS_SORT) {
      /* XXX Do a compare item */
    } else {
      if (lpls->mis.itemData == (DWORD)lpMatchStr) return Count;
    }
    lpls = lpls->lpNext;
    Count++;
  }

  return LB_ERR;
}


int ListBoxResetContent(LPHEADLIST lphl)
{
    LPLISTSTRUCT lpls;
    int i;

    if (lphl->ItemsCount == 0) return 0;

    dprintf_listbox(stddeb, "ListBoxResetContent // ItemCount = %d\n",
	lphl->ItemsCount);

    for(i = 0; i < lphl->ItemsCount; i++) {
      lpls = lphl->lpFirst;
      if (lpls == NULL) return LB_ERR;
      
      lphl->lpFirst = lpls->lpNext;
      if (lpls->hData != 0) LIST_HEAP_FREE(lphl, lpls->hData);
      free(lpls);
    }
    ListBoxInitialize(lphl);

    return TRUE;
}

/* --------------------- selection ------------------------- */

int ListBoxSetCurSel(LPHEADLIST lphl, WORD wIndex)
{
  LPLISTSTRUCT lpls;

  /* use ListBoxSetSel instead */
  if (lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL) ) return 0;

  /* unselect previous item */
  if (lphl->ItemFocused != -1) {
    lphl->PrevFocused = lphl->ItemFocused;
    lpls = ListBoxGetItem(lphl, lphl->ItemFocused);
    if (lpls == 0) return LB_ERR;
    lpls->itemState = 0;
  }

  if (wIndex != (UINT)-1) {
    lphl->ItemFocused = wIndex;
    lpls = ListBoxGetItem(lphl, wIndex);
    if (lpls == 0) return LB_ERR;
    lpls->itemState = ODS_SELECTED | ODS_FOCUS;

    return 0;
  }

  return LB_ERR;
}


int ListBoxSetSel(LPHEADLIST lphl, WORD wIndex, WORD state)
{
  LPLISTSTRUCT  lpls;
  int           n = 0;

  if (!(lphl->dwStyle &  (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)  )) 
        return LB_ERR;

  if (wIndex == (UINT)-1) {
    for (lpls = lphl->lpFirst; lpls != NULL; lpls = lpls->lpNext) {
      if( lpls->itemState & ODS_SELECTED) n++;
      lpls->itemState = state? lpls->itemState |  ODS_SELECTED
                             : lpls->itemState & ~ODS_SELECTED;
    }
    return n;
  }

  if (wIndex >= lphl->ItemsCount) return LB_ERR;

  lpls = ListBoxGetItem(lphl, wIndex);
  lpls->itemState = state? lpls->itemState |  ODS_SELECTED
                         : lpls->itemState & ~ODS_SELECTED;

  return 0;
}


int ListBoxGetSel(LPHEADLIST lphl, WORD wIndex)
{
  LPLISTSTRUCT lpls = ListBoxGetItem(lphl, wIndex);

  if (lpls == NULL) return LB_ERR;
  return lpls->itemState & ODS_SELECTED;
}

/* ------------------------- dir listing ------------------------ */

LONG ListBoxDirectory(LPHEADLIST lphl, UINT attrib, LPCSTR filespec)
{
    char temp[16], mask[13];
    char *path, *p;
    const char *ptr;
    int skip, count;
    LONG ret;
    DOS_DIRENT entry;

    dprintf_listbox(stddeb, "ListBoxDirectory: '%s' %04x\n", filespec, attrib);
    if (!filespec) return LB_ERR;
    if (!(ptr = DOSFS_GetUnixFileName( filespec, FALSE ))) return LB_ERR;
    path = xstrdup(ptr);
    p = strrchr( path, '/' );
    *p++ = '\0';
    if (!(ptr = DOSFS_ToDosFCBFormat( p )))
    {
        free( path );
        return LB_ERR;
    }
    strcpy( mask, ptr );

    dprintf_listbox(stddeb, "ListBoxDirectory: path=%s mask=%s\n", path, mask);

    skip = ret = 0;
    attrib &= ~FA_LABEL;
    while ((count = DOSFS_FindNext( path, mask, 0, attrib, skip, &entry )) > 0)
    {
        skip += count;
        if (entry.attr & FA_DIRECTORY)
        {
            if ((attrib & DDL_DIRECTORY) && strcmp(entry.name, ".          "))
            {
                sprintf(temp, "[%s]", DOSFS_ToDosDTAFormat( entry.name ) );
                AnsiLower( temp );
                if ((ret = ListBoxAddString(lphl, temp)) == LB_ERR) break;
            }
        }
        else  /* not a directory */
        {
            if (!(attrib & DDL_EXCLUSIVE) ||
                ((attrib & (FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_ARCHIVE)) ==
                 (entry.attr & (FA_RDONLY|FA_HIDDEN|FA_SYSTEM|FA_ARCHIVE))))
            {
                strcpy( temp, DOSFS_ToDosDTAFormat( entry.name ) );
                AnsiLower( temp );
                if ((ret = ListBoxAddString(lphl, temp)) == LB_ERR) break;
            }
        }
    }
    if (attrib & DDL_DRIVES)
    {
        int x;
        strcpy( temp, "[-a-]" );
        for (x = 0; x < MAX_DOS_DRIVES; x++, temp[2]++)
        {
            if (DRIVE_IsValid(x))
                if ((ret = ListBoxAddString(lphl, temp)) == LB_ERR) break;
        }
    }
    free( path );
    return ret;
}

/* ------------------------- dimensions ------------------------- */

int ListBoxGetItemRect(LPHEADLIST lphl, WORD wIndex, LPRECT lprect)
{
  LPLISTSTRUCT lpls = ListBoxGetItem(lphl,wIndex);

  if (lpls == NULL) return LB_ERR;
  *lprect = lpls->itemRect;
  return 0;
}


int ListBoxSetItemHeight(LPHEADLIST lphl, WORD wIndex, long height)
{
  LPLISTSTRUCT lpls;

  if (!(lphl->dwStyle & LBS_OWNERDRAWVARIABLE)) {
    lphl->StdItemHeight = (short)height;
    return 0;
  }
  
  lpls = ListBoxGetItem(lphl, wIndex);
  if (lpls == NULL) return LB_ERR;
  
  lpls->mis.itemHeight = height;
  return 0;
}

/* -------------------------- string search ------------------------ */  

int ListBoxFindNextMatch(LPHEADLIST lphl, WORD wChar)
{
  LPLISTSTRUCT lpls;
  UINT	       count,first;

  if ((char)wChar < ' ') return LB_ERR;
  if (!lphl->HasStrings) return LB_ERR;

  lpls = lphl->lpFirst;
  
  for (count = 0; lpls != NULL; lpls = lpls->lpNext, count++) {
    if (tolower(*lpls->itemText) == tolower((char)wChar)) break;
  }
  if (lpls == NULL) return LB_ERR;
  first = count;
  for(; lpls != NULL; lpls = lpls->lpNext, count++) {
    if (*lpls->itemText != (char)wChar) 
      break;
    if ((short) count > lphl->ItemFocused)
      return count;
  }
  return first;
}

/***********************************************************************
 *           LBCreate
 */
static LONG LBCreate(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST   lphl;
  LONG	       dwStyle = GetWindowLong(hwnd,GWL_STYLE);
  RECT rect;

  CreateListBoxStruct(hwnd, ODT_LISTBOX, dwStyle, GetParent(hwnd));
  lphl = ListBoxGetStorageHeader(hwnd);
  dprintf_listbox(stddeb,"ListBox created: lphl = %p dwStyle = %04x:%04x\n", 
			  lphl, HIWORD(dwStyle), LOWORD(dwStyle));

  GetClientRect(hwnd,&rect);
  lphl->ColumnsWidth = rect.right - rect.left;

  SetScrollRange(hwnd, SB_VERT, 0, ListMaxFirstVisible(lphl), TRUE);
  SetScrollRange(hwnd, SB_HORZ, 1, 1, TRUE);

  return 0;
}


/***********************************************************************
 *           LBDestroy
 */
static LONG LBDestroy(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  ListBoxResetContent(lphl);

  DestroyListBoxStruct(lphl);
  dprintf_listbox(stddeb,"ListBox destroyed: lphl = %p\n",lphl);
  return 0;
}

/***********************************************************************
 *           LBVScroll
 */
static LONG LBVScroll(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  int  y;

  dprintf_listbox(stddeb,"ListBox WM_VSCROLL w=%04X l=%08lX !\n",
		  wParam, lParam);
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
 *           LBHScroll
 */
static LONG LBHScroll(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl;
  int        y;

  dprintf_listbox(stddeb,"ListBox WM_HSCROLL w=%04X l=%08lX !\n",
		  wParam, lParam);
  lphl = ListBoxGetStorageHeader(hwnd);
  y = lphl->FirstVisible;
  switch(wParam) {
  case SB_LINEUP:
    if (lphl->FirstVisible > lphl->ItemsPerColumn) {
      lphl->FirstVisible -= lphl->ItemsPerColumn;
    } else {
      lphl->FirstVisible = 0;
    }
    break;
  case SB_LINEDOWN:
    lphl->FirstVisible += lphl->ItemsPerColumn;
    break;
  case SB_PAGEUP:
    if (lphl->ItemsPerColumn != 0) {
      int lbsub = lphl->ItemsVisible / lphl->ItemsPerColumn * lphl->ItemsPerColumn;
      if (lphl->FirstVisible > lbsub) {
	lphl->FirstVisible -= lbsub;
      } else {
	lphl->FirstVisible = 0;
      }
    }
    break;
  case SB_PAGEDOWN:
    if (lphl->ItemsPerColumn != 0)
      lphl->FirstVisible += lphl->ItemsVisible /
	lphl->ItemsPerColumn * lphl->ItemsPerColumn;
    break;
  case SB_THUMBTRACK:
    lphl->FirstVisible = lphl->ItemsPerColumn * LOWORD(lParam);
    break;
  } 
  if (lphl->FirstVisible > ListMaxFirstVisible(lphl))
    lphl->FirstVisible = ListMaxFirstVisible(lphl);

  if (lphl->ItemsPerColumn != 0) {
    lphl->FirstVisible = lphl->FirstVisible /
      lphl->ItemsPerColumn * lphl->ItemsPerColumn + 1;
    if (y != lphl->FirstVisible) {
      SetScrollPos(hwnd, SB_HORZ, lphl->FirstVisible / 
		   lphl->ItemsPerColumn + 1, TRUE);
      InvalidateRect(hwnd, NULL, TRUE);
    }
  }
  return 0;
}

/***********************************************************************
 *           LBLButtonDown
 */
static LONG LBLButtonDown(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  WORD       wRet;
  int        y,n;
  RECT       rectsel;
  POINT      tmpPOINT;
  tmpPOINT.x = LOWORD(lParam); tmpPOINT.y = HIWORD(lParam);

  SetFocus(hwnd);
  SetCapture(hwnd);

  lphl->PrevFocused = lphl->ItemFocused;

  y = ListBoxFindMouse(lphl, LOWORD(lParam), HIWORD(lParam));

  if (y == -1) return 0;

  if (lphl->dwStyle & LBS_NOTIFY && y!= LB_ERR )
     if( SendMessage(lphl->hParent, WM_LBTRACKPOINT, y, lParam) )
         return 0;


  switch( lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL) )
   {
        case LBS_MULTIPLESEL:
                lphl->ItemFocused = y;
                wRet = ListBoxGetSel(lphl, y);
                ListBoxSetSel(lphl, y, !wRet);
                break;
        case LBS_EXTENDEDSEL:
                /* should handle extended mode here and in kbd handler 
                 */ 

                if ( lphl->PrevFocused != y && y!= LB_ERR)
                 {
                   LPLISTSTRUCT lpls = ListBoxGetItem( lphl, lphl->ItemFocused = y );
                   n = ListBoxSetSel(lphl,-1,FALSE);

                   lpls->itemState = ODS_FOCUS | ODS_SELECTED;

                   if( n > 1 && n != LB_ERR )
                     InvalidateRect(hwnd,NULL,TRUE);
                 }
                else
                       return 0;

                break;
        case 0:
                if( y!=lphl->ItemFocused )
                  ListBoxSetCurSel(lphl, y);
                else
                  return 0;
                break;
        default:
                fprintf(stdnimp,"Listbox: LBS_MULTIPLESEL and LBS_EXTENDEDSEL are on!\n");
                return 0;
   }

 /* invalidate changed items */
 if( lphl->dwStyle & LBS_MULTIPLESEL || y!=lphl->PrevFocused )
   {
     ListBoxGetItemRect(lphl, y, &rectsel);
     InvalidateRect(hwnd, &rectsel, TRUE);
   }
 if( lphl->PrevFocused!=-1 && y!=lphl->PrevFocused ) 
   {
     ListBoxGetItemRect(lphl, lphl->PrevFocused, &rectsel);
     InvalidateRect(hwnd, &rectsel, TRUE);
   }

#ifndef WINELIB
  if (GetWindowLong(lphl->hSelf,GWL_EXSTYLE) & WS_EX_DRAGDETECT)
     if( DragDetect(lphl->hSelf,tmpPOINT) )
         SendMessage(lphl->hParent, WM_BEGINDRAG,0,0L);
#endif
  return 0;
}

/***********************************************************************
 *           LBLButtonUp
 */
static LONG LBLButtonUp(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  if (GetCapture() == hwnd) ReleaseCapture();

  if (lphl->PrevFocused != lphl->ItemFocused)
    ListBoxSendNotification(lphl, LBN_SELCHANGE);

  return 0;
}

/***********************************************************************
 *           LBRButtonUp
 */
static LONG LBRButtonUp(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

#ifdef WINELIB32
  SendMessage(lphl->hParent, WM_COMMAND, 
	      MAKEWPARAM(GetWindowWord(hwnd,GWW_ID),LBN_DBLCLK),
	      (LPARAM)hwnd);
#else
  SendMessage(lphl->hParent, WM_COMMAND, GetWindowWord(hwnd,GWW_ID),
		MAKELONG(hwnd, LBN_DBLCLK));
#endif

  return 0;
}

/***********************************************************************
 *           LBMouseMove
 */
static LONG LBMouseMove(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  int  y,redraw_prev = 0;
  int  iRet;
  RECT rect, rectsel;   /* XXX Broken */

  dprintf_listbox(stddeb,"LBMouseMove %d %d\n",SLOWORD(lParam),SHIWORD(lParam));
  if ((wParam & MK_LBUTTON) != 0) {
    y = SHIWORD(lParam);
    if (y < LBMM_EDGE) {
      if (lphl->FirstVisible > 0) {
	lphl->FirstVisible--;
	SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
	InvalidateRect(hwnd, NULL, TRUE);
	return 0;
      }
    }
    GetClientRect(hwnd, &rect);
    if (y >= (rect.bottom-LBMM_EDGE)) {
      if (lphl->FirstVisible < ListMaxFirstVisible(lphl)) {
	lphl->FirstVisible++;
	SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
	InvalidateRect(hwnd, NULL, TRUE);
	return 0;
      }
      }
    if ((y > 0) && (y < (rect.bottom - LBMM_EDGE))) {
      if ((y < rectsel.top) || (y > rectsel.bottom)) {
        iRet = ListBoxFindMouse(lphl, LOWORD(lParam), HIWORD(lParam));
        if (iRet == lphl->ItemFocused || iRet == -1)  {
	  return 0;
	}
	if (lphl->dwStyle & LBS_MULTIPLESEL) {
          lphl->ItemFocused = iRet;
	  ListBoxSendNotification(lphl, LBN_SELCHANGE);
        } else if ( lphl->dwStyle & LBS_EXTENDEDSEL )
                  {
                     /* Fixme: extended selection mode */
                     ListBoxSetSel( lphl, lphl->ItemFocused, 0);
                     lphl->PrevFocused = lphl->ItemFocused;
                     lphl->ItemFocused = iRet;
                     ListBoxSetSel( lphl, iRet, TRUE);
                     redraw_prev = 1;
                  }
               else
                  {
                     ListBoxSetCurSel(lphl, (WORD)iRet);
                     redraw_prev = 1; 
                  }
        if( lphl->PrevFocused!=-1 && redraw_prev )
          {
            ListBoxGetItemRect(lphl, lphl->PrevFocused, &rectsel);
            InvalidateRect(hwnd, &rectsel, TRUE);
          }
        ListBoxGetItemRect(lphl, iRet, &rectsel);
	InvalidateRect(hwnd, &rectsel, TRUE);
      }
    }
  }

  return 0;
  }

/***********************************************************************
 *           LBKeyDown
 *
 * Doesn't yet handle properly VK_SHIFT with LB_EXTENDEDSEL
 */
static LONG LBKeyDown(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  WORD       newFocused = 0xFFFF;
  RECT	     rect;

  ListBoxGetItemRect(lphl,lphl->ItemFocused,&rect);
  switch(wParam) 
    {
	case VK_HOME:
	case VK_END:
	case VK_LEFT:
	case VK_RIGHT:
	case VK_UP:
	case VK_DOWN:
	case VK_PRIOR:
	case VK_NEXT:
	     if ( lphl->dwStyle & LBS_WANTKEYBOARDINPUT )
	        {
		  newFocused = (WORD)(INT)SendMessage(lphl->hParent,WM_VKEYTOITEM,
					              wParam,MAKELPARAM(lphl->ItemFocused,hwnd));
	          if ( newFocused == 0xFFFE ) return 0L;
                }
	     if ( newFocused == 0xFFFF ) 
		{
		  newFocused = lphl->ItemFocused;

		  /* nested switch */
		  switch(wParam)
		    {
                        case VK_HOME:
                          newFocused = 0;
                          break;
                        case VK_END:
                          newFocused = lphl->ItemsCount - 1;
                          break;
                        case VK_LEFT:
  			  if (lphl->dwStyle & LBS_MULTICOLUMN) {
                            if (newFocused >= lphl->ItemsPerColumn) {
                                newFocused -= lphl->ItemsPerColumn;
                            } else {
                                newFocused = 0;
                            }
                          }
                          break;
                        case VK_UP:
                          if (newFocused > 0) newFocused--;
                          break;
                        case VK_RIGHT:
                          if (lphl->dwStyle & LBS_MULTICOLUMN) 
                             newFocused += lphl->ItemsPerColumn;
                          break;
                        case VK_DOWN:
                          newFocused++;
                          break;
                        case VK_PRIOR:
                          if (newFocused > lphl->ItemsVisible)
                              newFocused -= lphl->ItemsVisible;
			    else  newFocused = 0;
                            break;
                        case VK_NEXT:
                          newFocused += lphl->ItemsVisible;
                          break;
                        default:
                          return 0;
                    }
		  /* end of nested switch */
		}
	     break;   
	case VK_SPACE:
             if (lphl->dwStyle & LBS_MULTIPLESEL)
                {
                 WORD wRet = ListBoxGetSel(lphl, lphl->ItemFocused);
                 ListBoxSetSel(lphl, lphl->ItemFocused, !wRet);
                 }
             return 0;

        /* chars are handled in LBChar */
	default:
	     return 0;
    }

  /* at this point newFocused is set up */

  if (newFocused >= lphl->ItemsCount)
    newFocused = lphl->ItemsCount - 1;
  
  if (!(lphl->dwStyle & LBS_MULTIPLESEL)) 
     {
        ListBoxSetCurSel(lphl, newFocused);
	ListBoxSendNotification(lphl, LBN_SELCHANGE);
     }

  lphl->ItemFocused = newFocused;

  if( ListBoxScrollToFocus(lphl) || (lphl->dwStyle & 
                          (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)) )
  InvalidateRect(hwnd, NULL, TRUE);
  else
    {
	InvalidateRect(hwnd, &rect, TRUE);
	if( newFocused < 0x8000 )
          {
	   ListBoxGetItemRect(lphl, newFocused, &rect);
	   InvalidateRect(hwnd, &rect, TRUE);
          }
    }

  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);

  return 0;
}

/***********************************************************************
 *           LBChar
 */
static LONG LBChar(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  WORD       newFocused = 0xFFFF;

  if ( (lphl->dwStyle & LBS_WANTKEYBOARDINPUT) && !(lphl->HasStrings))
       {
        newFocused = (WORD)(INT)SendMessage(lphl->hParent,WM_CHARTOITEM,
                                            wParam,MAKELPARAM(lphl->ItemFocused,hwnd));
        if ( newFocused == 0xFFFE ) return 0L;
       }

  if (newFocused == 0xFFFF ) 
  newFocused = ListBoxFindNextMatch(lphl, wParam);

  if (newFocused == (WORD)LB_ERR) return 0;

  if (newFocused >= lphl->ItemsCount)
    newFocused = lphl->ItemsCount - 1;

  if (!(lphl->dwStyle & LBS_MULTIPLESEL)) 
     {
    ListBoxSetCurSel(lphl, newFocused);
	ListBoxSendNotification(lphl, LBN_SELCHANGE);
  }

  lphl->ItemFocused = newFocused;
  ListBoxScrollToFocus(lphl);
  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);

  InvalidateRect(hwnd, NULL, TRUE);

  return 0;
}

/***********************************************************************
 *           LBSetRedraw
 */
static LONG LBSetRedraw(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST  lphl = ListBoxGetStorageHeader(hwnd);

  dprintf_listbox(stddeb,"ListBox WM_SETREDRAW hWnd=%04x w=%04x !\n",
		  hwnd, wParam);
  lphl->bRedrawFlag = wParam;

  return 0;
}

/***********************************************************************
 *           LBSetFont
 */
static LONG LBSetFont(HWND hwnd, WPARAM wParam, LPARAM lParam)
{
  LPHEADLIST  lphl = ListBoxGetStorageHeader(hwnd);
  HDC hdc;

  if (wParam == 0)
    lphl->hFont = GetStockObject(SYSTEM_FONT);
  else
    lphl->hFont = (HFONT) wParam;

  /* a new font means possible new text height */
  /* does this mean the height of each entry must be separately changed? */
  /* or are we guaranteed to get a LBSetFont before the first insert/add? */
  if ((hdc = GetDC(0)))
  {
      TEXTMETRIC tm;
      GetTextMetrics( hdc, &tm );
      lphl->StdItemHeight = tm.tmHeight;
      dprintf_listbox(stddeb,"LBSetFont:  new font %d with height %d\n",
                      lphl->hFont, lphl->StdItemHeight);
      ReleaseDC( 0, hdc );
  }

  return 0;
}

/***********************************************************************
 *           LBPaint
 */
static LONG LBPaint(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST   lphl = ListBoxGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls;
  PAINTSTRUCT  ps;
  HBRUSH       hBrush;
  HFONT        hOldFont;
  HDC   hdc    = BeginPaint( hwnd, &ps );
  DC    *dc    = (DC *)GDI_GetObjPtr(hdc, DC_MAGIC);
  RECT  rect, paintRect, scratchRect;
  int   i, top, height, maxwidth, ipc;


  top = 0;

  if (!IsWindowVisible(hwnd) || !lphl->bRedrawFlag) {
    EndPaint(hwnd, &ps);
    return 0;
  }

  GetRgnBox(dc->w.hGCClipRgn,&paintRect);
  GetClientRect(hwnd, &rect);
  IntersectRect(&paintRect,&rect,&paintRect);

  hOldFont = SelectObject(hdc, lphl->hFont);

#ifdef WINELIB32
  hBrush = (HBRUSH) SendMessage(lphl->hParent, WM_CTLCOLORLISTBOX, (WPARAM)hdc,
				(LPARAM)hwnd);
#else
  hBrush = SendMessage(lphl->hParent, WM_CTLCOLOR, hdc,
		       MAKELONG(hwnd, CTLCOLOR_LISTBOX));
#endif

  if (hBrush == 0) hBrush = GetStockObject(WHITE_BRUSH);

  FillRect(hdc, &rect, hBrush);

  maxwidth = rect.right;
  if (lphl->dwStyle & LBS_MULTICOLUMN) {
    rect.right = lphl->ColumnsWidth;
  }
  lpls = lphl->lpFirst;

  lphl->ItemsVisible = 0;
  lphl->ItemsPerColumn = ipc = 0;

  for(i = 0; i < lphl->ItemsCount; i++) {
    if (lpls == NULL) break;

    if (i >= lphl->FirstVisible) {
      height = lpls->mis.itemHeight;

      if (top > (rect.bottom-height+1)) {
	if (lphl->dwStyle & LBS_MULTICOLUMN) {
	  lphl->ItemsPerColumn = MAX(lphl->ItemsPerColumn, ipc);
	  ipc = 0;
	  top = 0;
	  rect.left += lphl->ColumnsWidth;
	  rect.right += lphl->ColumnsWidth;
	  if (rect.left > maxwidth) break;
	} else {
	  break;
	}
      }

      lpls->itemRect.top    = top;
      lpls->itemRect.bottom = top + height;
      lpls->itemRect.left   = rect.left;
      lpls->itemRect.right  = rect.right;

      if( IntersectRect(&scratchRect,&paintRect,&lpls->itemRect) )
       {
        dprintf_listbox(stddeb,"LBPaint: drawing item: %d %d %d %d %d\n",
                        rect.left,top,rect.right,top+height,lpls->itemState);

        if (lphl->OwnerDrawn && (lphl->ItemFocused == i) && GetFocus() == hwnd)
           {
             ListBoxDrawItem (hwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_FOCUS, 
                                                      lpls->itemState & ~ODS_FOCUS);
             ListBoxDrawItem (hwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_DRAWENTIRE, 
                                                      lpls->itemState & ~ODS_FOCUS);
             ListBoxDrawItem (hwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_FOCUS, lpls->itemState);
           }
        else
            ListBoxDrawItem (hwnd, lphl, hdc, lpls, &lpls->itemRect, ODA_DRAWENTIRE,
                                                     lpls->itemState);
       }

      top += height;
      lphl->ItemsVisible++;
      ipc++;
    }

    lpls = lpls->lpNext;
  }
  ListBoxUpdateWindow(hwnd,lphl,FALSE);
  SelectObject(hdc,hOldFont);
  EndPaint( hwnd, &ps );
  return 0;
}

/***********************************************************************
 *           LBSetFocus
 */
static LONG LBSetFocus(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  dprintf_listbox(stddeb,"ListBox WM_SETFOCUS for %04x\n",hwnd);
  if(!(lphl->dwStyle & LBS_MULTIPLESEL) )
       if( lphl->ItemsCount && lphl->ItemFocused != -1)
         {
           HDC          hDC = GetDC(hwnd);
           HFONT        hOldFont = SelectObject(hDC, lphl->hFont);
           LPLISTSTRUCT lpls;

           lpls = ListBoxGetItem(lphl,lphl->ItemFocused);
           lpls->itemState |= ODS_FOCUS;

           ListBoxDrawItem(hwnd,lphl,hDC,lpls,&lpls->itemRect, ODA_FOCUS, lpls->itemState);
           SelectObject(hDC, hOldFont);
           ReleaseDC(hwnd,hDC);
         }

  ListBoxSendNotification(lphl, LBN_SETFOCUS);

  return 0;
}

/***********************************************************************
 *           LBKillFocus
 */
static LONG LBKillFocus(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  dprintf_listbox(stddeb,"ListBox WM_KILLFOCUS for %04x\n",hwnd);
  if (!(lphl->dwStyle & LBS_MULTIPLESEL))
     {
       if( lphl->ItemsCount )
           if( lphl->ItemFocused != -1 )
             {
              HDC          hDC = GetDC(hwnd);
              HFONT        hOldFont = SelectObject(hDC, lphl->hFont);
              LPLISTSTRUCT lpls;

              lpls = ListBoxGetItem(lphl,lphl->ItemFocused);
              lpls->itemState &= ~ODS_FOCUS;

              ListBoxDrawItem(hwnd,lphl,hDC,lpls,&lpls->itemRect, ODA_FOCUS, lpls->itemState);
              SelectObject(hDC, hOldFont);
              ReleaseDC(hwnd,hDC);
             }
           else
             dprintf_listbox(stddeb,"LBKillFocus: no focused item!\n");
     }
  else
     InvalidateRect(hwnd, NULL, TRUE);

  ListBoxSendNotification(lphl, LBN_KILLFOCUS);

  return 0;
}

/***********************************************************************
 *           LBResetContent
 */
static LONG LBResetContent(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  dprintf_listbox(stddeb,"ListBox LB_RESETCONTENT !\n");
  ListBoxResetContent(lphl);
  ListBoxUpdateWindow(hwnd, lphl, TRUE);
  return 0;
}

/***********************************************************************
 *           LBDir
 */
static LONG LBDir(HWND hwnd, WORD wParam, LONG lParam)
{
    LONG ret;
    LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
    dprintf_listbox(stddeb,"ListBox LB_DIR !\n");

    ret = ListBoxDirectory(lphl, wParam, (LPSTR)PTR_SEG_TO_LIN(lParam));
    ListBoxUpdateWindow(hwnd, lphl, TRUE);
    return ret;
}

/***********************************************************************
 *           LBAddString
 */
static LONG LBAddString(HWND hwnd, WORD wParam, LONG lParam)
{
  WORD  wRet;
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  if (lphl->HasStrings)
    wRet = ListBoxAddString(lphl, (LPCSTR)PTR_SEG_TO_LIN(lParam));
  else
    wRet = ListBoxAddString(lphl, (LPCSTR)lParam);

  ListBoxUpdateWindow(hwnd,lphl,TRUE);
  return wRet;
}

/***********************************************************************
 *           LBGetText
 */
static LONG LBGetText(HWND hwnd, WORD wParam, LONG lParam)
{
  LONG   wRet;
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  dprintf_listbox(stddeb, "LB_GETTEXT  wParam=%d\n",wParam);
  wRet = ListBoxGetText(lphl, wParam, (LPSTR)PTR_SEG_TO_LIN(lParam));

  return wRet;
}

/***********************************************************************
 *           LBInsertString
 */
static LONG LBInsertString(HWND hwnd, WORD wParam, LONG lParam)
{
  WORD  wRet;
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  if (lphl->HasStrings)
    wRet = ListBoxInsertString(lphl, wParam, (LPCSTR)PTR_SEG_TO_LIN(lParam));
  else
    wRet = ListBoxInsertString(lphl, wParam, (LPCSTR)lParam);

  ListBoxUpdateWindow(hwnd,lphl,TRUE);
  return wRet;
}

/***********************************************************************
 *           LBDeleteString
 */
static LONG LBDeleteString(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  LONG lRet = ListBoxDeleteString(lphl,wParam);
  
  ListBoxUpdateWindow(hwnd,lphl,TRUE);
  return lRet;
}

/***********************************************************************
 *           LBFindString
 */
static LONG LBFindString(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  return ListBoxFindString(lphl, wParam, (SEGPTR)lParam);
}

/***********************************************************************
 *           LBGetCaretIndex
 */
static LONG LBGetCaretIndex(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  return lphl->ItemFocused;
}

/***********************************************************************
 *           LBGetCount
 */
static LONG LBGetCount(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST  lphl;

  lphl = ListBoxGetStorageHeader(hwnd);
  return lphl->ItemsCount;
}

/***********************************************************************
 *           LBGetCurSel
 */
static LONG LBGetCurSel(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST  lphl;

  lphl = ListBoxGetStorageHeader(hwnd);
  dprintf_listbox(stddeb,"ListBox LB_GETCURSEL %i !\n", 
		  lphl->ItemFocused);
  return lphl->ItemFocused;
}

/***********************************************************************
 *           LBGetHorizontalExtent
 */
static LONG LBGetHorizontalExtent(HWND hwnd, WORD wParam, LONG lParam)
{    
  return 0;
}

/***********************************************************************
 *           LBGetItemHeight
 */
static LONG LBGetItemHeight(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls = ListBoxGetItem (lphl, wParam);
  
  if (lpls == NULL) return LB_ERR;
  return lpls->mis.itemHeight;
}

/***********************************************************************
 *           LBGetItemRect
 */
static LONG LBGetItemRect(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  return ListBoxGetItemRect(lphl, wParam, PTR_SEG_TO_LIN(lParam));
}

/***********************************************************************
 *           LBGetSel
 */
static LONG LBGetSel(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  int        iSel = ListBoxGetSel(lphl, wParam);

  dprintf_listbox(stdnimp,"LBGetSel: item %u - %i\n",wParam,iSel);

  return (iSel)? 1 : 0;
}

/***********************************************************************
 *           LBGetSelCount
 */
static LONG LBGetSelCount(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls;
  int          cnt = 0;
  int          items = 0;

  if (!(lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)  )) 
	 return LB_ERR;

  for( lpls = lphl->lpFirst;
       lpls;
       lpls = lpls->lpNext )
	{
	   items++;
           if (lpls->itemState ) 
               cnt++;
  }

  return cnt;
}

/***********************************************************************
 *           LBGetSelItems
 */
static LONG LBGetSelItems(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST  lphl = ListBoxGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls;
  int cnt, idx;
  int *lpItems = PTR_SEG_TO_LIN(lParam);

  if (!(lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)  )) 
        return LB_ERR;

  if (wParam == 0) return 0;

  lpls = lphl->lpFirst;
  cnt = 0; idx = 0;

  while (lpls != NULL) {
    if (lpls->itemState > 0) lpItems[cnt++] = idx;

    if (cnt == wParam) break;
    idx++;
    lpls = lpls->lpNext;
  }

  return cnt;
}

/***********************************************************************
 *           LBGetTextLen
 */
static LONG LBGetTextLen(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST   lphl = ListBoxGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls = ListBoxGetItem(lphl,wParam);

  if (lpls == NULL || !lphl->HasStrings) return LB_ERR;
  return strlen(lpls->itemText);
}

/***********************************************************************
 *           LBGetDlgCode
 */
static LONG LBGetDlgCode(HWND hwnd, WORD wParam, LONG lParam)
{
  return DLGC_WANTARROWS | DLGC_WANTCHARS;
}

/***********************************************************************
 *           LBGetTopIndex
 */
static LONG LBGetTopIndex(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  return lphl->FirstVisible;
}


/***********************************************************************
 *           LBSelectString
 */
static LONG LBSelectString(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  INT  iRet;

  iRet = ListBoxFindString(lphl, wParam, (SEGPTR)lParam);

  if( iRet != LB_ERR)
    {
      if( lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL) )
         ListBoxSetSel(lphl,iRet,TRUE);
      else
         ListBoxSetCurSel(lphl,iRet);

      lphl->ItemFocused = iRet;
      InvalidateRect(hwnd,0,TRUE);
    }
  return iRet;
}

/***********************************************************************
 *           LBSelItemRange
 */
static LONG LBSelItemRange(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST   lphl = ListBoxGetStorageHeader(hwnd);
  LPLISTSTRUCT lpls;
  WORD         cnt;
  WORD         first = LOWORD(lParam);
  WORD         last = HIWORD(lParam);
  BOOL         select = wParam;

  if (!(lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL)  )) 
        return LB_ERR;

  if (first >= lphl->ItemsCount ||
      last >= lphl->ItemsCount) return LB_ERR;

  lpls = lphl->lpFirst;
  cnt = 0;

  while (lpls != NULL) {
    if (cnt++ >= first)
      lpls->itemState = select ? lpls->itemState | ODS_SELECTED : 0;

    if (cnt > last)
      break;

    lpls = lpls->lpNext;
  }

  return 0;
}

/***********************************************************************
 *           LBSetCaretIndex
 */
static LONG LBSetCaretIndex(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST   lphl = ListBoxGetStorageHeader(hwnd);
  int          i;

  if (!(lphl->dwStyle & (LBS_MULTIPLESEL | LBS_EXTENDEDSEL) )) return 0;

  dprintf_listbox(stddeb,"LBSetCaretIndex: hwnd %04x n=%i\n",hwnd,wParam);  

  if (wParam >= lphl->ItemsCount) return LB_ERR;

  lphl->ItemFocused = wParam;
  i = ListBoxScrollToFocus (lphl);

  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
  if(i)
    InvalidateRect(hwnd, NULL, TRUE);
 
  return 1;
}

/***********************************************************************
 *           LBSetColumnWidth
 */
static LONG LBSetColumnWidth(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  lphl->ColumnsWidth = wParam;
  InvalidateRect(hwnd,NULL,TRUE);
  return 0;
}

/***********************************************************************
 *           LBSetHorizontalExtent
 */
static LONG LBSetHorizontalExtent(HWND hwnd, WORD wParam, LONG lParam)
{
  return 0;
}

/***********************************************************************
 *           LBGetItemData
 */
static LONG LBGetItemData(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  dprintf_listbox(stddeb, "LB_GETITEMDATA wParam=%x\n", wParam);
  return ListBoxGetItemData(lphl, wParam);
}

/***********************************************************************
 *           LBSetItemData
 */
static LONG LBSetItemData(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  dprintf_listbox(stddeb, "LB_SETITEMDATA  wParam=%x  lParam=%lx\n", wParam, lParam);
  return ListBoxSetItemData(lphl, wParam, lParam);
}

/***********************************************************************
 *           LBSetTabStops
 */
static LONG LBSetTabStops(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST  lphl;

  lphl = ListBoxGetStorageHeader(hwnd);

  if (lphl->TabStops != NULL) {
    lphl->iNumStops = 0;
    free (lphl->TabStops);
  }

  lphl->TabStops = malloc (wParam * sizeof (short));
  if (lphl->TabStops) {
    lphl->iNumStops = wParam;
    memcpy (lphl->TabStops, PTR_SEG_TO_LIN(lParam), wParam * sizeof (short));
    return TRUE;
  }

  return FALSE;
}

/***********************************************************************
 *           LBSetCurSel
 */
static LONG LBSetCurSel(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  WORD  wRet;

  dprintf_listbox(stddeb,"ListBox LB_SETCURSEL wParam=%x !\n", 
		  wParam);

  wRet = ListBoxSetCurSel(lphl, wParam);

  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);
  InvalidateRect(hwnd, NULL, TRUE);

  return wRet;
}

/***********************************************************************
 *           LBSetSel
 */
static LONG LBSetSel(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  RECT rect;
  int iRet;

  dprintf_listbox(stddeb,"ListBox LB_SETSEL wParam=%x lParam=%lX !\n", wParam, lParam);

  iRet = ListBoxSetSel(lphl, LOWORD(lParam), wParam);

  if( iRet > 1 )   
      InvalidateRect(hwnd, NULL, TRUE);
  else if( iRet != LB_ERR )
      {
        if( lphl->dwStyle & LBS_EXTENDEDSEL &&
            lphl->ItemFocused != LOWORD(lParam) )
          {
            ListBoxGetItemRect(lphl, lphl->ItemFocused , &rect);
            InvalidateRect(hwnd,&rect,TRUE);
            lphl->ItemFocused = LOWORD(lParam);
          }
        ListBoxGetItemRect(lphl,LOWORD(lParam),&rect);
        InvalidateRect(hwnd,&rect,TRUE);
      }

  return (iRet == (WORD)LB_ERR)? LB_ERR: 0;
}

/***********************************************************************
 *           LBSetTopIndex
 */
static LONG LBSetTopIndex(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

  dprintf_listbox(stddeb,"ListBox LB_SETTOPINDEX wParam=%x !\n",
		  wParam);
  lphl->FirstVisible = wParam;
  SetScrollPos(hwnd, SB_VERT, lphl->FirstVisible, TRUE);

  InvalidateRect(hwnd, NULL, TRUE);

  return 0;
}

/***********************************************************************
 *           LBSetItemHeight
 */
static LONG LBSetItemHeight(HWND hwnd, WORD wParam, LONG lParam)
{
  LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);
  WORD wRet;
  
  dprintf_listbox(stddeb,"ListBox LB_SETITEMHEIGHT wParam=%x lParam=%lX !\n", wParam, lParam);
  wRet = ListBoxSetItemHeight(lphl, wParam, lParam);
  InvalidateRect(hwnd,NULL,TRUE);
  return wRet;
}

/***********************************************************************
 *	     LBPassToParent
 */
static LRESULT LBPassToParent(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  WND* ptrWnd = WIN_FindWndPtr(hwnd);  

  if( ptrWnd )
      if( /* !(ptrWnd->dwExStyle & WS_EX_NOPARENTNOTIFY) && */ 
          ptrWnd->parent ) 
          return SendMessage(ptrWnd->parent->hwndSelf,message,wParam,lParam);
  return 0;
}

/***********************************************************************
 *           ListBoxWndProc 
 */
LRESULT ListBoxWndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{ 
    switch (message) {
     case WM_CREATE: return LBCreate(hwnd, wParam, lParam);
     case WM_DESTROY: return LBDestroy(hwnd, wParam, lParam);
     case WM_GETDLGCODE: return LBGetDlgCode(hwnd, wParam, lParam);
     case WM_VSCROLL: return LBVScroll(hwnd, wParam, lParam);
     case WM_HSCROLL: return LBHScroll(hwnd, wParam, lParam);
     case WM_LBUTTONDOWN: return LBLButtonDown(hwnd, wParam, lParam);
     case WM_LBUTTONUP: return LBLButtonUp(hwnd, wParam, lParam);
     case WM_RBUTTONUP: return LBRButtonUp(hwnd, wParam, lParam);
     case WM_LBUTTONDBLCLK: return LBRButtonUp(hwnd, wParam, lParam);
     case WM_MOUSEMOVE: return LBMouseMove(hwnd, wParam, lParam);
     case WM_KEYDOWN: return LBKeyDown(hwnd, wParam, lParam);
     case WM_CHAR: return LBChar(hwnd, wParam, lParam);
     case WM_SETFONT: return LBSetFont(hwnd, wParam, lParam);
     case WM_SETREDRAW: return LBSetRedraw(hwnd, wParam, lParam);
     case WM_PAINT: return LBPaint(hwnd, wParam, lParam);
     case WM_SETFOCUS: return LBSetFocus(hwnd, wParam, lParam);
     case WM_KILLFOCUS: return LBKillFocus(hwnd, wParam, lParam);
     case LB_RESETCONTENT: return LBResetContent(hwnd, wParam, lParam);
     case LB_DIR: return LBDir(hwnd, wParam, lParam);
     case LB_ADDSTRING: return LBAddString(hwnd, wParam, lParam);
     case LB_INSERTSTRING: return LBInsertString(hwnd, wParam, lParam);
     case LB_DELETESTRING: return LBDeleteString(hwnd, wParam, lParam);
     case LB_FINDSTRING: return LBFindString(hwnd, wParam, lParam);
     case LB_GETCARETINDEX: return LBGetCaretIndex(hwnd, wParam, lParam);
     case LB_GETCOUNT: return LBGetCount(hwnd, wParam, lParam);
     case LB_GETCURSEL: return LBGetCurSel(hwnd, wParam, lParam);
     case LB_GETHORIZONTALEXTENT: return LBGetHorizontalExtent(hwnd, wParam, lParam);
     case LB_GETITEMDATA: return LBGetItemData(hwnd, wParam, lParam);
     case LB_GETITEMHEIGHT: return LBGetItemHeight(hwnd, wParam, lParam);
     case LB_GETITEMRECT: return LBGetItemRect(hwnd, wParam, lParam);
     case LB_GETSEL: return LBGetSel(hwnd, wParam, lParam);
     case LB_GETSELCOUNT: return LBGetSelCount(hwnd, wParam, lParam);
     case LB_GETSELITEMS: return LBGetSelItems(hwnd, wParam, lParam);
     case LB_GETTEXT: return LBGetText(hwnd, wParam, lParam);
     case LB_GETTEXTLEN: return LBGetTextLen(hwnd, wParam, lParam);
     case LB_GETTOPINDEX: return LBGetTopIndex(hwnd, wParam, lParam);
     case LB_SELECTSTRING: return LBSelectString(hwnd, wParam, lParam);
     case LB_SELITEMRANGE: return LBSelItemRange(hwnd, wParam, lParam);
     case LB_SETCARETINDEX: return LBSetCaretIndex(hwnd, wParam, lParam);
     case LB_SETCOLUMNWIDTH: return LBSetColumnWidth(hwnd, wParam, lParam);
     case LB_SETHORIZONTALEXTENT: return LBSetHorizontalExtent(hwnd, wParam, lParam);
     case LB_SETITEMDATA: return LBSetItemData(hwnd, wParam, lParam);
     case LB_SETTABSTOPS: return LBSetTabStops(hwnd, wParam, lParam);
     case LB_SETCURSEL: return LBSetCurSel(hwnd, wParam, lParam);
     case LB_SETSEL: return LBSetSel(hwnd, wParam, lParam);
     case LB_SETTOPINDEX: return LBSetTopIndex(hwnd, wParam, lParam);
     case LB_SETITEMHEIGHT: return LBSetItemHeight(hwnd, wParam, lParam);

     case WM_DROPFILES: return LBPassToParent(hwnd, message, wParam, lParam);

     /* these will have to be implemented for proper LBS_EXTENDEDSEL -
      *
      * anchor item is an item that with caret (focused) item defines a 
      * range of currently selected items when listbox is in the extended 
      * selection mode.
      */
     case LB_SETANCHORINDEX: return LB_SETANCHORINDEX; /* that's what Windows returns */
     case LB_GETANCHORINDEX: return 0;

	case WM_DROPOBJECT:
	case WM_QUERYDROPOBJECT:
	case WM_DRAGSELECT:
	case WM_DRAGMOVE:
	        {
		 LPDRAGINFO lpDragInfo = (LPDRAGINFO) PTR_SEG_TO_LIN((SEGPTR)lParam);
		 LPHEADLIST lphl = ListBoxGetStorageHeader(hwnd);

		 lpDragInfo->l = ListBoxFindMouse(lphl,lpDragInfo->pt.x,
						       lpDragInfo->pt.y);
	        
		 return LBPassToParent(hwnd, message, wParam, lParam);
		}
    }
    
    return DefWindowProc(hwnd, message, wParam, lParam);
}


/**********************************************************************
 *	    DlgDirSelect    (USER.99)
 */
BOOL DlgDirSelect( HWND hDlg, LPSTR lpStr, INT id )
{
    char buffer[20];
    INT i;

    dprintf_listbox( stddeb, "DlgDirSelect: %04x '%s' %d\n", hDlg, lpStr, id );
    if ((i = SendDlgItemMessage( hDlg, id, LB_GETCURSEL, 0, 0 )) == LB_ERR)
        return FALSE;
    SendDlgItemMessage( hDlg, id, LB_GETTEXT, i, MAKE_SEGPTR(buffer) );
    if (buffer[0] == '[')  /* drive or directory */
    {
        if (buffer[1] == '-')  /* drive */
        {
            lpStr[0] = buffer[2];
            lpStr[1] = ':';
            lpStr[2] = '\0';
            dprintf_listbox( stddeb, "Returning drive '%s'\n", lpStr );
            return TRUE;
        }
        strcpy( lpStr, buffer + 1 );
        lpStr[strlen(lpStr)-1] = '\\';
        dprintf_listbox( stddeb, "Returning directory '%s'\n", lpStr );
        return TRUE;
    }
    strcpy( lpStr, buffer );
    dprintf_listbox( stddeb, "Returning file '%s'\n", lpStr );
    return FALSE;
}


/**********************************************************************
 *	    DlgDirList    (USER.100)
 */
INT DlgDirList( HWND hDlg, SEGPTR spec, INT idLBox, INT idStatic, UINT attrib )
{
    char *filespec = (char *)PTR_SEG_TO_LIN( spec );
    int drive;
    HWND hwnd;

#define SENDMSG(msg,wparam,lparam) \
    ((attrib & DDL_POSTMSGS) ? PostMessage( hwnd, msg, wparam, lparam ) \
                             : SendMessage( hwnd, msg, wparam, lparam ))

    dprintf_listbox( stddeb, "DlgDirList: %04x '%s' %d %d %04x\n",
                     hDlg, filespec ? filespec : "NULL",
                     idLBox, idStatic, attrib );

    if (filespec && filespec[0] && (filespec[1] == ':'))
    {
        drive = toupper( filespec[0] ) - 'A';
        filespec += 2;
        if (!DRIVE_SetCurrentDrive( drive )) return FALSE;
    }
    else drive = DRIVE_GetCurrentDrive();

    if (idLBox && ((hwnd = GetDlgItem( hDlg, idLBox )) != 0))
    {
        char mask[20];
        char temp[] = "*.*";
        
        if (!filespec[0]) strcpy( mask, "*.*" );
        else
        {
            /* If the path exists and is a directory, chdir to it */
            if (DRIVE_Chdir( drive, filespec )) strcpy( mask, "*.*" );
            else
            {
                char *p, *p2;
                p = filespec;
                if ((p2 = strrchr( p, '\\' ))) p = p2 + 1;
                if ((p2 = strrchr( p, '/' ))) p = p2 + 1;
                lstrcpyn( mask, p, sizeof(mask) );
                if (p != filespec)
                {
                    p[-1] = '\0';
                    if (!DRIVE_Chdir( drive, filespec )) return FALSE;
                }
            }
        }
        
        strcpy( (char *)PTR_SEG_TO_LIN(spec), mask );

        dprintf_listbox(stddeb, "ListBoxDirectory: path=%c:\\%s mask=%s\n",
                        'A' + drive, DRIVE_GetDosCwd(drive), mask);
        
        SENDMSG( LB_RESETCONTENT, 0, 0 );
        if ((attrib & DDL_DIRECTORY) && !(attrib & DDL_EXCLUSIVE))
        {
            if (SENDMSG( LB_DIR, attrib & ~(DDL_DIRECTORY | DDL_DRIVES),
                         (LPARAM)spec ) == LB_ERR) return FALSE;
            if (SENDMSG( LB_DIR, (attrib & (DDL_DIRECTORY | DDL_DRIVES)) | DDL_EXCLUSIVE,
                         (LPARAM)MAKE_SEGPTR(temp) ) == LB_ERR) return FALSE;
        }
        else
        {
            if (SENDMSG( LB_DIR, attrib, (LPARAM)spec) == LB_ERR) return FALSE;
        }
    }

    if (idStatic && ((hwnd = GetDlgItem( hDlg, idStatic )) != 0))
    {
        char temp[256];
        strcpy( temp, "A:\\" );
        temp[0] += drive;
        lstrcpyn( temp+3, DRIVE_GetDosCwd(drive), 253 );
        AnsiLower( temp );
        SENDMSG( WM_SETTEXT, 0, (LPARAM)MAKE_SEGPTR(temp) );
    }
    return TRUE;
#undef SENDMSG
}
