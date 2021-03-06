/*++

Copyright (c) 2004 - 2008, Intel Corporation                                                         
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  Ui.c

Abstract:
   
  Implementation for UI.

Revision History

--*/

#include "Ui.h"
#include "Setup.h"

EFI_LIST_ENTRY      Menu;
EFI_LIST_ENTRY      gMenuList;
MENU_REFRESH_ENTRY  *gMenuRefreshHead;

INTN                gEntryNumber;
BOOLEAN             gLastOpr;
BOOLEAN             gNeedSwitchToTextMode;

//
// Implementation
//
VOID
SetUnicodeMem (
  IN VOID   *Buffer,
  IN UINTN  Size,
  IN CHAR16 Value
  )
/*++

Routine Description:

  Set Buffer to Value for Size bytes.

Arguments:

  Buffer  - Memory to set.

  Size    - Number of bytes to set

  Value   - Value of the set operation.

Returns:

  None

--*/
{
  CHAR16  *Ptr;

  Ptr = Buffer;
  while (Size--) {
    *(Ptr++) = Value;
  }
}

VOID
StrnCpy (
  IN CHAR16   *Dest,
  IN CHAR16   *Src,
  IN UINTN    Length
  )
//
// copy strings
//
{
  while (*Src && Length) {
    *(Dest++) = *(Src++);
    Length--;
  }

  *Dest = 0;
}

VOID
UiInitMenu (
  VOID
  )
/*++

Routine Description:
  Initialize Menu option list.

Arguments:
           
Returns:

--*/
{
  InitializeListHead (&Menu);
}

VOID
UiInitMenuList (
  VOID
  )
/*++

Routine Description:
  Initialize Menu option list.

Arguments:
           
Returns:

--*/
{
  InitializeListHead (&gMenuList);
}

VOID
UiRemoveMenuListEntry (
  IN  UI_MENU_OPTION    *Selection,
  OUT UI_MENU_OPTION    **PreviousSelection
  )
/*++

Routine Description:
  Remove Menu option list.

Arguments:
           
Returns:

--*/
{
  UI_MENU_LIST  *UiMenuList;

  *PreviousSelection = EfiLibAllocateZeroPool (sizeof (UI_MENU_OPTION));
  ASSERT (*PreviousSelection != NULL);

  if (!IsListEmpty (&gMenuList)) {
    UiMenuList                      = CR (gMenuList.ForwardLink, UI_MENU_LIST, MenuLink, UI_MENU_LIST_SIGNATURE);
    (*PreviousSelection)->IfrNumber = UiMenuList->Selection.IfrNumber;
    (*PreviousSelection)->FormId    = UiMenuList->Selection.FormId;
    (*PreviousSelection)->Tags      = UiMenuList->Selection.Tags;
    (*PreviousSelection)->ThisTag   = UiMenuList->Selection.ThisTag;
    (*PreviousSelection)->Handle    = UiMenuList->Selection.Handle;
    gEntryNumber                    = UiMenuList->FormerEntryNumber;
    RemoveEntryList (&UiMenuList->MenuLink);
    gBS->FreePool (UiMenuList);
  }
}

VOID
UiFreeMenuList (
  VOID
  )
/*++

Routine Description:
  Free Menu option linked list.

Arguments:
           
Returns:

--*/
{
  UI_MENU_LIST  *UiMenuList;

  while (!IsListEmpty (&gMenuList)) {
    UiMenuList = CR (gMenuList.ForwardLink, UI_MENU_LIST, MenuLink, UI_MENU_LIST_SIGNATURE);
    RemoveEntryList (&UiMenuList->MenuLink);
    gBS->FreePool (UiMenuList);
  }
}

VOID
UiAddMenuListEntry (
  IN UI_MENU_OPTION   *Selection
  )
/*++

Routine Description:
  Add one menu entry to the linked lst

Arguments:
           
Returns:

--*/
{
  UI_MENU_LIST  *UiMenuList;

  UiMenuList = EfiLibAllocateZeroPool (sizeof (UI_MENU_LIST));
  ASSERT (UiMenuList != NULL);

  UiMenuList->Signature = UI_MENU_LIST_SIGNATURE;
  EfiCopyMem (&UiMenuList->Selection, Selection, sizeof (UI_MENU_OPTION));

  InsertHeadList (&gMenuList, &UiMenuList->MenuLink);
}

VOID
UiFreeMenu (
  VOID
  )
