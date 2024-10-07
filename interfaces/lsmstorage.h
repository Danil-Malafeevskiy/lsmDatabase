#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include "memtable.h"


class LSMStorage {
public:
    // Вставка в MemTable (+ проверка на переполнение и запись в SSTable)
    void put(std::vector<std::pair<std::string, int>>& data);
    
    // Получение данных по ключу (Сначала в MemTable) (Сначала в новых файлах (по дате))
    std::pair<std::string, int> get(std::string& key);

    // Запись обновленного значения в MemTable (при Merge - перезапись) (+ проверка на переполнение и запись в SSTable)
    void update(std::string& key, int& newValue);

    // Удаление ключа из базы (Добавление в removeKeyStorage - при мердже удаление)
    void remove(std::string& key);

    // Если переполнение количества файлов, то самые старые мерджатся в один (поочередно записываясь, т.к. значения в файлах отсортированы) (првоерки на removeKeyStorage и update)
    void merge();
private:
    int max_files_count;
    MemTable memtable;
    std::vector<std::string> removeKeyStorage;
    std::vector<std::string> filenames;
};