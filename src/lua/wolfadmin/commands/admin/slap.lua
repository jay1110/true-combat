
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

local auth = wolfa_requireModule("auth.auth")

local commands = wolfa_requireModule("commands.commands")

local players = wolfa_requireModule("players.players")

local constants = wolfa_requireModule("util.constants")
local settings = wolfa_requireModule("util.settings")

function commandSlap(clientId, command, victim, damage, ...)
    local cmdClient

    if victim == nil then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap usage: "..commands.getadmin("slap")["syntax"].."\";")

        return true
    else
        cmdClient = wolfa_tce_resolveClient(victim)
    end

    if cmdClient == -1 or cmdClient == nil then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap: ^9no or multiple matches for '^7"..victim.."^9'.\";")

        return true
    elseif not et.gentity_get(cmdClient, "pers.netname") then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap: ^9no connected player by that name or slot #\";")

        return true
    end

    if auth.isPlayerAllowed(cmdClient, "!") then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap: ^7"..et.gentity_get(cmdClient, "pers.netname").." ^9is immune to this command.\";")

        return true
    elseif auth.getPlayerLevel(cmdClient) > auth.getPlayerLevel(clientId) then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap: ^9sorry, but your intended victim has a higher admin level than you do.\";")

        return true
    elseif et.gentity_get(cmdClient, "sess.sessionTeam") ~= constants.TEAM_AXIS and et.gentity_get(cmdClient, "sess.sessionTeam") ~= constants.TEAM_ALLIES then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap: ^7"..et.gentity_get(cmdClient, "pers.netname").." ^9is not playing.\";")

        return true
    elseif et.gentity_get(cmdClient, "health") <= 0 then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^dslap: ^7"..et.gentity_get(cmdClient, "pers.netname").." ^9is not alive.\";")

        return true
    end

    local amount = damage == nil and 20 or tonumber(damage)
    if not amount or amount ~= math.floor(amount) or amount < 1 or amount > 9999 then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"slap: damage must be an integer from 1 to 9999.\";")
        return true
    end
    if not et.G_AdminSlap(cmdClient, amount) then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"slap: target is not alive and playing.\";")
        return true
    end
    local reason = table.concat({...}, " ")
    et.trap_SendConsoleCommand(et.EXEC_APPEND, "cchat -1 \"^dslap: ^7"..players.getName(cmdClient).." ^9was slapped"..(reason ~= "" and " ("..reason..")" or "")..".\";")
    -- Optional upstream notification audio is unavailable in TC:E.

    return true
end
commands.addadmin("slap", commandSlap, auth.PERM_SLAP, "give a player a specified amount of damage for a specified reason", "^9[^3name|slot#^9] (^hdamage^9) (^hreason^9)", nil, (settings.get("g_standalone") == 0))
