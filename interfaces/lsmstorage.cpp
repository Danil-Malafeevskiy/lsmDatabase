#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include "memtable.cpp"


class LSMStorage {
public:
    void put(const std::vector<std::pair<std::string, SimpleType>>& data) {
        this->memtable.put(data);
        bool check_result = this->memtable.check_overflow();
        if (check_result) this->sstables.push_back(this->memtable.flush());
    }
    
    std::pair<std::string, SimpleType> get(std::string key) {
        std::pair<bool, SimpleType> memResult = this->memtable.get(key);
        if (memResult.first) return std::pair<std::string, SimpleType>{key, memResult.second};

        for (auto iter {this->sstables.rbegin()}; iter != this->sstables.rend(); ++iter) {
            std::pair<bool, SimpleType> ssResult = (*iter).get(key);
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
    
private:
    MemTable memtable;
    std::vector<SSTable> sstables;
};

int main(){
    LSMStorage lsm;
    std::vector<std::pair<std::string, SimpleType>> data{{"data1", 1}, {"data2", 66555}, {"data3", 25}, {"data4", 125}, {"data5", 625}, {"data6", 1}, {"data7", 5}, {"data8", 25}, {"data9", 125}, {"data10", 62225}, {"data11", 1}, {"data12", 5}, {"data13", 25}, {"data14", 125}, {"data15", 625}};

    lsm.put(data);
    lsm.remove("data3");

    lsm.update("data4", "string");

    std::pair<std::string, SimpleType> result = lsm.get("data3");
    std::pair<std::string, SimpleType> result2 = lsm.get("data4");

    std::cout << std::get<std::string>(result.second) << std::endl;
    std::cout << std::get<std::string>(result2.second) << std::endl;
}