/** @file
  Colour-band probe for the ruyi display bring-up.

  Writes three horizontal bands into the physical buffer the ABL splash lives in, then returns.
  Nothing here depends on the UEFI memory map, on a GOP, or on a device tree name lookup, so the
  colours appearing on screen is proof that firmware code ran and that the address is the one
  being scanned out.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

//
// /reserved-memory/splash_region in the ruyi device tree:
//   reg = <0x0 0xFC400000 0x0 0x2B00000>
//
#define RUYI_FB_BASE  0xFC400000ULL

EFI_STATUS
EFIAPI
RuyiPaintEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT32  Width  = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  Height = FixedPcdGet32 (PcdFrameBufferHeight);
  UINT32  *Fb    = (UINT32 *)(UINTN)RUYI_FB_BASE;
  UINT32  X;
  UINT32  Y;

  if ((Width == 0) || (Height == 0) || (Width > 4096) || (Height > 4096)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: bad geometry %ux%u\n", Width, Height));
    return EFI_SUCCESS;
  }

  for (Y = 0; Y < Height; Y++) {
    UINT32  Colour;

    if (Y < Height / 3) {
      Colour = 0x000000FFU;   // red   (BGRX)
    } else if (Y < (2 * Height) / 3) {
      Colour = 0x0000FF00U;   // green
    } else {
      Colour = 0x00FF0000U;   // blue
    }

    for (X = 0; X < Width; X++) {
      Fb[(UINTN)Y * Width + X] = Colour;
    }
  }

  DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: painted %ux%u at 0x%lx\n", Width, Height, RUYI_FB_BASE));
  return EFI_SUCCESS;
}
