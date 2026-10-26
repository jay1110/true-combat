"""Temporarily remove bots without rewriting Omni-bot's persistent settings.

Names remain GameMonkey values, never interpolated into console commands.
The saved VM table belongs to one token; the backend separately checks the
exact owned process creation identity and map before any restoration.
"""
import re


def begin_script(token):
    if not re.fullmatch(r"[a-f0-9]{16}", token):
        raise ValueError("Invalid quiet-generation token")
    return ("bot script_run global NavStudioQuietState; "
            "if(NavStudioQuietState!=null){print('NAVSTUDIO_QUIET_BUSY');}else{"
            "NavStudioQuietState={Token='" + token + "',MinBots=Server.MinBots,MaxBots=Server.MaxBots,"
            "BalanceTeams=Server.BalanceTeams,SaveConfigChanges=Server.SaveConfigChanges,Bots={}};"
            "foreach(id and b in BotTable){NavStudioQuietState.Bots[id]={Team=b.GetTeam(),Class=b.GetClass(),Name=b.Name};}"
            "Server.SaveConfigChanges=false;Server.MinBots=-1;Server.MaxBots=0;Server.BalanceTeams=false;"
            "print('NAVSTUDIO_QUIET_OK');}")


def count_script():
    return "bot script_run local n=0; foreach(id and b in BotTable){n+=1;} print('NAVSTUDIO_QUIET_COUNT:'+n);"


def restore_script(token):
    if not re.fullmatch(r"[a-f0-9]{16}", token):
        raise ValueError("Invalid quiet-generation token")
    return ("bot script_run global NavStudioQuietState;"
            "if(NavStudioQuietState!=null && NavStudioQuietState.Token=='" + token + "'){"
            "local s=NavStudioQuietState;foreach(id and b in s.Bots){AddBot(b);}"
            "Server.MinBots=s.MinBots;Server.MaxBots=s.MaxBots;Server.BalanceTeams=s.BalanceTeams;"
            "Server.SaveConfigChanges=s.SaveConfigChanges;NavStudioQuietState=null;"
            "print('NAVSTUDIO_QUIET_RESTORED');}else if(NavStudioQuietState==null){"
            "print('NAVSTUDIO_QUIET_NOT_ACTIVE');}else{print('NAVSTUDIO_QUIET_TOKEN_MISSING');}")


def file_source(command):
    if not command.startswith("bot script_run "):
        raise ValueError("Unexpected quiet-generation script")
    return command[len("bot script_run "):].replace("'",'"')+"\n"
