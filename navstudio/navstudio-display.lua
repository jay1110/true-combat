-- Test-instance display only. No player movement, bindings or game rules.
local showAt = nil

function et_InitGame(levelTime, randomSeed, restart)
    et.RegisterModname("NavStudio waypoint display")
    showAt = nil
    if et.trap_Cvar_Get("navstudio_waypoint_view") == "1" then
        -- Lua initializes before Omni-bot. Each map/round gets one delayed
        -- request after Omni initialization, including warmup map_restart.
        showAt = levelTime + 3000
    end
end

function et_RunFrame(levelTime)
    if showAt and levelTime >= showAt then
        showAt = nil
        et.trap_Cvar_Set("cg_omnibotdrawing", "1")
        et.trap_SendConsoleCommand(2, "bot waypoint_view on\n")
        et.G_Print("NAVSTUDIO waypoint view requested after map/round initialization\n")
    end
end
