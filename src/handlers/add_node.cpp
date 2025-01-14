#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/formats/json.hpp>
#include <userver/logging/log.hpp>
#include "handler.hpp"

class AddNodeHandler : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "add-node-handler";

    AddNodeHandler(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context)
        : HttpHandlerJsonBase(config, context), lsm_storage(&context.FindComponent<LSMStorage>()) {}

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext&) const override {
        
        std::string socket = request_body["socket"].As<std::string>("");
        int node_id = request_body["node_id"].As<int>(-1);
        int match_index = request_body["match_index"].As<int>(-1);

        Peer peer(node_id);
        peer.socket = socket;
        peer.match_index = match_index;
        peer.next_index = match_index + 1;

        lsm_storage->context.peers->peers.push_back(peer);

        LOG_INFO() << "Добавлен новый узел: " << node_id;

        return userver::formats::json::MakeObject("result", true);
    }

private:
    LSMStorage* lsm_storage;
};
