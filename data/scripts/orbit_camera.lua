-- OpenRepublic showcase example: orbit the camera while preserving the current look.
local angle = 0
local api = _ENV["or"]

function on_frame(dt)
    angle = (angle + 24 * dt) % 360
    api.camera.set {
        yawDegrees = angle - 180,
        elevationDegrees = 24.5,
        distanceFactor = 1.15,
    }
    if angle < 24 * dt then
        api.look.preset("golden_hour")
    end
end
