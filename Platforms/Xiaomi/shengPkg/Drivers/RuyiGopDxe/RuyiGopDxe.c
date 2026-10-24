/** @file
  Minimal GOP on the frame buffer the ABL leaves scanning out, plus a status strip that reports
  the timer plumbing without needing a working timer.

  Every Blt ends by repainting a black band at row 2600 with three 64x64 blocks on it, so the
  strip lands on top of whatever the boot manager drew and survives full-screen redraws. The row
  sits far from the colour bars the earlier probes left at the top of the screen.

    block 1 (x=0)    CreateEvent  green ok / red failed
    block 2 (x=110)  SetTimer     green ok / red failed
    block 3 (x=220)  white once the timer callback has fired at least once

  Frame buffer writes are followed by a cache write-back because the buffer is reserved RAM
  mapped as normal cacheable memory.

  Copyright (c) 2026, UniXNU contributors. All rights reserved.
  SPDX-License-Identifier: BSD-2-Clause-Patent
**/

#include <Uefi.h>
#include <Library/BaseMemoryLib.h>
#include <Library/CacheMaintenanceLib.h>
#include <Library/DebugLib.h>
#include <Library/MemoryAllocationLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>
#include <Protocol/DevicePath.h>
#include <Protocol/GraphicsOutput.h>

#define RUYI_FB_BASE  0xFC400000ULL

#define RED    0x000000FFU
#define GREEN  0x0000FF00U
#define BLACK  0x00000000U
#define WHITE  0x00FFFFFFU

//
// Row of the status strip. Kept away from the colour bars at the top of the screen.
//
#define STRIP_ROW     2600
#define BLOCK_SIZE    64
#define BLOCK_STEP    110
#define STRIP_WIDTH   330

//
// Row of the moving heartbeat block, only used once the timer works.
//
#define BEAT_ROW      1000

#pragma pack(1)
typedef struct {
  VENDOR_DEVICE_PATH        Vendor;
  EFI_DEVICE_PATH_PROTOCOL  End;
} RUYI_DISPLAY_DEVICE_PATH;
#pragma pack()

STATIC RUYI_DISPLAY_DEVICE_PATH mDevicePath = {
  {
    { HARDWARE_DEVICE_PATH, HW_VENDOR_DP, { sizeof (VENDOR_DEVICE_PATH), 0 } },
    { 0x1B7E5C2A, 0x5F3D, 0x4B9A, { 0x9C, 0x21, 0x7D, 0x6E, 0x4F, 0x8A, 0x0B, 0x13 } }
  },
  { END_DEVICE_PATH_TYPE, END_ENTIRE_DEVICE_PATH_SUBTYPE, { sizeof (EFI_DEVICE_PATH_PROTOCOL), 0 } }
};

STATIC EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  mModeInfo;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL_MODE    mMode;
STATIC EFI_GRAPHICS_OUTPUT_PROTOCOL         mGop;

STATIC EFI_EVENT  mBlinkEvent  = NULL;
STATIC BOOLEAN    mBlinkOn     = FALSE;

STATIC BOOLEAN    mStripReady  = FALSE;
STATIC UINT32     mStripCreate = BLACK;
STATIC UINT32     mStripTimer  = BLACK;
STATIC UINT32     mStripTick   = BLACK;

STATIC
UINT32 *
FrameBuffer (
  VOID
  )
{
  return (UINT32 *)(UINTN)RUYI_FB_BASE;
}

/**
  Write back the given rows so the display engine sees them.

  @param[in]  FirstRow  First row that was written.
  @param[in]  RowCount  Number of rows that were written.
**/
STATIC
VOID
FlushRows (
  IN UINTN  FirstRow,
  IN UINTN  RowCount
  )
{
  UINTN  Pitch  = mModeInfo.PixelsPerScanLine * sizeof (UINT32);
  UINTN  Offset = FirstRow * Pitch;
  UINTN  Length = RowCount * Pitch;

  if ((Length == 0) || (Offset >= mMode.FrameBufferSize)) {
    return;
  }

  if (Offset + Length > mMode.FrameBufferSize) {
    Length = mMode.FrameBufferSize - Offset;
  }

  WriteBackDataCacheRange ((VOID *)(UINTN)(RUYI_FB_BASE + Offset), Length);
}

/**
  Paint a 64x64 block on the status strip row at the given column.
**/
STATIC
VOID
PaintBlock (
  IN UINTN   Column,
  IN UINT32  Colour
  )
{
  UINT32  *Fb   = FrameBuffer ();
  UINTN   Pitch = mModeInfo.PixelsPerScanLine;
  UINTN   X;
  UINTN   Y;

  for (Y = 0; Y < BLOCK_SIZE; Y++) {
    for (X = 0; X < BLOCK_SIZE; X++) {
      Fb[(STRIP_ROW + Y) * Pitch + Column + X] = Colour;
    }
  }
}

