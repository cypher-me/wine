name	comctl32
type	win32

# Functions exported by the Win95 comctl32.dll 
# (these need to have these exact ordinals, because some win95 dlls 
#  import comctl32.dll by ordinal)
#   This list was created from a comctl32.dll v4.72 (IE4.01).

  2 stub MenuHelp
  3 stub ShowHideMenuCtl
  4 stub GetEffectiveClientRect
  5 stdcall DrawStatusTextA(long ptr ptr long) DrawStatusText32A
  6 stdcall CreateStatusWindowA(long ptr long long) CreateStatusWindow32A
  7 stub CreateToolbar
  8 stub CreateMappedBitmap
  9 stub COMCTL32_9
 10 stub COMCTL32_10
 11 stub COMCTL32_11
 12 stub Cctl1632_ThunkData32
 13 stub MakeDragList
 14 stub LBItemFromPt
 15 stub DrawInsert
 16 stdcall CreateUpDownControl(long long long long long long long long long long long long) CreateUpDownControl
 17 stdcall InitCommonControls() InitCommonControls
 18 stub CreatePropertySheetPage
 19 stub CreatePropertySheetPageA
 20 stub CreatePropertySheetPageW
# 21 pascal16 CreateStatusWindow(word ptr word word) CreateStatusWindow16
 21 stub CreateStatusWindow
 22 stdcall CreateStatusWindowW(long ptr long long) CreateStatusWindow32W
 23 stub CreateToolbarEx
 24 stub DestroyPropertySheetPage
 25 stub DllGetVersion
 26 stub DllInstall
# 27 pascal16 DrawStatusText(word ptr ptr word) DrawStatusText16
 27 stub DrawStatusText
 28 stdcall DrawStatusTextW(long ptr ptr long) DrawStatusText32W
 29 stub FlatSB_EnableScrollBar
 30 stub FlatSB_GetScrollInfo
 31 stub FlatSB_GetScrollPos
 32 stub FlatSB_GetScrollProp
 33 stub FlatSB_GetScrollRange
 34 stub FlatSB_SetScrollInfo
 35 stub FlatSB_SetScrollPos
 36 stub FlatSB_SetScrollProp
 37 stub FlatSB_SetScrollRange
 38 stub FlatSB_ShowScrollBar
 39 stdcall ImageList_Add(ptr long long) ImageList_Add
 40 stub ImageList_AddIcon
 41 stdcall ImageList_AddMasked(ptr long long) ImageList_AddMasked
 42 stdcall ImageList_BeginDrag(ptr long long long) ImageList_BeginDrag
 43 stdcall ImageList_Copy(ptr long ptr long long) ImageList_Copy
 44 stdcall ImageList_Create(long long long long long) ImageList_Create
 45 stdcall ImageList_Destroy(ptr) ImageList_Destroy
 46 stdcall ImageList_DragEnter(long long long) ImageList_DragEnter
 47 stdcall ImageList_DragLeave(long) ImageList_DragLeave
 48 stdcall ImageList_DragMove(long long) ImageList_DragMove
 49 stdcall ImageList_DragShowNolock(long) ImageList_DragShowNolock
 50 stdcall ImageList_Draw(ptr long long long long long) ImageList_Draw
 51 stdcall ImageList_DrawEx(ptr long long long long long long long long long) ImageList_DrawEx
 52 stdcall ImageList_DrawIndirect(ptr) ImageList_DrawIndirect
 53 stdcall ImageList_Duplicate(ptr) ImageList_Duplicate
 54 stdcall ImageList_EndDrag() ImageList_EndDrag 
 55 stdcall ImageList_GetBkColor(ptr) ImageList_GetBkColor
 56 stdcall ImageList_GetDragImage(ptr ptr) ImageList_GetDragImage
 57 stdcall ImageList_GetIcon(ptr long long) ImageList_GetIcon
 58 stdcall ImageList_GetIconSize(ptr ptr ptr) ImageList_GetIconSize
 59 stdcall ImageList_GetImageCount(ptr) ImageList_GetImageCount
 60 stdcall ImageList_GetImageInfo(ptr long ptr) ImageList_GetImageInfo
 61 stdcall ImageList_GetImageRect(ptr long ptr) ImageList_GetImageRect
 62 stub ImageList_LoadImage
 63 stdcall ImageList_LoadImageA(long ptr long long long long long) ImageList_LoadImage32A
 64 stdcall ImageList_LoadImageW(long ptr long long long long long) ImageList_LoadImage32W
 65 stdcall ImageList_Merge(ptr long ptr long long long) ImageList_Merge
