
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

local commands = wolfa_requireModule("commands.commands")
local players = wolfa_requireModule("players.players")

function commandR(clientId, command, ...)
    if players.isMuted(clientId, players.MUTE_CHAT) then
        et.trap_SendServerCommand(clientId, "cp \"^1You are muted\"")
        return true
    end
    if not ... then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^9usage: r [message]\";")
        return true
    end
    local recipient = players.getLastPMSender(clientId)
    if not recipient or wolfa_tce_resolveClient(recipient) == -1 then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"player not found\";")
        return true
    end
    return commands.getclient("pm")["function"](clientId, "pm", tostring(recipient), ...)
end
commands.addclient("r", commandR, "", "[^2message^7]", true)
