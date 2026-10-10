
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

local util = wolfa_requireModule("util.util")
local events = wolfa_requireModule("util.events")
local files = wolfa_requireModule("util.files")
local logs = wolfa_requireModule("util.logs")
local tables = wolfa_requireModule("util.tables")

local commands = {}

local clientcmds = {}
local servercmds = {}
local admincmds = {}

-- A bad optional command must not disable the complete administration VM.
-- Report the bounded Lua error to the server and fail closed for this command.
local function invoke(clientId, name, callback, ...)
    local ok, result = xpcall(callback, function(err)
        if debug and debug.traceback then return debug.traceback(tostring(err), 2) end
        return tostring(err)
    end, ...)
    if ok then return result end
    local detail = tostring(result):gsub("[%c]", " "):sub(1, 700)
    et.G_Print("WolfAdmin command "..name.." failed: "..detail.."\n")
    if clientId ~= -1337 then
        et.trap_SendServerCommand(clientId, 'print "WolfAdmin: '..name..' failed; see server console. No further command processing was performed."')
    end
    return 1
end


function commands.getclient(command)
    if command then
        return clientcmds[command]
    end

    return clientcmds
end

function commands.getserver(command)
    if command then
        return servercmds[command]
    end

    return servercmds
end

function commands.getadmin(command)
    if command then
        return admincmds[command]
    end

    return admincmds
end

function commands.addclient(command, func, flag, syntax, chat, disabled)
    if disabled then
        return
    end

    clientcmds[command] = {
        ["function"] = func,
        ["flag"] = flag,
        ["syntax"] = "^7"..command..(syntax and " "..syntax or ""),
        ["chat"] = chat,
    }
end

function commands.addserver(command, func, disabled)
    if disabled then
        return
    end

    local outputCommands = {csay=true, cchat=true, ccp=true, ccpm=true,
        cbp=true, cannounce=true, cmusic=true}
    local callback = func
    if outputCommands[command] then
        callback = function(name, recipient, text, ...)
            local id = tonumber(recipient)
            local max = tonumber(et.trap_Cvar_Get("sv_maxclients")) or 0
            if not id or id ~= math.floor(id) or
                (id ~= -1337 and (id < -1 or id >= max)) or type(text) ~= "string" then
                et.G_Print("usage: "..name.." <slot|-1> <quoted text>\n")
                return true
            end
            return func(name, id, text, ...)
        end
    end
    servercmds[command] = {
        ["function"] = callback,
    }
end

function commands.addadmin(command, func, flag, help, syntax, hidden, disabled)
    if disabled or wolfa_tce_disabled[command] then
        return
    end

    admincmds[command] = {
        ["function"] = func,
        ["flag"] = flag,
        ["help"] = help or "N/A",
        ["syntax"] = "^2!"..command..(syntax and " "..syntax or ""),
        ["hidden"] = hidden
    }
end

function commands.loadFiles(dir)
    local amount = 0
    local files = files.ls("commands/"..dir.."/")
    table.sort(files)

    for _, file in pairs(files) do
        if string.match(string.lower(file), "^[a-z0-9]+%.lua$") and not (dir == "admin" and wolfa_tce_disabled[file:sub(1, -5)]) then
            wolfa_requireModule("commands."..dir.."."..string.sub(file, 1, string.len(file) - 4))

            amount = amount + 1
        end
    end

    return amount
end

function commands.load()
    local functionStart = et.trap_Milliseconds()

    local clientAmount = commands.loadFiles("client")
    local serverAmount = commands.loadFiles("server")
    local adminAmount = commands.loadFiles("admin")

    local totalAmount = clientAmount + serverAmount + adminAmount

    outputDebug("commands.load(): "..totalAmount.." entries loaded in "..et.trap_Milliseconds() - functionStart.." ms")

    return totalAmount
end

function commands.log(clientId, command, victim, ...)
    local victimId

    -- funny, NoQuarter actually checks EACH command for a victim (so even
    -- !help [playername] will log a victimname). so why not do the same :D
    -- todo: do this more nicely, maybe change .register() function
    if victim then
        local cmdClient

        cmdClient = wolfa_tce_resolveClient(victim)

        if cmdClient ~= -1 and cmdClient ~= nil and et.gentity_get(cmdClient, "pers.netname") then
            victimId = cmdClient
        end
    end

    logs.writeAdmin(clientId, command, victimId, ...)
end

function commands.onGameInit()
    commands.load()
end
events.handle("onGameInit", commands.onGameInit)

function commands.onServerCommand(command)
    local wolfCmd = string.lower(command)
    local args = {}

    if servercmds[wolfCmd] and servercmds[wolfCmd]["function"] then
        for i = 1, et.trap_Argc() - 1 do
            table.insert(args, et.trap_Argv(i))
        end

        invoke(-1337, wolfCmd, servercmds[wolfCmd]["function"], wolfCmd, tables.unpack(args))
        return 1
    end

    local shrubCmd = command

    if string.find(command, "!") == 1 then
        shrubCmd = string.lower(string.sub(command, 2, string.len(command)))
    end

    if wolfa_tce_disabled[shrubCmd] then
        et.G_Print("WolfAdmin: "..shrubCmd.." is unavailable in TC:E (requires unsupported upstream game data).\n")
        return 1
    end

    if admincmds[shrubCmd] and admincmds[shrubCmd]["function"] and admincmds[shrubCmd]["flag"] then
        for i = 1, et.trap_Argc() - 1 do
            table.insert(args, et.trap_Argv(i))
        end

        if not admincmds[shrubCmd]["hidden"] then
            commands.log(-1337, shrubCmd, tables.unpack(args))
        end

        invoke(-1337, shrubCmd, admincmds[shrubCmd]["function"], -1337, shrubCmd, tables.unpack(args))
        -- These handlers often return false after printing help/config results.
        -- The command is still ours; never fall through to native unknown.
        return 1
    end
