constexpr uintptr_t GWorld = 0xC2B5640;
constexpr uintptr_t FnamePool = 0xC458640;

constexpr uintptr_t OwningGameInstance = 0x01D8;
constexpr uintptr_t PersistentLevel = 0x0038;
constexpr uintptr_t LocalPlayers = 0x0040;
constexpr uintptr_t GameState = 0x0178;

constexpr uintptr_t PlayerController = 0x0038;
constexpr uintptr_t AcknowledgedPawn = 0x0518;
constexpr uintptr_t PlayerState = 0x0488;
constexpr uintptr_t PlayerArray = 0x0480; //not sure
constexpr uintptr_t RelativeRotation = 0x0188;

constexpr uintptr_t PlayerCameraManager = 0x0528;
constexpr uintptr_t CameraRotation = 0x059C; //not sure
constexpr uintptr_t CameraFOV = 0x0580; //not sure
constexpr uintptr_t CameraPrivate = 0x017B0;

constexpr uintptr_t ActorArray = 0x0A0;
inline std:trdiff_t ActorCount = ActorArray + 0x8;

constexpr uintptr_t RootComponent = 0x290;
constexpr uintptr_t Mesh = 0x04F0;
constexpr uintptr_t TeamComponent = 0x06A8;
constexpr uintptr_t DamageHandler = 0x0C68;
constexpr uintptr_t Inventory = 0x0C08;

constexpr uintptr_t ComponentToWorld = 0x02D0; //not sure
constexpr uintptr_t RelativeLocation = 0x0170;
constexpr uintptr_t BoundsScale = 0x0474;
constexpr uintptr_t LastSubmitTime = BoundsScale + 0x4;
constexpr uintptr_t LastSubmitTimeOnScreen = BoundsScale + 0x8;

constexpr uintptr_t BoneArray = 0x0740;
constexpr uintptr_t BoneCount = BoneArray + 0x8;
constexpr uintptr_t BoneArrayCache = BoneArray + 0x10;

constexpr uintptr_t Health = 0x0200;
constexpr uintptr_t TeamID = 0xF8; //not sure
constexpr uintptr_t WasAlly = 0x0F68;
constexpr uintptr_t IsDormant = 0x0225;

constexpr uintptr_t CurrentEquippable = 0x0278;



testing's sign;;


fmemory_malloc = 0xC84080
static_find_object = 0x108EC30
static_load_object = 0x1091140
play_finisher_effect = 0x1F76A60
set_ares_outline = 0x31640D0
bone_matrix = 0x3197400
get_spread_values = 0x4DD1390
get_spread_angles = 0x586BFD0
tovector_and_normalize = 0xD6AD90
toangle_and_normalize = 0xD67FB0
firing_state_component = 0x1228
seed_data = 0x4a0
unknown_pad = 0xD8
stability_component = 0x490
error_power = 0x49C
error_retries = 0x470
OwningGameInstance = 0x1A0
inventory = 0x09A8
equippable = 0x0248
mesh3pgun = 0x0F38
mesh1pgun = 0x0F48
bone_cout = 0x748
real_time_dormant = 0x101
mesh3p_mids = 0xf90
mesh1p = 0xf40
mesh1p_overlay = 0xf48
mesh_cosmetic_3p = 0xf50
InventoryIcon = 0xf30
was_invisible = 0xc8b
sky_mesh_component = 0x290
wireframe_num = 0x8fe
wireframe_num_1 = 0xC0
defuse_percentage = 0x5D0
bomb_time_remaining = 0x5A8
skin_data_assets = 0xFB0
skin_pointer = 0x3a8
skin_pointer_2 = 0x0B0
skin_pointer_3 = 0x080
equippable_models = 0xe8
charm_instance = 0x438
set_pov_hook = 0xf2
draw_transition = 99
uworld_pointer = 0x80
uWorld (State) = 0xC2B5640
FMemoryMalloc = 0x178A3F0
ProcessEvent = 0x1BA2AE0
TriggerVEH = 0x17C1CA6
StaticFindObject = 0x1BCA7F0
StaticLoadObject = 0x1BCE320
GetSpreadValues = 0x62FF7C0
GetSpreadAngles = 0x6F68370
AresOutline = 0x4083090
BoneMatrix = 0x40EEC10
PlayFinisher = 0x62604A0
ToVectorAndNormalize = 0x1889AD0
ToAngleAndNormalize = 0x18842D0
Mount = 0x28D2AD0
BypassSigning = 0xC643038
GetFPAK = 0xC2E7278
