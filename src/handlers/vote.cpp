#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/formats/json.hpp>
#include <userver/logging/log.hpp>
#include "handler.hpp"

class VoteHandler : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "vote-handler";

    VoteHandler(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context)
        : HttpHandlerJsonBase(config, context), lsm_storage(&context.FindComponent<LSMStorage>()) {}

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext&) const override {
        
        bool checkTerm;

        int candidate_term = request_body["term"].As<int>(-1);
        int candidate_id = request_body["candidate_id"].As<int>(-1);

        LOG_INFO() << "Получен запрос голосования от узла: " << candidate_id << ", term: " << candidate_term;

        if (candidate_term < lsm_storage->context.term){
            return userver::formats::json::MakeObject("term", lsm_storage->context.term,"voteGranted", false);
        }
        else {
            if (candidate_term == lsm_storage->context.term){
                checkTerm = (lsm_storage->context.voted_for == -1 || lsm_storage->context.voted_for == candidate_id);
            } else {
                checkTerm = true;
                lsm_storage->context.term = candidate_term;
            }
        }

        bool checkLog = ((lsm_storage->operationLogs.lastLogTerm < request_body["last_log_term"].As<int>(-1)) || 
        (lsm_storage->operationLogs.lastLogTerm == request_body["last_log_term"].As<int>(-1) && lsm_storage->operationLogs.lastLogIndex <= request_body["last_log_index"].As<int>(-1)));

        bool voteGranted = checkLog&&checkTerm;

        if (voteGranted == true) {
            lsm_storage->context.voted_for = candidate_id;

            LOG_INFO() << "Проголосовали за узел: " << candidate_id;
        };

        return userver::formats::json::MakeObject("term", lsm_storage->context.term, "voteGranted", voteGranted);
    }

private:
    LSMStorage* lsm_storage;
};
