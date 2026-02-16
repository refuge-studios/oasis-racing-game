#include "game_api.hpp"
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <glm/glm.hpp>
#include <vector>

// Add Shooting.
// Add Point Queue
// Add Bosses Template
// Roundstate.
// Health Kits.
// Ammo Packs
// Live / Dead State
// Use UDAG, Fixup TSDAG
//
// Map Design (Rotation of Maps)
// Animations.

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
// Per-entity state
// ============================================================================
struct player_state
{
  uint64_t  entity_id = 0;
  glm::vec3 velocity{0.0f};
  float     pitch = 0.0f;
  float     yaw   = 0.0f;
};


struct projectile
{
  size_t    entity_index; // index into g->entities
  glm::vec3 velocity;
};

// ============================================================================
// GAME STATE (ENGINE-OWNED)
// ============================================================================
struct game_state
{
  const engine_api_t* api = nullptr;

  void* scene = nullptr;

  std::vector<game_entity_t> entities;    // all entities
  std::vector<player_state>  players;     // player states
  std::vector<projectile>    projectiles; // active projectiles

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

static player_state* find_player(uint64_t id)
{
  for (auto& p : g->players)
    if (p.entity_id == id)
      return &p;

  g->players.push_back({});
  player_state& p = g->players.back();
  p.entity_id     = id;
  p.velocity      = glm::vec3(0.0f);
  p.pitch         = 0.0f;
  p.yaw           = 0.0f;
  return &p;
}

static void set_clear_color()
{
  glm::vec3 sky(0.5f, 0.7f, 1.0f);
  float     c[4] = {sky.x, sky.y, sky.z, 1.0f};
  g->api->clear_color(c);
}

static game_entity_t* find_entity(uint64_t id)
{
    for (auto& e : g->entities)
        if (e.id == id)
            return &e;
    return nullptr;
}


static void add_entity(const game_entity_t& e)
{
  g->entities.push_back(e);
}

static void remove_entity(uint64_t id)
{
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

  for (auto it = g->players.begin(); it != g->players.end(); ++it)
  {
    if (it->entity_id == id)
    {
      g->players.erase(it);
      break;
    }
  }
}



static bool raycast(const float origin[3], const float dir[3], float max_dist,
                    oasis_raycast_hit& hit)
{
  hit.hit = false;
  return g->api->raycast_scene(g->scene, origin, dir, max_dist, &hit) && hit.hit;
}



static bool ray_sphere_hit(
    const glm::vec3& ro,
    const glm::vec3& rd,
    const glm::vec3& center,
    float radius,
    float& out_t)
{
    glm::vec3 oc = ro - center;
    float b = glm::dot(oc, rd);
    float c = glm::dot(oc, oc) - radius * radius;
    float h = b * b - c;
    if (h < 0.0f) return false;

    h = std::sqrt(h);
    float t = -b - h;
    if (t < 0.0f) t = -b + h;
    if (t < 0.0f) return false;

    out_t = t;
    return true;
}


// ============================================================================
// Players
// ============================================================================

// ============================================================================
class base_player
{
public:
  static constexpr float LOOK_SENS         = 0.002f;
  static constexpr float MOVE_SPEED        = 0.15f;
  static constexpr float JUMP_SPEED        = 0.25f; // 1.0 = hale
  static constexpr float PLAYER_RADIUS     = 0.005f;
  static constexpr float PLAYER_HEIGHT     = 0.125f;
  static constexpr float SKIN_WIDTH        = 0.01f;
  static constexpr float GRAVITY           = -1.0f; //-9.8f;
  static constexpr float TERMINAL_VELOCITY = -50.0f;

  uint64_t id = 0;
  glm::vec3 position{0.0f};
  glm::vec3 velocity{0.0f};
  float pitch = 0.0f;
  float yaw   = 0.0f;

  float move_speed = 0.15f;
  float jump_speed = 0.25f;
  float height     = 0.125f;
  float radius     = 0.005f;

  bool alive = true;
  bool on_ground = false;

  game_entity_t entity{}; // engine handle

  base_player() = default;

  virtual ~base_player()
  {
    if (entity.model && g && g->api)
      g->api->remove_model(entity.model);
  }