/*++

Routine Description:
  Free Menu option linked list.

Arguments:
           
Returns:

--*/
{
  UI_MENU_OPTION  *MenuOption;

  while (!IsListEmpty (&Menu)) {
    MenuOption = CR (Menu.ForwardLink, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
    RemoveEntryList (&MenuOption->Link);

    //
    // We allocated space for this description when we did a GetToken, free it here
    //
    gBS->FreePool (MenuOption->Description);
    gBS->FreePool (MenuOption);
  }
}

VOID
UpdateDateAndTime (
  VOID
  )
/*++

Routine Description:
  Refresh screen with current date and/or time based on screen context

Arguments:
           
Returns:

--*/
{
  CHAR16              *OptionString;
  MENU_REFRESH_ENTRY  *MenuRefreshEntry;
  UINTN               Index;
  UINTN               Loop;

  OptionString = NULL;

  if (gMenuRefreshHead != NULL) {

    MenuRefreshEntry = gMenuRefreshHead;

    do {
      if ((MenuRefreshEntry->MenuOption->ThisTag->Flags & FLAG_DATE_TIME_BEING_MODIFY) == 0) {
        //
        // We only update those not being modified
        //
        gST->ConOut->SetAttribute (gST->ConOut, MenuRefreshEntry->CurrentAttribute);
        ProcessOptions (MenuRefreshEntry->MenuOption, FALSE, MenuRefreshEntry->FileFormTagsHead, NULL, &OptionString);

        if (OptionString != NULL) {
          //
          // If leading spaces on OptionString - remove the spaces
          //
          for (Index = 0; OptionString[Index] == L' '; Index++)
            ;

          for (Loop = 0; OptionString[Index] != CHAR_NULL; Index++) {
            OptionString[Loop] = OptionString[Index];
            Loop++;
          }

          OptionString[Loop] = CHAR_NULL;

          PrintStringAt (MenuRefreshEntry->CurrentColumn, MenuRefreshEntry->CurrentRow, OptionString);
        }
      }

      MenuRefreshEntry = MenuRefreshEntry->Next;

    } while (MenuRefreshEntry != NULL);
  }

  if (OptionString != NULL) {
    gBS->FreePool (OptionString);
  }
}

EFI_STATUS
UiWaitForSingleEvent (
  IN EFI_EVENT                Event,
  IN UINT64                   Timeout OPTIONAL
  )
/*++

Routine Description:
  Wait for a given event to fire, or for an optional timeout to expire.

Arguments:
  Event            - The event to wait for

  Timeout          - An optional timeout value in 100 ns units.

Returns:

  EFI_SUCCESS      - Event fired before Timeout expired.
  EFI_TIME_OUT     - Timout expired before Event fired.

--*/
{
  EFI_STATUS  Status;
  UINTN       Index;
  EFI_EVENT   TimerEvent;
  EFI_EVENT   WaitList[2];

  if (Timeout) {
    //
    // Create a timer event
    //
    Status = gBS->CreateEvent (EFI_EVENT_TIMER, 0, NULL, NULL, &TimerEvent);
    if (!EFI_ERROR (Status)) {
      //
      // Set the timer event
      //
      gBS->SetTimer (
            TimerEvent,
            TimerRelative,
            Timeout
            );

      //
      // Wait for the original event or the timer
      //
      WaitList[0] = Event;
      WaitList[1] = TimerEvent;
      Status      = gBS->WaitForEvent (2, WaitList, &Index);
      gBS->CloseEvent (TimerEvent);

      //
      // If the timer expired, change the return to timed out
      //
      if (!EFI_ERROR (Status) && Index == 1) {
        Status = EFI_TIMEOUT;
      }
    }
  } else {
    //
    // Update screen every second
    //
    Timeout = ONE_SECOND;

    do {
      Status = gBS->CreateEvent (EFI_EVENT_TIMER, 0, NULL, NULL, &TimerEvent);

      //
      // Set the timer event
      //
      gBS->SetTimer (
            TimerEvent,
            TimerRelative,
            Timeout
            );

      //
      // Wait for the original event or the timer
      //
      WaitList[0] = Event;
      WaitList[1] = TimerEvent;
      Status      = gBS->WaitForEvent (2, WaitList, &Index);

      //
      // If the timer expired, update anything that needs a refresh and keep waiting
      //
      if (!EFI_ERROR (Status) && Index == 1) {
        Status = EFI_TIMEOUT;
        UpdateDateAndTime ();
      }

      gBS->CloseEvent (TimerEvent);
    } while (Status == EFI_TIMEOUT);
  }

  return Status;
}

VOID
UiAddMenuOption (
  IN CHAR16         *String,
  IN EFI_HII_HANDLE Handle,
  IN EFI_TAG        *Tags,
  IN VOID           *FormBinary,
  IN UINTN          IfrNumber
  )
/*++

Routine Description:
  Add one menu option by specified description and context.

Arguments:
  String - String description for this option.
  Context - Context data for entry.
           
Returns:

--*/
{
  UI_MENU_OPTION  *MenuOption;

  MenuOption = EfiLibAllocateZeroPool (sizeof (UI_MENU_OPTION));
  ASSERT (MenuOption);

  MenuOption->Signature   = UI_MENU_OPTION_SIGNATURE;
  MenuOption->Description = String;
  MenuOption->Handle      = Handle;
  MenuOption->FormBinary  = FormBinary;
  MenuOption->IfrNumber   = IfrNumber;
  MenuOption->Skip        = 1;
  MenuOption->Tags        = Tags;
  MenuOption->TagIndex    = 0;
  MenuOption->ThisTag     = &(MenuOption->Tags[MenuOption->TagIndex]);
  MenuOption->EntryNumber = (UINT16) IfrNumber;

  InsertTailList (&Menu, &MenuOption->Link);
}

VOID
UiAddSubMenuOption (
  IN CHAR16           *String,
  IN EFI_HII_HANDLE   Handle,
  IN EFI_TAG          *Tags,
  IN UINTN            TagIndex,
  IN UINT16           FormId,
  IN UINT16           MenuItemCount
  )
/*++

Routine Description:
  Add one menu option by specified description and context.

Arguments:
  String - String description for this option.
  Context - Context data for entry.
           
Returns:

--*/
{
  UI_MENU_OPTION  *MenuOption;

  MenuOption = EfiLibAllocateZeroPool (sizeof (UI_MENU_OPTION));
  ASSERT (MenuOption);

  MenuOption->Signature   = UI_MENU_OPTION_SIGNATURE;
  MenuOption->Description = String;
  MenuOption->Handle      = Handle;
  MenuOption->Skip        = Tags[TagIndex].NumberOfLines;
  MenuOption->IfrNumber   = gActiveIfr;
  MenuOption->Tags        = Tags;
  MenuOption->TagIndex    = TagIndex;
  MenuOption->ThisTag     = &(MenuOption->Tags[MenuOption->TagIndex]);
  MenuOption->Consistency = Tags[TagIndex].Consistency;
  MenuOption->FormId      = FormId;
  MenuOption->GrayOut     = Tags[TagIndex].GrayOut;
  MenuOption->EntryNumber = MenuItemCount;

  InsertTailList (&Menu, &MenuOption->Link);
}

EFI_STATUS
CreateDialog (
  IN  UINTN                       NumberOfLines,
  IN  BOOLEAN                     HotKey,
  IN  UINTN                       MaximumStringSize,
  OUT CHAR16                      *StringBuffer,
  OUT EFI_INPUT_KEY               *KeyValue,
  IN  CHAR16                      *String,
  ...
  )
/*++

Routine Description:
  Routine used to abstract a generic dialog interface and return the selected key or string

Arguments:
  NumberOfLines -     The number of lines for the dialog box
  HotKey -            Defines whether a single character is parsed (TRUE) and returned in KeyValue 
                      or a string is returned in StringBuffer.  Two special characters are considered when entering a string, a SCAN_ESC and
                      an CHAR_CARRIAGE_RETURN.  SCAN_ESC terminates string input and returns
  MaximumStringSize - The maximum size in bytes of a typed in string (each character is a CHAR16) and the minimum string returned is two bytes
  StringBuffer -      The passed in pointer to the buffer which will hold the typed in string if HotKey is FALSE
  KeyValue -          The EFI_KEY value returned if HotKey is TRUE..
  String -            Pointer to the first string in the list
  ... -               A series of (quantity == NumberOfLines) text strings which will be used to construct the dialog box
           
Returns:
  EFI_SUCCESS -           Displayed dialog and received user interaction
  EFI_INVALID_PARAMETER - One of the parameters was invalid (e.g. (StringBuffer == NULL) && (HotKey == FALSE))
  EFI_DEVICE_ERROR -      User typed in an ESC character to exit the routine

--*/
{
  VA_LIST       Marker;
  UINTN         Count;
  EFI_INPUT_KEY Key;
  UINTN         LargestString;
  CHAR16        *TempString;
  CHAR16        *BufferedString;
  CHAR16        *StackString;
  CHAR16        KeyPad[2];
  UINTN         Start;
  UINTN         Top;
  UINTN         Index;
  EFI_STATUS    Status;
  BOOLEAN       SelectionComplete;
  UINTN         InputOffset;
  UINTN         CurrentAttribute;
  UINTN         DimensionsWidth;
  UINTN         DimensionsHeight;

  DimensionsWidth   = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  DimensionsHeight  = gScreenDimensions.BottomRow - gScreenDimensions.TopRow;

  SelectionComplete = FALSE;
  InputOffset       = 0;
  TempString        = EfiLibAllocateZeroPool (MaximumStringSize * 2);
  BufferedString    = EfiLibAllocateZeroPool (MaximumStringSize * 2);
  CurrentAttribute  = gST->ConOut->Mode->Attribute;

  ASSERT (TempString);
  ASSERT (BufferedString);

  VA_START (Marker, String);

  //
  // Zero the outgoing buffer
  //
  EfiZeroMem (StringBuffer, MaximumStringSize);

  if (HotKey) {
    if (KeyValue == NULL) {
      return EFI_INVALID_PARAMETER;
    }
  } else {
    if (StringBuffer == NULL) {
      return EFI_INVALID_PARAMETER;
    }
  }
  //
  // Disable cursor
  //
  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  LargestString = (GetStringWidth (String) / 2);

  if (LargestString == L' ') {
    InputOffset = 1;
  }
  //
  // Determine the largest string in the dialog box
  // Notice we are starting with 1 since String is the first string
  //
  for (Count = 1; Count < NumberOfLines; Count++) {
    StackString = VA_ARG (Marker, CHAR16 *);

    if (StackString[0] == L' ') {
      InputOffset = Count + 1;
    }

    if ((GetStringWidth (StackString) / 2) > LargestString) {
      //
      // Size of the string visually and subtract the width by one for the null-terminator
      //
      LargestString = (GetStringWidth (StackString) / 2);
    }
  }

  Start = (DimensionsWidth - LargestString - 2) / 2 + gScreenDimensions.LeftColumn + 1;
  Top   = ((DimensionsHeight - NumberOfLines - 2) / 2) + gScreenDimensions.TopRow - 1;

  Count = 0;

  //
  // Display the Popup
  //
  CreateSharedPopUp (LargestString, NumberOfLines, &String);

  //
  // Take the first key typed and report it back?
  //
  if (HotKey) {
    Status = WaitForKeyStroke (&Key);
    EfiCopyMem (KeyValue, &Key, sizeof (EFI_INPUT_KEY));

  } else {
    do {
      Status = WaitForKeyStroke (&Key);

      switch (Key.UnicodeChar) {
      case CHAR_NULL:
        switch (Key.ScanCode) {
        case SCAN_ESC:
          gBS->FreePool (TempString);
          gBS->FreePool (BufferedString);
          gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
          gST->ConOut->EnableCursor (gST->ConOut, TRUE);
          return EFI_DEVICE_ERROR;

        default:
          break;
        }

        break;

      case CHAR_CARRIAGE_RETURN:
        SelectionComplete = TRUE;
        gBS->FreePool (TempString);
        gBS->FreePool (BufferedString);
        gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
        gST->ConOut->EnableCursor (gST->ConOut, TRUE);
        return EFI_SUCCESS;
        break;

      case CHAR_BACKSPACE:
        if (StringBuffer[0] != CHAR_NULL) {
          for (Index = 0; StringBuffer[Index] != CHAR_NULL; Index++) {
            TempString[Index] = StringBuffer[Index];
          }
          //
          // Effectively truncate string by 1 character
          //
          TempString[Index - 1] = CHAR_NULL;
          EfiStrCpy (StringBuffer, TempString);
        }

      default:
        //
        // If it is the beginning of the string, don't worry about checking maximum limits
        //
        if ((StringBuffer[0] == CHAR_NULL) && (Key.UnicodeChar != CHAR_BACKSPACE)) {
          StrnCpy (StringBuffer, &Key.UnicodeChar, 1);
          StrnCpy (TempString, &Key.UnicodeChar, 1);
        } else if ((GetStringWidth (StringBuffer) < MaximumStringSize) && (Key.UnicodeChar != CHAR_BACKSPACE)) {
          KeyPad[0] = Key.UnicodeChar;
          KeyPad[1] = CHAR_NULL;
          EfiStrCat (StringBuffer, KeyPad);
          EfiStrCat (TempString, KeyPad);
        }
        //
        // If the width of the input string is now larger than the screen, we nee to
        // adjust the index to start printing portions of the string
        //
        SetUnicodeMem (BufferedString, LargestString, L' ');

        PrintStringAt (Start + 1, Top + InputOffset, BufferedString);

        if ((GetStringWidth (StringBuffer) / 2) > (DimensionsWidth - 2)) {
          Index = (GetStringWidth (StringBuffer) / 2) - DimensionsWidth + 2;
        } else {
          Index = 0;
        }

        for (Count = 0; Index + 1 < GetStringWidth (StringBuffer) / 2; Index++, Count++) {
          BufferedString[Count] = StringBuffer[Index];
        }

        PrintStringAt (Start + 1, Top + InputOffset, BufferedString);
        break;
      }
    } while (!SelectionComplete);
  }

  gST->ConOut->SetAttribute (gST->ConOut, CurrentAttribute);
  gST->ConOut->EnableCursor (gST->ConOut, TRUE);
  return EFI_SUCCESS;
}

VOID
CreateSharedPopUp (
  IN  UINTN                       RequestedWidth,
  IN  UINTN                       NumberOfLines,
  IN  CHAR16                      **ArrayOfStrings
  )
{
  UINTN   Index;
  UINTN   Count;
  CHAR16  Character;
  UINTN   Start;
  UINTN   End;
  UINTN   Top;
  UINTN   Bottom;
  CHAR16  *String;

  UINTN   DimensionsWidth;
  UINTN   DimensionsHeight;

  DimensionsWidth   = gScreenDimensions.RightColumn - gScreenDimensions.LeftColumn;
  DimensionsHeight  = gScreenDimensions.BottomRow - gScreenDimensions.TopRow;

  Count             = 0;

  gST->ConOut->SetAttribute (gST->ConOut, POPUP_TEXT | POPUP_BACKGROUND);

  if ((RequestedWidth + 2) > DimensionsWidth) {
    RequestedWidth = DimensionsWidth - 2;
  }
  //
  // Subtract the PopUp width from total Columns, allow for one space extra on
  // each end plus a border.
  //
  Start     = (DimensionsWidth - RequestedWidth - 2) / 2 + gScreenDimensions.LeftColumn + 1;
  End       = Start + RequestedWidth + 1;

  Top       = ((DimensionsHeight - NumberOfLines - 2) / 2) + gScreenDimensions.TopRow - 1;
  Bottom    = Top + NumberOfLines + 2;

  Character = BOXDRAW_DOWN_RIGHT;
  PrintCharAt (Start, Top, Character);
  Character = BOXDRAW_HORIZONTAL;
  for (Index = Start; Index + 2 < End; Index++) {
    PrintChar (Character);
  }

  Character = BOXDRAW_DOWN_LEFT;
  PrintChar (Character);
  Character = BOXDRAW_VERTICAL;
  for (Index = Top; Index + 2 < Bottom; Index++) {
    String = ArrayOfStrings[Count];
    Count++;

    //
    // This will clear the background of the line - we never know who might have been
    // here before us.  This differs from the next clear in that it used the non-reverse
    // video for normal printing.
    //
    if (GetStringWidth (String) / 2 > 1) {
      ClearLines (Start, End, Index + 1, Index + 1, POPUP_TEXT | POPUP_BACKGROUND);
    }
    //
    // Passing in a space results in the assumption that this is where typing will occur
    //
    if (String[0] == L' ') {
      ClearLines (Start + 1, End - 1, Index + 1, Index + 1, POPUP_INVERSE_TEXT | POPUP_INVERSE_BACKGROUND);
    }
    //
    // Passing in a NULL results in a blank space
    //
    if (String[0] == CHAR_NULL) {
      ClearLines (Start, End, Index + 1, Index + 1, POPUP_TEXT | POPUP_BACKGROUND);
    }

    PrintStringAt (
      ((DimensionsWidth - GetStringWidth (String) / 2) / 2) + gScreenDimensions.LeftColumn + 1,
      Index + 1,
      String
      );
    gST->ConOut->SetAttribute (gST->ConOut, POPUP_TEXT | POPUP_BACKGROUND);
    PrintCharAt (Start, Index + 1, Character);
    PrintCharAt (End - 1, Index + 1, Character);
  }

  Character = BOXDRAW_UP_RIGHT;
  PrintCharAt (Start, Bottom - 1, Character);
  Character = BOXDRAW_HORIZONTAL;
  for (Index = Start; Index + 2 < End; Index++) {
    PrintChar (Character);
  }

  Character = BOXDRAW_UP_LEFT;
  PrintChar (Character);
}

VOID
CreatePopUp (
  IN  UINTN                       RequestedWidth,
  IN  UINTN                       NumberOfLines,
  IN  CHAR16                      *ArrayOfStrings,
  ...
  )
{
  CreateSharedPopUp (RequestedWidth, NumberOfLines, &ArrayOfStrings);
}

VOID
UpdateStatusBar (
  IN  UINTN                       MessageType,
  IN  UINT8                       Flags,
  IN  BOOLEAN                     State
  )
{
  UINTN           Index;
  STATIC BOOLEAN  InputError;
  CHAR16          *NvUpdateMessage;
  CHAR16          *InputErrorMessage;

  NvUpdateMessage   = GetToken (STRING_TOKEN (NV_UPDATE_MESSAGE), gHiiHandle);
  InputErrorMessage = GetToken (STRING_TOKEN (INPUT_ERROR_MESSAGE), gHiiHandle);

  switch (MessageType) {
  case INPUT_ERROR:
    if (State) {
      gST->ConOut->SetAttribute (gST->ConOut, ERROR_TEXT);
      PrintStringAt (
        gScreenDimensions.LeftColumn + gPromptBlockWidth,
        gScreenDimensions.BottomRow - 1,
        InputErrorMessage
        );
      InputError = TRUE;
    } else {
      gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_HIGHLIGHT);
      for (Index = 0; Index < (GetStringWidth (InputErrorMessage) - 2) / 2; Index++) {
        PrintAt (gScreenDimensions.LeftColumn + gPromptBlockWidth + Index, gScreenDimensions.BottomRow - 1, L"  ");
      }

      InputError = FALSE;
    }
    break;

  case NV_UPDATE_REQUIRED:
    if (gClassOfVfr != EFI_FRONT_PAGE_SUBCLASS) {
      if (State) {
        gST->ConOut->SetAttribute (gST->ConOut, INFO_TEXT);
        PrintStringAt (
          gScreenDimensions.LeftColumn + gPromptBlockWidth + gOptionBlockWidth,
          gScreenDimensions.BottomRow - 1,
          NvUpdateMessage
          );
        gResetRequired    = (BOOLEAN) (gResetRequired | ((Flags & EFI_IFR_FLAG_RESET_REQUIRED) == EFI_IFR_FLAG_RESET_REQUIRED));

        gNvUpdateRequired = TRUE;
      } else {
        gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_HIGHLIGHT);
        for (Index = 0; Index < (GetStringWidth (NvUpdateMessage) - 2) / 2; Index++) {
          PrintAt (
            (gScreenDimensions.LeftColumn + gPromptBlockWidth + gOptionBlockWidth + Index),
            gScreenDimensions.BottomRow - 1,
            L"  "
            );
        }

        gNvUpdateRequired = FALSE;
      }
    }
    break;

  case REFRESH_STATUS_BAR:
    if (InputError) {
      UpdateStatusBar (INPUT_ERROR, Flags, TRUE);
    }

    if (gNvUpdateRequired) {
      UpdateStatusBar (NV_UPDATE_REQUIRED, Flags, TRUE);
    }
    break;

  default:
    break;
  }

  gBS->FreePool (InputErrorMessage);
  gBS->FreePool (NvUpdateMessage);
  return ;
}

VOID
FreeData (
  IN EFI_FILE_FORM_TAGS           *FileFormTagsHead,
  IN CHAR16                       *FormattedString,
  IN CHAR16                       *OptionString
  )
