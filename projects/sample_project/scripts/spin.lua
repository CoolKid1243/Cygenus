function start()
    cube = engine.find_entity("Cube")
    camera = engine.find_entity("Camera")

    local s = engine.create_sphere()
    engine.set_position(s, 0, 0, 2)
    engine.set_tint(s, 1, 0.4, 0.2)

    local p = engine.create_plane()
    engine.set_position(p, 0, -1, 0)
    engine.set_tint(p, 0.4, 0.6, 0.4)
end

function update(dt)
    if cube then
        local rx, ry, rz = engine.get_rotation(cube)
        engine.set_rotation(cube, rx, ry + 45 * dt, rz)
    end

    -- Build forward/right vectors from the camera's current yaw (rotation.y),
    -- so WASD moves relative to where the camera is facing, not world axes.
    local crx, cry, crz = engine.get_rotation(camera)
    local yaw_rad = math.rad(cry)

    local front_x = math.cos(yaw_rad)
    local front_z = math.sin(yaw_rad)

    local right_x = -front_z
    local right_z = front_x

    local move_speed = 2 * dt

    if input.key_held("W") then
        local x, y, z = engine.get_position(camera)
        engine.set_position(camera, x + front_x * move_speed, y, z + front_z * move_speed)
    end

    if input.key_held("S") then
        local x, y, z = engine.get_position(camera)
        engine.set_position(camera, x - front_x * move_speed, y, z - front_z * move_speed)
    end

    if input.key_held("A") then
        local x, y, z = engine.get_position(camera)
        engine.set_position(camera, x - right_x * move_speed, y, z - right_z * move_speed)
    end

    if input.key_held("D") then
        local x, y, z = engine.get_position(camera)
        engine.set_position(camera, x + right_x * move_speed, y, z + right_z * move_speed)
    end

    if input.key_held("Q") then
        local x, y, z = engine.get_position(camera)
        engine.set_position(camera, x, y - 2 * dt, z)
    end

    if input.key_held("E") then
        local x, y, z = engine.get_position(camera)
        engine.set_position(camera, x, y + 2 * dt, z)
    end

    if input.key_held("LEFT") then
        local rx, ry, rz = engine.get_rotation(camera)
        engine.set_rotation(camera, rx, ry - 60 * dt, rz)
    end

    if input.key_held("RIGHT") then
        local rx, ry, rz = engine.get_rotation(camera)
        engine.set_rotation(camera, rx, ry + 60 * dt, rz)
    end

    if input.key_held("SPACE") then
        print("space pressed!")
    end
end