  void spawn(const glm::vec3& pos, const char* model_path)
{
    id = g->local_client_id;
    position = pos;

    game_entity_t e{};
    e.id = id;

    e.position[0] = pos.x;
    e.position[1] = pos.y;
    e.position[2] = pos.z;

    e.rotation[0] = 0.0f;
    e.rotation[1] = yaw;   // face forward
    e.rotation[2] = 0.0f;

    e.scale = 0.035f;
    e.flags = ENTITY_FLAG_LOCAL;
    e.model = g->api->load_model(model_path);

    add_entity(e);
}



  virtual void update(float dt)
  {
    if (!alive || !g || !g->api)
        return;

    glm::vec3 wish_dir(0.0f);
    handle_input(wish_dir);

    // Gravity
    velocity.y += GRAVITY * dt;
    velocity.y = glm::max(velocity.y, TERMINAL_VELOCITY);

    // Horizontal movement
    glm::vec3 horizontal(wish_dir.x, 0.0f, wish_dir.z);
    if (glm::length(horizontal) > 0.0f)
        horizontal = glm::normalize(horizontal);

    move(horizontal, dt);

    // ------------------------------------------------
    // Sync ENGINE ENTITY ← PLAYER (authoritative)
    // ------------------------------------------------
    // ------------------------------------------------
// Sync ENGINE ENTITY ← PLAYER (authoritative)
// ------------------------------------------------
if (game_entity_t* e = find_entity(id))
{
    e->position[0] = position.x;
    e->position[1] = position.y + 0.015f;
    e->position[2] = position.z;

    // 🔥 FACE WHERE THE PLAYER IS LOOKING
    e->rotation[0] = 0.0f; // no pitch for FPS body
    e->rotation[1] = yaw;  // yaw = facing direction
    e->rotation[2] = 0.0f;
}


    // ------------------------------------------------
    // Camera follows player
    // ------------------------------------------------
    glm::vec3 cam_pos = position + glm::vec3(0.0f, height - 0.1f, 0.0f);

    g->camera.position[0] = cam_pos.x;
    g->camera.position[1] = cam_pos.y;
    g->camera.position[2] = cam_pos.z;
    g->camera.rotation[0] = pitch;
    g->camera.rotation[1] = yaw;

    g->api->set_camera_state(&g->camera);
  }


protected:
    void handle_input(glm::vec3& out_dir)
{
    // ------------------------------------------------
    // Mouse look
    // ------------------------------------------------
    float dx = 0.0f, dy = 0.0f;
    g->api->get_mouse_delta(&dx, &dy);

    yaw   -= dx * LOOK_SENS;
    pitch -= dy * LOOK_SENS;
    pitch  = glm::clamp(pitch, -1.57f, 1.57f);

    // ------------------------------------------------
    // Movement vectors (yaw-only)
    // ------------------------------------------------
    const glm::vec3 forward(
        std::sin(yaw),
        0.0f,
        std::cos(yaw)
    );

    const glm::vec3 right =
        glm::normalize(glm::cross(forward, glm::vec3(0.0f, 1.0f, 0.0f)));

    if (g->api->is_key_down(KEY_W)) out_dir += forward;
    if (g->api->is_key_down(KEY_S)) out_dir -= forward;
    if (g->api->is_key_down(KEY_D)) out_dir += right;
    if (g->api->is_key_down(KEY_A)) out_dir -= right;

    // ------------------------------------------------
    // Jump
    // ------------------------------------------------
    on_ground = check_ground();
    if (on_ground && g->api->is_key_down(KEY_SPACE))
        velocity.y = jump_speed;

    // ------------------------------------------------
    // Shooting (hitscan, cursor-based)
    // ------------------------------------------------
    static bool was_mouse_down = false;
    bool mouse_down = g->api->is_mouse_down(MOUSE_LEFT); // LMB

    if (mouse_down && !was_mouse_down)
    {
        constexpr float SHOOT_RANGE = 100.0f;
        constexpr float HIT_RADIUS  = 0.03f;

        // Camera ray
        const glm::vec3 ray_origin =
            position + glm::vec3(0.0f, height - 0.1f, 0.0f);

        const glm::vec3 ray_dir = glm::normalize(glm::vec3(
            std::sin(yaw) * std::cos(pitch),
           -std::sin(pitch),
            std::cos(yaw) * std::cos(pitch)
        ));

        uint64_t hit_id = UINT64_MAX;
        float    hit_t  = SHOOT_RANGE;

        for (auto& e : g->entities)
        {
            if (!(e.flags & ENTITY_FLAG_REMOTE))
                continue;

            const glm::vec3 center(
                e.position[0],
                e.position[1] + PLAYER_HEIGHT * 0.5f,
                e.position[2]
            );

            float t;
            if (ray_sphere_hit(ray_origin, ray_dir, center, HIT_RADIUS, t))
            {
                if (t < hit_t)
                {
                    hit_t  = t;
                    hit_id = e.id;
                }
            }
        }

        if (hit_id != UINT64_MAX)
        {
            remove_entity(hit_id);
            g->api->log("Hit confirmed");
        }
    }

    was_mouse_down = mouse_down;
}


