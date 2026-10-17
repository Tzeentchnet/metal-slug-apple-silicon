/* Optional revision-specific frontend enhancements.
 *
 * The Metal Slug program has not yet been profiled for direct RAM writes, game-routine calls, or
 * title-logo extraction. Keep those features disabled until their addresses and code windows have
 * been independently verified for 201-P1.
 */
#include "profile.h"

const GameProfile GAME_PROFILE_ARCADE = {
    .id = "mslug-arcade-201-p1-unprofiled",
};

const GameProfile *profile_current(void) { return 0; }

int profile_supported(void) { return 0; }
