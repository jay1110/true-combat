-- Example: set lua_modules "lua/example.lua" before map start.
et.RegisterModname("TC:E Lua example")
function et_InitGame(levelTime, randomSeed, restart)
    et.G_Print("TC:E Lua example initialized\n")
end
function et_ClientCommand(clientNum, command)
    if command == "luahello" then
        et.trap_SendServerCommand(clientNum, 'print "Hello from TC:E Lua!\n"')
        return 1
    end
    return 0
end
function et_ConsoleCommand(command)
    if command == "luahello" then
        et.G_Print("Hello from TC:E Lua!\n")
        return 1
    end
    return 0
end
