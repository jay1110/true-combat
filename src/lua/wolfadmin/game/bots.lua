
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
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot minbots -1;")
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot maxbots "..settings.get("omnibot_maxbots")..";")
    else
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot minbots -1;")
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot maxbots -1;")
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "bot kickall;")
    end
end

function bots.oninit(levelTime, randomSeed, restartMap)
end
events.handle("onGameInit", bots.oninit)

return bots
