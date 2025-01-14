#pragma once
#include <userver/server/handlers/http_handler_base.hpp>
#include <userver/utest/using_namespace_userver.hpp>
#include <userver/formats/json/value.hpp>
#include "../interfaces/lsmstorage.cpp"

class CrudHandler final : public server::handlers::HttpHandlerBase {
public:
    static constexpr std::string_view kName = "crud-lsm-database-handler";
    
    CrudHandler(const components::ComponentConfig& config,
                const components::ComponentContext& context)
        : HttpHandlerBase(config, context), lsm_storage(&context.FindComponent<LSMStorage>()) {}
    
    std::string HandleRequestThrow(const server::http::HttpRequest& request,
                                    server::request::RequestContext& context) const override;
private:
    LSMStorage* lsm_storage;
};