    bool check_ground()
    {
        oasis_raycast_hit hit;
        float origin[3] = {position.x, position.y, position.z};
        float dir[3] = {0.0f,-1.0f,0.0f};
        return raycast(origin, dir, radius*2.0f, hit) && hit.hit;
    }

    void move(const glm::vec3& dir, float dt)
    {


        glm::vec3 horizontal_move = dir * move_speed * dt;
        glm::vec3 movement(horizontal_move.x, velocity.y*dt, horizontal_move.z);

        glm::vec3 new_pos = position;
        glm::vec3 vel = movement;
        oasis_raycast_hit hit;



         constexpr float STEP_HEIGHT    = 0.005f; // Max step height
  constexpr int   COLLISION_ITER = 4;      // Substeps per frame
  constexpr float EPSILON        = 1e-6f;

  // ------------------------
  // Initialize
  // ------------------------


  // ------------------------
  // Movement iteration
  // ------------------------
  for (int iter = 0; iter < COLLISION_ITER; ++iter)
  {
    glm::vec3 move     = vel / float(COLLISION_ITER);
    float     move_len = glm::length(move);
    if (move_len < EPSILON)
      break;

    // glm::vec3 move_dir = move / move_len;

    // ------------------------
    // Vertical sweep first
    // ------------------------
    glm::vec3 vert_origin = new_pos;
    glm::vec3 vert_dir    = glm::vec3(0.0f, move.y > 0.0f ? 1.0f : -1.0f, 0.0f);
    if (raycast((float*)&vert_origin, (float*)&vert_dir, fabsf(move.y) + PLAYER_RADIUS, hit) &&
        hit.hit)
    {
      move.y = 0.0f;
      vel.y  = 0.0f;
    }

    // ------------------------
    // Horizontal sweep with sliding
    // ------------------------
    glm::vec3 horiz_move(move.x, 0.0f, move.z);
    float     horiz_len = glm::length(horiz_move);
    if (horiz_len > EPSILON)
    {
      glm::vec3 horiz_dir = horiz_move / horiz_len;

      glm::vec3 capsule_bottom = new_pos;
      glm::vec3 capsule_top    = new_pos + glm::vec3(0, PLAYER_HEIGHT, 0);

      bool hit_bottom =
          raycast((float*)&capsule_bottom, (float*)&horiz_dir, horiz_len + PLAYER_RADIUS, hit);
      bool hit_top =
          raycast((float*)&capsule_top, (float*)&horiz_dir, horiz_len + PLAYER_RADIUS, hit);

      if (hit_bottom || hit_top)
      {
        glm::vec3 normal(hit.normal[0], hit.normal[1], hit.normal[2]);

        // Step-up attempt
        if (normal.y < 0.7f)
        {
          glm::vec3 step_pos = new_pos + glm::vec3(0, STEP_HEIGHT, 0);
          bool      step_hit =
              raycast((float*)&step_pos, (float*)&horiz_dir, horiz_len + PLAYER_RADIUS, hit);
          if (!step_hit)
          {
            // Step up horizontally
            new_pos = step_pos + glm::vec3(horiz_move.x, 0.0f, horiz_move.z);
            continue;
          }
        }

        // Slide along wall
        float     dot   = glm::dot(horiz_move, normal);
        glm::vec3 slide = horiz_move - normal * dot;
        vel.x           = slide.x;
        vel.z           = slide.z;

        // Stop if slide is tiny
        if (glm::length(slide) < EPSILON)
          break;
      }
      else
      {
        // Free horizontal move
        new_pos += horiz_move;
      }
    }

    // ------------------------
    // Apply vertical move last
    // ------------------------
    new_pos.y += move.y;
  }

      
        velocity.y = vel.y / dt;
        position = new_pos;
    }
};

// ============================================================================
class base_boss : public base_player
{
public:
    base_boss() = default;

