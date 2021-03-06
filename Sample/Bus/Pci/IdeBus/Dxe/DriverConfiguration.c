/*++

Copyright (c) 2004 - 2008, Intel Corporation                                                  
All rights reserved. This program and the accompanying materials                          
are licensed and made available under the terms and conditions of the BSD License         
which accompanies this distribution.  The full text of the license may be found at        
http://opensource.org/licenses/bsd-license.php                                            
                                                                                          
THE PROGRAM IS DISTRIBUTED UNDER THE BSD LICENSE ON AN "AS IS" BASIS,                     
WITHOUT WARRANTIES OR REPRESENTATIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED.             

Module Name:

  DriverConfiguration.c

Abstract:

--*/

#include "IDEBus.h"

CHAR16 *OptionString[4] = {
  L"Enable Primary Master    (Y/N)? -->",
  L"Enable Primary Slave     (Y/N)? -->",
  L"Enable Secondary Master  (Y/N)? -->",
  L"Enable Secondary Slave   (Y/N)? -->"
};
//
// EFI Driver Configuration Functions
//
EFI_STATUS
IDEBusDriverConfigurationSetOptions (
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN  EFI_DRIVER_CONFIGURATION2_PROTOCOL                     *This,
#else
  IN  EFI_DRIVER_CONFIGURATION_PROTOCOL                      *This,
#endif
  IN  EFI_HANDLE                                             ControllerHandle,
  IN  EFI_HANDLE                                             ChildHandle  OPTIONAL,
  IN  CHAR8                                                  *Language,
  OUT EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED               *ActionRequired
  );

EFI_STATUS
IDEBusDriverConfigurationOptionsValid (
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN  EFI_DRIVER_CONFIGURATION2_PROTOCOL              *This,
#else
  IN  EFI_DRIVER_CONFIGURATION_PROTOCOL               *This,
#endif
  IN  EFI_HANDLE                                      ControllerHandle,
  IN  EFI_HANDLE                                      ChildHandle  OPTIONAL
  );

EFI_STATUS
IDEBusDriverConfigurationForceDefaults (
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN  EFI_DRIVER_CONFIGURATION2_PROTOCOL                     *This,
#else
  IN  EFI_DRIVER_CONFIGURATION_PROTOCOL                      *This,
#endif
  IN  EFI_HANDLE                                             ControllerHandle,
  IN  EFI_HANDLE                                             ChildHandle  OPTIONAL,
  IN  UINT32                                                 DefaultType,
  OUT EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED               *ActionRequired
  );

