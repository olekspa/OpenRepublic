-- OpenRepublic presentation example: a deterministic 06:00 to 18:00 sun arc.
local day_seconds = 120
local api = _ENV["or"]

-- Small sine approximation keeps this example self-contained in the empty sandbox.
local function sine(x)
	local pi = 3.141592653589793
	return 4 * x * (pi - x) / (pi * pi)
end

function on_frame(dt)
    local progress = api.time / day_seconds
    if progress > 1 then progress = 1 end
    local hour = 6 + 12 * progress
    local fraction = (hour - 6) / 12
    local arc = sine(3.141592653589793 * fraction)
    api.look.set {
        sunElevationDegrees = 2 + 68 * arc,
        sunYawOffsetDegrees = -90 + 180 * fraction,
        sunWarmthKelvin = 2600 + 3400 * arc,
    }
end