    virtual void update(float dt) override
    {
        if (!alive) return;

        // Apply boss multipliers
        move_speed = 0.15f;
        jump_speed = 1.0f;
        base_player::update(dt); // human input is reused
    }
};



// Bosses
static base_boss* g_boss = nullptr;

// ============================================================================
// Lifecycle
// ============================================================================
extern "C" void game_init(const engine_api_t* api)
{
    if (!api || api->abi_version != OASIS_GAME_ABI_VERSION)
        return;

    if (!g)
        g = new game_state{};

    g->api = api;
    g->entities.clear();
    g->players.clear();
    g->local_client_id = UINT32_MAX;

    api->log("Racing game initialized");

    g->scene = api->load_scene("games/racing-game/assets/track.svdag");

    g->camera.position[1] = 1.5f;
    g->camera.position[2] = 1.0f;
    g->camera.fov_y       = 1.0472f;
    g->camera.near_plane  = 0.1f;
    g->camera.far_plane   = 1000.0f;
    g->camera.mode        = CAMERA_MODE_FREE;

    api->enable_game_camera(true);
}

extern "C" void game_shutdown()
{
    if (!g || !g->api)
        return;

    if (g_boss)
    {
        delete g_boss;
        g_boss = nullptr;
    }

    for (auto& e : g->entities)
        if (e.model)
            g->api->remove_model(e.model);

    if (g->scene)
        g->api->remove_scene(g->scene);

    g->entities.clear();
    g->players.clear();
    g->scene = nullptr;
}








extern "C" void game_update(float dt)
{
  if (!g || !g->api)
    return;
  set_clear_color();

  // ---------------------------------------------------------------------
  // Fetch player and state
  // ---------------------------------------------------------------------
  game_entity_t* player = find_local_entity();
  if (!player)
  {
    g->api->set_camera_state(&g->camera);
    return;
  }

  player_state* state = find_player(player->id);
  if (!state)
    return;

  g_boss->update(dt);
/*
    // ---------------------------------------------------------------------
  // Camera follow
  // ---------------------------------------------------------------------
  g->camera.position[0] = player->position[0];
  g->camera.position[1] = player->position[1];
  g->camera.position[2] = player->position[2];
  g->camera.rotation[0] = state->pitch;
  g->camera.rotation[1] = state->yaw;
  g->api->set_camera_state(&g->camera);*/
  return;

  // ---------------------------------------------------------------------
  // Constants
  // ---------------------------------------------------------------------
  constexpr float LOOK_SENS         = 0.002f;
  constexpr float MOVE_SPEED        = 0.15f;
  constexpr float JUMP_SPEED        = 0.25f; // 1.0 = hale
  constexpr float PLAYER_RADIUS     = 0.005f;
  constexpr float PLAYER_HEIGHT     = 0.125f;
  constexpr float SKIN_WIDTH        = 0.01f;
  constexpr float GRAVITY           = -1.0f; //-9.8f;
  constexpr float TERMINAL_VELOCITY = -50.0f;
  // constexpr int MAX_SUBSTEPS       = 5; // subdivide movement to avoid tunneling

  // ---------------------------------------------------------------------
  // Mouse look
  // ---------------------------------------------------------------------
  float dx = 0.0f, dy = 0.0f;
  g->api->get_mouse_delta(&dx, &dy);
  state->yaw -= dx * LOOK_SENS;
  state->pitch -= dy * LOOK_SENS;
  state->pitch = glm::clamp(state->pitch, -1.57f, 1.57f);

  glm::vec3 forward(std::sin(state->yaw), 0.0f, std::cos(state->yaw));
  glm::vec3 right = glm::normalize(glm::cross(forward, glm::vec3(0, 1, 0)));

  // ---------------------------------------------------------------------
  // Input
  // ---------------------------------------------------------------------
  glm::vec3 wish_dir(0.0f);
  if (g->api->is_key_down((int)KEY_W))
    wish_dir += forward;
  if (g->api->is_key_down((int)KEY_S))
    wish_dir -= forward;
  if (g->api->is_key_down((int)KEY_D))
    wish_dir += right;
  if (g->api->is_key_down((int)KEY_A))
    wish_dir -= right;

  // Normalize horizontal movement
  glm::vec3 horizontal_dir(wish_dir.x, 0.0f, wish_dir.z);
  if (glm::length(horizontal_dir) > 0.0f)
    horizontal_dir = glm::normalize(horizontal_dir);

  // ---------------------------------------------------------------------
  // Jumping
  // ---------------------------------------------------------------------
  bool on_ground = false;
  {
    oasis_raycast_hit hit;
    float             origin[3] = {player->position[0], player->position[1], player->position[2]};
    float             dir[3]    = {0.0f, -1.0f, 0.0f};
    if (raycast(origin, dir, SKIN_WIDTH * 2.0f, hit) && hit.hit)
      on_ground = true;
  }
  if (g->api->is_key_down((int)KEY_SPACE) && on_ground)
  {
    state->velocity.y = JUMP_SPEED;
  }

  // ---------------------------------------------------------------------
  // Apply gravity
  // ---------------------------------------------------------------------
  state->velocity.y += GRAVITY * dt;
  if (state->velocity.y < TERMINAL_VELOCITY)
    state->velocity.y = TERMINAL_VELOCITY;

  // Full movement this frame
  glm::vec3 horizontal_move = horizontal_dir * MOVE_SPEED * dt;
  glm::vec3 movement(horizontal_move.x, state->velocity.y * dt, horizontal_move.z);

  // ------------------------
  // Constants
  // ------------------------
  constexpr float STEP_HEIGHT    = 0.005f; // Max step height
  constexpr int   COLLISION_ITER = 4;      // Substeps per frame
  constexpr float EPSILON        = 1e-6f;

  // ------------------------
  // Initialize
  // ------------------------
  glm::vec3 new_pos(player->position[0], player->position[1], player->position[2]);
  glm::vec3 vel(horizontal_move.x, state->velocity.y * dt, horizontal_move.z);

  oasis_raycast_hit hit;

  // ------------------------
  // Movement iteration
  // ------------------------
  for (int iter = 0; iter < COLLISION_ITER; ++iter)
  {
    glm::vec3 move     = vel / float(COLLISION_ITER);
    float     move_len = glm::length(move);
    if (move_len < EPSILON)
      break;

    // glm::vec3 move_dir = move / move_len;

    // ------------------------
    // Vertical sweep first
    // ------------------------
    glm::vec3 vert_origin = new_pos;
    glm::vec3 vert_dir    = glm::vec3(0.0f, move.y > 0.0f ? 1.0f : -1.0f, 0.0f);
    if (raycast((float*)&vert_origin, (float*)&vert_dir, fabsf(move.y) + PLAYER_RADIUS, hit) &&
        hit.hit)
    {
      move.y = 0.0f;
      vel.y  = 0.0f;
    }

    // ------------------------
    // Horizontal sweep with sliding
    // ------------------------
    glm::vec3 horiz_move(move.x, 0.0f, move.z);
    float     horiz_len = glm::length(horiz_move);
    if (horiz_len > EPSILON)
    {
      glm::vec3 horiz_dir = horiz_move / horiz_len;

      glm::vec3 capsule_bottom = new_pos;
      glm::vec3 capsule_top    = new_pos + glm::vec3(0, PLAYER_HEIGHT, 0);

      bool hit_bottom =
          raycast((float*)&capsule_bottom, (float*)&horiz_dir, horiz_len + PLAYER_RADIUS, hit);
      bool hit_top =
          raycast((float*)&capsule_top, (float*)&horiz_dir, horiz_len + PLAYER_RADIUS, hit);

      if (hit_bottom || hit_top)
      {
        glm::vec3 normal(hit.normal[0], hit.normal[1], hit.normal[2]);

        // Step-up attempt
        if (normal.y < 0.7f)
        {
          glm::vec3 step_pos = new_pos + glm::vec3(0, STEP_HEIGHT, 0);
          bool      step_hit =
              raycast((float*)&step_pos, (float*)&horiz_dir, horiz_len + PLAYER_RADIUS, hit);
          if (!step_hit)
          {
            // Step up horizontally
            new_pos = step_pos + glm::vec3(horiz_move.x, 0.0f, horiz_move.z);
            continue;
          }
        }

        // Slide along wall
        float     dot   = glm::dot(horiz_move, normal);
        glm::vec3 slide = horiz_move - normal * dot;
        vel.x           = slide.x;
        vel.z           = slide.z;

        // Stop if slide is tiny
        if (glm::length(slide) < EPSILON)
          break;
      }
      else
      {
        // Free horizontal move
        new_pos += horiz_move;
      }
    }

    // ------------------------
    // Apply vertical move last
    // ------------------------
    new_pos.y += move.y;
  }

  // ------------------------
  // Update vertical velocity
  // ------------------------
  state->velocity.y = vel.y / dt;

  // ------------------------
  // Apply final position
  // ------------------------
  player->position[0] = new_pos.x;
  player->position[1] = new_pos.y;
  player->position[2] = new_pos.z;

  // ------------------------
  // Check on_ground
  // ------------------------
  {
    oasis_raycast_hit ground_hit;
    float             origin[3] = {new_pos.x, new_pos.y, new_pos.z};
    float             dir[3]    = {0.0f, -1.0f, 0.0f};
    on_ground = raycast(origin, dir, SKIN_WIDTH * 2.0f, ground_hit) && ground_hit.hit;
    if (on_ground && state->velocity.y < 0.0f)
      state->velocity.y = 0.0f;
  }

  // ---------------------------------------------------------------------
  // Camera follow
  // ---------------------------------------------------------------------
  g->camera.position[0] = player->position[0];
  g->camera.position[1] = player->position[1] + PLAYER_HEIGHT - 0.1f;
  g->camera.position[2] = player->position[2];
  g->camera.rotation[0] = state->pitch;
  g->camera.rotation[1] = state->yaw;
  g->api->set_camera_state(&g->camera);
}

// ============================================================================
// Multiplayer
// ============================================================================
extern "C" void game_on_local_client_ready(uint32_t id)
{
  g->local_client_id = id;

  /*
  game_entity_t e{};
  e.id = id;

  // Try to spawn slightly above the track
  constexpr float   SPAWN_HEIGHT = 0.1f; // above ground
  oasis_raycast_hit hit;
  float             origin[3] = {1.5f, 5.0f, 1.5f}; // top of map
  float             down[3]   = {0.0f, -1.0f, 0.0f};

  if (raycast(origin, down, 10.0f, hit))
    e.position[1] = hit.position[1] + SPAWN_HEIGHT; // ground + offset
  else
    e.position[1] = 1.5f; // fallback if raycast fails

  // Random-ish spawn X/Z within bounds to avoid crowding center
  e.position[0] = 1.2f + (float(rand() % 80) / 100.0f); // [1.2, 2.0]
  e.position[2] = 1.2f + (float(rand() % 80) / 100.0f); // [1.2, 2.0]

  e.scale = 0.025f;
  e.flags = ENTITY_FLAG_LOCAL;
  e.model = g->api->load_model("games/racing-game/assets/car.svdag");
*/

      

   // ----------------------------
    // Spawn the global boss
    // ----------------------------
    if (!g_boss)
    {
        g_boss = new base_boss();
        g_boss->spawn(glm::vec3(1.5f, 1.5f, 1.5f), "games/racing-game/assets/car.svdag");

        // Assign the local player as the target if they exist
        //game_entity_t* player = find_local_entity();
        //if (player)
        //    g_boss->assign_player(player->id);
    }

  //add_entity(e);
}

extern "C" void game_on_client_join(uint32_t id)
{
  if (id == g->local_client_id)
    return;

  game_entity_t e{};
  e.id          = id;
  e.position[1] = 1.5f;
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