/**
  Repaint the status strip on top of the current frame.

  A black backdrop is laid down first so a black block means "painted and not fired" rather than
  "never drawn", and so the strip reads the same on top of any artwork.
**/
STATIC
VOID
PaintStatusStrip (
  VOID
  )
{
  UINT32  *Fb   = FrameBuffer ();
  UINTN   Pitch = mModeInfo.PixelsPerScanLine;
  UINTN   X;
  UINTN   Y;

  if (!mStripReady) {
    return;
  }

  for (Y = 0; Y < BLOCK_SIZE; Y++) {
    for (X = 0; X < STRIP_WIDTH; X++) {
      Fb[(STRIP_ROW + Y) * Pitch + X] = BLACK;
    }
  }

  PaintBlock (0 * BLOCK_STEP, mStripCreate);
  PaintBlock (1 * BLOCK_STEP, mStripTimer);
  PaintBlock (2 * BLOCK_STEP, mStripTick);

  FlushRows (STRIP_ROW, BLOCK_SIZE);
}

STATIC
EFI_STATUS
EFIAPI
GopQueryMode (
  IN  EFI_GRAPHICS_OUTPUT_PROTOCOL          *This,
  IN  UINT32                                ModeNumber,
  OUT UINTN                                 *SizeOfInfo,
  OUT EFI_GRAPHICS_OUTPUT_MODE_INFORMATION  **Info
  )
{
  if (ModeNumber != 0) {
    return EFI_UNSUPPORTED;
  }

  *Info = AllocateCopyPool (sizeof (mModeInfo), &mModeInfo);
  if (*Info == NULL) {
    return EFI_OUT_OF_RESOURCES;
  }

  *SizeOfInfo = sizeof (mModeInfo);
  return EFI_SUCCESS;
}

STATIC
EFI_STATUS
EFIAPI
GopSetMode (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL  *This,
  IN UINT32                        ModeNumber
  )
{
  return (ModeNumber == 0) ? EFI_SUCCESS : EFI_UNSUPPORTED;
}

STATIC
EFI_STATUS
EFIAPI
GopBlt (
  IN EFI_GRAPHICS_OUTPUT_PROTOCOL       *This,
  IN EFI_GRAPHICS_OUTPUT_BLT_PIXEL      *BltBuffer OPTIONAL,
  IN EFI_GRAPHICS_OUTPUT_BLT_OPERATION  BltOperation,
  IN UINTN                              SourceX,
  IN UINTN                              SourceY,
  IN UINTN                              DestinationX,
  IN UINTN                              DestinationY,
  IN UINTN                              Width,
  IN UINTN                              Height,
  IN UINTN                              Delta OPTIONAL
  )
{
  UINT32  *Fb    = FrameBuffer ();
  UINTN   Pitch  = mModeInfo.PixelsPerScanLine;
  UINTN   SrcDelta;
  UINTN   X;
  UINTN   Y;

  if ((Width == 0) || (Height == 0)) {
    return EFI_SUCCESS;
  }

  if ((DestinationX + Width) > mModeInfo.HorizontalResolution) {
    return EFI_INVALID_PARAMETER;
  }

  if ((DestinationY + Height) > mModeInfo.VerticalResolution) {
    return EFI_INVALID_PARAMETER;
  }

  SrcDelta = (Delta == 0) ? Width * sizeof (UINT32) : Delta;

  switch (BltOperation) {
    case EfiBltVideoFill:
    {
      UINT32  Colour = *(UINT32 *)BltBuffer;

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (DestinationY + Y) * Pitch + DestinationX;
        for (X = 0; X < Width; X++) {
          Row[X] = Colour;
        }
      }

      FlushRows (DestinationY, Height);
      break;
    }

    case EfiBltBufferToVideo:
    {
      UINT8  *Src = (UINT8 *)BltBuffer + SourceY * SrcDelta + SourceX * sizeof (UINT32);

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (DestinationY + Y) * Pitch + DestinationX;
        CopyMem (Row, Src + Y * SrcDelta, Width * sizeof (UINT32));
      }

      FlushRows (DestinationY, Height);
      break;
    }

    case EfiBltVideoToBltBuffer:
    {
      UINT8  *Dst = (UINT8 *)BltBuffer + DestinationY * SrcDelta + DestinationX * sizeof (UINT32);

      //
      // Only read from the frame buffer here. Invalidate the source rows first so the copy sees
      // what the display engine sees.
      //
      InvalidateDataCacheRange (
        (VOID *)(UINTN)(RUYI_FB_BASE + SourceY * Pitch * sizeof (UINT32)),
        Height * Pitch * sizeof (UINT32)
        );

      for (Y = 0; Y < Height; Y++) {
        UINT32  *Row = Fb + (SourceY + Y) * Pitch + SourceX;
        CopyMem (Dst + Y * SrcDelta, Row, Width * sizeof (UINT32));
      }

      break;
    }

    case EfiBltVideoToVideo:
    {
      for (Y = 0; Y < Height; Y++) {
        UINT32  *Src = Fb + (SourceY + Y) * Pitch + SourceX;
        UINT32  *Dst = Fb + (DestinationY + Y) * Pitch + DestinationX;
        CopyMem (Dst, Src, Width * sizeof (UINT32));
      }

      FlushRows (DestinationY, Height);
      break;
    }

    default:
      return EFI_UNSUPPORTED;
  }

  //
  // Land the status strip on top of whatever was just drawn.
  //
  PaintStatusStrip ();

  return EFI_SUCCESS;
}