/*++

Routine Description:
  
  Used to remove the allocated data instances

Arguments:
             
Returns:

--*/
{
  EFI_FILE_FORM_TAGS      *FileForm;
  EFI_FILE_FORM_TAGS      *PreviousFileForm;
  EFI_FORM_TAGS           *FormTags;
  EFI_FORM_TAGS           *PreviousFormTags;
  EFI_IFR_BINARY          *IfrBinary;
  EFI_IFR_BINARY          *PreviousIfrBinary;
  EFI_INCONSISTENCY_DATA  *Inconsistent;
  EFI_VARIABLE_DEFINITION *VariableDefinition;
  EFI_VARIABLE_DEFINITION *PreviousVariableDefinition;
  VOID                    *Buffer;
  UINTN                   Index;

  FileForm = FileFormTagsHead;

  if (FormattedString != NULL) {
    gBS->FreePool (FormattedString);
  }

  if (OptionString != NULL) {
    gBS->FreePool (OptionString);
  }

  for (; FileForm != NULL;) {
    PreviousFileForm = NULL;

    //
    // Advance FileForm to the last entry
    //
    for (; FileForm->NextFile != NULL; FileForm = FileForm->NextFile) {
      PreviousFileForm = FileForm;
    }

    FormTags = &FileForm->FormTags;

    for (; FormTags != NULL;) {
      FormTags          = &FileForm->FormTags;
      PreviousFormTags  = NULL;

      //
      // Advance FormTags to the last entry
      //
      for (; FormTags->Next != NULL; FormTags = FormTags->Next) {
        PreviousFormTags = FormTags;
      }
      //
      // Walk through each of the tags and free the IntList allocation
      //
      for (Index = 0; FormTags->Tags[Index].Operand != EFI_IFR_END_FORM_OP; Index++) {
        //
        // It is more than likely that the very last page will contain an end formset
        //
        if (FormTags->Tags[Index].Operand == EFI_IFR_END_FORM_SET_OP) {
          break;
        }

        if (FormTags->Tags[Index].IntList != NULL) {
          gBS->FreePool (FormTags->Tags[Index].IntList);
        }
      }

      if (PreviousFormTags != NULL) {
        gBS->FreePool (FormTags->Tags);
        FormTags = PreviousFormTags;
        gBS->FreePool (FormTags->Next);
        FormTags->Next = NULL;
      } else {
        gBS->FreePool (FormTags->Tags);
        FormTags = NULL;
      }
    }
    //
    // Last FileForm entry's Inconsistent database
    //
    Inconsistent = FileForm->InconsistentTags;

    //
    // Advance Inconsistent to the last entry
    //
    for (; Inconsistent->Next != NULL; Inconsistent = Inconsistent->Next)
      ;

    for (; Inconsistent != NULL;) {
      //
      // Preserve the Previous pointer
      //
      Buffer = (VOID *) Inconsistent->Previous;

      //
      // Free the current entry
      //
      gBS->FreePool (Inconsistent);

      //
      // Restore the Previous pointer
      //
      Inconsistent = (EFI_INCONSISTENCY_DATA *) Buffer;
    }

    VariableDefinition = FileForm->VariableDefinitions;

    for (; VariableDefinition != NULL;) {
      VariableDefinition          = FileForm->VariableDefinitions;
      PreviousVariableDefinition  = NULL;

      //
      // Advance VariableDefinitions to the last entry
      //
      for (; VariableDefinition->Next != NULL; VariableDefinition = VariableDefinition->Next) {
        PreviousVariableDefinition = VariableDefinition;
      }

      gBS->FreePool (VariableDefinition->VariableName);
      gBS->FreePool (VariableDefinition->NvRamMap);
      gBS->FreePool (VariableDefinition->FakeNvRamMap);

      if (PreviousVariableDefinition != NULL) {
        VariableDefinition = PreviousVariableDefinition;
        gBS->FreePool (VariableDefinition->Next);
        VariableDefinition->Next = NULL;
      } else {
        gBS->FreePool (VariableDefinition);
        VariableDefinition = NULL;
      }
    }

    if (PreviousFileForm != NULL) {
      FileForm = PreviousFileForm;
      gBS->FreePool (FileForm->NextFile);
      FileForm->NextFile = NULL;
    } else {
      gBS->FreePool (FileForm);
      FileForm = NULL;
    }
  }

  IfrBinary = gBinaryDataHead;

  for (; IfrBinary != NULL;) {
    IfrBinary         = gBinaryDataHead;
    PreviousIfrBinary = NULL;

    //
    // Advance IfrBinary to the last entry
    //
    for (; IfrBinary->Next != NULL; IfrBinary = IfrBinary->Next) {
      PreviousIfrBinary = IfrBinary;
    }

    gBS->FreePool (IfrBinary->IfrPackage);

    if (PreviousIfrBinary != NULL) {
      IfrBinary = PreviousIfrBinary;
      gBS->FreePool (IfrBinary->Next);
      IfrBinary->Next = NULL;
    } else {
      gBS->FreePool (IfrBinary);
      IfrBinary = NULL;
    }
  }

  gBS->FreePool (gPreviousValue);
  gPreviousValue = NULL;

  //
  // Free Browser Strings
  //
  gBS->FreePool (gPressEnter);
  gBS->FreePool (gConfirmError);
  gBS->FreePool (gConfirmPassword);
  gBS->FreePool (gPromptForNewPassword);
  gBS->FreePool (gPromptForPassword);
  gBS->FreePool (gToggleCheckBox);
  gBS->FreePool (gNumericInput);
  gBS->FreePool (gMakeSelection);
  gBS->FreePool (gMoveHighlight);
  gBS->FreePool (gEscapeString);
  gBS->FreePool (gEnterCommitString);
  gBS->FreePool (gEnterString);
  gBS->FreePool (gFunctionOneString);
  gBS->FreePool (gFunctionTwoString);
  gBS->FreePool (gFunctionNineString);
  gBS->FreePool (gFunctionTenString);
  return ;
}

BOOLEAN
SelectionsAreValid (
  IN  UI_MENU_OPTION               *MenuOption,
  IN  EFI_FILE_FORM_TAGS           *FileFormTagsHead
  )
/*++

Routine Description:
  Initiate late consistency checks against the current page.  

Arguments:
  None
           
Returns:

--*/
{
  EFI_LIST_ENTRY          *Link;
  EFI_TAG                 *Tag;
  EFI_FILE_FORM_TAGS      *FileFormTags;
  CHAR16                  *StringPtr;
  CHAR16                  NullCharacter;
  EFI_STATUS              Status;
  UINTN                   Index;
  UINT16                  *NvRamMap;
  STRING_REF              PopUp;
  EFI_INPUT_KEY           Key;
  EFI_VARIABLE_DEFINITION *VariableDefinition;

  StringPtr     = L"\0";
  NullCharacter = CHAR_NULL;

  FileFormTags  = FileFormTagsHead;

  for (Index = 0; Index < MenuOption->IfrNumber; Index++) {
    FileFormTags = FileFormTags->NextFile;
  }

  for (Link = Menu.ForwardLink; Link != &Menu; Link = Link->ForwardLink) {
    MenuOption  = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

    Tag         = MenuOption->ThisTag;

    ExtractRequestedNvMap (FileFormTags, Tag->VariableNumber, &VariableDefinition);
    NvRamMap = (UINT16 *) &VariableDefinition->NvRamMap[Tag->StorageStart];

    //
    // If the op-code has a late check, ensure consistency checks are now applied
    //
    if (Tag->Flags & EFI_IFR_FLAG_LATE_CHECK) {
      if (ValueIsNotValid (TRUE, 0, Tag, FileFormTags, &PopUp)) {
        if (PopUp != 0x0000) {
          StringPtr = GetToken (PopUp, MenuOption->Handle);

          CreatePopUp (GetStringWidth (StringPtr) / 2, 3, &NullCharacter, StringPtr, &NullCharacter);

          do {
            Status = WaitForKeyStroke (&Key);

            switch (Key.UnicodeChar) {

            case CHAR_CARRIAGE_RETURN:
              //
              // Since the value can be one byte long or two bytes long, do a CopyMem based on StorageWidth
              //
              EfiCopyMem (NvRamMap, &Tag->OldValue, Tag->StorageWidth);
              gBS->FreePool (StringPtr);
              break;

            default:
              break;
            }
          } while (Key.UnicodeChar != CHAR_CARRIAGE_RETURN);
        }

        return FALSE;
      }
    }
  }

  return TRUE;
}

UINT16
GetWidth (
  IN EFI_TAG                        *Tag,
  IN EFI_HII_HANDLE                 Handle
  )
/*++

Routine Description:
  Get the supported width for a particular op-code

Arguments:
  Tag - The Tag structure passed in.
  Handle - The handle in the HII database being used
  
Returns:
  Returns the number of CHAR16 characters that is support.


--*/
{
  CHAR16  *String;
  UINTN   Size;

  Size = 0x00;

  //
  // See if the second text parameter is really NULL
  //
  if ((Tag->Operand == EFI_IFR_TEXT_OP) && (Tag->TextTwo != 0)) {
    String  = GetToken (Tag->TextTwo, Handle);
    Size    = EfiStrLen (String);
    gBS->FreePool (String);
  }

  if ((Tag->Operand == EFI_IFR_SUBTITLE_OP) ||
      (Tag->Operand == EFI_IFR_REF_OP) ||
      (Tag->Operand == EFI_IFR_PASSWORD_OP) ||
      (Tag->Operand == EFI_IFR_STRING_OP) ||
      (Tag->Operand == EFI_IFR_INVENTORY_OP) ||
      //
      // Allow a wide display if text op-code and no secondary text op-code
      //
      ((Tag->Operand == EFI_IFR_TEXT_OP) && (Size == 0x0000))
      ) {
    return (UINT16) (gPromptBlockWidth + gOptionBlockWidth);
  } else {
    return (UINT16) gPromptBlockWidth;
  }
}

UINT16
GetLineByWidth (
  IN      CHAR16                      *InputString,
  IN      UINT16                      LineWidth,
  IN OUT  UINTN                       *Index,
  OUT     CHAR16                      **OutputString
  )
/*++

Routine Description:
  Will copy LineWidth amount of a string in the OutputString buffer and return the
  number of CHAR16 characters that were copied into the OutputString buffer.

Arguments:
  InputString - String description for this option.
  LineWidth - Width of the desired string to extract in CHAR16 characters
  Index - Where in InputString to start the copy process
  OutputString - Buffer to copy the string into
           
Returns:
  Returns the number of CHAR16 characters that were copied into the OutputString buffer.


--*/
{
  static BOOLEAN  Finished;
  UINT16          Count;
  UINT16          Count2;

  if (Finished) {
    Finished = FALSE;
    return (UINT16) 0;
  }

  Count         = LineWidth;
  Count2        = 0;

  *OutputString = EfiLibAllocateZeroPool (((UINTN) (LineWidth + 1) * 2));

  //
  // Ensure we have got a valid buffer
  //
  if (*OutputString != NULL) {
  
    //
    //NARROW_CHAR can not be printed in screen, so if a line only contain  the two CHARs: 'NARROW_CHAR + CHAR_CARRIAGE_RETURN' , it is a empty line  in Screen.
    //To avoid displaying this  empty line in screen,  just skip  the two CHARs here.
    //
   if ((InputString[*Index] == NARROW_CHAR) && (InputString[*Index + 1] == CHAR_CARRIAGE_RETURN)) {
     *Index = *Index + 2;
   } 

    //
    // Fast-forward the string and see if there is a carriage-return in the string
    //
    for (; (InputString[*Index + Count2] != CHAR_CARRIAGE_RETURN) && (Count2 != LineWidth); Count2++)
      ;

    //
    // Copy the desired LineWidth of data to the output buffer.
    // Also make sure that we don't copy more than the string.
    // Also make sure that if there are linefeeds, we account for them.
    //
    if ((EfiStrSize (&InputString[*Index]) <= ((UINTN) (LineWidth + 1) * 2)) &&
        (EfiStrSize (&InputString[*Index]) <= ((UINTN) (Count2 + 1) * 2))
        ) {
      //
      // Convert to CHAR16 value and show that we are done with this operation
      //
      LineWidth = (UINT16) ((EfiStrSize (&InputString[*Index]) - 2) / 2);
      if (LineWidth != 0) {
        Finished = TRUE;
      }
    } else {
      if (Count2 == LineWidth) {
        //
        // Rewind the string from the maximum size until we see a space to break the line
        //
        for (; (InputString[*Index + LineWidth] != CHAR_SPACE) && (LineWidth != 0); LineWidth--)
          ;
        if (LineWidth == 0) {
          LineWidth = Count;
        }
      } else {
        LineWidth = Count2;
      }
    }

    EfiCopyMem (*OutputString, &InputString[*Index], LineWidth * 2);

    //
    // If currently pointing to a space, increment the index to the first non-space character
    //
    for (;
         (InputString[*Index + LineWidth] == CHAR_SPACE) || (InputString[*Index + LineWidth] == CHAR_CARRIAGE_RETURN);
         (*Index)++
        )
      ;
    *Index = (UINT16) (*Index + LineWidth);
    return LineWidth;
  } else {
    return (UINT16) 0;
  }
}

