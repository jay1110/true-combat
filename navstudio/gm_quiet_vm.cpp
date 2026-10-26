// Offline quiet-script regression driver, linked with Omni-bot's real GM VM.
// No engine, networking, game process, or Omni-bot hooks are invoked here.
#include "gmMachine.h"
#include "gmMachineLib.h"
#include "gmTableObject.h"
#include "gmStringObject.h"
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

static void State(gmMachine &machine, int step) {
    gmVariable value=machine.GetGlobals()->Get(&machine,"NavStudioQuietState");
    gmTableObject *quiet=value.GetTableObjectSafe();
    gmTableObject *server=machine.GetGlobals()->Get(&machine,"Server").GetTableObjectSafe();
    gmTableObject *bots=machine.GetGlobals()->Get(&machine,"BotTable").GetTableObjectSafe();
    std::cout<<"STATE step="<<step<<" quiet_type="<<value.m_type;
    if(quiet) {
        gmStringObject *token=quiet->Get(&machine,"Token").GetStringObjectSafe();
        gmTableObject *saved=quiet->Get(&machine,"Bots").GetTableObjectSafe();
        std::cout<<" token="<<(token?token->GetString():"null")<<" savedbots="<<(saved?saved->Count():-1);
    }
    if(server) std::cout<<" minbots="<<server->Get(&machine,"MinBots").GetIntSafe(-999)
                       <<" maxbots="<<server->Get(&machine,"MaxBots").GetIntSafe(-999)
                       <<" balance="<<server->Get(&machine,"BalanceTeams").GetIntSafe(-999)
                       <<" saveconfig="<<server->Get(&machine,"SaveConfigChanges").GetIntSafe(-999);
    std::cout<<" botcount="<<(bots?bots->Count():-1)<<"\n";
}

int main(int argc,char **argv) {
    gmMachine machine;gmMachineLib(&machine);
    for(int index=1;index<argc;++index) {
        if(std::string(argv[index])=="--reset-vm") {
            machine.ResetAndFreeMemory();machine.Init();gmMachineLib(&machine);
            State(machine,index);continue;
        }
        std::ifstream input(argv[index],std::ios::binary);
        if(!input) {std::cerr<<"Cannot open script: "<<argv[index]<<"\n";return 2;}
        std::ostringstream source;source<<input.rdbuf();
        machine.GetLog().Reset();
        int thread=0,errors=machine.ExecuteString(source.str().c_str(),&thread,true,argv[index]);
        machine.Execute(1);machine.CollectGarbage(true);
        bool first=true;const char *entry=machine.GetLog().GetEntry(first);
        if(errors||entry) {
            std::cerr<<"GM_SCRIPT_ERROR step="<<index<<" count="<<errors<<"\n";
            while(entry){std::cerr<<entry<<"\n";entry=machine.GetLog().GetEntry(first);}
            return 3;
        }
        State(machine,index);
    }
    return 0;
}
