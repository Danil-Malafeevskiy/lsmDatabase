#include "handler.hpp"
#include <userver/logging/log.hpp>
#include <userver/formats/json.hpp>

std::string CrudHandler::HandleRequestThrow(const server::http::HttpRequest& request,
                                            userver::server::request::RequestContext& context) const {
    if (request.GetMethod() == server::http::HttpMethod::kPost) {
        if (lsm_storage -> context.state != State::Leader){
            return "Запрос может работать только у лидера!";
        }

        auto json = formats::json::FromString(request.RequestBody());

        std::vector<std::pair<std::string, SimpleType>> data;
        if (json.IsObject()) {
            auto jsonObj = json.As<std::map<std::string, userver::formats::json::Value>>();
            for (const auto& item : jsonObj) {
                std::string key = item.first;
                std::string value = item.second.As<std::string>();
                lsm_storage -> add_operations(key, value, "insert");
            }
        }

        LOG_INFO() << "Создан объект: " << json;

        return request.RequestBody();

    } else if (request.GetMethod() == server::http::HttpMethod::kGet) {
        auto result = lsm_storage -> get(request.RequestBody());

        std::string response = std::visit([](const auto& v) -> std::string {
            std::ostringstream oss;
            oss << v;
            return oss.str();
        }, result.second);;

        return response;

    } else if (request.GetMethod() == server::http::HttpMethod::kPut) {
        if (lsm_storage -> context.state != State::Leader){
            return "Запрос может работать только у лидера!";
        }

        auto json = formats::json::FromString(request.RequestBody());

        if (json.IsObject()) {
            auto jsonObj = json.As<std::map<std::string, userver::formats::json::Value>>();
            for (const auto& item : jsonObj) {
                lsm_storage -> add_operations(item.first, item.second.As<std::string>(), "update");
            };
        }

        LOG_INFO() << "Обновлен объект: " << json;

        return "Обновлен объект: " + request.RequestBody();

    } else if (request.GetMethod() == server::http::HttpMethod::kDelete) {
        if (lsm_storage -> context.state != State::Leader){
            return "Запрос может работать только у лидера!";
        }
        auto key = request.RequestBody();

        lsm_storage -> add_operations(key, "", "delete");
        
        return "Объект по ключу " + key + " удален!";
    }
    return "Неподдерживаемый метод";
};
