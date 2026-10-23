/** @file
  Geometry calibration probe: draws a frame, three vertical rulers and a moving bar.
  A photo of the result determines the real width and stride in one shot.
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

#define RUYI_FB_BASE  0xFC400000ULL

STATIC
VOID
PutPx (
  IN UINT32  X,
  IN UINT32  Y,
  IN UINT32  Colour
  )
{
  UINT32  Width = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  *Fb   = (UINT32 *)(UINTN)RUYI_FB_BASE;

  Fb[(UINTN)Y * Width + X] = Colour;
}

STATIC
VOID
FillRect (
  IN UINT32  X,
  IN UINT32  Y,
  IN UINT32  W,
  IN UINT32  H,
  IN UINT32  Colour
  )
{
  UINT32  dx;
  UINT32  dy;

  for (dy = 0; dy < H; dy++) {
    for (dx = 0; dx < W; dx++) {
      PutPx (X + dx, Y + dy, Colour);
    }
  }
}

STATIC
VOID
DrawFrame (
  VOID
  )
{
  UINT32  Width  = FixedPcdGet32 (PcdFrameBufferWidth);
  UINT32  Height = FixedPcdGet32 (PcdFrameBufferHeight);
  UINT32  i;

  // 1 px border: white
  for (i = 0; i < Width; i++) {
    PutPx (i, 0, 0x00FFFFFFU);
    PutPx (i, Height - 1, 0x00FFFFFFU);
  }
  for (i = 0; i < Height; i++) {
    PutPx (0, i, 0x00FFFFFFU);
    PutPx (Width - 1, i, 0x00FFFFFFU);
  }

  // Vertical rulers at 1/4, 1/2, 3/4 of the assumed width: red, green, blue
  for (i = 0; i < Height; i++) {
    PutPx (Width / 4,     i, 0x000000FFU);
    PutPx (Width / 2,     i, 0x0000FF00U);
    PutPx (Width * 3 / 4, i, 0x00FF0000U);
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

  DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: frame %ux%u at 0x%lx\n", Width, Height, RUYI_FB_BASE));

  DrawFrame ();
  gBS->Stall (2000000);   // 2 s to photograph the frame

  Span = (Height > 160) ? (Height - 160) : 1;
  for (Step = 0; Step < 10; Step++) {
    FillRect (Width / 8, (Step * Span) / 10, Width * 3 / 4, 60, 0x00FF00FFU);
    gBS->Stall (500000);
  }

  return EFI_SUCCESS;
}
