#include <cstddef>
namespace offsets
{
// core engine offsets (unchanged)
inline std:trdiff_t uworld_pointer = 0x0A4B02F0;
inline std:trdiff_t game_instance = 0x1D8;
inline std:trdiff_t gamestate = 0x0178;
inline std:trdiff_t persistent_level = 0x38;
inline std:trdiff_t localplayers_array = 0x480;
inline std:trdiff_t localplayer = 0x40;
inline std:trdiff_t player_controller = 0x38;
inline std:trdiff_t player_camera_manager = 0x528; // UPDATED: was 0x520
inline std:trdiff_t camera_private = 0x20a0;
inline std:trdiff_t camera_manager = 0x528; // UPDATED: was 0x520
inline std:trdiff_t control_rotation = 0x04E8; // UPDATED: was 0x04E0
inline std:trdiff_t fname_pool = 0xA547C00;
inline std:trdiff_t outline_mode = 0x31640D0;
inline std:trdiff_t outline_component = 0x0D80;

// component/attachment (unchanged)
inline std:trdiff_t attach_children = 0x1A0;
inline std:trdiff_t attach_children_count = 0x1A8;

// player/pawn related (unchanged except root_component)
inline std:trdiff_t acknowledge_pawn = 0x518;
inline std:trdiff_t apawn = 0x518;
inline std:trdiff_t player_state = 0x488;
inline std:trdiff_t player_state_array = 0x0178;
inline std:trdiff_t spawned_character = 0x0A18;
inline std:trdiff_t root_component = 0x0290;
inline std:trdiff_t root_position = 0x0170;

// health/damage
inline std:trdiff_t damage_handler = 0x0C68; // UPDATED: was 0x0C50
inline std:trdiff_t health = 0x0200;
inline std:trdiff_t shieldtype = 0x118;
inline std:trdiff_t shieldlife = 0x124;
inline std:trdiff_t shieldmaxlife = 0x128;
inline std:trdiff_t damagesections = 0x1c8;

// mesh/rendering (unchanged)
inline std:trdiff_t mesh = 0x04f0;
inline std:trdiff_t component_to_world = 0x2d0;
inline std:trdiff_t BoundsScale = 0x474;
inline std:trdiff_t bone_array = 0x740;
inline std:trdiff_t bone_array_cache = 0x748;
inline std:trdiff_t bone_count = 0x748;
inline std:trdiff_t LocalBounds = 0x0730;
inline std:trdiff_t last_submit_time = 0x478;
inline std:trdiff_t last_render_time = 0x47C;
inline std:trdiff_t relative_location = 0x170;
inline std:trdiff_t relative_rotation = 0x170;

// team Related
inline std:trdiff_t team_component = 0x6a8;
inline std:trdiff_t team_id = 0xE8; // UPDATED: was 0xF8
inline std:trdiff_t was_ally = 0xf68;

// minimap/UI (unchanged)
inline std:trdiff_t CharacterPortraitMinimapComponent = 0x1210;
inline std:trdiff_t ShooterCharacterMinimapComponent = 0x1218;

// visibility
inline std:trdiff_t VisibilityComponent = 0x538;
inline std:trdiff_t CharactersWithVisibility = 0x118;
inline std:trdiff_t bIsVisible = 0x521;
inline std:trdiff_t bLocalObserver = 0x550;
inline std:trdiff_t dormant = 0x225; // Keep original, test 0xd8 if issues

// actor Management
inline std:trdiff_t actor_array = 0xa0;
inline std:trdiff_t actors_count = 0xa8;
inline std:trdiff_t actor_id = 0x18;
inline std:trdiff_t unique_id = 0x30; // UPDATED: was 0x38
}
