// game_api.hpp
#pragma once
#include <cstddef>
#include <cstdint>

#define OASIS_GAME_ABI_VERSION 1

extern "C"
{

  // -----------------------------------------------------------------------------
  // Game metadata (read-only, returned by the module)
  // -----------------------------------------------------------------------------
  struct game_info_t
  {
    uint32_t    abi_version; // Must match OASIS_GAME_ABI_VERSION
    const char* game_id;     // Stable unique ID (e.g. "racing-demo")
    const char* name;
    const char* version;
    const char* author;
    const char* description;
    const char* homepage; // Optional (nullptr allowed)
  };

  // -----------------------------------------------------------------------------
  // Game entity (shared between engine & game module)
  // -----------------------------------------------------------------------------
  struct game_entity_t
  {
    uint64_t id;          // Unique entity ID (player/car)
    void*    model;       // Optional model handle (svdag pointer)
    float    position[3]; // world position
    float    rotation;    // rotation around Y-axis
    float    scale;       // uniform scale
    uint32_t flags;       // custom flags (optional)
  };

  // -----------------------------------------------------------------------------
  // Engine input codes (DO NOT expose GLFW)
  // -----------------------------------------------------------------------------
  enum oasis_key
  {
    KEY_UNKNOWN = 0,

    // -------------------------------------------------
    // Letters
    // -------------------------------------------------
    KEY_A,
    KEY_B,
    KEY_C,
    KEY_D,
    KEY_E,
    KEY_F,
    KEY_G,
    KEY_H,
    KEY_I,
    KEY_J,
    KEY_K,
    KEY_L,
    KEY_M,
    KEY_N,
    KEY_O,
    KEY_P,
    KEY_Q,
    KEY_R,
    KEY_S,
    KEY_T,
    KEY_U,
    KEY_V,
    KEY_W,
    KEY_X,
    KEY_Y,
    KEY_Z,

    // -------------------------------------------------
    // Numbers (top row)
    // -------------------------------------------------
    KEY_0,
    KEY_1,
    KEY_2,
    KEY_3,
    KEY_4,
    KEY_5,
    KEY_6,
    KEY_7,
    KEY_8,
    KEY_9,

    // -------------------------------------------------
    // Function keys
    // -------------------------------------------------
    KEY_F1,
    KEY_F2,
    KEY_F3,
    KEY_F4,
    KEY_F5,
    KEY_F6,
    KEY_F7,
    KEY_F8,
    KEY_F9,
    KEY_F10,
    KEY_F11,
    KEY_F12,
    KEY_F13,
    KEY_F14,
    KEY_F15,
    KEY_F16,
    KEY_F17,
    KEY_F18,
    KEY_F19,
    KEY_F20,
    KEY_F21,
    KEY_F22,
    KEY_F23,
    KEY_F24,

    // -------------------------------------------------
    // Modifiers
    // -------------------------------------------------
    KEY_LEFT_SHIFT,
    KEY_RIGHT_SHIFT,
    KEY_LEFT_CTRL,
    KEY_RIGHT_CTRL,
    KEY_LEFT_ALT,
    KEY_RIGHT_ALT,
    KEY_LEFT_SUPER,
    KEY_RIGHT_SUPER,

    // -------------------------------------------------
    // Navigation
    // -------------------------------------------------
    KEY_UP,
    KEY_DOWN,
    KEY_LEFT,
    KEY_RIGHT,
    KEY_HOME,
    KEY_END,
    KEY_PAGE_UP,
    KEY_PAGE_DOWN,

    // -------------------------------------------------
    // Editing
    // -------------------------------------------------
    KEY_ENTER,
    KEY_ESCAPE,
    KEY_TAB,
    KEY_BACKSPACE,
    KEY_INSERT,
    KEY_DELETE,

    // -------------------------------------------------
    // Symbols
    // -------------------------------------------------
    KEY_SPACE,
    KEY_MINUS,
    KEY_EQUAL,
    KEY_LEFT_BRACKET,
    KEY_RIGHT_BRACKET,
    KEY_BACKSLASH,
    KEY_SEMICOLON,
    KEY_APOSTROPHE,
    KEY_GRAVE,
    KEY_COMMA,
    KEY_PERIOD,
    KEY_SLASH,

    // -------------------------------------------------
    // Numpad
    // -------------------------------------------------
    KEY_NUM_LOCK,
    KEY_KP_0,
    KEY_KP_1,
    KEY_KP_2,
    KEY_KP_3,
    KEY_KP_4,
    KEY_KP_5,
    KEY_KP_6,
    KEY_KP_7,
    KEY_KP_8,
    KEY_KP_9,
    KEY_KP_DECIMAL,
    KEY_KP_DIVIDE,
    KEY_KP_MULTIPLY,
    KEY_KP_SUBTRACT,
    KEY_KP_ADD,
    KEY_KP_ENTER,
    KEY_KP_EQUAL,

    // -------------------------------------------------
    // Media / system
    // -------------------------------------------------
    KEY_PRINT_SCREEN,
    KEY_SCROLL_LOCK,
    KEY_PAUSE,
    KEY_MENU,

    // -------------------------------------------------
    // Sentinel
    // -------------------------------------------------
    KEY_COUNT
  };

  enum oasis_mouse_button
  {
    MOUSE_LEFT = 0,
    MOUSE_RIGHT,
    MOUSE_MIDDLE,
    MOUSE_BUTTON_4,
    MOUSE_BUTTON_5,
    MOUSE_BUTTON_6,
    MOUSE_BUTTON_7,
    MOUSE_BUTTON_8,

    MOUSE_BUTTON_COUNT
  };

  // -----------------------------------------------------------------------------
  // Camera
  // -----------------------------------------------------------------------------

  enum oasis_camera_mode
  {
    CAMERA_MODE_ENGINE_DEFAULT = 0, // engine handles camera fully
    CAMERA_MODE_FREE,
    CAMERA_MODE_FOLLOW,
    CAMERA_MODE_FIXED
  };

  struct oasis_camera_state
  {
    // Spatial
    float position[3]; // world-space
    float rotation[3]; // pitch, yaw, roll (radians)

    // Projection
    float fov_y; // radians
    float near_plane;
    float far_plane;

    // Optional behavior
    float follow_distance;
    float follow_height;
    float shake_strength;

    oasis_camera_mode mode;
  };

  // -----------------------------------------------------------------------------
  // Engine → Game API (function table)
  // -----------------------------------------------------------------------------
  struct engine_api_t
  {
    // ABI validation
    uint32_t abi_version;
    uint32_t _reserved0; // alignment / future use

    // -------------------------
    // Logging
    // -------------------------
    void (*log)(const char*);
    void (*warn)(const char*);
    void (*error)(const char*);

    // -------------------------
    // Timing
    // -------------------------
    uint64_t (*get_time_ms)();
    float (*get_delta_time)();

    // -------------------------
    // Input
    // -------------------------

    // Keyboard
    bool (*is_key_down)(int key);
    bool (*is_key_pressed)(int key);
    bool (*is_key_released)(int key);

    // Mouse buttons
    bool (*is_mouse_down)(int button);
    bool (*is_mouse_pressed)(int button);
    bool (*is_mouse_released)(int button);

    // Mouse position
    void (*get_mouse_position)(float* x, float* y);
    void (*get_mouse_delta)(float* dx, float* dy);

    // -------------------------
    // Camera control
    // -------------------------

    // Push full camera state (game-controlled camera)
    void (*set_camera_state)(const oasis_camera_state* state);

    // Read current camera state (engine → game)
    void (*get_camera_state)(oasis_camera_state* out_state);

    // Enable / disable game control of camera
    void (*enable_game_camera)(bool enabled);

    // -------------------------
    // Volumetric assets
    // -------------------------
    void* (*load_scene)(const char* svdag_path); // returns scene handle
    void* (*load_model)(const char* svdag_path); // returns model handle

    // -------------------------
    // Scene interaction (optional)
    // -------------------------
    void (*add_model_to_scene)(void* scene, void* model, float x, float y, float z);

    void (*update_model_transform)(void* model, float x, float y, float z, float rotation);

    // -------------------------
    // Lifetime
    // -------------------------
    void (*remove_model)(void* model);
    void (*remove_scene)(void* scene);

    void (*clear_color)(const float color[4]);

    // -------------------------
    // Reserved (MUST stay zero)
    // -------------------------
    void* _reserved[8];
  };

  // -----------------------------------------------------------------------------
  // Required module entry points
  // -----------------------------------------------------------------------------
  //
  game_info_t* game_get_info();

  //
  void game_init(const engine_api_t* api);

  //
  void game_update(float dt);

  //
  void game_shutdown();

  // Called when a new client joins
  void game_on_client_join(uint32_t client_id);

  // Called when a client disconnects
  void game_on_client_disconnect(uint32_t client_id);

  // -----------------------------------------------------------------------------
  // Optional: engine ↔ game entity query
  // -----------------------------------------------------------------------------
  size_t game_get_entity_count();

  game_entity_t* game_get_entities(); // engine can read/write positions directly

} // extern "C"
