# Cygenus — Lua Scripting API Reference

This covers everything currently callable from Lua scripts in Cygenus, as of the
current codebase. Two tables are available: `engine` (entities/transforms) and
`input` (keyboard/mouse), plus two lifecycle functions the engine calls for you.

Scripts live anywhere inside your project folder as `.lua` files and only run
in **Play mode** — the engine scans the whole project directory recursively
and loads every `.lua` file it finds when you hit Play.

---

## Lifecycle functions

Define these as top-level functions in your script; the engine calls them
automatically. Both are optional.

```lua
function start()
    -- Called once, right when Play mode begins.
end

function update(dt)
    -- Called every frame while in Play mode.
    -- dt = delta time in seconds since the last frame.
end
```

---

## `engine` table — entities & transforms

Every function that takes an `id` expects an entity id returned by one of the
`create_*`/`find_entity` calls below. Passing a destroyed/invalid id raises a
Lua error.

### Creating & destroying entities

```lua
engine.create_entity(name)        -- name: string, optional (default "Entity")
                                   -- returns: id
                                   -- Bare entity with only a Transform (position 0,0,0 / scale 1,1,1).

engine.create_cube(name)          -- name: string, optional
engine.create_sphere(name)        -- name: string, optional
engine.create_plane(name)         -- name: string, optional
                                   -- Each returns: id
                                   -- Spawns a primitive mesh entity (cube/sphere/plane).

engine.destroy_entity(id)         -- Destroys the entity. No return value.
```

### Finding & naming entities

```lua
engine.find_entity(name)          -- returns: id, or nil if no entity has that name
engine.get_name(id)               -- returns: name (string)
engine.set_name(id, name)         -- Renames the entity. Gets a "(1)" suffix
                                   -- automatically if the name is already taken.
```

### Transform — position

```lua
engine.get_position(id)           -- returns: x, y, z
engine.set_position(id, x, y, z)  -- Sets world position. Marks transform dirty.
```

### Transform — rotation

```lua
engine.get_rotation(id)           -- returns: x, y, z  (Euler angles, degrees)
engine.set_rotation(id, x, y, z)  -- Sets rotation in degrees. Marks transform dirty.
```

### Transform — scale

```lua
engine.get_scale(id)              -- returns: x, y, z
engine.set_scale(id, x, y, z)     -- Sets scale. Marks transform dirty.
```

### Appearance

```lua
engine.set_tint(id, r, g, b)      -- r, g, b: 0.0-1.0 floats.
                                   -- Adds a Material component to the entity
                                   -- if it doesn't already have one.
```

### Hierarchy

```lua
engine.set_parent(child_id, parent_id)  -- Parents child_id under parent_id.
engine.set_parent(child_id, nil)        -- Un-parents child_id (moves it to world root).
```

### Misc

```lua
engine.entity_count()             -- returns: total number of alive entities in the scene
```

---

## `input` table — keyboard & mouse

```lua
input.key_held(name)              -- returns: true while the key is held down
input.key_pressed(name)           -- returns: true only on the frame the key was pressed
input.key_released(name)          -- returns: true only on the frame the key was released
```

**Supported key names:** any character

```lua
input.mouse_position()            -- returns: x, y  (screen-space pixel coordinates)

input.mouse_button_pressed(name)  -- returns: true only on the frame the button was pressed
input.mouse_button_released(name) -- returns: true only on the frame the button was released
```

**Supported mouse button names:** `"LEFT"`, `"RIGHT"`, `"MIDDLE"`.

```lua
input.set_mouse_locked(locked)    -- locked: true/false. Locks/hides the cursor
                                   -- for FPS-style mouse-look.
```

---

## Example script

```lua
-- Spins a cube and lets the player nudge it with arrow-ish WASD keys,
-- reporting its position once a second.

local cube_id
local timer = 0

function start()
    cube_id = engine.create_cube("SpinningCube")
    engine.set_position(cube_id, 0, 1, 0)
    engine.set_tint(cube_id, 1.0, 0.4, 0.2)
end

function update(dt)
    local x, y, z = engine.get_rotation(cube_id)
    engine.set_rotation(cube_id, x, y + 90 * dt, z)

    if input.key_held("D") then
        local px, py, pz = engine.get_position(cube_id)
        engine.set_position(cube_id, px + 2 * dt, py, pz)
    end

    timer = timer + dt
    if timer >= 1.0 then
        local px, py, pz = engine.get_position(cube_id)
        print(string.format("Cube at %.2f, %.2f, %.2f", px, py, pz))
        timer = 0
    end
end
```

---

## Not yet exposed to gameplay scripts

These exist in the engine's C/C++ code but aren't callable from your project's
`.lua` scripts yet — they're either editor-internal or only used by the
engine's own built-in camera-control script:

- **Camera component fields** (fov, near/far plane) — no `engine.get_camera_*`/
  `set_camera_*` bindings yet.
- **Physics** — not implemented yet.
- **Raycasting / picking** — not implemented yet.
- **Free-fly camera control** (`pos_x/y/z`, `yaw`, `pitch`, `front_x/y/z`) —
  this lives in the engine's own internal `engine_input.lua` script (a
  separate, engine-owned Lua state), not in the `engine`/`input` tables your
  project scripts see.

If you add new bindings, they'll show up as new entries in
`src/scripting/lua_engine.c` (`engine` table) or
`src/input/lua_input.c` (`input` table) — update this file to match.
