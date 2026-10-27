#ifndef OMNIBOT_PUBLIC_BOT_COMMANDS_H
#define OMNIBOT_PUBLIC_BOT_COMMANDS_H

#include <cstring>

// Public servers can retain older, unfiltered GM files. The compiled command
// boundary must therefore be enforced independently of script packaging.
inline bool Omnibot_IsPublicCommand(const char *name)
{
    static const char *const commands[] = {
        "help", "addbot", "kickbot", "kickall", "dontshoot", "reload_weapons",
        "version", "revision", "update_nav", "update_all_nav",
        "showweapons", "numthreads", "kt", "ip", "ab", "keepbot", "time",
        "rollcall", "moveskill", "minbots", "maxbots", "balanceteams",
        "initialdelaytime", "idt", "botwar", "botteam", "humanteam",
        "botsperhuman", "difficulty", "resetxp", "maxxp", "ft_controls",
        "minformortar", "minformobilemg", "tceweapon", "tceclass"
    };
    if (!name) return false;
    for (unsigned int i = 0; i < sizeof(commands) / sizeof(commands[0]); ++i)
        if (std::strcmp(name, commands[i]) == 0) return true;
    return false;
}

#endif