end
events.handle("onServerCommand", commands.onServerCommand)

function commands.onClientCommand(clientId, command)
    local wolfCmd = string.lower(command)
    local args = {}

    -- mod-specific or custom commands loading
    -- syntax: command arg1 arg2 ... argN
    if clientcmds[wolfCmd] and clientcmds[wolfCmd]["function"] and clientcmds[wolfCmd]["flag"] then
        if clientcmds[wolfCmd]["flag"] == "" or auth.isPlayerAllowed(clientId, clientcmds[wolfCmd]["flag"]) then
            for i = 1, et.trap_Argc() - 1 do
                table.insert(args, et.trap_Argv(i))
            end

            local isFinished = invoke(clientId, wolfCmd, clientcmds[wolfCmd]["function"], clientId, wolfCmd, tables.unpack(args))

            if isFinished ~= nil then
                return wolfa_tce_consumed(isFinished)
            end
        end
    end

    -- client cmds
    -- syntax: say or say_*
    local clientCmd

    if (wolfCmd == "say" or wolfCmd == "say_team" or wolfCmd == "say_buddy") and string.find(et.trap_Argv(1), "/") == 1 then
        args = util.split(et.trap_Argv(1), " ")

        -- say "/command arg1 arg2 argN"
        if #args > 1 then
            clientCmd = string.sub(args[1], 2, string.len(args[1]))
            table.remove(args, 1)
        -- say /command arg1 arg2 argN
        else
            clientCmd = string.sub(et.trap_Argv(1), 2, string.len(et.trap_Argv(1)))

            for i = 2, et.trap_Argc() - 1 do
                table.insert(args, et.trap_Argv(i))
            end
            if args[1] == et.trap_Argv(1) then table.remove(args, 1) end
        end
    end

    -- handle client cmds
    if clientCmd then
        clientCmd = string.lower(clientCmd)

        if clientcmds[clientCmd] and clientcmds[clientCmd]["function"] and clientcmds[clientCmd]["chat"] then
            if clientcmds[clientCmd]["flag"] == "" or auth.isPlayerAllowed(clientId, clientcmds[clientCmd]["flag"]) then
                return wolfa_tce_consumed(invoke(clientId, clientCmd, clientcmds[clientCmd]["function"], clientId, clientCmd, tables.unpack(args)))
            end
        end
    end

    -- shrub cmds
    local shrubCmd

    -- syntax: say or say_*
    if (wolfCmd == "say" or wolfCmd == "say_team" or wolfCmd == "say_buddy") and string.find(et.trap_Argv(1), "!") == 1 then
        args = util.split(et.trap_Argv(1), " ")

        -- syntax: say "!command arg1 arg2 ... argN"
        if #args > 1 then
            shrubCmd = string.sub(args[1], 2, string.len(args[1]))

            table.remove(args, 1)
        -- syntax: say !command arg1 arg2 ... argN
        else
            shrubCmd = string.sub(et.trap_Argv(1), 2, string.len(et.trap_Argv(1)))

            for i = 2, et.trap_Argc() - 1 do
                table.insert(args, et.trap_Argv(i))
            end
            if args[1] == et.trap_Argv(1) then table.remove(args, 1) end
        end
    -- syntax: !command arg1 arg2 ... argN
    elseif string.find(wolfCmd, "!") == 1 then
        shrubCmd = string.sub(wolfCmd, 2, string.len(wolfCmd))

        for i = 1, et.trap_Argc() - 1 do
            table.insert(args, et.trap_Argv(i))
        end
    end

    -- handle shrub commands
    if shrubCmd then
        shrubCmd = string.lower(shrubCmd)

        if wolfa_tce_disabled[shrubCmd] then
            et.trap_SendServerCommand(clientId, 'print "WolfAdmin: '..shrubCmd..' is unavailable in TC:E (requires unsupported upstream game data)."')
            return 1
        end

        if admincmds[shrubCmd] and admincmds[shrubCmd]["function"] and admincmds[shrubCmd]["flag"] then
            if wolfCmd == "say" or (((wolfCmd == "say_team" and et.gentity_get(clientId, "sess.sessionTeam") ~= et.TEAM_SPECTATOR) or wolfCmd == "say_buddy") and auth.isPlayerAllowed(clientId, auth.PERM_TEAMCMDS)) or (wolfCmd == "!"..shrubCmd and auth.isPlayerAllowed(clientId, auth.PERM_SILENTCMDS)) then
                if admincmds[shrubCmd]["flag"] ~= "" and auth.isPlayerAllowed(clientId, admincmds[shrubCmd]["flag"]) then
                    if not admincmds[shrubCmd]["hidden"] then
                        commands.log(clientId, shrubCmd, tables.unpack(args))
                    end
                    local isFinished = invoke(clientId, shrubCmd, admincmds[shrubCmd]["function"], clientId, shrubCmd, tables.unpack(args))

                    if wolfa_tce_consumed(isFinished) == 1 and "!"..shrubCmd == wolfCmd then -- silent command via console, removes "unknown command" message
                        return 1
                    end
                else
                    et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \""..shrubCmd..": permission denied\";")
                end
            elseif wolfCmd == "!"..shrubCmd then
                et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay "..clientId.." \""..shrubCmd..": permission denied\";")
            end
            -- A known silent command remains handled when permission is denied
            -- or its handler only prints usage. Do not leak into native unknown.
            if wolfCmd == "!"..shrubCmd then return 1 end
        end
    end

    return 0
end
events.handle("onClientCommand", commands.onClientCommand)

return commands
