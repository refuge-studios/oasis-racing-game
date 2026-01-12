#include "game_api.hpp"
#include <algorithm>
#include <cmath>
#include <cstdio>
#include <glm/glm.hpp>
#include <string>
#include <unordered_map>
#include <vector>

// -----------------------------------------------------------------------------
// Entity flags
// -----------------------------------------------------------------------------
enum entity_flags : uint32_t
{
  ENTITY_FLAG_NONE   = 0,
  ENTITY_FLAG_LOCAL  = 1 << 0,
  ENTITY_FLAG_REMOTE = 1 << 1,
  ENTITY_FLAG_STATIC = 1 << 2,
};

uint32_t g_local_client_id = UINT32_MAX;

// -----------------------------------------------------------------------------
// Driving state
// -----------------------------------------------------------------------------
struct drive_state
{
  float speed = 0.0f; // forward speed
  float roll  = 0.0f; // visual roll (camera-only)
};

std::unordered_map<uint32_t, drive_state> g_drive;

// -----------------------------------------------------------------------------
// Module state
// -----------------------------------------------------------------------------
namespace
{

const engine_api_t* g_api   = nullptr;
void*               g_scene = nullptr;

// All players
std::vector<game_entity_t> g_entities;

// Per-entity velocity (keyed by entity id)
std::unordered_map<uint32_t, glm::vec3> g_velocity;

// Camera
oasis_camera_state g_camera{};
float              g_log_timer = 0.0f;

} // namespace

// -----------------------------------------------------------------------------
// Metadata
// -----------------------------------------------------------------------------
extern "C" game_info_t* game_get_info()
{
  static game_info_t info{.abi_version = OASIS_GAME_ABI_VERSION,
                          .game_id     = "racing-game",
                          .name        = "Racing Game",
                          .version     = "1.0.0",
                          .author      = "Aidan Sanders <aidan.sanders@refugestudios.com.au>",
                          .description = "Minimal racing demo",
                          .homepage    = "https://oasis.refugestudios.com.au"};
  return &info;
}

// -----------------------------------------------------------------------------
// Helpers
// -----------------------------------------------------------------------------
static game_entity_t* find_local_entity()
{
  for (auto& e : g_entities)
    if (e.flags & ENTITY_FLAG_LOCAL)
      return &e;
  return nullptr;
}

static void set_sky_clear_color()
{
  if (!g_api || !g_api->clear_color)
    return;

  glm::vec3 horizon(0.6f, 0.8f, 1.0f);
  glm::vec3 zenith(0.2f, 0.4f, 0.8f);
  glm::vec3 sky = glm::mix(horizon, zenith, 0.5f);

  float col[4] = {sky.r, sky.g, sky.b, 1.0f};
  g_api->clear_color(col);
}

// -----------------------------------------------------------------------------
// Lifecycle
// -----------------------------------------------------------------------------
extern "C" void game_init(const engine_api_t* api)
{
  if (!api || api->abi_version != OASIS_GAME_ABI_VERSION)
    return;

  g_api = api;
  g_api->log("Initializing Racing Demo");

  g_scene = g_api->load_scene("games/racing-demo/assets/track.svdag");
  if (!g_scene)
  {
    g_api->error("Failed to load scene");
    return;
  }

  g_camera             = {};
  g_camera.position[1] = 2.0f;
  g_camera.position[2] = 6.0f;
  g_camera.fov_y       = 1.0472f;
  g_camera.near_plane  = 0.1f;
  g_camera.far_plane   = 1000.0f;
  g_camera.mode        = CAMERA_MODE_FOLLOW;

  g_api->enable_game_camera(true);
}