/**
  Timer callback: turns the third status block white and keeps a red/white block blinking at
  row 1000.
**/
STATIC
VOID
EFIAPI
BlinkTick (
  IN EFI_EVENT  Event,
  IN VOID       *Context
  )
{
  UINT32  *Fb    = FrameBuffer ();
  UINTN   Pitch  = mModeInfo.PixelsPerScanLine;
  UINT32  Colour = mBlinkOn ? RED : WHITE;
  UINTN   X;
  UINTN   Y;

  mStripTick = WHITE;

  mBlinkOn = !mBlinkOn;

  for (Y = 0; Y < BLOCK_SIZE; Y++) {
    for (X = 0; X < BLOCK_SIZE; X++) {
      Fb[(BEAT_ROW + Y) * Pitch + X] = Colour;
    }
  }

  WriteBackDataCacheRange (
    (VOID *)(UINTN)(RUYI_FB_BASE + BEAT_ROW * Pitch * sizeof (UINT32)),
    Pitch * BLOCK_SIZE * sizeof (UINT32)
    );
}

EFI_STATUS
EFIAPI
RuyiGopEntry (
  IN EFI_HANDLE        ImageHandle,
  IN EFI_SYSTEM_TABLE  *SystemTable
  )
{
  EFI_STATUS  Status;
  UINT32      Width  = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32      Height = FixedPcdGet32 (PcdFrameBufferHeight);
  EFI_STATUS  CreateStatus;
  EFI_STATUS  TimerStatus;

  if ((Width < 640) || (Height < 475)) {
    DEBUG ((EFI_D_ERROR, "RuyiGopDxe: geometry %ux%u too small\n", Width, Height));
    return EFI_SUCCESS;
  }

  ZeroMem (&mModeInfo, sizeof (mModeInfo));
  mModeInfo.Version              = 0;
  mModeInfo.HorizontalResolution = Width;
  mModeInfo.VerticalResolution   = Height;
  mModeInfo.PixelFormat          = PixelBlueGreenRedReserved8BitPerColor;
  mModeInfo.PixelsPerScanLine    = Width;

  ZeroMem (&mMode, sizeof (mMode));
  mMode.MaxMode         = 1;
  mMode.Mode            = 0;
  mMode.Info            = &mModeInfo;
  mMode.SizeOfInfo      = sizeof (mModeInfo);
  mMode.FrameBufferBase = RUYI_FB_BASE;
  mMode.FrameBufferSize = (UINTN)Width * Height * sizeof (UINT32);

  mGop.QueryMode = GopQueryMode;
  mGop.SetMode   = GopSetMode;
  mGop.Blt       = GopBlt;
  mGop.Mode      = &mMode;

  Status = gBS->InstallMultipleProtocolInterfaces (
                  &ImageHandle,
                  &gEfiDevicePathProtocolGuid, &mDevicePath,
                  &gEfiGraphicsOutputProtocolGuid, &mGop,
                  NULL
                  );
  DEBUG ((EFI_D_ERROR, "RuyiGopDxe: install %ux%u -> %r\n", Width, Height, Status));

  //
  // Arm the heartbeat and record the outcome of both calls for the status strip.
  //
  CreateStatus = gBS->CreateEvent (
                  EVT_TIMER | EVT_NOTIFY_SIGNAL,
                  TPL_CALLBACK,
                  BlinkTick,
                  NULL,
                  &mBlinkEvent
                  );
  TimerStatus  = EFI_NOT_STARTED;
  if (!EFI_ERROR (CreateStatus)) {
    TimerStatus = gBS->SetTimer (mBlinkEvent, TimerPeriodic, 10000000);
  }

  mStripCreate = EFI_ERROR (CreateStatus) ? RED : GREEN;
  mStripTimer  = EFI_ERROR (TimerStatus)  ? RED : GREEN;
  mStripReady  = TRUE;

  DEBUG ((
    EFI_D_ERROR,
    "RuyiGopDxe: CreateEvent %r, SetTimer %r\n",
    CreateStatus,
    TimerStatus
    ));

  return EFI_SUCCESS;
}
