#include <vector>
#include <string>

enum class State { Follower, Candidate, Leader };

struct Peer {
    Peer(int id): node_id(id){}

    int node_id;
    int next_index = 0;
    int match_index = -1;
    bool vote_granted = false;
    std::string socket;
};


struct Peers {

    Peer get(int id){
        for (auto peer : peers){
            if (peer.node_id == id){
                return peer;
            }
        }

        throw "PeerId не найдено!";
    }

    void remove_peer(int id) {
        for (auto iter = peers.begin(); iter != peers.end(); iter++){
            if ((*iter).node_id == id){
                peers.erase(iter);
                return;
            }
        }
        throw "PeerId не найдено!";
    }

    std::vector<Peer> peers;
};


struct Context {
    Peers* peers = new Peers();
    State state = State::Follower;
    long term = 0;
    int voted_for = -1;
    int node_id;
};


struct Operation{
    std::string key;
    std::string value;
    std::string operation_type;
    int term = -1;
    int index = -1;
};


struct OperationsLog{
    Operation get(int index){
        for (auto iter {operations.rbegin()}; iter != operations.rend(); ++iter) {
            if ((*iter).index == index){
                return *iter;
            }
        }

        Operation oper;
        return oper;
    }

    std::vector<Operation> operations;
    int commitIndex = -1;
    int lastLogIndex = -1;
    int lastLogTerm = -1;
};