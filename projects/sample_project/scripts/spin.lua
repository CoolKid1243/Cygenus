function start()
    cube = engine.find_entity("Cube")
    camera = engine.find_entity("Camera")

    -- Spawned entities get unique names like Unity: Sphere, Sphere (1)...
    local s = engine.create_sphere()
    engine.set_position(s, 2, 0, 0)
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

    if camera then
        local rx, ry, rz = engine.get_rotation(camera)
        engine.set_rotation(camera, rx, ry + 45 * dt, rz)
    end

    if input.key_pressed("SPACE") then
        print("space pressed!")
    end
end
