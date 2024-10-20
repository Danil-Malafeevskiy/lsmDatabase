#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include "sstable.cpp"


class MemTable {
public:
    void put(const std::vector<std::pair<std::string, SimpleType>>& data) {
        for (const auto&[key, value] : data) {
            this->memTable[key] = value;
        }
    }

    std::pair<bool, SimpleType> get(std::string& key) {
        auto valueIt = this->memTable.find(key);

        if (valueIt != this->memTable.end()) {
            SimpleType value = (*valueIt).second;

            if (std::holds_alternative<std::string>(value) && std::get<std::string>((*valueIt).second) == TOMBSTONE){
                std::cerr <<  "Запрашиваемый ключ удален из базы!" << std::endl;
            }

            return std::pair<bool, SimpleType>{true, value};
        } else {
            return std::pair<bool, SimpleType>{false, "0"};
        }
    };

    bool check_overflow(){
        return this->memTable.size() >= this->max_data_count;
    }

    SSTable flush(){
        SSTable sstable;
        sstable.write(this->memTable);

        this->memTable.clear();

        return sstable;
    };

private:
    std::map<std::string, SimpleType> memTable;
    int max_data_count = 1000;
};
