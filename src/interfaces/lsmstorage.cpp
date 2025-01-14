#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <filesystem>
#include <vector>
#include <thread>
#include <atomic>
#include <mutex>
#include <deque>
#include <condition_variable>
#include <userver/components/component_base.hpp>
#include <userver/components/component_list.hpp>
#include <userver/utest/using_namespace_userver.hpp>
#include <userver/logging/log.hpp>
#include "../raft/context.cpp"
#include "memtable.cpp"
#include <userver/clients/http/client.hpp>
#include <userver/clients/http/request.hpp>
#include <userver/clients/http/response.hpp>
#include <userver/formats/json.hpp>
#include <userver/components/component_context.hpp>
#include <userver/clients/http/component.hpp>
#include <userver/yaml_config/schema.hpp>
#include <userver/yaml_config/merge_schemas.hpp>
#include <userver/engine/task/task.hpp>
#include <userver/engine/async.hpp>
#include <userver/engine/sleep.hpp>


namespace fs = std::filesystem;


class LSMStorage : public components::ComponentBase {
public:

    LSMStorage(const components::ComponentConfig& config,
                const components::ComponentContext& context) : ComponentBase(config, context), http_client_(&context.FindComponent<components::HttpClient>().GetHttpClient()) {

        auto peers = config["peers"].As<std::vector<std::string>>();
        auto peerNodeIds = config["peerNodeIds"].As<std::vector<int>>();
        auto nodeId = config["nodeId"].As<int>();
        
        this->context.node_id = nodeId;

        for (size_t i = 0; i < peers.size(); i++) {
            Peer peer(peerNodeIds[i]);
            peer.socket = peers[i];

            this->context.peers->peers.push_back(peer);
        }

        auto dataPath = config["dataPath"].As<std::string>();
        dataDir = dataPath;
        
        std::pair<int, int> dataLogs = readLogInfo();

        operationLogs.commitIndex = dataLogs.first;
        operationLogs.lastLogIndex = dataLogs.first;
        operationLogs.lastLogTerm = dataLogs.second;

        auto socket = config["socket"].As<std::string>();
        for (const auto& peer : this->context.peers->peers){
            sendAddNodeRequest(peer.socket, socket, nodeId, operationLogs.commitIndex);
        }

        fs::path directoryPath = "../data/" + dataPath;
        std::vector<std::string> fileNames;

        for (const auto& entry : fs::directory_iterator(directoryPath)) {
            if (fs::is_regular_file(entry.status())) {
                if (entry.path().filename().string() != "index"){
                    fileNames.push_back(entry.path().filename().string());
                }
            }
        }

        std::sort(fileNames.begin(), fileNames.end());
        
        for (auto filename : fileNames){
            LOG_INFO() << filename;
            sstables.push_back(SSTable(filename));
        };

        wait_time = config["wait-time"].As<int>();

        background_task_ = engine::AsyncNoSpan(context.GetTaskProcessor("lsm-task-processor"), [this] {
            runBackgroundTasks();
        });
    };

    static constexpr std::string_view kName = "LSMStorage";
    
    static yaml_config::Schema GetStaticConfigSchema() {
        return yaml_config::MergeSchemas<components::ComponentBase>(R"(
    type: object
    description: |
      Component for accepting incoming TCP connections and responding with some
      greeting as long as the client sends 'hi'.
    additionalProperties: false
    properties:
      peers:
          type: array
          items:
            type: string
            description: peer of cluster
          description: peers of cluster
          defaultDescription: hi
      nodeId:
          type: integer
          description: nodeId in cluster
          defaultDescription: hi
      peerNodeIds:
          type: array
          items:
            type: integer
            description: peer id of cluster
          description: peer ids of cluster
          defaultDescription: hi
      dataPath:
          type: string
          description: dataPath for node in cluster
          defaultDescription: hi
      lsm-task-processor:
          type: string
          description: Name of the TaskProcessor for background tasks
          defaultDescription: main-task-processor
      wait-time:
          type: integer
          description: wait-time in cluster in milliseconds
          defaultDescription: hi
      socket:
          type: string
          description: socket for node in cluster
          defaultDescription: hi
  )");
    }

    void put(const std::vector<std::pair<std::string, SimpleType>>& data) {
        this->memtable.put(data);
        bool check_result = this->memtable.check_overflow();
        if (check_result) {
            this->sstables.push_back(this->memtable.flush(dataDir));
            writeLogInfo(false);
        }
    }
    
    std::pair<std::string, SimpleType> get(std::string key) {
        std::pair<bool, SimpleType> memResult = this->memtable.get(key);
        if (memResult.first) return std::pair<std::string, SimpleType>{key, memResult.second};

        for (auto iter {this->sstables.rbegin()}; iter != this->sstables.rend(); ++iter) {
            std::pair<bool, SimpleType> ssResult = (*iter).get(key, dataDir);
            if (ssResult.first) return std::pair<std::string, SimpleType>{key, ssResult.second};
        }

        std::cerr << "Запрашиваемый ключ не найден" << std::endl;
        return std::pair<std::string, SimpleType>{key, "not found"};
    };