# 66 stdcall ImageList_Read(ptr) ImageList_Read
 66 stub ImageList_Read
 67 stdcall ImageList_Remove(ptr long) ImageList_Remove
 68 stdcall ImageList_Replace(ptr long long long) ImageList_Replace
 69 stdcall ImageList_ReplaceIcon(ptr long long) ImageList_ReplaceIcon
 70 stdcall ImageList_SetBkColor(ptr long) ImageList_SetBkColor
 71 stub Alloc
 72 stub ReAlloc
 73 stub Free
 74 stub GetSize
 75 stdcall ImageList_SetDragCursorImage(ptr long long long) ImageList_SetDragCursorImage
 76 stub ImageList_SetFilter
 77 stdcall ImageList_SetIconSize(ptr long long) ImageList_SetIconSize
 78 stdcall ImageList_SetImageCount(ptr long) ImageList_SetImageCount
 79 stdcall ImageList_SetOverlayImage(ptr long long) ImageList_SetOverlayImage
 80 stub ImageList_Write
 81 stub InitCommonControlsEx
 82 stub InitializeFlatSB
 83 stub PropertySheet
 84 stub PropertySheetA
 85 stub PropertySheetW
 86 stub UninitializeFlatSB
 87 stub _TrackMouseEvent

151 stub CreateMRUList
152 stub FreeMRUList
153 stub AddMRUStringA
154 stub EnumMRUListA
155 stub FindMRUStringA
156 stub DelMRUString
157 stub COMCTL32_157

163 stub CreatePage
164 stub CreateProxyPage

167 stub AddMRUData
169 stub FindMRUData

233 stub Str_GetPtrA
234 stub Str_SetPtrA
235 stub Str_GetPtrW
236 stub Str_SetPtrW

320 stub DSA_Create
321 stub DSA_Destroy
322 stub DSA_GetItem
323 stub DSA_GetItemPtr
324 stub DSA_InsertItem
325 stub DSA_SetItem
326 stub DSA_DeleteItem
327 stub DSA_DeleteAllItems
328 stub DPA_Create
329 stub DPA_Destroy
330 stub DPA_Grow
331 stub DPA_Clone
332 stub DPA_GetPtr
333 stub DPA_GetPtrIndex
334 stub DPA_InsertPtr
335 stub DPA_SetPtr
336 stub DPA_DeletePtr
337 stub DPA_DeleteAllPtrs
338 stub DPA_Sort
339 stub DPA_Search
340 stub DPA_CreateEx
341 stub SendNotify
342 stub SendNotifyEx

350 stub StrChrA
351 stub StrRChr
352 stub StrCmpNA
353 stub StrCmpNIA
354 stub StrStrA
355 stub StrStrIA
356 stub StrCSpnA
357 stub StrToIntA
358 stub StrChrW
359 stub StrRChrW
360 stub StrCmpNW
361 stub StrCmpNIW
362 stub StrStrW
363 stub StrStrIW
364 stub StrSpnW
365 stub StrToIntW
366 stub StrChrIA
367 stub StrChrIW
368 stub StrRChrIA
369 stub StrRChrIW

372 stub StrRStrIA
373 stub StrRStrIW
374 stub StrCSpnIA
375 stub StrCSpnIW
376 stub IntlStrEqWorkerA
377 stub IntlStrEqWorkerW

382 stub SmoothScrollWindow
383 stub DoReaderMode
384 stub SetPathWordBreakProc
385 stub COMCTL32_385
386 stub COMCTL32_386
387 stub COMCTL32_387
388 stub COMCTL32_388
389 stub COMCTL32_389
390 stub COMCTL32_390

400 stub COMCTL32_400
401 stub COMCTL32_401
402 stub COMCTL32_402
403 stub COMCTL32_403
404 stub COMCTL32_404

410 stub COMCTL32_410
411 stub COMCTL32_411
412 stub COMCTL32_412
413 stub COMCTL32_413

