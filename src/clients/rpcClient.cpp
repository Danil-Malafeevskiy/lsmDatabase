#include <userver/clients/http/client.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/formats/json.hpp>

class RpcClient {
public:
    RpcClient() : http_client_(&context.FindComponent<components::HttpClient>().GetHttpClient()) {}

    std::optional<userver::formats::json::Value> send(const std::string& target_node,
                                                      const std::string& endpoint,
                                                      const userver::formats::json::Value& payload) {
        try {
            auto request = http_client_->CreateRequest();

            auto response = request
                .url(target_node + endpoint)
                .post()
                .data(userver::formats::json::ToString(payload))
                .headers({{"Content-Type", "application/json"}})
                .perform();
            
            return userver::formats::json::FromString(response->body());

        } catch (const std::exception& ex) {
            LOG_ERROR() << "RPC send failed to " << target_node << ": " << ex.what();
        }
        return std::nullopt;
    }

private:
    clients::http::Client* http_client_;
};