//
// EFI Driver Configuration Protocol
//
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
EFI_DRIVER_CONFIGURATION2_PROTOCOL gIDEBusDriverConfiguration = {
#else
EFI_DRIVER_CONFIGURATION_PROTOCOL gIDEBusDriverConfiguration = {
#endif
  IDEBusDriverConfigurationSetOptions,
  IDEBusDriverConfigurationOptionsValid,
  IDEBusDriverConfigurationForceDefaults,
  LANGUAGE_CODE_ENGLISH
};

EFI_STATUS
GetResponse (
  VOID
  )
/*++

Routine Description:

  TODO: Add function description

Arguments:

  None

Returns:

  EFI_ABORTED - TODO: Add description for return value
  EFI_SUCCESS - TODO: Add description for return value
  EFI_NOT_FOUND - TODO: Add description for return value

--*/
{
  EFI_STATUS    Status;
  EFI_INPUT_KEY Key;

  while (TRUE) {
    Status = gST->ConIn->ReadKeyStroke (gST->ConIn, &Key);
    if (!EFI_ERROR (Status)) {
      if (Key.ScanCode == SCAN_ESC) {
        return EFI_ABORTED;
      }

      switch (Key.UnicodeChar) {

      //
      // fall through
      //
      case L'y':
      case L'Y':
        gST->ConOut->OutputString (gST->ConOut, L"Y\n");
        return EFI_SUCCESS;

      //
      // fall through
      //
      case L'n':
      case L'N':
        gST->ConOut->OutputString (gST->ConOut, L"N\n");
        return EFI_NOT_FOUND;
      }

    }
  }
}

EFI_STATUS
IDEBusDriverConfigurationSetOptions (
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN  EFI_DRIVER_CONFIGURATION2_PROTOCOL                     *This,
#else
  IN  EFI_DRIVER_CONFIGURATION_PROTOCOL                      *This,
#endif
  IN  EFI_HANDLE                                             ControllerHandle,
  IN  EFI_HANDLE                                             ChildHandle  OPTIONAL,
  IN  CHAR8                                                  *Language,
  OUT EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED               *ActionRequired
  )
/*++

  Routine Description:
    Allows the user to set controller specific options for a controller that a 
    driver is currently managing.

  Arguments:
    This             - A pointer to the EFI_DRIVER_CONFIGURATION_ PROTOCOL 
                       instance.
    ControllerHandle - The handle of the controller to set options on.
    ChildHandle      - The handle of the child controller to set options on.  
                       This is an optional parameter that may be NULL.  
                       It will be NULL for device drivers, and for a bus drivers
                       that wish to set options for the bus controller.  
                       It will not be NULL for a bus driver that wishes to set 
                       options for one of its child controllers.
    Language         - A pointer to a three character ISO 639-2 language 
                       identifier. This is the language of the user interface 
                       that should be presented to the user, and it must match 
                       one of the languages specified in SupportedLanguages.  
                       The number of languages supported by a driver is up to 
                       the driver writer.
    ActionRequired   - A pointer to the action that the calling agent is 
                       required to perform when this function returns.  
                       See "Related Definitions" for a list of the actions that
                       the calling agent is required to perform prior to 
                       accessing ControllerHandle again.

  Returns:
    EFI_SUCCESS           - The driver specified by This successfully set the 
                            configuration options for the controller specified 
                            by ControllerHandle..
    EFI_INVALID_PARAMETER - ControllerHandle is not a valid EFI_HANDLE.
    EFI_INVALID_PARAMETER - ChildHandle is not NULL and it is not a 
                            valid EFI_HANDLE.
    EFI_INVALID_PARAMETER - ActionRequired is NULL.
    EFI_UNSUPPORTED       - The driver specified by This does not support 
                            setting configuration options for the controller 
                            specified by ControllerHandle and ChildHandle.
    EFI_UNSUPPORTED       - The driver specified by This does not support the 
                            language specified by Language.
    EFI_DEVICE_ERROR      - A device error occurred while attempt to set the 
                            configuration options for the controller specified 
                            by ControllerHandle and ChildHandle.
    EFI_OUT_RESOURCES     - There are not enough resources available to set the 
                            configuration options for the controller specified 
                            by ControllerHandle and ChildHandle.

--*/
{
  EFI_STATUS  Status;
  UINT8       Value;
  UINT8       NewValue;
  UINTN       DataSize;
  UINTN       Index;
  UINT32      Attributes;

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  *ActionRequired = EfiDriverConfigurationActionNone;

  DataSize        = sizeof (Value);
  Status = gRT->GetVariable (
                  L"Configuration",
                  &gIDEBusDriverGuid,
                  &Attributes,
                  &DataSize,
                  &Value
                  );

  gST->ConOut->OutputString (gST->ConOut, L"IDE Bus Driver Configuration\n");
  gST->ConOut->OutputString (gST->ConOut, L"===============================\n");

  NewValue = 0;
  for (Index = 0; Index < 4; Index++) {
    gST->ConOut->OutputString (gST->ConOut, OptionString[Index]);

    Status = GetResponse ();
    if (Status == EFI_ABORTED) {
      return EFI_SUCCESS;
    }

    if (!EFI_ERROR (Status)) {
      NewValue |= (UINT8) (1 << Index);
    }
  }

  if (EFI_ERROR (Status) || (NewValue != Value)) {
    gRT->SetVariable (
          L"Configuration",
          &gIDEBusDriverGuid,
          EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
          sizeof (NewValue),
          &NewValue
          );

    *ActionRequired = EfiDriverConfigurationActionRestartController;
  } else {
    *ActionRequired = EfiDriverConfigurationActionNone;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
IDEBusDriverConfigurationOptionsValid (
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN  EFI_DRIVER_CONFIGURATION2_PROTOCOL              *This,
#else
  IN  EFI_DRIVER_CONFIGURATION_PROTOCOL               *This,
#endif
  IN  EFI_HANDLE                                      ControllerHandle,
  IN  EFI_HANDLE                                      ChildHandle  OPTIONAL
  )
/*++

  Routine Description:
    Tests to see if a controller's current configuration options are valid.

  Arguments:
    This             - A pointer to the EFI_DRIVER_CONFIGURATION_PROTOCOL 
                       instance.
    ControllerHandle - The handle of the controller to test if it's current 
                       configuration options are valid.
    ChildHandle      - The handle of the child controller to test if it's 
                       current
                       configuration options are valid.  This is an optional 
                       parameter that may be NULL.  It will be NULL for device 
                       drivers.  It will also be NULL for a bus drivers that 
                       wish to test the configuration options for the bus 
                       controller. It will not be NULL for a bus driver that 
                       wishes to test configuration options for one of 
                       its child controllers.

  Returns:
    EFI_SUCCESS           - The controller specified by ControllerHandle and 
                            ChildHandle that is being managed by the driver 
                            specified by This has a valid set of  configuration
                            options.
    EFI_INVALID_PARAMETER - ControllerHandle is not a valid EFI_HANDLE.
    EFI_INVALID_PARAMETER - ChildHandle is not NULL and it is not a valid 
                            EFI_HANDLE.
    EFI_UNSUPPORTED       - The driver specified by This is not currently 
                            managing the controller specified by 
                            ControllerHandle and ChildHandle.
    EFI_DEVICE_ERROR      - The controller specified by ControllerHandle and 
                            ChildHandle that is being managed by the driver 
                            specified by This has an invalid set of 
                            configuration options.

--*/
{
  EFI_STATUS  Status;
  UINT8       Value;
  UINTN       DataSize;
  UINT32      Attributes;

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  DataSize = sizeof (Value);
  Status = gRT->GetVariable (
                  L"Configuration",
                  &gIDEBusDriverGuid,
                  &Attributes,
                  &DataSize,
                  &Value
                  );
  if (EFI_ERROR (Status) || Value > 0x0f) {
    return EFI_DEVICE_ERROR;
  }

  return EFI_SUCCESS;
}

EFI_STATUS
IDEBusDriverConfigurationForceDefaults (
#if (EFI_SPECIFICATION_VERSION >= 0x00020000)
  IN  EFI_DRIVER_CONFIGURATION2_PROTOCOL                     *This,
#else
  IN  EFI_DRIVER_CONFIGURATION_PROTOCOL                      *This,
#endif
  IN  EFI_HANDLE                                             ControllerHandle,
  IN  EFI_HANDLE                                             ChildHandle  OPTIONAL,
  IN  UINT32                                                 DefaultType,
  OUT EFI_DRIVER_CONFIGURATION_ACTION_REQUIRED               *ActionRequired
  )
/*++

  Routine Description:
    Forces a driver to set the default configuration options for a controller.

  Arguments:
    This             - A pointer to the EFI_DRIVER_CONFIGURATION_ PROTOCOL 
                       instance.
    ControllerHandle - The handle of the controller to force default 
                       configuration options on.
    ChildHandle      - The handle of the child controller to force default 
                       configuration options on  This is an optional parameter 
                       that may be NULL.  It will be NULL for device drivers.  
                       It will also be NULL for a bus drivers that wish to 
                       force default configuration options for the bus 
                       controller.  It will not be NULL for a bus driver that 
                       wishes to force default configuration options for one 
                       of its child controllers.
    DefaultType      - The type of default configuration options to force on 
                       the controller specified by ControllerHandle and 
                       ChildHandle.  See Table 9-1 for legal values.  
                       A DefaultType of 0x00000000 must be supported 
                       by this protocol.
    ActionRequired   - A pointer to the action that the calling agent 
                       is required to perform when this function returns.  
                       

  Returns:
    EFI_SUCCESS           - The driver specified by This successfully forced 
                            the default configuration options on the 
                            controller specified by ControllerHandle and 
                            ChildHandle.
    EFI_INVALID_PARAMETER - ControllerHandle is not a valid EFI_HANDLE.
    EFI_INVALID_PARAMETER - ChildHandle is not NULL and it is not a 
                            valid EFI_HANDLE.
    EFI_INVALID_PARAMETER - ActionRequired is NULL.
    EFI_UNSUPPORTED       - The driver specified by This does not support 
                            forcing the default configuration options on 
                            the controller specified by ControllerHandle 
                            and ChildHandle.
    EFI_UNSUPPORTED       - The driver specified by This does not support 
                            the configuration type specified by DefaultType.
    EFI_DEVICE_ERROR      - A device error occurred while attempt to force 
                            the default configuration options on the controller 
                            specified by  ControllerHandle and ChildHandle.
    EFI_OUT_RESOURCES     - There are not enough resources available to force 
                            the default configuration options on the controller 
                            specified by ControllerHandle and ChildHandle.

--*/
{
  UINT8 Value;

  if (ChildHandle != NULL) {
    return EFI_UNSUPPORTED;
  }

  Value = 0x0f;
  gRT->SetVariable (
        L"Configuration",
        &gIDEBusDriverGuid,
        EFI_VARIABLE_NON_VOLATILE | EFI_VARIABLE_BOOTSERVICE_ACCESS,
        sizeof (Value),
        &Value
        );
  *ActionRequired = EfiDriverConfigurationActionRestartController;
  return EFI_SUCCESS;
}
