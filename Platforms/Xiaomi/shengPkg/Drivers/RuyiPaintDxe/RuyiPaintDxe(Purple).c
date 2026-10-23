/** @file
  Synchronous display probe for the ruyi display bring-up.

  Paints a magenta block that walks down the screen using gBS->Stall() between steps, then
  returns. Nothing here depends on the timer API, on a GOP, or on the console: it only writes
  into the physical buffer the ABL left scanning out, at /reserved-memory/splash_region.

  Five seconds of visible motion prove three things at once: the driver executes, the frame
  buffer address is the scanned-out one, and the firmware is alive at DXE dispatch time.

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
// Block height in pixels and number of animation steps.
//
#define RUYI_BLOCK_ROWS  60
#define RUYI_STEPS       10

STATIC
VOID
PaintBlock (
  IN UINT32  Row
  )
{
  UINT32  Width = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  *Fb   = (UINT32 *)(UINTN)RUYI_FB_BASE;
  UINT32  X;
  UINT32  Y;

  for (Y = Row; Y < Row + RUYI_BLOCK_ROWS; Y++) {
    for (X = 0; X < Width; X++) {
      Fb[(UINTN)Y * Width + X] = 0x00FF00FFU;   // magenta, BGRX
    }
  }
}

EFI_STATUS
EFIAPI
RuyiPaintEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  UINT32  Width  = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  Height = FixedPcdGet32 (PcdFrameBufferHeight);
  UINT32  Step;
  UINT32  Span;

  DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: entry %ux%u at 0x%lx\n", Width, Height, RUYI_FB_BASE));

  if ((Width == 0) || (Height == 0) || (Width > 4096) || (Height > 4096)) {
    DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: bad geometry\n"));
    return EFI_SUCCESS;
  }

  Span = (Height > 2 * RUYI_BLOCK_ROWS) ? (Height - 2 * RUYI_BLOCK_ROWS) : 1;

  for (Step = 0; Step < RUYI_STEPS; Step++) {
    PaintBlock ((Step * Span) / RUYI_STEPS);
    gBS->Stall (500000);   // 500 ms per step, 10 steps = 5 s
  }

  DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: walked %u steps\n", RUYI_STEPS));
  return EFI_SUCCESS;
}
