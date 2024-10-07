#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>

class MemTable {
public:
    // Вставка в RAM
    void put(std::vector<std::pair<std::string, int>>& data);

    // Поиск и получение из RAM по ключу
    void get(std::string& key);

    // Отправка на запись на диск
    void flush();

private:
    std::map<std::string, int> memStorage;
    int max_data_count;
};