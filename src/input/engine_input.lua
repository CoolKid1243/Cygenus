-- Engine-owned input: free-look camera and editor toggle.

move_speed = 2.5
sensitivity = 0.1

pos_x = 0.0
pos_y = 0.0
pos_z = 3.0

yaw = -90.0
pitch = 0.0

mouse_locked = true
first_mouse = true
last_mouse_x = 0.0
last_mouse_y = 0.0

-- Written every frame, read back by C to build the view matrix.
front_x = 0.0
front_y = 0.0
front_z = 1.0

-- True only on the frame F1 was pressed - C reads this once per frame.
editor_toggle = false

function update(dt)
    if input.key_pressed("COMMA") then
        mouse_locked = not mouse_locked
        input.set_mouse_locked(mouse_locked)
        first_mouse = true
    end

    editor_toggle = input.key_pressed("F1")

    local mx, my = input.mouse_position()
    if first_mouse then
        last_mouse_x = mx
        last_mouse_y = my
        first_mouse = false
    end

    local dx = mx - last_mouse_x
    local dy = last_mouse_y - my
    last_mouse_x = mx
    last_mouse_y = my

    if mouse_locked then
        yaw = yaw + dx * sensitivity
        pitch = pitch - dy * sensitivity 
    end

    if pitch > 89.0 then pitch = 89.0 end
    if pitch < -89.0 then pitch = -89.0 end

    local ry = math.rad(yaw)
    local rp = math.rad(pitch)
    front_x = math.cos(ry) * math.cos(rp)
    front_y = math.sin(rp)
    front_z = math.sin(ry) * math.cos(rp)

    local len = math.sqrt(front_x*front_x + front_y*front_y + front_z*front_z)
    front_x = front_x / len
    front_y = front_y / len
    front_z = front_z / len

    local right_x = -front_z
    local right_z = front_x

    if input.key_held("W") then
        pos_x = pos_x + front_x * move_speed * dt
        pos_y = pos_y + front_y * move_speed * dt
        pos_z = pos_z + front_z * move_speed * dt
    end
    if input.key_held("S") then
        pos_x = pos_x - front_x * move_speed * dt
        pos_y = pos_y - front_y * move_speed * dt
        pos_z = pos_z - front_z * move_speed * dt
    end
    if input.key_held("A") then
        pos_x = pos_x - right_x * move_speed * dt
        pos_z = pos_z - right_z * move_speed * dt
    end
    if input.key_held("D") then
        pos_x = pos_x + right_x * move_speed * dt
        pos_z = pos_z + right_z * move_speed * dt
    end
    if input.key_held("E") then
            pos_y = -pos_y + move_speed * dt
    end
    if input.key_held("Q") then
        pos_y = -pos_y - move_speed * dt
    end
end