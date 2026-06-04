/*
 * NOCTURNE_OS — Forest OS v3.0 boot sequence: BIOS post, logo, loading bar.
 */
#ifndef NOCTURNE_BOOT_ANIM_H
#define NOCTURNE_BOOT_ANIM_H

class DisplayEngine;

/** Runs full boot sequence (blocking): Phase 1 BIOS scroll, Phase 2 logo +
 * title, Phase 3 loading bar. */
void drawBootSequence(DisplayEngine &display, bool fancy = true);

#endif
