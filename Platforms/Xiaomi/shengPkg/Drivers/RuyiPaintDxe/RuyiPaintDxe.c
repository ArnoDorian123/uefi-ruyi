/** @file
  Colour-band probe for the ruyi display bring-up.

  Writes a handful of horizontal bands into the physical buffer the ABL splash lives in, then
  returns. Nothing here depends on the UEFI memory map, on a GOP, or on a device tree name
  lookup, so colours appearing on screen is proof that firmware code ran and that the address
  is the one being scanned out.

  Only the first rows are painted on purpose: the reserved region is declared as 45 MiB, but
  the firmware page tables are not guaranteed to map all of it. Staying inside a couple of
  megabytes removes the "write into an unmapped page" failure mode, which would otherwise be
  indistinguishable from "wrong address".

  Remove this driver once a real display driver exists.

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

//
// Upper bound on painted rows: 320 rows at 1224 pixels and 32 bpp is about 1.5 MiB.
//
#define RUYI_PAINT_MAX_ROWS  320

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
  UINT32  Rows;
  UINT32  X;
  UINT32  Y;

  if ((Width == 0) || (Height == 0) || (Width > 4096) || (Height > 4096)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: bad geometry %ux%u\n", Width, Height));
    return EFI_SUCCESS;
  }

  Rows = Height / 8;
  if (Rows > RUYI_PAINT_MAX_ROWS) {
    Rows = RUYI_PAINT_MAX_ROWS;
  }

  for (Y = 0; Y < Rows; Y++) {
    UINT32  Colour;

    if (Y < Rows / 3) {
      Colour = 0x000000FFU;   // red   (BGRX)
    } else if (Y < (2 * Rows) / 3) {
      Colour = 0x0000FF00U;   // green
    } else {
      Colour = 0x00FF0000U;   // blue
    }

    for (X = 0; X < Width; X++) {
      Fb[(UINTN)Y * Width + X] = Colour;
    }
  }

  DEBUG ((
    EFI_D_ERROR,
    "RuyiPaintDxe: painted %u rows of %ux%u at 0x%lx\n",
    Rows,
    Width,
    Height,
    RUYI_FB_BASE
    ));

  return EFI_SUCCESS;
}
