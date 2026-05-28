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