VOID
UpdateOptionSkipLines (
  IN EFI_IFR_DATA_ARRAY           *PageData,
  IN UI_MENU_OPTION               *MenuOption,
  IN EFI_FILE_FORM_TAGS           *FileFormTagsHead,
  IN CHAR16                       **OptionalString,
  IN UINTN                        SkipValue
  )
{
  UINTN   Index;
  UINT16  Width;
  UINTN   Row;
  UINTN   OriginalRow;
  CHAR16  *OutputString;
  CHAR16  *OptionString;

  Row           = 0;
  OptionString  = *OptionalString;
  OutputString  = NULL;

  ProcessOptions (MenuOption, FALSE, FileFormTagsHead, PageData, &OptionString);

  if (OptionString != NULL) {
    Width               = (UINT16) gOptionBlockWidth;

    OriginalRow         = Row;

    for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
      //
      // If there is more string to process print on the next row and increment the Skip value
      //
      if (EfiStrLen (&OptionString[Index])) {
        if (SkipValue == 0) {
          Row++;
          //
          // Since the Number of lines for this menu entry may or may not be reflected accurately
          // since the prompt might be 1 lines and option might be many, and vice versa, we need to do
          // some testing to ensure we are keeping this in-sync.
          //
          // If the difference in rows is greater than or equal to the skip value, increase the skip value
          //
          if ((Row - OriginalRow) >= MenuOption->Skip) {
            MenuOption->Skip++;
          }
        }
      }

      gBS->FreePool (OutputString);
      if (SkipValue != 0) {
        SkipValue--;
      }
    }

    Row = OriginalRow;
  }

  *OptionalString = OptionString;
}
//
// Search table for UiDisplayMenu()
//
SCAN_CODE_TO_SCREEN_OPERATION     gScanCodeToOperation[] = {
  SCAN_UP,
  UiUp,
  SCAN_DOWN,
  UiDown,
  SCAN_PAGE_UP,
  UiPageUp,
  SCAN_PAGE_DOWN,
  UiPageDown,
  SCAN_ESC,
  UiReset,
  SCAN_F2,
  UiPrevious,
  SCAN_LEFT,
  UiLeft,
  SCAN_RIGHT,
  UiRight,
  SCAN_F9,
  UiDefault,
  SCAN_F10,
  UiSave
};

SCREEN_OPERATION_T0_CONTROL_FLAG  gScreenOperationToControlFlag[] = {
  UiNoOperation,
  CfUiNoOperation,
  UiDefault,
  CfUiDefault,
  UiSelect,
  CfUiSelect,
  UiUp,
  CfUiUp,
  UiDown,
  CfUiDown,
  UiLeft,
  CfUiLeft,
  UiRight,
  CfUiRight,
  UiReset,
  CfUiReset,
  UiSave,
  CfUiSave,
  UiPrevious,
  CfUiPrevious,
  UiPageUp,
  CfUiPageUp,
  UiPageDown,
  CfUiPageDown
};