    void update(std::string key, SimpleType newValue) {
        this->put(std::vector<std::pair<std::string, SimpleType>>{{key, newValue}});
    };

    void remove(std::string key) {
        this->put(std::vector<std::pair<std::string, SimpleType>>{{key, TOMBSTONE}});
    };

    void add_operations(std::string key, std::string value, std::string type){
        Operation operation;
        operation.key = key;
        operation.value = value;
        operation.operation_type = type;
        operation.term = context.term;
        operation.index = operationLogs.lastLogIndex + 1;

        operationLogs.lastLogIndex++;
        operationLogs.lastLogTerm = context.term;
        operationLogs.operations.push_back(operation);
    }

    void commit_follower(std::deque<Operation> data_operations){
        while (data_operations.size() != 0){
            apply_operations(data_operations.front());
            data_operations.pop_front();
            operationLogs.commitIndex++;
        }
    }

    ~LSMStorage() {
        LOG_INFO() << "LSMStorage is being destroyed";

        stop_.store(true);
        background_task_.Wait();

        for (const auto& peer : this->context.peers->peers){
            sendRemoveNodeRequest(peer.socket, context.node_id);
        }

        FlushMemTableOnStopServer();
        writeLogInfo(true);
    };

    Context context;
    OperationsLog operationLogs;
    std::chrono::steady_clock::time_point last_heartbeat_time_;
    
private:

    void FlushMemTableOnStopServer() {
        LOG_INFO() << "Flushing MemTable to SSTable...";
        if (memtable.size() > 0) {
            memtable.flush(dataDir);
        }
    };

    std::pair<int, int> readLogInfo(){
        std::ifstream file("../data/" + dataDir + "/index");
        if (!file) {
            std::cerr << "Не удалось открыть файл.\n";
        }

        while (file.peek() != EOF) {
            std::string line;
            std::getline(file, line);
            size_t pos = line.find(' ');
            if (pos != std::string::npos) {
                int first = std::stoi(line.substr(0, pos));
                int second = std::stoi(line.substr(pos + 1));
                return {first, second};
            }
        }

        return {-1, -1};
    }

    void writeLogInfo(bool force){
        std::ofstream outFile("../data/" + dataDir + "/index", std::ios::out);
        if (!outFile) {
            std::cerr << "Не удалось открыть файл для записи: " << "index" << std::endl;
            return;
        }

        if (force){
            outFile << operationLogs.commitIndex << " " << operationLogs.get(operationLogs.commitIndex).term;
        } else {
            outFile << operationLogs.commitIndex + 1 << " " << operationLogs.get(operationLogs.commitIndex + 1).term;
        }
        
    }

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

    void sendAddNodeRequest(std::string targetSocket, std::string socket, int node_id, int last_log_index){
        userver::formats::json::Value payload = userver::formats::json::MakeObject(
            "socket", socket,
            "node_id", node_id,
            "match_index", last_log_index
        );

        LOG_INFO() << "Отправляем запрос на добаление ноды в кластер";

        std::optional<userver::formats::json::Value> response = send(targetSocket, "add-node", payload);

        LOG_INFO() << response;
    }

    void sendRemoveNodeRequest(std::string targetSocket, int node_id){
        userver::formats::json::Value payload = userver::formats::json::MakeObject(
            "node_id", node_id
        );

        LOG_INFO() << "Отправляем запрос на удаление ноды из кластера";

        std::optional<userver::formats::json::Value> response = send(targetSocket, "remove-node", payload);

        LOG_INFO() << response;
    }

    std::optional<userver::formats::json::Value> sendVoteRequest(std::string targetNodeSocket, int term) {
        userver::formats::json::Value payload = userver::formats::json::MakeObject(
            "term", term,
            "candidate_id", context.node_id,
            "last_log_index", operationLogs.lastLogIndex,
            "last_log_term", operationLogs.lastLogTerm
        );

        LOG_INFO() << "Отправляем запрос на голосование узлу " << targetNodeSocket;

        std::optional<userver::formats::json::Value> response = send(targetNodeSocket, "vote", payload);

        LOG_INFO() << response;

        return response;
    };

    void runBackgroundTasks() {
        last_heartbeat_time_ = std::chrono::steady_clock::now();
        while (!stop_.load()) {
            if (context.state == State::Leader) {
                sendHeartbeats();
            } else if (context.state == State::Follower && 
                       std::chrono::steady_clock::now() - last_heartbeat_time_ > std::chrono::milliseconds(wait_time)) {
                startElection();
            }

            userver::engine::SleepFor(std::chrono::milliseconds(1000));
        }
    }

