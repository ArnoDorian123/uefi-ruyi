/** @file
  Solid-colour probe: fills the whole frame buffer once, then returns.
  If nothing appears, this driver is not executing at all.
**/

#include <Uefi.h>
#include <Library/DebugLib.h>
#include <Library/PcdLib.h>
#include <Library/UefiBootServicesTableLib.h>

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
  UINT32  Total;

  if ((Width == 0) || (Height == 0) || (Width > 4096) || (Height > 4096)) {
    return EFI_SUCCESS;
  }

  //
  // Only the first 200 rows: about 1.1 MiB at 1392 x 4. Keeps the write inside the region
  // the earlier band probe already proved writable.
  //
  Total = Width * 200;
  for (X = 0; X < Total; X++) {
    Fb[X] = 0x00FF00FFU;   // magenta
  }

  DEBUG ((EFI_D_ERROR, "RuyiPaintDxe: solid %u px at 0x%lx\n", Total, RUYI_FB_BASE));
  return EFI_SUCCESS;
}
