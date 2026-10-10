-- TC:E adaptation of WolfAdmin 1.2.1, GPL-3.0-or-later.
-- Only this module's et table is affected: Lua modules have separate states.
wolfa_tce_disabled = {}

local sessions, loginAttempts = {}, {}
function wolfa_tce_clearSession(clientId)
    sessions[clientId], loginAttempts[clientId] = nil, nil
end

function wolfa_tce_authenticated(clientId)
    local password = et.trap_Cvar_Get("wolfadmin_password")
    if password == "" or sessions[clientId] ~= password then
        sessions[clientId] = nil
        return false
    end
    return true
end

function wolfa_tce_login(clientId, command)
    if command:lower() ~= "wolfauth" then return false end
    if et.trap_Argc() ~= 2 then
        et.trap_SendServerCommand(clientId, 'print "WolfAdmin: usage: /wolfauth <server admin password> (game console, not chat)."')
        return true
    end
    local expected = et.trap_Cvar_Get("wolfadmin_password")
    if expected == "" then
        et.trap_SendServerCommand(clientId, 'print "WolfAdmin: password login is disabled. The server owner must set wolfadmin_password in wolfadmin-private.cfg."')
        return true
    end
    local now = et.trap_Milliseconds()
    if loginAttempts[clientId] and now - loginAttempts[clientId] < 5000 then
        et.trap_SendServerCommand(clientId, 'print "WolfAdmin: wait five seconds before another login attempt."')
        return true
    end
    loginAttempts[clientId] = now
    sessions[clientId] = nil
    local players = wolfa_requireModule("players.players")
    if expected ~= "" and players.isConnected(clientId) and not players.isBot(clientId) and
        et.trap_Argc() == 2 and et.trap_Argv(1) == expected then
        sessions[clientId] = expected
        et.trap_SendServerCommand(clientId, 'print "WolfAdmin: session admin login successful. Rights expire on disconnect, map/Lua restart or password change."')
    else
        et.trap_SendServerCommand(clientId, 'print "WolfAdmin: login failed or password login is disabled."')
    end
    return true
end

-- Resolve numeric slots before any entity access; fractional, negative and
-- non-finite numbers are never passed to the native integer API.
function wolfa_tce_resolveClient(value)
    if value == nil then return -1 end
    local id = tonumber(value)
    local maximum = tonumber(et.trap_Cvar_Get("sv_maxclients"))
    if id ~= nil then
        if id ~= id or id < 0 or id >= maximum or id ~= math.floor(id) then return -1 end
    else
        id = et.ClientNumberFromString(tostring(value))
    end
    if type(id) ~= "number" or id < 0 or id >= maximum or id ~= math.floor(id) then return -1 end
    local players = wolfa_requireModule("players.players")
    if not players.isConnected(id) then return -1 end
    local ok, connected = pcall(et.gentity_get, id, "pers.connected")
    if not ok or connected ~= et.CON_CONNECTED then return -1 end
    return id
end

function wolfa_tce_consumed(result)
    return (result == true or result == 1) and 1 or 0
end

function wolfa_tce_validGUID(guid)
    return type(guid) == "string" and #guid == 32 and
        guid:match("^[%x]+$") ~= nil and guid:match("^0+$") == nil
end

function wolfa_tce_uniqueGUID(clientId)
    if type(clientId) ~= "number" or clientId < 0 then return false end
    local players = wolfa_requireModule("players.players")
    if not players.isConnected(clientId) or players.isBot(clientId) then return false end
    local guid = players.getGUID(clientId)
    if not wolfa_tce_validGUID(guid) then return false end
    if et.Info_ValueForKey(et.trap_GetUserinfo(clientId), "cl_guid"):upper() ~= guid then return false end
    for id = 0, tonumber(et.trap_Cvar_Get("sv_maxclients")) - 1 do
        if id ~= clientId and players.isConnected(id) and
            et.Info_ValueForKey(et.trap_GetUserinfo(id), "cl_guid"):upper() == guid then
            return false
        end
    end
    return true
end

function wolfa_tce_trustedClient(clientId)
    -- TC:E currently exposes client-supplied userinfo, not authenticated GUIDs.
    -- Explicit operator opt-in is necessary; console commands need no opt-in.
    return et.trap_Cvar_Get("g_wolfadminTrustGuid") == "1" and wolfa_tce_uniqueGUID(clientId)
