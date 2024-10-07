#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>


class SSTable {
public:
    // Запись в файл (имя файла - время (int))
    void write();

    // Поиск в файле
    void get(std::string& key, std::string& filename);
private:
    std::map<std::string, int> memLocalStorage;
};
