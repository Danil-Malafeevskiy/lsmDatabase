#include <userver/server/handlers/http_handler_json_base.hpp>
#include <userver/formats/json.hpp>
#include <userver/logging/log.hpp>
#include "vote.cpp"
#include <deque>

class HeartBeatHandler : public userver::server::handlers::HttpHandlerJsonBase {
public:
    static constexpr std::string_view kName = "heart-beat-handler";

    HeartBeatHandler(const userver::components::ComponentConfig& config,
                const userver::components::ComponentContext& context)
        : HttpHandlerJsonBase(config, context), lsm_storage(&context.FindComponent<LSMStorage>()) {}

    userver::formats::json::Value HandleRequestJsonThrow(
        const userver::server::http::HttpRequest& request,
        const userver::formats::json::Value& request_body,
        userver::server::request::RequestContext&) const override {

        int leader_term = request_body["term"].As<int>(-1);
        int leader_id = request_body["leader_id"].As<int>(-1);

        LOG_INFO() << "Получен heartbeat от узла: " << leader_id;

        if (leader_term < lsm_storage->context.term){
            return userver::formats::json::MakeObject(
                "append", false,
                "term", lsm_storage->context.term
            );
        } else if (leader_term > lsm_storage->context.term){
            lsm_storage->context.term = leader_term;
            lsm_storage->context.voted_for = -1;
        }

        lsm_storage->last_heartbeat_time_ = std::chrono::steady_clock::now();
        LOG_INFO() << "Таймер обновлен: " << leader_id;

        if (lsm_storage->context.state != State::Follower){
            lsm_storage->context.state = State::Follower;
        }

        if (request_body["prev_index"].As<int>(-1) > lsm_storage->operationLogs.lastLogIndex){
            return userver::formats::json::MakeObject(
                "append", false,
                "term", lsm_storage->context.term
            );
        }

        Operation newOperation;
        newOperation.key = request_body["operation_key"].As<std::string>("");
        newOperation.value = request_body["operation_value"].As<std::string>("");
        newOperation.operation_type = request_body["operation_type"].As<std::string>("");
        newOperation.term = request_body["operation_term"].As<int>(-1);
        newOperation.index = request_body["operation_index"].As<int>(-1);

        if (newOperation.term != -1) {
            int newOperationIndex = request_body["prev_index"].As<int>(-1) + 1;

            if (newOperationIndex <= lsm_storage->operationLogs.lastLogIndex && newOperation.term != lsm_storage->operationLogs.get(newOperationIndex).term){
                while (lsm_storage->operationLogs.operations.back().index >= newOperationIndex){
                    lsm_storage->operationLogs.operations.pop_back();
                }

                lsm_storage->operationLogs.lastLogIndex = lsm_storage->operationLogs.operations.back().index;
            }

            if (newOperationIndex <= lsm_storage->operationLogs.lastLogIndex){
                return userver::formats::json::MakeObject("term", lsm_storage->context.term, "append", true, "match_index", lsm_storage->operationLogs.lastLogIndex);
            }
            
            lsm_storage->operationLogs.operations.push_back(newOperation);
            lsm_storage->operationLogs.lastLogIndex = newOperationIndex;
            lsm_storage->operationLogs.lastLogTerm = newOperation.term;
        }

        if (request_body["commit_index"].As<int>(-1) > lsm_storage->operationLogs.commitIndex){
            std::deque<Operation> operations;
            for (auto iter {lsm_storage->operationLogs.operations.rbegin()}; iter != lsm_storage->operationLogs.operations.rend(); ++iter) {
                if ((*iter).index == lsm_storage->operationLogs.commitIndex){
                    break;
                }
                if ((*iter).index <= request_body["commit_index"].As<int>(-1)) {
                    operations.push_front(*iter);
                }
            }
            lsm_storage->commit_follower(operations);
        }

        if (request_body["prev_index"].As<int>(-1) == lsm_storage->operationLogs.lastLogIndex){
            return userver::formats::json::MakeObject("append", true, "term", lsm_storage->context.term, "match_index", request_body["prev_index"].As<int>(-1));
        }

        return userver::formats::json::MakeObject("append", true, "term", lsm_storage->context.term, "match_index", request_body["prev_index"].As<int>(-1) + 1);
    }

private:
    LSMStorage* lsm_storage;
};
