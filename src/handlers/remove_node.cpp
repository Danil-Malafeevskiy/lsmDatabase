#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/formats/json.hpp>
#include <userver/logging/log.hpp>
#include "handler.hpp"

class RemoveNodeHandler : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "remove-node-handler";

    RemoveNodeHandler(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context)
        : HttpHandlerJsonBase(config, context), lsm_storage(&context.FindComponent<LSMStorage>()) {}

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext&) const override {
        
        int node_id = request_body["node_id"].As<int>(-1);

        lsm_storage->context.peers->remove_peer(node_id);

        LOG_INFO() << "Удален узел из кластера: " << node_id;

        return userver::formats::json::MakeObject("result", true);
    }

private:
    LSMStorage* lsm_storage;
};