// -----------------------------------------------------------------------------
// Update
// -----------------------------------------------------------------------------
extern "C" void game_update(float dt)
{
  if (!g_api)
    return;

  set_sky_clear_color();

  game_entity_t* local = find_local_entity();
  if (!local)
  {
    g_api->set_camera_state(&g_camera);
    return;
  }

  auto& car   = *local;
  auto& drive = g_drive[car.id];

  // ---------------------------------------------------------------------------
  // Tunables
  // ---------------------------------------------------------------------------
  constexpr float ENGINE_FORCE = 0.5f;
  constexpr float BRAKE_FORCE  = 1.0f;
  constexpr float MAX_SPEED    = 1.0f;
  constexpr float DRAG         = 2.0f;
  constexpr float STEER_RATE   = 22.0f;
  constexpr float MAX_CAM_ROLL = 0.25f; // ~14 degrees
  constexpr float ROLL_DAMP    = 4.0f;

  // ---------------------------------------------------------------------------
  // Input
  // ---------------------------------------------------------------------------
  float throttle = g_api->is_key_down(KEY_W) ? 1.0f : 0.0f;
  float brake    = g_api->is_key_down(KEY_S) ? 1.0f : 0.0f;

  float steer = 0.0f;
  if (g_api->is_key_down(KEY_D))
    steer -= 1.0f;
  if (g_api->is_key_down(KEY_A))
    steer += 1.0f;

  // ---------------------------------------------------------------------------
  // Speed integration
  // ---------------------------------------------------------------------------
  drive.speed += throttle * ENGINE_FORCE * dt;
  drive.speed -= brake * BRAKE_FORCE * dt;
  drive.speed -= drive.speed * DRAG * dt;

  drive.speed = std::clamp(drive.speed, -MAX_SPEED * 0.4f, MAX_SPEED);

  // ---------------------------------------------------------------------------
  // Steering (speed-scaled)
  // ---------------------------------------------------------------------------
  float speed_factor = std::min(std::abs(drive.speed) / MAX_SPEED, 1.0f);
  car.rotation += steer * STEER_RATE * speed_factor * dt;

  // ---------------------------------------------------------------------------
  // Forward motion
  // ---------------------------------------------------------------------------
  glm::vec3 forward{std::sin(car.rotation), 0.0f, std::cos(car.rotation)};

  car.position[0] += forward.x * drive.speed * dt;
  car.position[2] += forward.z * drive.speed * dt;

  // ---------------------------------------------------------------------------
  // Camera roll (visual feedback)
  // ---------------------------------------------------------------------------
  float target_roll = -steer * speed_factor * MAX_CAM_ROLL;
  drive.roll += (target_roll - drive.roll) * std::min(1.0f, ROLL_DAMP * dt);

  // ---------------------------------------------------------------------------
  // Camera follow
  // ---------------------------------------------------------------------------
  constexpr float FOLLOW_DIST = 0.35f;
  constexpr float FOLLOW_H    = 0.25f;
  constexpr float CAM_DAMP    = 6.0f;

  glm::vec3 car_pos(car.position[0], car.position[1], car.position[2]);
  glm::vec3 desired = car_pos - forward * FOLLOW_DIST + glm::vec3(0, FOLLOW_H, 0);

  for (int i = 0; i < 3; ++i)
    g_camera.position[i] += (desired[i] - g_camera.position[i]) * std::min(1.0f, CAM_DAMP * dt);

  glm::vec3 look = glm::normalize(
      car_pos - glm::vec3(g_camera.position[0], g_camera.position[1], g_camera.position[2]));

  g_camera.rotation[0] = std::asin(look.y);
  g_camera.rotation[1] = std::atan2(look.x, look.z);
  g_camera.rotation[2] = drive.roll;

  g_api->set_camera_state(&g_camera);

  // ---------------------------------------------------------------------------
  // Logging
  // ---------------------------------------------------------------------------
  g_log_timer += dt;
  if (g_log_timer > 0.25f)
  {
    char buf[128];
    std::snprintf(buf, sizeof(buf), "Speed %.2f | yaw %.2f", drive.speed, car.rotation);
    g_api->log(buf);
    g_log_timer = 0.0f;
  }
}

// -----------------------------------------------------------------------------
// Shutdown
// -----------------------------------------------------------------------------
extern "C" void game_shutdown()
{
  if (!g_api)
    return;

  for (auto& e : g_entities)
    if (e.model)
      g_api->remove_model(e.model);

  if (g_scene)
    g_api->remove_scene(g_scene);

  g_entities.clear();
  g_velocity.clear();
  g_scene = nullptr;
  g_api   = nullptr;
}

// -----------------------------------------------------------------------------
// Multiplayer
// -----------------------------------------------------------------------------
extern "C" void game_on_client_join(uint32_t client_id)
{
  if (!g_api)
    return;

  // Ignore local client
  if (client_id == g_local_client_id)
    return;

  if (std::any_of(g_entities.begin(), g_entities.end(), [&](auto& e) { return e.id == client_id; }))
    return;

  game_entity_t e{};
  e.id          = client_id;
  e.position[1] = -0.09f;
  e.scale       = 0.02f;
  e.flags       = ENTITY_FLAG_REMOTE;
  e.model       = g_api->load_model("games/racing-demo/assets/car.svdag");
  e.rotation    = 3.14159265f;
  g_entities.push_back(e);
}

extern "C" void game_on_client_disconnect(uint32_t client_id)
{
  auto it = std::remove_if(g_entities.begin(), g_entities.end(),
                           [&](auto& e) { return e.id == client_id; });

  for (auto i = it; i != g_entities.end(); ++i)
    if (i->model)
      g_api->remove_model(i->model);

  g_entities.erase(it, g_entities.end());
  g_velocity.erase(client_id);
}

extern "C" void game_on_local_client_ready(uint32_t client_id)
{
  if (g_local_client_id != UINT32_MAX)
    return;

  g_local_client_id = client_id;

  game_entity_t e{};
  e.id          = client_id;
  e.position[1] = -0.09f;
  e.scale       = 0.02f;
  e.flags       = ENTITY_FLAG_LOCAL;
  e.model       = g_api->load_model("games/racing-demo/assets/car.svdag");

  g_entities.push_back(e);
}

// -----------------------------------------------------------------------------
// Engine queries
// -----------------------------------------------------------------------------
extern "C" size_t game_get_entity_count()
{
  return g_entities.size();
}

extern "C" game_entity_t* game_get_entities()
{
  return g_entities.empty() ? nullptr : g_entities.data();
}