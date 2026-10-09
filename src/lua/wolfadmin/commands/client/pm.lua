
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
local logs = wolfa_requireModule("util.logs")

-- TC:E has no native pm/m command. Implement the original private-message
-- presentation here, using one unambiguous, connected recipient.
function commandPersonalMessage(clientId, command, target, ...)
    if players.isMuted(clientId, players.MUTE_CHAT) then
        et.trap_SendServerCommand(clientId, "cp \"^1You are muted\"")
        return true
    end
    local args = {...}
    if not target or #args == 0 or table.concat(args, " ") == "" then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^9usage: pm [name|slot#] [message]\";")
        return true
    end
    local recipient = wolfa_tce_resolveClient(target)
    if recipient == -1 then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \"^9pm: ^7invalid, ambiguous or disconnected recipient.\";")
        return true
    end

    local message = table.concat(args, " "):gsub("[%c]", " ")
    local senderName, recipientName = players.getName(clientId), players.getName(recipient)
    local text = "^7"..senderName.."^7 -> "..recipientName.."^7: ^3"..message
    players.setLastPMSender(recipient, clientId)
    et.trap_SendConsoleCommand(et.EXEC_APPEND, "cchat "..clientId.." \""..text.."\";")
    if clientId ~= recipient then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "cchat "..recipient.." \""..text.."\";")
    end
    et.trap_SendConsoleCommand(et.EXEC_APPEND, "ccp "..recipient.." \"^3private message from "..senderName.."\";")
    et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..recipient.." \"^9reply: ^7r [message]\";")
    logs.writeChat(clientId, "priv", recipient, message)
    return true
end
commands.addclient("pm", commandPersonalMessage, "", "[^2name^7|^2slot#^7] [^2message^7]", true)
commands.addclient("m", commandPersonalMessage, "", "[^2name^7|^2slot#^7] [^2message^7]", true)
