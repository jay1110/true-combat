
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

local db = wolfa_requireModule("db.db")

local players = wolfa_requireModule("players.players")

local events = wolfa_requireModule("util.events")
local settings = wolfa_requireModule("util.settings")
local util = wolfa_requireModule("util.util")

local admin = {}

local playerRenames = {}

function admin.putPlayer(clientId, teamId)
    local teamNames = {[1] = "axis", [2] = "allies", [3] = "spectator"}
    if not teamNames[teamId] or et.gentity_get(clientId, "pers.connected") ~= et.CON_CONNECTED then return false end
    -- SetTeam returns false both for already-on-team and for a successful
    -- assignment deferred until next round. The resulting session team is
    -- authoritative; do not misreport those cases as refused.
    if tonumber(et.gentity_get(clientId, "sess.sessionTeam")) == teamId then return true end
    et.G_SetTeam(clientId, teamNames[teamId])
    if tonumber(et.gentity_get(clientId, "sess.sessionTeam")) ~= teamId then return false end
    -- Native operations skip callbacks into the executing Lua state.
    players.onClientInfoChange(clientId)
    return true
end

function admin.kickPlayer(victimId, invokerId, reason)
    et.trap_DropClient(victimId, "You have been kicked, Reason: "..reason, 0)
end

function admin.setPlayerLevel(clientId, level)
    local playerId = db.getPlayer(players.getGUID(clientId))["id"]

    db.updatePlayerLevel(playerId, level)
end

function admin.onClientConnectAttempt(clientId, firstTime, isBot)
    if firstTime and db.isConnected() then
        local guid = et.Info_ValueForKey(et.trap_GetUserinfo(clientId), "cl_guid")

        if not wolfa_tce_validGUID(guid) or isBot then
            events.trigger("onClientConnect", clientId, firstTime, isBot)
            return
        end
        guid = guid:upper()

        if settings.get("g_standalone") ~= 0 then
            local player = db.getPlayer(guid)
            if player then
                local playerId = player["id"]
                local ban = db.getBanByPlayer(playerId)
                if ban then
                    local period = tonumber(ban["expires"]) == 0 and "permanently" or ("for "..math.max(0, tonumber(ban["expires"]) - os.time()).." more seconds")
                    return "\n\nYou have been banned "..period..", Reason: "..ban["reason"]
                end
            end
        end
    end

    events.trigger("onClientConnect", clientId, firstTime, isBot)
end
events.handle("onClientConnectAttempt", admin.onClientConnectAttempt)

function admin.onClientConnect(clientId, firstTime, isBot)
    if settings.get("g_standalone") ~= 0 and db.isConnected() then
        local guid = players.getGUID(clientId)
        local player = db.getPlayer(guid)

        if player then
            local playerId = player["id"]
            local mute = db.getMuteByPlayer(playerId)

            if mute then
                players.setMuted(clientId, true, mute["type"], mute["issued"], mute["expires"])
            end
        end
    end
end
events.handle("onClientConnect", admin.onClientConnect)

function admin.onClientDisconnect(clientId)
    if playerRenames[clientId] then
        playerRenames[clientId] = nil
    end
end
events.handle("onClientDisconnect", admin.onClientDisconnect)

function admin.onClientNameChange(clientId, oldName, newName)
    -- rename filter
    if not playerRenames[clientId] or playerRenames[clientId]["last"] < os.time() - 60 then
        playerRenames[clientId] = {
            ["first"] = os.time(),
            ["last"] = os.time(),
            ["count"] = 1
        }
    else
        playerRenames[clientId]["count"] = playerRenames[clientId]["count"] + 1
        playerRenames[clientId]["last"] = os.time()

        -- give them some time
        if (playerRenames[clientId]["last"] - playerRenames[clientId]["first"]) > 3 then
            local renamesPerMinute = playerRenames[clientId]["count"] / (playerRenames[clientId]["last"] - playerRenames[clientId]["first"]) * 60

            if renamesPerMinute > settings.get("g_renameLimit") then
                admin.kickPlayer(clientId, -1337, "Too many name changes.")
                return
            end
        end
    end

    -- on some mods, this message is already printed
    -- known: old NQ versions, Legacy
    if et.trap_Cvar_Get("fs_game") ~= "legacy" then
        et.trap_SendConsoleCommand(et.EXEC_APPEND, "csay -1 \""..oldName.." ^7is now known as "..newName.."\";")
    end

    -- update database
    if db.isConnected() then
        local playerId = db.getPlayer(players.getGUID(clientId))["id"]
        local alias = db.getAliasByName(playerId, newName)

        if alias then
            db.updateAlias(alias["id"], os.time())
        else
            db.addAlias(playerId, newName, os.time())
        end
    end
end
events.handle("onClientNameChange", admin.onClientNameChange)

return admin
