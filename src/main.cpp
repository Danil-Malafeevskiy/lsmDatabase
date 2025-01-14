#include <userver/components/minimal_server_component_list.hpp>
#include <userver/utest/using_namespace_userver.hpp>
#include <userver/utils/daemon_run.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/clients/dns/component.hpp>


//#include "handlers/handler.hpp"
#include "handlers/heartbeat.cpp"
#include "handlers/add_node.cpp"
#include "handlers/remove_node.cpp"

int main(int argc, char* argv[]) {
    auto component_list = components::MinimalServerComponentList()
        .Append<clients::dns::Component>()
        .Append<components::HttpClient>()
        .Append<CrudHandler>()
        .Append<LSMStorage>()
        .Append<VoteHandler>()
        .Append<HeartBeatHandler>()
        .Append<AddNodeHandler>()
        .Append<RemoveNodeHandler>();
    
    return utils::DaemonMain(argc, argv, component_list);
}