STATIC
INTN
SkipNoFocusTag (
  IN     BOOLEAN                   GoUp,
  IN OUT EFI_LIST_ENTRY            **CurrentPosition
  )
{
  INTN             Distance;
  EFI_LIST_ENTRY   *Pos;
  BOOLEAN          NothingLeft;
  UI_MENU_OPTION   *NextMenuOption;

  Distance    = 0;
  Pos         = *CurrentPosition;
  NothingLeft = FALSE;
  while (TRUE) {
    NextMenuOption = CR (Pos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
    if (!(NextMenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP || NextMenuOption->ThisTag->GrayOut)) {
      break;
    }
    if ((GoUp ? Pos->BackLink : Pos->ForwardLink) == &Menu) {
      NothingLeft = TRUE;
      break;
    }
    Distance      += NextMenuOption->Skip;
    Pos            = (GoUp ? Pos->BackLink : Pos->ForwardLink);
  }

  if (NothingLeft) {
    //
    // If we hit end there is still no TAG can be focused, 
    //  we go backwards to find the tag can be focused.
    //
    Distance    = 0;
    Pos         = *CurrentPosition;
    while (TRUE) {
      NextMenuOption = CR (Pos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
      if (!(NextMenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP || NextMenuOption->ThisTag->GrayOut)) {
        break;
      }
      if ((!GoUp ? Pos->BackLink : Pos->ForwardLink) == &Menu) {
        ASSERT (FALSE);
        break;
      }
      Distance      -= NextMenuOption->Skip;
      Pos            = (!GoUp ? Pos->BackLink : Pos->ForwardLink);
    }
  }
  *CurrentPosition = &NextMenuOption->Link;
  return Distance;
}

UI_MENU_OPTION *
UiDisplayMenu (
  IN  BOOLEAN                      SubMenu,
  IN  EFI_FILE_FORM_TAGS           *FileFormTagsHead,
  OUT EFI_IFR_DATA_ARRAY           *PageData
  )
/*++

Routine Description:
  Display menu and wait for user to select one menu option, then return it.
  If AutoBoot is enabled, then if user doesn't select any option,
  after period of time, it will automatically return the first menu option.

Arguments:
  SubMenu          - Indicate is sub menu.
  FileFormTagsHead - A pointer to the EFI_FILE_FORM_TAGS structure.
  PageData         - A pointer to the EFI_IFR_DATA_ARRAY.
           
Returns:
  Return the pointer of the menu which selected, 
  otherwise return NULL.

--*/
{
  INTN                        SkipValue;
  INTN                        Difference;
  INTN                        OldSkipValue;
  UINTN                       DistanceValue;
  UINTN                       Row;
  UINTN                       Col;
  UINTN                       Temp;
  UINTN                       Temp2;
  UINTN                       TopRow;
  UINTN                       BottomRow;
  UINTN                       OriginalRow;
  UINTN                       Index;
  UINT32                      Count;
  UINT16                      OriginalTimeOut;
  UINT8                       *Location;
  UINT16                      Width;
  CHAR16                      *StringPtr;
  CHAR16                      *OptionString;
  CHAR16                      *OutputString;
  CHAR16                      *FormattedString;
  CHAR16                      YesResponse;
  CHAR16                      NoResponse;
  BOOLEAN                     NewLine;
  BOOLEAN                     Repaint;
  BOOLEAN                     SavedValue;
  EFI_STATUS                  Status;
  UI_MENU_LIST                *UiMenuList;
  EFI_INPUT_KEY               Key;
  EFI_LIST_ENTRY              *Link;
  EFI_LIST_ENTRY              *NewPos;
  EFI_LIST_ENTRY              *TopOfScreen;
  EFI_LIST_ENTRY              *SavedListEntry;
  UI_MENU_OPTION              *Selection;
  UI_MENU_OPTION              *MenuOption;
  UI_MENU_OPTION              *NextMenuOption;
  UI_MENU_OPTION              *SavedMenuOption;
  UI_MENU_OPTION              *PreviousMenuOption;
  EFI_IFR_BINARY              *IfrBinary;
  UI_CONTROL_FLAG             ControlFlag;
  SCREEN_DESCRIPTOR           LocalScreen;
  EFI_FILE_FORM_TAGS          *FileFormTags;
  MENU_REFRESH_ENTRY          *MenuRefreshEntry;
  MENU_REFRESH_ENTRY          *OldMenuRefreshEntry;
  UI_SCREEN_OPERATION         ScreenOperation;
  EFI_VARIABLE_DEFINITION     *VariableDefinition;
  EFI_FORM_CALLBACK_PROTOCOL  *FormCallback;
  EFI_HII_VARIABLE_PACK_LIST  *NvMapListHead;
  EFI_HII_VARIABLE_PACK_LIST  *NvMapListNode;
  VOID                        *NvMap;
  UINTN                       NvMapSize;

  EfiCopyMem (&LocalScreen, &gScreenDimensions, sizeof (SCREEN_DESCRIPTOR));

  VariableDefinition  = NULL;
  Status              = EFI_SUCCESS;
  FormattedString     = NULL;
  OptionString        = NULL;
  ScreenOperation     = UiNoOperation;
  NewLine             = TRUE;
  FormCallback        = NULL;
  FileFormTags        = NULL;
  OutputString        = NULL;
  gUpArrow            = FALSE;
  gDownArrow          = FALSE;
  SkipValue           = 0;
  OldSkipValue        = 0;
  MenuRefreshEntry    = gMenuRefreshHead;
  OldMenuRefreshEntry = gMenuRefreshHead;
  NextMenuOption      = NULL;
  PreviousMenuOption  = NULL;
  SavedMenuOption     = NULL;
  IfrBinary           = NULL;
  NvMap               = NULL;
  NvMapSize           = 0;

  EfiZeroMem (&Key, sizeof (EFI_INPUT_KEY));

  if (gClassOfVfr == EFI_FRONT_PAGE_SUBCLASS) {
    TopRow  = LocalScreen.TopRow + FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
    Row     = LocalScreen.TopRow + FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
  } else {
    TopRow  = LocalScreen.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
    Row     = LocalScreen.TopRow + NONE_FRONT_PAGE_HEADER_HEIGHT + SCROLL_ARROW_HEIGHT;
  }

  if (SubMenu) {
    Col = LocalScreen.LeftColumn;
  } else {
    Col = LocalScreen.LeftColumn + LEFT_SKIPPED_COLUMNS;
  }

  BottomRow   = LocalScreen.BottomRow - STATUS_BAR_HEIGHT - FOOTER_HEIGHT - SCROLL_ARROW_HEIGHT - 1;

  TopOfScreen = Menu.ForwardLink;
  Repaint     = TRUE;
  MenuOption  = NULL;

  //
  // Get user's selection
  //
  Selection = NULL;
  NewPos    = Menu.ForwardLink;
  gST->ConOut->EnableCursor (gST->ConOut, FALSE);

  UpdateStatusBar (REFRESH_STATUS_BAR, (UINT8) 0, TRUE);

  ControlFlag = CfInitialization;

  while (TRUE) {
    switch (ControlFlag) {
    case CfInitialization:
      ControlFlag = CfCheckSelection;
      if (gExitRequired) {
        ScreenOperation = UiReset;
        ControlFlag     = CfScreenOperation;
      } else if (gSaveRequired) {
        ScreenOperation = UiSave;
        ControlFlag     = CfScreenOperation;
      } else if (IsListEmpty (&Menu)) {
        ControlFlag = CfReadKey;
      }
      break;

    case CfCheckSelection:
      if (Selection != NULL) {
        ControlFlag = CfExit;
      } else {
        ControlFlag = CfRepaint;
      }

      FileFormTags = FileFormTagsHead;
      break;

    case CfRepaint:
      ControlFlag = CfRefreshHighLight;

      if (Repaint) {
        //
        // Display menu
        //
        gDownArrow      = FALSE;
        gUpArrow        = FALSE;
        Row             = TopRow;

        Temp            = SkipValue;
        Temp2           = SkipValue;

        ClearLines (
          LocalScreen.LeftColumn,
          LocalScreen.RightColumn,
          TopRow - SCROLL_ARROW_HEIGHT,
          BottomRow + SCROLL_ARROW_HEIGHT,
          FIELD_TEXT | FIELD_BACKGROUND
          );

        while (gMenuRefreshHead != NULL) {
          OldMenuRefreshEntry = gMenuRefreshHead->Next;

          gBS->FreePool (gMenuRefreshHead);

          gMenuRefreshHead = OldMenuRefreshEntry;
        }

        for (Link = TopOfScreen; Link != &Menu; Link = Link->ForwardLink) {
          MenuOption          = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
          MenuOption->Row     = Row;
          OriginalRow         = Row;
          MenuOption->Col     = Col;
          MenuOption->OptCol  = gPromptBlockWidth + 1 + LocalScreen.LeftColumn;

          if (SubMenu) {
            if (MenuOption->ThisTag->GrayOut) {
              gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_GRAYED | FIELD_BACKGROUND);
            } else {
              if (MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP) {
                gST->ConOut->SetAttribute (gST->ConOut, SUBTITLE_TEXT | FIELD_BACKGROUND);
              }
            }

            Width       = GetWidth (MenuOption->ThisTag, MenuOption->Handle);

            OriginalRow = Row;

            for (Index = 0; GetLineByWidth (MenuOption->Description, Width, &Index, &OutputString) != 0x0000;) {
              if ((Temp == 0) && (Row <= BottomRow)) {
                PrintStringAt (Col, Row, OutputString);
              }
              //
              // If there is more string to process print on the next row and increment the Skip value
              //
              if (EfiStrLen (&MenuOption->Description[Index])) {
                if (Temp == 0) {
                  Row++;
                }
              }

              gBS->FreePool (OutputString);
              if (Temp != 0) {
                Temp--;
              }
            }

            Temp  = 0;

            Row   = OriginalRow;

            gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
            ProcessOptions (MenuOption, FALSE, FileFormTagsHead, PageData, &OptionString);

            if (OptionString != NULL) {
              if (MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP ||
                  MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP
                  ) {
                //
                // If leading spaces on OptionString - remove the spaces
                //
                for (Index = 0; OptionString[Index] == L' '; Index++) {
                  MenuOption->OptCol++;
                }

                for (Count = 0; OptionString[Index] != CHAR_NULL; Index++) {
                  OptionString[Count] = OptionString[Index];
                  Count++;
                }

                OptionString[Count] = CHAR_NULL;
              }

              //
              // If this is a date or time op-code and is used to reflect an RTC, register the op-code
              //
                if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP ||
                     MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP) &&
                    (MenuOption->ThisTag->StorageStart >= FileFormTags->FormTags.Tags[0].NvDataSize)) {

                if (gMenuRefreshHead == NULL) {
                  MenuRefreshEntry = EfiLibAllocateZeroPool (sizeof (MENU_REFRESH_ENTRY));
                  ASSERT (MenuRefreshEntry != NULL);
                  MenuRefreshEntry->MenuOption        = MenuOption;
                  MenuRefreshEntry->FileFormTagsHead  = FileFormTagsHead;
                  MenuRefreshEntry->CurrentColumn     = MenuOption->OptCol;
                  MenuRefreshEntry->CurrentRow        = MenuOption->Row;
                  MenuRefreshEntry->CurrentAttribute  = FIELD_TEXT | FIELD_BACKGROUND;
                  gMenuRefreshHead                    = MenuRefreshEntry;
                } else {
                  //
                  // Advance to the last entry
                  //
                  for (MenuRefreshEntry = gMenuRefreshHead;
                       MenuRefreshEntry->Next != NULL;
                       MenuRefreshEntry = MenuRefreshEntry->Next
                      )
                    ;
                  MenuRefreshEntry->Next = EfiLibAllocateZeroPool (sizeof (MENU_REFRESH_ENTRY));
                  ASSERT (MenuRefreshEntry->Next != NULL);
                  MenuRefreshEntry                    = MenuRefreshEntry->Next;
                  MenuRefreshEntry->MenuOption        = MenuOption;
                  MenuRefreshEntry->FileFormTagsHead  = FileFormTagsHead;
                  MenuRefreshEntry->CurrentColumn     = MenuOption->OptCol;
                  MenuRefreshEntry->CurrentRow        = MenuOption->Row;
                  MenuRefreshEntry->CurrentAttribute  = FIELD_TEXT | FIELD_BACKGROUND;
                }
              }

              Width       = (UINT16) gOptionBlockWidth;

              OriginalRow = Row;

              for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
                if ((Temp2 == 0) && (Row <= BottomRow)) {
                  PrintStringAt (MenuOption->OptCol, Row, OutputString);
                }
                //
                // If there is more string to process print on the next row and increment the Skip value
                //
                if (EfiStrLen (&OptionString[Index])) {
                  if (Temp2 == 0) {
                    Row++;
                    //
                    // Since the Number of lines for this menu entry may or may not be reflected accurately
                    // since the prompt might be 1 lines and option might be many, and vice versa, we need to do
                    // some testing to ensure we are keeping this in-sync.
                    //
                    // If the difference in rows is greater than or equal to the skip value, increase the skip value
                    //
                    if ((Row - OriginalRow) >= MenuOption->Skip) {
                      MenuOption->Skip++;
                    }
                  }
                }

                gBS->FreePool (OutputString);
                if (Temp2 != 0) {
                  Temp2--;
                }
              }

              Temp2 = 0;
              Row   = OriginalRow;
            }
            //
            // If this is a text op with secondary text information
            //
            if ((MenuOption->ThisTag->Operand == EFI_IFR_TEXT_OP) && (MenuOption->ThisTag->TextTwo != 0)) {
              StringPtr   = GetToken (MenuOption->ThisTag->TextTwo, MenuOption->Handle);

              Width       = (UINT16) gOptionBlockWidth;

              OriginalRow = Row;

              for (Index = 0; GetLineByWidth (StringPtr, Width, &Index, &OutputString) != 0x0000;) {
                if ((Temp == 0) && (Row <= BottomRow)) {
                  PrintStringAt (MenuOption->OptCol, Row, OutputString);
                }
                //
                // If there is more string to process print on the next row and increment the Skip value
                //
                if (EfiStrLen (&StringPtr[Index])) {
                  if (Temp2 == 0) {
                    Row++;
                    //
                    // Since the Number of lines for this menu entry may or may not be reflected accurately
                    // since the prompt might be 1 lines and option might be many, and vice versa, we need to do
                    // some testing to ensure we are keeping this in-sync.
                    //
                    // If the difference in rows is greater than or equal to the skip value, increase the skip value
                    //
                    if ((Row - OriginalRow) >= MenuOption->Skip) {
                      MenuOption->Skip++;
                    }
                  }
                }

                gBS->FreePool (OutputString);
                if (Temp2 != 0) {
                  Temp2--;
                }
              }

              Row = OriginalRow;
              gBS->FreePool (StringPtr);
            }
          } else {
            //
            // For now, assume left-justified 72 width max setup entries
            //
            PrintStringAt (Col, Row, MenuOption->Description);
          }
          //
          // Tracker 6210 - need to handle the bottom of the display
          //
          if (MenuOption->Skip > 1) {
            Row += MenuOption->Skip - SkipValue;
            SkipValue = 0;
          } else {
            Row += MenuOption->Skip;
          }

          if (Row > BottomRow) {
            if (!ValueIsScroll (FALSE, Link)) {
              gDownArrow = TRUE;
            }

            Row = BottomRow + 1;
            break;
          }
        }

        if (!ValueIsScroll (TRUE, TopOfScreen)) {
          gUpArrow = TRUE;
        }

        if (gUpArrow) {
          gST->ConOut->SetAttribute (gST->ConOut, ARROW_TEXT | ARROW_BACKGROUND);
          PrintAt (
            LocalScreen.LeftColumn + gPromptBlockWidth + gOptionBlockWidth + 1,
            TopRow - SCROLL_ARROW_HEIGHT,
            L"%c",
            ARROW_UP
            );
          gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
        }

        if (gDownArrow) {
          gST->ConOut->SetAttribute (gST->ConOut, ARROW_TEXT | ARROW_BACKGROUND);
          PrintAt (
            LocalScreen.LeftColumn + gPromptBlockWidth + gOptionBlockWidth + 1,
            BottomRow + SCROLL_ARROW_HEIGHT,
            L"%c",
            ARROW_DOWN
            );
          gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
        }

        MenuOption = NULL;
      }
      break;

    case CfRefreshHighLight:
      //
      // MenuOption: Last menu option that need to remove hilight
      //             MenuOption is set to NULL in Repaint
      // NewPos:     Current menu option that need to hilight
      //
      ControlFlag = CfUpdateHelpString;
      //
      // Repaint flag is normally reset when finish processing CfUpdateHelpString. Temporarily
      // reset Repaint flag because we may break halfway and skip CfUpdateHelpString processing.
      //
      SavedValue  = Repaint;
      Repaint     = FALSE;

      if (NewPos != NULL && (MenuOption == NULL || NewPos != &MenuOption->Link)) {
        if (SubMenu && gLastOpr && (gEntryNumber != -1)) {
          while (NewPos != &Menu) {
            MenuOption = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
            if (gEntryNumber == MenuOption->EntryNumber) {
              break;
            }
            NewPos     = NewPos->ForwardLink;
          }
          
          if (gEntryNumber == MenuOption->EntryNumber) {
            //
            // If NewPos is not in the current page, simply scroll page so that NewPos is in the end of the page
            //
            Link = TopOfScreen;

            for (Index = TopRow; Index <= BottomRow && Link != NewPos;) {
              MenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
              Index     += MenuOption->Skip;
              Link       = Link->ForwardLink;
            }
            if ((Link == NewPos) && (Index <= BottomRow)) {
              //
              // NewPos is in the current page, needn't scroll page
              //
              ControlFlag     = CfRefreshHighLight;
            } else {
              Repaint = TRUE;
              Link    = NewPos;
              for (Index = TopRow; Index <= BottomRow; ) {
                Link       = Link->BackLink;
                MenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
                Index     += MenuOption->Skip;
              }
              TopOfScreen     = Link->ForwardLink;
              ControlFlag     = CfRepaint;
            }
          } else {
            ControlFlag     = CfInitialization;
          }
          ScreenOperation = UiNoOperation;
          gLastOpr        = FALSE;
          MenuOption      = NULL;
          break;
        }

        if (MenuOption != NULL) {
          if (SubMenu) {
            gST->ConOut->SetCursorPosition (gST->ConOut, MenuOption->Col, MenuOption->Row);
            ProcessOptions (MenuOption, FALSE, FileFormTagsHead, PageData, &OptionString);
            gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
            if (OptionString != NULL) {
              if (MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP ||
                  MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP
                  ) {
                //
                // If leading spaces on OptionString - remove the spaces
                //
                for (Index = 0; OptionString[Index] == L' '; Index++)
                  ;

                for (Count = 0; OptionString[Index] != CHAR_NULL; Index++) {
                  OptionString[Count] = OptionString[Index];
                  Count++;
                }

                OptionString[Count] = CHAR_NULL;
              }

              Width               = (UINT16) gOptionBlockWidth;

              OriginalRow         = MenuOption->Row;

              for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
                if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                  PrintStringAt (MenuOption->OptCol, MenuOption->Row, OutputString);
                }
                //
                // If there is more string to process print on the next row and increment the Skip value
                //
                if (EfiStrLen (&OptionString[Index])) {
                  MenuOption->Row++;
                }

                gBS->FreePool (OutputString);
              }

              MenuOption->Row = OriginalRow;
            } else {
              if (NewLine) {
                if (MenuOption->ThisTag->GrayOut) {
                  gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_GRAYED | FIELD_BACKGROUND);
                } else {
                  if (MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP) {
                    gST->ConOut->SetAttribute (gST->ConOut, SUBTITLE_TEXT | FIELD_BACKGROUND);
                  }
                }

                OriginalRow = MenuOption->Row;
                Width       = GetWidth (MenuOption->ThisTag, MenuOption->Handle);

                for (Index = 0; GetLineByWidth (MenuOption->Description, Width, &Index, &OutputString) != 0x0000;) {
                  if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                    PrintStringAt (Col, MenuOption->Row, OutputString);
                  }
                  //
                  // If there is more string to process print on the next row and increment the Skip value
                  //
                  if (EfiStrLen (&MenuOption->Description[Index])) {
                    MenuOption->Row++;
                  }

                  gBS->FreePool (OutputString);
                }

                MenuOption->Row = OriginalRow;
                gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
              }
            }
          } else {
            gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
            gST->ConOut->OutputString (gST->ConOut, MenuOption->Description);
          }
        }


        MenuOption = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

        if (gPriorMenuEntry != 0) {
          while (MenuOption->EntryNumber != gPriorMenuEntry && NewPos->ForwardLink != &Menu) {
            NewPos     = NewPos->ForwardLink;
            MenuOption = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
          }
          if (MenuOption->EntryNumber == gPriorMenuEntry) {
            //
            // If NewPos is not in the current page, simply scroll page so that NewPos is in the end of the page
            //
            Link = TopOfScreen;

            for (Index = TopRow; Index <= BottomRow && Link != NewPos;) {
              MenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
              Index     += MenuOption->Skip;
              Link       = Link->ForwardLink;
            }
            if ((Link == NewPos) && (Index <= BottomRow)) {
              //
              // NewPos is in the current page, needn't scroll page
              //
              ScreenOperation = UiNoOperation;
              ControlFlag     = CfRefreshHighLight;
            } else {
              Repaint = TRUE;
              Link    = NewPos;
              for (Index = TopRow; Index <= BottomRow; ) {
                Link       = Link->BackLink;
                MenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
                Index     += MenuOption->Skip;
              }
              TopOfScreen     = Link->ForwardLink;
              ScreenOperation = UiNoOperation;
              ControlFlag     = CfRepaint;
            }
            gPriorMenuEntry = 0;
            MenuOption      = NULL;
            break;
          }
          gPriorMenuEntry = 0;
        }

        //
        // This is only possible if we entered this page and the first menu option is
        // a "non-menu" item.  In that case, force it UiDown
        //
        if (MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP || MenuOption->ThisTag->GrayOut) {
          ASSERT (ScreenOperation == UiNoOperation);
          ScreenOperation = UiDown;
          ControlFlag     = CfScreenOperation;
          break;
        }
        //
        // Set reverse attribute
        //
        gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT_HIGHLIGHT | FIELD_BACKGROUND_HIGHLIGHT);
        gST->ConOut->SetCursorPosition (gST->ConOut, MenuOption->Col, MenuOption->Row);

        //
        // Assuming that we have a refresh linked-list created, lets annotate the
        // appropriate entry that we are highlighting with its new attribute.  Just prior to this
        // lets reset all of the entries' attribute so we do not get multiple highlights in he refresh
        //
        if (gMenuRefreshHead != NULL) {
          for (MenuRefreshEntry = gMenuRefreshHead; MenuRefreshEntry != NULL; MenuRefreshEntry = MenuRefreshEntry->Next) {
            MenuRefreshEntry->CurrentAttribute = FIELD_TEXT | FIELD_BACKGROUND;
            if (MenuRefreshEntry->MenuOption == MenuOption) {
              MenuRefreshEntry->CurrentAttribute = FIELD_TEXT_HIGHLIGHT | FIELD_BACKGROUND_HIGHLIGHT;
            }
          }
        }

        if (SubMenu) {
          ProcessOptions (MenuOption, FALSE, FileFormTagsHead, PageData, &OptionString);
          if (OptionString != NULL) {
            if (MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP ||
                MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP
                ) {
              //
              // If leading spaces on OptionString - remove the spaces
              //
              for (Index = 0; OptionString[Index] == L' '; Index++)
                ;

              for (Count = 0; OptionString[Index] != CHAR_NULL; Index++) {
                OptionString[Count] = OptionString[Index];
                Count++;
              }

              OptionString[Count] = CHAR_NULL;
            }
            Width               = (UINT16) gOptionBlockWidth;

            OriginalRow         = MenuOption->Row;

            for (Index = 0; GetLineByWidth (OptionString, Width, &Index, &OutputString) != 0x0000;) {
              if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                PrintStringAt (MenuOption->OptCol, MenuOption->Row, OutputString);
              }
              //
              // If there is more string to process print on the next row and increment the Skip value
              //
              if (EfiStrLen (&OptionString[Index])) {
                MenuOption->Row++;
              }

              gBS->FreePool (OutputString);
            }

            MenuOption->Row = OriginalRow;
          } else {
            if (NewLine) {
              OriginalRow = MenuOption->Row;

              Width       = GetWidth (MenuOption->ThisTag, MenuOption->Handle);

              for (Index = 0; GetLineByWidth (MenuOption->Description, Width, &Index, &OutputString) != 0x0000;) {
                if (MenuOption->Row >= TopRow && MenuOption->Row <= BottomRow) {
                  PrintStringAt (Col, MenuOption->Row, OutputString);
                }
                //
                // If there is more string to process print on the next row and increment the Skip value
                //
                if (EfiStrLen (&MenuOption->Description[Index])) {
                  MenuOption->Row++;
                }

                gBS->FreePool (OutputString);
              }

              MenuOption->Row = OriginalRow;

            }
          }

          if (((NewPos->ForwardLink != &Menu) && (ScreenOperation == UiDown)) ||
              ((NewPos->BackLink != &Menu) && (ScreenOperation == UiUp)) ||
              (ScreenOperation == UiNoOperation)
              ) {
            UpdateKeyHelp (MenuOption, FALSE);
          }
        } else {
          gST->ConOut->OutputString (gST->ConOut, MenuOption->Description);
        }
        //
        // Clear reverse attribute
        //
        gST->ConOut->SetAttribute (gST->ConOut, FIELD_TEXT | FIELD_BACKGROUND);
      }
      //
      // Repaint flag will be used when process CfUpdateHelpString, so restore its value
      // if we didn't break halfway when process CfRefreshHighLight.
      //
      Repaint = SavedValue;
      break;

    case CfUpdateHelpString:
      ControlFlag = CfPrepareToReadKey;

        if (SubMenu && 
            (Repaint || NewLine || 
             (MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) ||
             (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) && 
            !(gClassOfVfr == EFI_GENERAL_APPLICATION_SUBCLASS)) {
        //
        // Don't print anything if it is a NULL help token
        //
        if (MenuOption->ThisTag->Help == 0x00000000) {
          StringPtr = L"\0";
        } else {
          StringPtr = GetToken (MenuOption->ThisTag->Help, MenuOption->Handle);
        }

        ProcessHelpString (StringPtr, &FormattedString, BottomRow - TopRow);

        gST->ConOut->SetAttribute (gST->ConOut, HELP_TEXT | FIELD_BACKGROUND);

        for (Index = 0; Index < BottomRow - TopRow; Index++) {
          //
          // Pad String with spaces to simulate a clearing of the previous line
          //
          for (; GetStringWidth (&FormattedString[Index * gHelpBlockWidth * 2]) / 2 < gHelpBlockWidth;) {
            EfiStrCat (&FormattedString[Index * gHelpBlockWidth * 2], L" ");
          }

          PrintStringAt (
            LocalScreen.RightColumn - gHelpBlockWidth,
            Index + TopRow,
            &FormattedString[Index * gHelpBlockWidth * 2]
            );
        }
      }
      //
      // Reset this flag every time we finish using it.
      //
      Repaint = FALSE;
      NewLine = FALSE;
      break;

    case CfPrepareToReadKey:
      ControlFlag = CfReadKey;

      for (Index = 0; Index < MenuOption->IfrNumber; Index++) {
        FileFormTags = FileFormTags->NextFile;
      }

      ScreenOperation = UiNoOperation;

      Status = gBS->HandleProtocol (
                      (VOID *) (UINTN) FileFormTags->FormTags.Tags[0].CallbackHandle,
                      &gEfiFormCallbackProtocolGuid,
                      &FormCallback
                      );

      break;

    case CfReadKey:
      ControlFlag     = CfScreenOperation;

      OriginalTimeOut = FrontPageTimeOutValue;
      do {
        if (FrontPageTimeOutValue >= 0 && (gClassOfVfr == EFI_FRONT_PAGE_SUBCLASS) && FrontPageTimeOutValue != 0xFFFF) {
          //
          // Remember that if set to 0, must immediately boot an option
          //
          if (FrontPageTimeOutValue == 0) {
            FrontPageTimeOutValue = 0xFFFF;
            Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
            if (EFI_ERROR (Status)) {
              Status = EFI_TIMEOUT;
            }
            break;
          }

          Status = UiWaitForSingleEvent (gST->ConIn->WaitForKey, ONE_SECOND);
          if (Status == EFI_TIMEOUT) {
            PageData->EntryCount  = 1;
            Count                 = (UINT32) ((OriginalTimeOut - FrontPageTimeOutValue) * 100 / OriginalTimeOut);
            EfiCopyMem (&PageData->Data->Data, &Count, sizeof (UINT32));

            if ((FormCallback != NULL) && (FormCallback->Callback != NULL)) {
              FormCallback->Callback (
                              FormCallback,
                              0xFFFF,
                              (EFI_IFR_DATA_ARRAY *) PageData,
                              NULL
                              );
            }
            //
            // Count down 1 second
            //
            FrontPageTimeOutValue--;

          } else {
            ASSERT (!EFI_ERROR (Status));
            PageData->EntryCount = 0;
            if ((FormCallback != NULL) && (FormCallback->Callback != NULL)) {
              FormCallback->Callback (
                              FormCallback,
                              0xFFFE,
                              (EFI_IFR_DATA_ARRAY *) PageData,
                              NULL
                              );
            }

            FrontPageTimeOutValue = 0xFFFF;
          }
        } else {
          //
          // Wait for user's selection, no auto boot
          //
          Status = UiWaitForSingleEvent (gST->ConIn->WaitForKey, 0);
        }
      } while (Status == EFI_TIMEOUT);

      if (gNeedSwitchToTextMode) {
        gNeedSwitchToTextMode = FALSE;
        gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK));
        DisableQuietBoot ();
      }

      if (Status == EFI_TIMEOUT) {
        Key.UnicodeChar = CHAR_CARRIAGE_RETURN;
      } else {
        Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
        //
        // if we encounter error, continue to read another key in.
        //
        if (EFI_ERROR (Status)) {
          ControlFlag = CfReadKey;
          continue;
        }
      }

      switch (Key.UnicodeChar) {
      case CHAR_CARRIAGE_RETURN:
        Selection       = MenuOption;
        ScreenOperation = UiSelect;
        gDirection      = 0;
        break;

      //
      // We will push the adjustment of these numeric values directly to the input handler
      //  NOTE: we won't handle manual input numeric TAG
      //
      case '+':
      case '-':
        if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP)
          || (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)
          || ((MenuOption->ThisTag->Operand == EFI_IFR_NUMERIC_OP) && (MenuOption->ThisTag->Step != 0))
        ){
          gDirection = Key.UnicodeChar;
          Status = ProcessOptions (MenuOption, TRUE, FileFormTagsHead, NULL, &OptionString);
        }
        break;

      case '^':
        ScreenOperation = UiUp;
        break;

      case 'V':
      case 'v':
        ScreenOperation = UiDown;
        break;

      case ' ':
        if (gClassOfVfr != EFI_FRONT_PAGE_SUBCLASS) {
          if (SubMenu) {
            if (MenuOption->ThisTag->Operand == EFI_IFR_CHECKBOX_OP && !(MenuOption->ThisTag->GrayOut)) {
              gST->ConOut->SetCursorPosition (gST->ConOut, MenuOption->Col, MenuOption->Row);
              gST->ConOut->OutputString (gST->ConOut, MenuOption->Description);
              Selection       = MenuOption;
              ScreenOperation = UiSelect;
            }
          }
        }
        break;

      case CHAR_NULL:
        if (((Key.ScanCode == SCAN_F1) && ((gFunctionKeySetting & FUNCTION_ONE) != FUNCTION_ONE)) ||
            ((Key.ScanCode == SCAN_F2) && ((gFunctionKeySetting & FUNCTION_TWO) != FUNCTION_TWO)) ||
            ((Key.ScanCode == SCAN_F9) && ((gFunctionKeySetting & FUNCTION_NINE) != FUNCTION_NINE)) ||
            ((Key.ScanCode == SCAN_F10) && ((gFunctionKeySetting & FUNCTION_TEN) != FUNCTION_TEN))
            ) {
          //
          // If the function key has been disabled, just ignore the key.
          //
        } else {
          for (Index = 0; Index < sizeof (gScanCodeToOperation) / sizeof (gScanCodeToOperation[0]); Index++) {
            if (Key.ScanCode == gScanCodeToOperation[Index].ScanCode) {
              if ((Key.ScanCode == SCAN_F9) || (Key.ScanCode == SCAN_F10)) {
                if (SubMenu) {
                  ScreenOperation = gScanCodeToOperation[Index].ScreenOperation;
                }
              } else {
                ScreenOperation = gScanCodeToOperation[Index].ScreenOperation;
              }
            }
          }
        }
        break;
      }
      break;

    case CfScreenOperation:
      IfrBinary = gBinaryDataHead;

      //
      // Advance to the Ifr we are using
      //
      for (Index = 0; Index < gActiveIfr; Index++) {
        IfrBinary = IfrBinary->Next;
      }

      if (ScreenOperation != UiPrevious && ScreenOperation != UiReset) {
        //
        // If the screen has no menu items, and the user didn't select UiPrevious, or UiReset
        // ignore the selection and go back to reading keys.
        //
        if (IsListEmpty (&Menu)) {
          ControlFlag = CfReadKey;
          break;
        }
        //
        // if there is nothing logical to place a cursor on, just move on to wait for a key.
        //
        for (Link = Menu.ForwardLink; Link != &Menu; Link = Link->ForwardLink) {
          NextMenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
          if (!(NextMenuOption->ThisTag->GrayOut) && (NextMenuOption->ThisTag->Operand != EFI_IFR_SUBTITLE_OP)) {
            break;
          }
        }

        if (Link == &Menu) {
          ControlFlag = CfPrepareToReadKey;
          break;
        }
      }

      for (Index = 0;
           Index < sizeof (gScreenOperationToControlFlag) / sizeof (gScreenOperationToControlFlag[0]);
           Index++
          ) {
        if (ScreenOperation == gScreenOperationToControlFlag[Index].ScreenOperation) {
          ControlFlag = gScreenOperationToControlFlag[Index].ControlFlag;
        }
      }

      break;

    case CfUiPrevious:
      ControlFlag = CfCheckSelection;
      //
      // Check for tags that might have LATE_CHECK enabled.  If they do, we can't switch pages or save NV data.
      //
      if (MenuOption != NULL) {
        if (!SelectionsAreValid (MenuOption, FileFormTagsHead)) {
          Selection = NULL;
          Repaint   = TRUE;
          break;
        }
      }

      if (IsListEmpty (&gMenuList)) {
        Selection = NULL;
        if (IsListEmpty (&Menu)) {
          ControlFlag = CfReadKey;
        }
        break;
      }

      gLastOpr = TRUE;

      while (gMenuRefreshHead != NULL) {
        OldMenuRefreshEntry = gMenuRefreshHead->Next;

        gBS->FreePool (gMenuRefreshHead);

        gMenuRefreshHead = OldMenuRefreshEntry;
      }
      //
      // Remove the Cached page entry, free and init the menus, flag Selection as jumping to previous page and a valid Tag
      //
      if (SubMenu) {
        UiRemoveMenuListEntry (MenuOption, &Selection);
        Selection->Previous = TRUE;
        UiFreeMenu ();
        UiInitMenu ();
      }

      gActiveIfr = Selection->IfrNumber;
      return Selection;

    case CfUiSelect:
      ControlFlag = CfCheckSelection;

      ExtractRequestedNvMap (FileFormTags, MenuOption->ThisTag->VariableNumber, &VariableDefinition);

      if (SubMenu) {
        if ((MenuOption->ThisTag->Operand == EFI_IFR_TEXT_OP && 
            !(MenuOption->ThisTag->Flags & EFI_IFR_FLAG_INTERACTIVE)) ||
            (MenuOption->ThisTag->GrayOut) ||
            (MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) ||
            (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) {
            Selection = NULL;
            break;
          }

        NewLine = TRUE;
        UpdateKeyHelp (MenuOption, TRUE);
        Status = ProcessOptions (MenuOption, TRUE, FileFormTagsHead, PageData, &OptionString);

        if (EFI_ERROR (Status)) {
          Selection  = NULL;
          Repaint    = TRUE;
          break;
        }

        if (OptionString != NULL) {
          PrintStringAt (LocalScreen.LeftColumn + gPromptBlockWidth + 1, MenuOption->Row, OptionString);
        }

        if (MenuOption->ThisTag->Flags & EFI_IFR_FLAG_INTERACTIVE) {
          Selection = MenuOption;
        }

        if (Selection == NULL) {
          break;
        }

        Location = (UINT8 *) &PageData->EntryCount;

        //
        // If not a goto, dump single piece of data, otherwise dump everything
        //
        if (Selection->ThisTag->Operand == EFI_IFR_REF_OP) {
          //
          // Check for tags that might have LATE_CHECK enabled.  If they do, we can't switch pages or save NV data.
          //
          if (!SelectionsAreValid (MenuOption, FileFormTagsHead)) {
            Selection = NULL;
            Repaint   = TRUE;
            break;
          }

          UiAddMenuListEntry (Selection);
          gPriorMenuEntry = 0;

          //
          // Now that we added a menu entry specific to a goto, we can always go back when someone hits the UiPrevious
          //
          UiMenuList                    = CR (gMenuList.ForwardLink, UI_MENU_LIST, MenuLink, UI_MENU_LIST_SIGNATURE);
          UiMenuList->FormerEntryNumber = MenuOption->EntryNumber;

          gLastOpr                      = FALSE;

          //
          // Rewind to the beginning of the menu
          //
          for (; NewPos->BackLink != &Menu; NewPos = NewPos->BackLink)
            ;

          //
          // Get Total Count of Menu entries
          //
          for (Count = 1; NewPos->ForwardLink != &Menu; NewPos = NewPos->ForwardLink) {
            Count++;
          }
          //
          // Rewind to the beginning of the menu
          //
          for (; NewPos->BackLink != &Menu; NewPos = NewPos->BackLink)
            ;

          //
          // Copy the number of entries being described to the PageData location
          //
          EfiCopyMem (&Location[0], &Count, sizeof (UINT32));

          for (Index = 4; NewPos->ForwardLink != &Menu; Index = Index + MenuOption->ThisTag->StorageWidth + 2) {

            MenuOption          = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
            Location[Index]     = MenuOption->ThisTag->Operand;
            Location[Index + 1] = (UINT8) (MenuOption->ThisTag->StorageWidth + 4);
            EfiCopyMem (
              &Location[Index + 4],
              &VariableDefinition->NvRamMap[MenuOption->ThisTag->StorageStart],
              MenuOption->ThisTag->StorageWidth
              );
            NewPos = NewPos->ForwardLink;
          }
        } else {

          gPriorMenuEntry = MenuOption->EntryNumber;

          Count           = 1;

          //
          // Copy the number of entries being described to the PageData location
          //
          EfiCopyMem (&Location[0], &Count, sizeof (UINT32));

          //
          // Start at PageData[4] since the EntryCount is a UINT32
          //
          Index = 4;

          //
          // Copy data to destination
          //
          Location[Index]     = MenuOption->ThisTag->Operand;
          Location[Index + 1] = (UINT8) (MenuOption->ThisTag->StorageWidth + 4);
          EfiCopyMem (
            &Location[Index + 4],
            &VariableDefinition->NvRamMap[MenuOption->ThisTag->StorageStart],
            MenuOption->ThisTag->StorageWidth
            );
        }
      }
      break;

    case CfUiReset:
      ControlFlag = CfCheckSelection;
      gLastOpr    = FALSE;
      if (gClassOfVfr == EFI_FRONT_PAGE_SUBCLASS) {
        break;
      }
      //
      // If NV flag is up, prompt user
      //
      if (gNvUpdateRequired) {
        Status      = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);

        YesResponse = gYesResponse[0];
        NoResponse  = gNoResponse[0];

        do {
          CreateDialog (3, TRUE, 0, NULL, &Key, gEmptyString, gAreYouSure, gEmptyString);
        } while
        (
          (Key.ScanCode != SCAN_ESC) &&
          ((Key.UnicodeChar | UPPER_LOWER_CASE_OFFSET) != (NoResponse | UPPER_LOWER_CASE_OFFSET)) &&
          ((Key.UnicodeChar | UPPER_LOWER_CASE_OFFSET) != (YesResponse | UPPER_LOWER_CASE_OFFSET))
        );

        //
        // If the user hits the YesResponse key
        //
        if ((Key.UnicodeChar | UPPER_LOWER_CASE_OFFSET) == (YesResponse | UPPER_LOWER_CASE_OFFSET)) {
        } else {
          Repaint = TRUE;
          NewLine = TRUE;
          break;
        }
      }
      //
      // Check for tags that might have LATE_CHECK enabled.  If they do, we can't switch pages or save NV data.
      //
      if (MenuOption != NULL) {
        if (!SelectionsAreValid (MenuOption, FileFormTagsHead)) {
          Selection = NULL;
          Repaint   = TRUE;
          NewLine   = TRUE;
          break;
        }
      }

      gST->ConOut->SetAttribute (gST->ConOut, EFI_TEXT_ATTR (EFI_LIGHTGRAY, EFI_BLACK));
      gST->ConOut->EnableCursor (gST->ConOut, TRUE);

      if (SubMenu) {
        UiFreeMenuList ();
        gST->ConOut->ClearScreen (gST->ConOut);
        return NULL;
      }

      UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->Flags, FALSE);
      UpdateStatusBar (NV_UPDATE_REQUIRED, MenuOption->ThisTag->Flags, FALSE);

      if (IfrBinary->UnRegisterOnExit) {
        Hii->RemovePack (Hii, MenuOption->Handle);
      }

      UiFreeMenu ();

      //
      // Clean up the allocated data buffers
      //
      FreeData (FileFormTagsHead, FormattedString, OptionString);

      gST->ConOut->ClearScreen (gST->ConOut);
      return NULL;

    case CfUiLeft:
      ControlFlag = CfCheckSelection;
      if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) || (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) {
        if (MenuOption->Skip == 1) {
          //
          // In the tail of the Date/Time op-code set, go left.
          //
          NewPos = NewPos->BackLink;
        } else {
          //
          // In the middle of the Data/Time op-code set, go left.
          //
          NextMenuOption = CR (NewPos->ForwardLink, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
          if (NextMenuOption->Skip == 1) {
            NewPos = NewPos->BackLink;
          }
        }
      }
      break;

    case CfUiRight:
      ControlFlag = CfCheckSelection;
      if ((MenuOption->Skip == 0) &&
          ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) || (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP))
          ) {
        //
        // We are in the head or middle of the Date/Time op-code set, advance right.
        //
        NewPos = NewPos->ForwardLink;
      }
      break;

    case CfUiUp:
      ControlFlag = CfCheckSelection;

      SavedListEntry = TopOfScreen;

      if (NewPos->BackLink != &Menu) {
        NewLine = TRUE;
        //
        // Adjust Date/Time position before we advance forward.
        //
        AdjustDateAndTimePosition (TRUE, &NewPos);

        //
        // Caution that we have already rewind to the top, don't go backward in this situation.
        //
        if (NewPos->BackLink != &Menu) {
          NewPos = NewPos->BackLink;
        }

        PreviousMenuOption = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
        DistanceValue = PreviousMenuOption->Skip;
        //
        // Since the behavior of hitting the up arrow on a Date/Time op-code is intended
        // to be one that back to the previous set of op-codes, we need to advance to the sencond
        // Date/Time op-code and leave the remaining logic in UiDown intact so the appropriate
        // checking can be done.
        //
        DistanceValue += AdjustDateAndTimePosition (TRUE, &NewPos);
        
        //
        // Check the previous menu entry to see if it was a zero-length advance.  If it was,
        // don't worry about a redraw.
        //
        if ((INTN) MenuOption->Row - (INTN) DistanceValue < (INTN) TopRow) {
          Repaint     = TRUE;
          TopOfScreen = NewPos;
        }

        if (SubMenu) {
          Difference   = SkipNoFocusTag (TRUE, &NewPos);
          if ((INTN) MenuOption->Row - (INTN) DistanceValue < (INTN) TopRow) {
            if (Difference > 0) {
              //
              // Case: The previous focus tag is above the TopOfScreen, so we need to scroll
              //
              TopOfScreen = NewPos;
              Repaint     = TRUE;
            }
          } 
          if (Difference < 0) {
            //
            // Case: We want to goto previous tag, but finally we go down.
            //       it means that we hit the begining tag that can be focused
            //       so we simply scroll to the top
            //
            if (SavedListEntry != Menu.ForwardLink) {
              TopOfScreen = Menu.ForwardLink;
              Repaint     = TRUE;
            }
          }
        }

        
        //
        // If we encounter a Date/Time op-code set, rewind to the first op-code of the set.
        //
        AdjustDateAndTimePosition (TRUE, &TopOfScreen);

        UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->Flags, FALSE);
      } else {
        if (SubMenu) {
          SavedMenuOption = MenuOption;
          MenuOption      = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
          if (MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP || MenuOption->ThisTag->GrayOut) {
            //
            // If we are at the end of the list and sitting on a text op, we need to more forward
            //
            ScreenOperation = UiDown;
            ControlFlag     = CfScreenOperation;
            break;
          }

          MenuOption = SavedMenuOption;
        }
      }
      break;

    case CfUiPageUp:
      ControlFlag     = CfCheckSelection;

      if (NewPos->BackLink == &Menu) {
        NewLine = FALSE;
        Repaint = FALSE;
        break;
      }

      NewLine   = TRUE;
      Repaint   = TRUE;
      Link      = TopOfScreen;
      for (Index = BottomRow; Index >= TopRow + 1; Index -= PreviousMenuOption->Skip) {
        if (Link->BackLink == &Menu) {
          break;
        }

        Link       = Link->BackLink;
        PreviousMenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
      }

      TopOfScreen = Link;
      Difference = SkipNoFocusTag (TRUE, &Link);
      if (Difference > 0) {
        //
        // The focus TAG is above the TopOfScreen
        //
        TopOfScreen = Link;
      } else if (Difference < 0) {
        //
        // This happens when there is no TAG can be focused from Current TAG to the first TAG
        //
        TopOfScreen = Menu.ForwardLink;
      }
      Index += Difference;
      if (Index < TopRow + 1) {
        MenuOption = NULL;
      }

      if (NewPos == Link) {
        Repaint = FALSE;
        NewLine = FALSE;
      } else {
        NewPos      = Link;
      }

      //
      // If we encounter a Date/Time op-code set, rewind to the first op-code of the set.
      // Don't do this when we are already in the first page.
      //
      AdjustDateAndTimePosition (TRUE, &TopOfScreen);
      AdjustDateAndTimePosition (TRUE, &NewPos);
      break;

    case CfUiPageDown:
      ControlFlag     = CfCheckSelection;

      if (NewPos->ForwardLink == &Menu) {
        NewLine = FALSE;
        Repaint = FALSE;
        break;
      }

      NewLine = TRUE;
      Repaint = TRUE;
      Link    = TopOfScreen;
      for (Index = TopRow; Index <= BottomRow - 1; Index += NextMenuOption->Skip) {
        if (Link->ForwardLink == &Menu) {
          break;
        }

        Link           = Link->ForwardLink;
        NextMenuOption = CR (Link, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
      }

      Index += SkipNoFocusTag (FALSE, &Link);
      //
      // If (Index > BottomRow - 1) is TRUE, it means that there are more TAGs needing scrolling
      //
      if (Index > BottomRow - 1) {
        TopOfScreen = Link;
        MenuOption = NULL;
      }
      if (NewPos == Link && Index <= BottomRow - 1) {
        //
        // Finally we know that NewPos is the last TAG can be focused.
        //
        NewLine = FALSE;
        Repaint = FALSE;
      } else {
        NewPos  = Link;
      }


      //
      // If we encounter a Date/Time op-code set, rewind to the first op-code of the set.
      // Don't do this when we are already in the last page.
      //
      AdjustDateAndTimePosition (TRUE, &TopOfScreen);
      AdjustDateAndTimePosition (TRUE, &NewPos);
      break;

    case CfUiDown:
      ControlFlag = CfCheckSelection;
      //
      // Since the behavior of hitting the down arrow on a Date/Time op-code is intended
      // to be one that progresses to the next set of op-codes, we need to advance to the last
      // Date/Time op-code and leave the remaining logic in UiDown intact so the appropriate
      // checking can be done.  The only other logic we need to introduce is that if a Date/Time
      // op-code is the last entry in the menu, we need to rewind back to the first op-code of
      // the Date/Time op-code.
      //
      SavedListEntry = NewPos;
      DistanceValue  = AdjustDateAndTimePosition (FALSE, &NewPos);

      if (NewPos->ForwardLink != &Menu) {
        MenuOption      = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);        
        NewLine         = TRUE;
        NewPos          = NewPos->ForwardLink;
        NextMenuOption  = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

        DistanceValue  += NextMenuOption->Skip;
        if (SubMenu) {
          DistanceValue  += SkipNoFocusTag (FALSE, &NewPos);
        }
        //
        // An option might be multi-line, so we need to reflect that data in the overall skip value
        //
        UpdateOptionSkipLines (PageData, NextMenuOption, FileFormTagsHead, &OptionString, SkipValue);

        Temp = MenuOption->Row + MenuOption->Skip + DistanceValue - 1;
        if ((MenuOption->Row + MenuOption->Skip == BottomRow + 1) &&
            (NextMenuOption->ThisTag->Operand == EFI_IFR_DATE_OP || 
             NextMenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)
            ) {
          Temp ++;
        }

        //
        // If we are going to scroll, update TopOfScreen
        //
        if (Temp > BottomRow) {
          do {
            //
            // Is the current top of screen a zero-advance op-code?
            // If so, keep moving forward till we hit a >0 advance op-code
            //
            SavedMenuOption = CR (TopOfScreen, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

            //
            // If bottom op-code is more than one line or top op-code is more than one line
            //
            if ((DistanceValue > 1) || (MenuOption->Skip > 1)) {
              //
              // Is the bottom op-code greater than or equal in size to the top op-code?
              //
              if ((Temp - BottomRow) >= (SavedMenuOption->Skip - OldSkipValue)) {
                //
                // Skip the top op-code
                //
                TopOfScreen     = TopOfScreen->ForwardLink;
                Difference      = (Temp - BottomRow) - (SavedMenuOption->Skip - OldSkipValue);

                OldSkipValue    = Difference;

                SavedMenuOption = CR (TopOfScreen, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

                //
                // If we have a remainder, skip that many more op-codes until we drain the remainder
                //
                for (;
                     Difference >= (INTN) SavedMenuOption->Skip;
                     Difference = Difference - (INTN) SavedMenuOption->Skip
                    ) {
                  //
                  // Since the Difference is greater than or equal to this op-code's skip value, skip it
                  //
                  TopOfScreen     = TopOfScreen->ForwardLink;
                  SavedMenuOption = CR (TopOfScreen, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
                  if (Difference < (INTN) SavedMenuOption->Skip) {
                    Difference = SavedMenuOption->Skip - Difference - 1;
                    break;
                  } else {
                    if (Difference == (INTN) SavedMenuOption->Skip) {
                      TopOfScreen     = TopOfScreen->ForwardLink;
                      SavedMenuOption = CR (TopOfScreen, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
                      Difference      = SavedMenuOption->Skip - Difference;
                      break;
                    }
                  }
                }
                //
                // Since we will act on this op-code in the next routine, and increment the
                // SkipValue, set the skips to one less than what is required.
                //
                SkipValue = Difference - 1;

              } else {
                //
                // Since we will act on this op-code in the next routine, and increment the
                // SkipValue, set the skips to one less than what is required.
                //
                SkipValue = OldSkipValue + (Temp - BottomRow) - 1;
              }
            } else {
              if ((OldSkipValue + 1) == (INTN) SavedMenuOption->Skip) {
                TopOfScreen = TopOfScreen->ForwardLink;
                break;
              } else {
                SkipValue = OldSkipValue;
              }
            }
            //
            // If the op-code at the top of the screen is more than one line, let's not skip it yet
            // Let's set a skip flag to smoothly scroll the top of the screen.
            //
            if (SavedMenuOption->Skip > 1) {
              if (SavedMenuOption == NextMenuOption) {
                SkipValue = 0;
              } else {
                SkipValue++;
              }
            } else {
              SkipValue   = 0;
              TopOfScreen = TopOfScreen->ForwardLink;
            }
          } while (SavedMenuOption->Skip == 0);

          Repaint       = TRUE;
          OldSkipValue  = SkipValue;
        }
        
        MenuOption = CR (SavedListEntry, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

        UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->Flags, FALSE);

      } else {
        if (SubMenu) {
          SavedMenuOption = MenuOption;
          MenuOption      = CR (NewPos, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
          if (MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP || MenuOption->ThisTag->GrayOut) {
            //
            // If we are at the end of the list and sitting on a text op, we need to more forward
            //
            ScreenOperation = UiUp;
            ControlFlag     = CfScreenOperation;
            break;
          }

          MenuOption = SavedMenuOption;
          //
          // If we are at the end of the list and sitting on a Date/Time op, rewind to the head.
          //
          AdjustDateAndTimePosition (TRUE, &NewPos);
        }
      }
      break;

    case CfUiSave:
      ControlFlag = CfCheckSelection;
      //
      // Check for tags that might have LATE_CHECK enabled.  If they do, we can't switch pages or save NV data.
      //
      if (MenuOption != NULL) {
        if (!SelectionsAreValid (MenuOption, FileFormTagsHead)) {
          Selection = NULL;
          Repaint   = TRUE;
          break;
        }
      }
      //
      // If callbacks are active, and the callback has a Write method, try to use it
      //
      if (FileFormTags->VariableDefinitions->VariableName == NULL) {
        if ((FormCallback != NULL) && (FormCallback->NvWrite != NULL)) {
          Status = FormCallback->NvWrite (
                                  FormCallback,
                                  L"Setup",
                                  &FileFormTags->FormTags.Tags[0].GuidValue,
                                  EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                                  VariableDefinition->VariableSize,
                                  (VOID *) VariableDefinition->NvRamMap,
                                  &gResetRequired
                                  );

        } else {
          Status = gRT->SetVariable (
                          L"Setup",
                          &FileFormTags->FormTags.Tags[0].GuidValue,
                          EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                          VariableDefinition->VariableSize,
                          (VOID *) VariableDefinition->NvRamMap
                          );
        }
      } else {
        VariableDefinition = FileFormTags->VariableDefinitions;

        for (; VariableDefinition != NULL; VariableDefinition = VariableDefinition->Next) {
          if ((FormCallback != NULL) && (FormCallback->NvWrite != NULL)) {
            Status = FormCallback->NvWrite (
                                    FormCallback,
                                    VariableDefinition->VariableName,
                                    &VariableDefinition->Guid,
                                    EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                                    VariableDefinition->VariableSize,
                                    (VOID *) VariableDefinition->NvRamMap,
                                    &gResetRequired
                                    );

          } else {
            Status = gRT->SetVariable (
                            VariableDefinition->VariableName,
                            &VariableDefinition->Guid,
                            EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS | EFI_VARIABLE_RUNTIME_ACCESS,
                            VariableDefinition->VariableSize,
                            (VOID *) VariableDefinition->NvRamMap
                            );
          }
        }
      }

      UpdateStatusBar (INPUT_ERROR, MenuOption->ThisTag->Flags, FALSE);
      UpdateStatusBar (NV_UPDATE_REQUIRED, MenuOption->ThisTag->Flags, FALSE);
      break;

    case CfUiDefault:
      ControlFlag = CfCheckSelection;

      NvMapListHead = NULL;

      Status = Hii->GetDefaultImage (Hii, MenuOption->Handle, EFI_IFR_FLAG_DEFAULT, &NvMapListHead);

      if (!EFI_ERROR (Status)) {
        ASSERT_EFI_ERROR (NULL != NvMapListHead);
        
        NvMapListNode = NvMapListHead;
        
        while (NULL != NvMapListNode) {
          if (FileFormTags->VariableDefinitions->VariableId == NvMapListNode->VariablePack->VariableId) {
            NvMap     = (VOID *) ((CHAR8 *) NvMapListNode->VariablePack + sizeof (EFI_HII_VARIABLE_PACK) + NvMapListNode->VariablePack->VariableNameLength);
            NvMapSize = NvMapListNode->VariablePack->Header.Length  - sizeof (EFI_HII_VARIABLE_PACK) - NvMapListNode->VariablePack->VariableNameLength;
            break;
            }
          NvMapListNode = NvMapListNode->NextVariablePack;
        }
        
        //
        // Free the buffer that was allocated.
        //
        gBS->FreePool (FileFormTags->VariableDefinitions->NvRamMap);
        gBS->FreePool (FileFormTags->VariableDefinitions->FakeNvRamMap);
        
        //
        // Allocate, copy the NvRamMap.
        //
        FileFormTags->VariableDefinitions->VariableFakeSize = (UINT16) (FileFormTags->VariableDefinitions->VariableFakeSize - FileFormTags->VariableDefinitions->VariableSize);
        FileFormTags->VariableDefinitions->VariableSize = (UINT16) NvMapSize;
        FileFormTags->VariableDefinitions->VariableFakeSize = (UINT16) (FileFormTags->VariableDefinitions->VariableFakeSize + FileFormTags->VariableDefinitions->VariableSize);
        
        FileFormTags->VariableDefinitions->NvRamMap = EfiLibAllocateZeroPool (FileFormTags->VariableDefinitions->VariableSize);
        ASSERT ( FileFormTags->VariableDefinitions->NvRamMap != NULL );
        FileFormTags->VariableDefinitions->FakeNvRamMap = EfiLibAllocateZeroPool (NvMapSize + FileFormTags->VariableDefinitions->VariableFakeSize);
        ASSERT ( FileFormTags->VariableDefinitions->FakeNvRamMap != NULL );
        
        EfiCopyMem (FileFormTags->VariableDefinitions->NvRamMap, NvMap, NvMapSize);
        gBS->FreePool (NvMapListHead);
      }

      UpdateStatusBar (NV_UPDATE_REQUIRED, MenuOption->ThisTag->Flags, TRUE);
      Repaint    = TRUE;
      //
      // After the repaint operation, we should refresh the highlight.
      //
      NewLine    = TRUE;
      break;

    case CfUiNoOperation:
      ControlFlag = CfCheckSelection;
      break;

    case CfExit:
      while (gMenuRefreshHead != NULL) {
        OldMenuRefreshEntry = gMenuRefreshHead->Next;

        gBS->FreePool (gMenuRefreshHead);

        gMenuRefreshHead = OldMenuRefreshEntry;
      }

      gST->ConOut->SetCursorPosition (gST->ConOut, 0, Row + 4);
      gST->ConOut->EnableCursor (gST->ConOut, TRUE);
      gST->ConOut->OutputString (gST->ConOut, L"\n");

      gActiveIfr = MenuOption->IfrNumber;
      return Selection;

    default:
      break;
    }
  }
}

BOOLEAN
ValueIsScroll (
  IN  BOOLEAN                     Direction,
  IN  EFI_LIST_ENTRY              *CurrentPos
  )
/*++

Routine Description:
  Determine if the menu is the last menu that can be selected. 

Arguments:
  Direction - the scroll direction. False is down. True is up.
           
Returns:
  FALSE -- the menu isn't the last menu that can be selected.
  TRUE  -- the menu is the last menu that can be selected.
--*/
{
  EFI_LIST_ENTRY  *Temp;
  UI_MENU_OPTION  *MenuOption;

  Temp        = Direction ? CurrentPos->BackLink : CurrentPos->ForwardLink;

  if (Temp == &Menu) {
    return TRUE;
  }

  for (; Temp != &Menu; Temp = Direction ? Temp->BackLink : Temp->ForwardLink) {
    MenuOption = CR (Temp, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
    if (!(MenuOption->ThisTag->Operand == EFI_IFR_SUBTITLE_OP || MenuOption->ThisTag->GrayOut)) {
      return FALSE;
    }
  }

  return TRUE;
}

UINTN
AdjustDateAndTimePosition (
  IN     BOOLEAN                     DirectionUp,
  IN OUT EFI_LIST_ENTRY              **CurrentPosition
  )
/*++
Routine Description:
  Adjust Data and Time tag position accordingly.
  Data format :      [01/02/2004]      [11:22:33]
  Line number :        0  0    1         0  0  1

Arguments:
  DirectionUp     - the up or down direction. False is down. True is up.
  CurrentPosition - Current position.
                    On return: Point to the last TAG (Year or Second) if up;
                               Point to the first TAG (Month or Hour) if down.
           
Returns:
  Return line number to pad. It is possible that we stand on a zero-advance 
  data or time opcode, so pad one line when we judge if we are going to scroll outside.
--*/
{
  UINTN           Count;
  EFI_LIST_ENTRY  *NewPosition;
  UI_MENU_OPTION  *MenuOption;
  UINTN           PadLineNumber;

  PadLineNumber = 0;
  NewPosition   = *CurrentPosition;
  MenuOption    = CR (NewPosition, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);

  if ((MenuOption->ThisTag->Operand == EFI_IFR_DATE_OP) || (MenuOption->ThisTag->Operand == EFI_IFR_TIME_OP)) {
    //
    // Calculate the distance from current position to the last Date/Time op-code.
    //
    Count = 0;
    while (MenuOption->ThisTag->NumberOfLines == 0) {
      Count++;
      NewPosition   = NewPosition->ForwardLink;
      MenuOption    = CR (NewPosition, UI_MENU_OPTION, Link, UI_MENU_OPTION_SIGNATURE);
      PadLineNumber = 1;
    }

    NewPosition = *CurrentPosition;
    if (DirectionUp) {
      //
      // Since the behavior of hitting the up arrow on a Date/Time op-code is intended
      // to be one that back to the previous set of op-codes, we need to advance to the first
      // Date/Time op-code and leave the remaining logic in CfUiUp intact so the appropriate
      // checking can be done.
      //
      while (Count++ < 2) {
        NewPosition = NewPosition->BackLink;
      }
    } else {
      //
      // Since the behavior of hitting the down arrow on a Date/Time op-code is intended
      // to be one that progresses to the next set of op-codes, we need to advance to the last
      // Date/Time op-code and leave the remaining logic in CfUiDown intact so the appropriate
      // checking can be done.
      //
      while (Count-- > 0) {
        NewPosition = NewPosition->ForwardLink;
      }
    }

    *CurrentPosition = NewPosition;
  }

  return PadLineNumber;
}