    void apply_operations(Operation operation){
        if (operation.operation_type == "delete") {
            remove(operation.key);
        } else if (operation.operation_type == "update") {
            std::cout << operation.key << " " << operation.value << std::endl;
            update(operation.key, operation.value);
        } else {
            put({std::make_pair(operation.key, operation.value)});
        }
    }

    void commit_leader(){
        int next_commit_index = operationLogs.commitIndex + 1;
        while (true){
            size_t log_count = 1;
            for (const auto& peer : context.peers->peers){
                if (peer.match_index >= next_commit_index) {
                    log_count++;
                }
            }

            if (log_count >= (context.peers->peers.size()+1)/2 + 1) {
                apply_operations(operationLogs.get(next_commit_index));
                operationLogs.commitIndex++;
                next_commit_index++;
            }
            else {
                return;
            }
        }
    }
    
    void sendHeartbeats() {
        for (auto& peer : context.peers->peers) {
            Operation operation;
            int prevIndex;

            if (peer.next_index <= operationLogs.lastLogIndex) {
                operation = operationLogs.get(peer.next_index);
                prevIndex = peer.next_index - 1;
            } else {
                prevIndex = operationLogs.lastLogIndex;
            }

            userver::formats::json::Value payload = userver::formats::json::MakeObject(
                "term", context.term,
                "leader_id", context.node_id,
                "prev_index", prevIndex,
                "term_prev_index", operationLogs.lastLogTerm,
                "commit_index", operationLogs.commitIndex,
                "operation_key", operation.key,
                "operation_value", operation.value,
                "operation_type", operation.operation_type,
                "operation_term", operation.term,
                "operation_index", operation.index
            );

            std::optional<userver::formats::json::Value> answer = send(peer.socket, "heartbeat", payload);

            LOG_INFO() << answer;

            if (answer.has_value()) {
                auto json_response = answer.value();

                if (json_response["term"].As<int>() > context.term) { 
                    context.term = json_response["term"].As<int>();
                    context.state = State::Follower;
                    context.voted_for = -1;

                    return;
                }

                if (json_response["append"].As<bool>() == true){
                    peer.match_index = json_response["match_index"].As<int>();
                    peer.next_index = json_response["match_index"].As<int>() + 1;
                }
                else {
                    peer.next_index--;
                }
            }
        }
        commit_leader();
    }

    void startElection() {
        context.state = State::Candidate;
        long term = ++context.term;
        context.voted_for = context.node_id;
        size_t voteGrantedCount = 1;
        size_t voteRevokedCount = 0;

        LOG_INFO() << "Старт выборов. Термин: " << term;

        size_t node_count = context.peers->peers.size();
        std::vector<Peer> not_response;

        while (node_count != 0){
            if (context.state == State::Follower) {
                return;
            }

            std::vector<Peer> peers;
            if (node_count == context.peers->peers.size()){
                peers = context.peers->peers;
            } else {
                peers = not_response;
            }
            
            for (auto peer : peers) {
                auto response = sendVoteRequest(peer.socket, term);
                
                if (response.has_value()) {
                    auto json_response = response.value();

                    LOG_INFO() << json_response;
                    
                    if (json_response["term"].As<int>() > term) { 
                        context.term = json_response["term"].As<int>();
                        context.state = State::Follower;

                        LOG_INFO() << "Я остаюсь фолловером!";

                        return;
                    }
                    if (json_response["voteGranted"].As<bool>() == true) {
                        LOG_INFO() << "Получен положительный голос от узла " << peer.socket;

                        peer.vote_granted = true;
                        voteGrantedCount++;
                    }
                    else {
                        LOG_INFO() << "Получен отрицательный голос от узла " << peer.socket;
                        voteRevokedCount++;
                    }
                    node_count--;
                } else {
                    not_response.push_back(peer);
                    continue;
                }
            }

            if (voteGrantedCount >= ((context.peers->peers.size()+1)/2 + 1)){
                context.state = State::Leader;

                LOG_INFO() << "Я выбран лидером!";

                for (auto& peer : context.peers->peers){
                    peer.next_index = operationLogs.lastLogIndex + 1;
                    peer.match_index = operationLogs.lastLogIndex;
                }
                return;
            } else if (voteRevokedCount >= ((context.peers->peers.size()+1)/2 + 1)){
                context.state = State::Follower;

                LOG_INFO() << "Я остаюсь фолловером!";

                return;
            }
        }
    };

    std::string dataDir;
    engine::Task background_task_;
    std::atomic<bool> stop_;
    userver::clients::http::Client* http_client_;
    MemTable memtable;
    std::vector<SSTable> sstables;
    int wait_time;
};
