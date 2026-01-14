#include "game_api.hpp"
#include <cmath>
#include <glm/glm.hpp>
#include <vector>

// ============================================================================
// Flags
// ============================================================================
enum entity_flags : uint32_t
{
  ENTITY_FLAG_NONE   = 0,
  ENTITY_FLAG_LOCAL  = 1 << 0,
  ENTITY_FLAG_REMOTE = 1 << 1,
};

// ============================================================================
// Per-entity driving state
// ============================================================================
struct drive_state
{
  uint64_t entity_id = 0;
  float    speed     = 0.0f;
  float    roll      = 0.0f;
};

// ============================================================================
// GAME STATE (ENGINE-OWNED)
// ============================================================================
struct game_state
{
  const engine_api_t* api = nullptr;

  void* scene = nullptr;

  // Entities
  std::vector<game_entity_t> entities;

  // Drive states
  std::vector<drive_state> drives;

  // Camera
  oasis_camera_state camera{};

  uint32_t local_client_id = UINT32_MAX;
};

// SINGLE global
static game_state* g = nullptr;

// ============================================================================
// Metadata
// ============================================================================
extern "C" game_info_t* game_get_info()
{
  static game_info_t info{OASIS_GAME_ABI_VERSION,
                          "racing-game",
                          "Racing Game",
                          "1.0.0",
                          "Aidan Sanders",
                          "Minimal racing demo",
                          "https://oasis.refugestudios.com.au"};
  return &info;
}

// ============================================================================
// Helpers
// ============================================================================
static game_entity_t* find_local_entity()
{
  for (auto& e : g->entities)
    if (e.flags & ENTITY_FLAG_LOCAL)
      return &e;
  return nullptr;
}

static drive_state* find_drive(uint64_t id)
{
  for (auto& d : g->drives)
    if (d.entity_id == id)
      return &d;

  g->drives.push_back({});
  drive_state& d = g->drives.back();
  d.entity_id    = id;
  d.speed        = 0.0f;
  d.roll         = 0.0f;
  return &d;
}

static void set_clear_color()
{
  glm::vec3 sky(0.4f, 0.6f, 0.9f);
  float     c[4] = {sky.x, sky.y, sky.z, 1.0f};
  g->api->clear_color(c);
}

static void add_entity(const game_entity_t& e)
{
  g->entities.push_back(e);
}

static void remove_entity(uint64_t id)
{
  // Remove entity
  for (auto it = g->entities.begin(); it != g->entities.end(); ++it)
  {
    if (it->id == id)
    {
      if (it->model)
        g->api->remove_model(it->model);
      g->entities.erase(it);
      break;
    }
  }

  // Remove drive state
  for (auto it = g->drives.begin(); it != g->drives.end(); ++it)
  {
    if (it->entity_id == id)
    {
      g->drives.erase(it);
      break;
    }
  }
}

// ============================================================================
// Lifecycle
// ============================================================================
extern "C" void game_init(const engine_api_t* api)
{
  if (!api || api->abi_version != OASIS_GAME_ABI_VERSION)
    return;

  if (!g)
  {
    g = new game_state{};
  }

  g->api = api;
  g->entities.clear();
  g->drives.clear();
  g->local_client_id = UINT32_MAX;

  api->log("Racing game initialized");

  g->scene = api->load_scene("games/racing-game/assets/track.svdag");

  g->camera.position[1] = 2.0f;
  g->camera.position[2] = 6.0f;
  g->camera.fov_y       = 1.0472f;
  g->camera.near_plane  = 0.1f;
  g->camera.far_plane   = 1000.0f;
  g->camera.mode        = CAMERA_MODE_FOLLOW;
  api->enable_game_camera(true);
}

extern "C" void game_shutdown()
{
  if (!g || !g->api)
    return;

  for (auto& e : g->entities)
    if (e.model)
      g->api->remove_model(e.model);

  if (g->scene)
    g->api->remove_scene(g->scene);

  g->entities.clear();
  g->drives.clear();
  g->scene = nullptr;
}

