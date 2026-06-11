local isReady = false
local isOverlayOpen = false

local cfg = {
    enabled = false,
    scaleL = 1.06, scaleR = 1.05,
    xOffL = 0.0, yOffL = 0.0, zOffL = 0.23,
    xOffR = 0.0, yOffR = 0.0, zOffR = 0.23,
    swingL = 1.0, swingR = 1.0,
    poleL = 0.0, poleR = 0.0,
    pitchL = -180.0, yawL = -90.0, rollL = 0.0,
    pitchR = 0.0, yawR = -90.0, rollR = 0.0,
}

local function LoadConfig()
    local file = io.open("config.json", "r")
    if file then
        local content = file:read("*a")
        if content and content ~= "" then
            local parsed = json.decode(content)
            if parsed then
                for k, v in pairs(parsed) do
                    cfg[k] = v
                end
            end
        end
        file:close()
    end
end

local function SaveConfig()
    local file = io.open("config.json", "w")
    if file then
        file:write(json.encode(cfg))
        file:close()
    end
end

local function applyCalibration()
    if not isReady then return end
    pcall(function() Game.SetVRBindParams(cfg.scaleL, cfg.xOffL, cfg.yOffL, cfg.zOffL, 0, 1) end) -- Left
    pcall(function() Game.SetVRBindParams(cfg.scaleR, cfg.xOffR, cfg.yOffR, cfg.zOffR, 0, 0) end) -- Right

    pcall(function() Game.SetVRElbowSwing(cfg.swingL, 1) end)
    pcall(function() Game.SetVRElbowSwing(cfg.swingR, 0) end)

    pcall(function() Game.SetVRElbowPole(cfg.poleL, 1) end)
    pcall(function() Game.SetVRElbowPole(cfg.poleR, 0) end)

    pcall(function() Game.SetVRHandOffset(cfg.pitchL, cfg.yawL, cfg.rollL, 1) end)
    pcall(function() Game.SetVRHandOffset(cfg.pitchR, cfg.yawR, cfg.rollR, 0) end)

    if cfg.enabled then
        pcall(function() Game.InstallVRAnimPoseHook() end)
        pcall(function() Game.ArmVRAnimPosePlayer() end)
        pcall(function() Game.SetVRBindMode(4) end)
    else
        pcall(function() Game.SetVRBindMode(0) end)
    end
end

registerForEvent("onOverlayOpen", function()
    isOverlayOpen = true
end)

registerForEvent("onOverlayClose", function()
    isOverlayOpen = false
end)

registerForEvent("onDraw", function()
    if not isOverlayOpen then return end

    if ImGui.Begin("Cyberpunk Universal Hands") then
        local changed = false

        -- Status indicator
        local linked = false
        pcall(function() linked = Game.IsVRHandLinked() end)
        if linked then
            ImGui.TextColored(0.0, 1.0, 0.0, 1.0, "Shared Memory: CONNECTED")
        else
            ImGui.TextColored(1.0, 0.3, 0.3, 1.0, "Shared Memory: NOT CONNECTED")
        end

        local cEnabled, toggled = ImGui.Checkbox("Enable Universal Hands", cfg.enabled)
        if toggled then
            cfg.enabled = cEnabled
            changed = true
        end

        ImGui.Separator()
        ImGui.Text("Left Hand Calibration")
        local l_scale, c1 = ImGui.SliderFloat("Reach Scale L", cfg.scaleL, 0.5, 2.0)
        local l_xoff, c_x1 = ImGui.SliderFloat("Left/Right Offset L", cfg.xOffL, -1.0, 1.0)
        local l_yoff, c_y1 = ImGui.SliderFloat("Forward Offset L", cfg.yOffL, -1.0, 1.0)
        local l_zoff, c2 = ImGui.SliderFloat("Height Offset L", cfg.zOffL, -1.0, 1.0)
        local l_swing, c3 = ImGui.SliderFloat("Elbow Swing L", cfg.swingL, 0.0, 1.0)
        local l_pole, c4 = ImGui.SliderFloat("Elbow Pole L", cfg.poleL, -180.0, 180.0)
        local l_p, c5 = ImGui.SliderFloat("Wrist Pitch L", cfg.pitchL, -180.0, 180.0)
        local l_y, c6 = ImGui.SliderFloat("Wrist Yaw L", cfg.yawL, -180.0, 180.0)
        local l_r, c7 = ImGui.SliderFloat("Wrist Roll L", cfg.rollL, -180.0, 180.0)
        if c1 or c_x1 or c_y1 or c2 or c3 or c4 or c5 or c6 or c7 then
            cfg.scaleL, cfg.xOffL, cfg.yOffL, cfg.zOffL, cfg.swingL, cfg.poleL = l_scale, l_xoff, l_yoff, l_zoff, l_swing, l_pole
            cfg.pitchL, cfg.yawL, cfg.rollL = l_p, l_y, l_r
            changed = true
        end

        ImGui.Separator()
        ImGui.Text("Right Hand Calibration")
        local r_scale, c8 = ImGui.SliderFloat("Reach Scale R", cfg.scaleR, 0.5, 2.0)
        local r_xoff, c_x2 = ImGui.SliderFloat("Left/Right Offset R", cfg.xOffR, -1.0, 1.0)
        local r_yoff, c_y2 = ImGui.SliderFloat("Forward Offset R", cfg.yOffR, -1.0, 1.0)
        local r_zoff, c9 = ImGui.SliderFloat("Height Offset R", cfg.zOffR, -1.0, 1.0)
        local r_swing, c10 = ImGui.SliderFloat("Elbow Swing R", cfg.swingR, 0.0, 1.0)
        local r_pole, c11 = ImGui.SliderFloat("Elbow Pole R", cfg.poleR, -180.0, 180.0)
        local r_p, c12 = ImGui.SliderFloat("Wrist Pitch R", cfg.pitchR, -180.0, 180.0)
        local r_y, c13 = ImGui.SliderFloat("Wrist Yaw R", cfg.yawR, -180.0, 180.0)
        local r_r, c14 = ImGui.SliderFloat("Wrist Roll R", cfg.rollR, -180.0, 180.0)
        if c8 or c_x2 or c_y2 or c9 or c10 or c11 or c12 or c13 or c14 then
            cfg.scaleR, cfg.xOffR, cfg.yOffR, cfg.zOffR, cfg.swingR, cfg.poleR = r_scale, r_xoff, r_yoff, r_zoff, r_swing, r_pole
            cfg.pitchR, cfg.yawR, cfg.rollR = r_p, r_y, r_r
            changed = true
        end

        if changed then
            applyCalibration()
            SaveConfig()
        end

    end
    ImGui.End()
end)

registerForEvent("onInit", function()
    isReady = true
    LoadConfig()
    applyCalibration()
end)

registerForEvent("onUpdate", function(dt)
    if not isReady then return end
    -- Call the RED4ext-exported function every frame to drive persistent graph vars
    pcall(function() Game.UpdateVRIKAnimInputs() end)
end)
