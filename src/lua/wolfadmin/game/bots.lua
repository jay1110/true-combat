
-- WolfAdmin module for Wolfenstein: Enemy Territory servers.
-- Copyright (C) 2015-2020 Timo 'Timothy' Smit

-- This program is free software: you can redistribute it and/or modify
-- it under the terms of the GNU General Public License as published by
-- the Free Software Foundation, either version 3 of the License, or
-- at your option any later version.

-- This program is distributed in the hope that it will be useful,
-- but WITHOUT ANY WARRANTY; without even the implied warranty of
-- MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
-- GNU General Public License for more details.

-- You should have received a copy of the GNU General Public License
-- along with this program.  If not, see <http://www.gnu.org/licenses/>.

local players = wolfa_requireModule("players.players")

local events = wolfa_requireModule("util.events")
local settings = wolfa_requireModule("util.settings")
local util = wolfa_requireModule("util.util")

local bots = {}

function bots.put(team)
    local admin = wolfa_requireModule("admin.admin")
    local moved, refused = 0, 0
    for playerId = 0, et.trap_Cvar_Get("sv_maxclients") - 1 do
        if players.isConnected(playerId) and players.isBot(playerId) then
            if admin.putPlayer(playerId, team) then moved = moved + 1
            else refused = refused + 1 end
        end
    end
    return moved, refused
end

function bots.enable(enable)
    if enable then
        if tonumber(et.trap_Cvar_Get("omnibot_enable")) ~= 1 then return false, 0 end
        local maximum = tonumber(settings.get("omnibot_maxbots")) or 10
        local capacity = math.max(0, (tonumber(et.trap_Cvar_Get("sv_maxclients")) or 1) - 1)
        maximum = math.min(capacity, math.max(1, math.floor(maximum)))
        if maximum < 1 then return false, 0 end
        local minimum = tonumber(settings.get("omnibot_minbots")) or -1
        -- An explicit !needbots must actually request population even when
        -- automatic map-script population was disabled with minbots=-1.
        if minimum < 1 then minimum = maximum end
        minimum = math.min(maximum, math.floor(minimum))
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot maxbots "..maximum..";")
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot minbots "..minimum..";")
        return true, minimum
    else
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot minbots -1;")
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot maxbots -1;")
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot kickall;")
        return true, 0
    end
end

function bots.oninit(levelTime, randomSeed, restartMap)
end
events.handle("onGameInit", bots.oninit)

return bots