end

local nativeConsole = et.trap_SendConsoleCommand
local nativeServer = et.trap_SendServerCommand
local function clean(text)
    return (tostring(text):gsub('[%c"\\]', ' ')):sub(1, 900)
end

-- Upstream builds console messages by concatenating player-controlled strings.
-- Consume the complete message directly, so embedded quotes cannot create
-- additional engine commands. The client protocol also uses quoted strings.
local outputs = {csay = "print", cchat = "chat", ccp = "cp", ccpm = "cpm",
                 cbp = "cp", cannounce = "cp"}
et.trap_SendConsoleCommand = function(mode, text)
    local command, id, body = text:match('^%s*(%w+)%s+(-?%d+)%s+"(.*)"%s*;?%s*$')
    if command and outputs[command] then
        id = tonumber(id)
        if id == -1337 then et.G_Print(clean(body).."\n")
        elseif id >= -1 and id < tonumber(et.trap_Cvar_Get("sv_maxclients")) then
            nativeServer(id, outputs[command]..' "'..clean(body)..(command == "csay" and "\n" or "")..'"')
        end
        return
    end
    -- Greeting broadcasts use cp/bp/cpm without a numeric recipient.
    local broadcast, message = text:match('^%s*(%w+)%s+"(.*)"%s*;?%s*$')
    local broadcastTypes = {cp = "cp", bp = "cp", cpm = "cpm", chat = "chat", say = "chat"}
    if broadcast and broadcastTypes[broadcast] then
        nativeServer(-1, broadcastTypes[broadcast]..' "'..clean(message)..'"')
        return
    end
    local soundClient, soundPath = text:match('^%s*playsound%s+(-?%d+)%s+"([^"%c]+)"%s*;?%s*$')
    if not soundPath then
        soundPath = text:match('^%s*playsound%s+"([^"%c]+)"%s*;?%s*$')
        soundClient = -1
    end
    if soundPath then
        soundPath = soundPath:gsub("^/", "")
        if soundPath:match("^sound/") and not soundPath:find("..", 1, true) and
            not soundPath:find('[\\:;]') then
            et.G_AdminSound(tonumber(soundClient), soundPath)
        else
            et.G_Print("WolfAdmin: invalid greeting sound path.\n")
        end
        return
    end
    text = text:gsub("%s*;%s*$", ""):gsub("%s+$", "")
    -- All non-message commands used by enabled upstream modules have a fixed
    -- grammar. Never submit arbitrary player text to the engine console.
    if text:match("^sets mod_wolfadmin [%d%.]+$") or
       text:match("^map_restart %d+ %d+$") or text == "vstr nextmap" or
       text == "reset_match" or text == "swap_teams" or text == "shuffle_teams" or
       text == "ref pause" or text == "ref unpause" or
       text:match("^forceteam %d+ [rbs]$") or
       text:match("^bot minbots %-?%d+$") or text:match("^bot maxbots %-?%d+$") or
       text == "bot kickall" then
        nativeConsole(mode, text.."\n")
    else
        et.G_Print("WolfAdmin: unsupported console operation suppressed.\n")
    end
end

et.trap_SendServerCommand = function(id, text)
    local command, body = text:match('^(%w+)%s+"(.*)"%s*;?%s*$')
    if command == "bp" or command == "announce" then command = "cp" end
    if command then return nativeServer(id, command..' "'..clean(body)..(command == "print" and "\n" or "")..'"') end
    return nativeServer(id, tostring(text):sub(1, 1000))
end

-- Native drops run synchronously while callbacks into this Lua state are busy.
-- All disconnect handlers operate on caches and are safe to repeat if the
-- engine subsequently sends the ordinary disconnect notification.
local nativeDrop = et.trap_DropClient
et.trap_DropClient = function(clientId, reason, timeout)
    nativeDrop(clientId, clean(reason), timeout)
    if et_ClientDisconnect then et_ClientDisconnect(clientId) end
end
local nativeLog = et.G_LogPrint
et.G_LogPrint = function(text)
    return nativeLog(tostring(text):sub(1, 1000))
end

return true