// ============================================================================
// Update
// ============================================================================
extern "C" void game_update(float dt)
{
  if (!g || !g->api)
    return;

  set_clear_color();

  game_entity_t* car = find_local_entity();
  if (!car)
  {
    g->api->set_camera_state(&g->camera);
    return;
  }

  drive_state* drive = find_drive(car->id);
  if (!drive)
    return;

  // ---------------------------------------------------------------------------
  // Tunables
  // ---------------------------------------------------------------------------
  constexpr float ENGINE_FORCE = 0.5f;
  constexpr float BRAKE_FORCE  = 0.2f;
  constexpr float MAX_SPEED    = 1.0f;
  constexpr float DRAG         = 2.0f;
  constexpr float STEER_RATE   = 22.0f;
  constexpr float MAX_CAM_ROLL = 0.25f; // ~14 degrees
  constexpr float ROLL_DAMP    = 4.0f;

  // ---------------------------------------------------------------------------
  // Input
  // ---------------------------------------------------------------------------
  float throttle = g->api->is_key_down(KEY_W) ? 1.0f : 0.0f;
  float brake    = g->api->is_key_down(KEY_S) ? 1.0f : 0.0f;

  float steer = 0.0f;
  if (g->api->is_key_down(KEY_D))
    steer -= 1.0f;
  if (g->api->is_key_down(KEY_A))
    steer += 1.0f;

  // ---------------------------------------------------------------------------
  // Speed integration
  // ---------------------------------------------------------------------------
  drive->speed += throttle * ENGINE_FORCE * dt;
  drive->speed -= brake * BRAKE_FORCE * dt;
  drive->speed -= drive->speed * DRAG * dt;
  drive->speed = glm::clamp(drive->speed, -MAX_SPEED * 0.4f, MAX_SPEED);

  // ---------------------------------------------------------------------------
  // Steering (speed-scaled)
  // ---------------------------------------------------------------------------
  float speed_factor = std::min(std::abs(drive->speed) / MAX_SPEED, 1.0f);
  car->rotation[1] += steer * STEER_RATE * speed_factor * dt;

  // ---------------------------------------------------------------------------
  // Forward motion
  // ---------------------------------------------------------------------------
  glm::vec3 forward{std::sin(car->rotation[1]), 0.0f, std::cos(car->rotation[1])};

  car->position[0] += forward.x * drive->speed * dt;
  car->position[2] += forward.z * drive->speed * dt;

  // ---------------------------------------------------------------------------
  // Camera roll (visual feedback)
  // ---------------------------------------------------------------------------
  float target_roll = -steer * speed_factor * MAX_CAM_ROLL;
  drive->roll += (target_roll - drive->roll) * std::min(1.0f, ROLL_DAMP * dt);

  // ---------------------------------------------------------------------------
  // Camera follow
  // ---------------------------------------------------------------------------
  constexpr float FOLLOW_DIST = 0.35f;
  constexpr float FOLLOW_H    = 0.25f;
  constexpr float CAM_DAMP    = 6.0f;

  glm::vec3 car_pos(car->position[0], car->position[1], car->position[2]);
  glm::vec3 desired = car_pos - forward * FOLLOW_DIST + glm::vec3(0.0f, FOLLOW_H, 0.0f);

  for (int i = 0; i < 3; ++i)
    g->camera.position[i] += (desired[i] - g->camera.position[i]) * std::min(1.0f, CAM_DAMP * dt);

  glm::vec3 look = glm::normalize(
      car_pos - glm::vec3(g->camera.position[0], g->camera.position[1], g->camera.position[2]));

  g->camera.rotation[0] = std::asin(look.y);          // pitch
  g->camera.rotation[1] = std::atan2(look.x, look.z); // yaw
  g->camera.rotation[2] = drive->roll;                // roll

  g->api->set_camera_state(&g->camera);
}

// ============================================================================
// Multiplayer
// ============================================================================
extern "C" void game_on_local_client_ready(uint32_t id)
{
  g->local_client_id = id;

  game_entity_t e{};
  e.id          = id;
  e.position[1] = -0.09f;
  e.scale       = 0.02f;
  e.flags       = ENTITY_FLAG_LOCAL;
  e.model       = g->api->load_model("games/racing-game/assets/car.svdag");
  add_entity(e);
}

extern "C" void game_on_client_join(uint32_t id)
{
  if (id == g->local_client_id)
    return;

  game_entity_t e{};
  e.id          = id;
  e.position[1] = -0.09f;
  e.scale       = 0.02f;
  e.flags       = ENTITY_FLAG_REMOTE;
  e.model       = g->api->load_model("games/racing-game/assets/car.svdag");
  add_entity(e);
}

extern "C" void game_on_client_disconnect(uint32_t id)
{
  remove_entity(id);
}

// ============================================================================
// Engine queries
// ============================================================================
extern "C" size_t game_get_entity_count()
{
  return g ? g->entities.size() : 0;
}

extern "C" game_entity_t* game_get_entities()
{
  return g && !g->entities.empty() ? g->entities.data() : nullptr;
}