#include <iostream>
#include <map>
#include <string>
#include <fstream>
#include <vector>
#include <variant>
#include <ctime>
#include <unistd.h>

const std::string TOMBSTONE = "__DELETED__";

using SimpleType = std::variant<int, float, std::string, double>;

class SSTable {
public:
    void write(std::map<std::string, SimpleType>  memTableData){
        this->filename = std::to_string(time(NULL));

        std::ofstream outFile(this->filename, std::ios::app);
        if (!outFile) {
            std::cerr << "Не удалось открыть файл для записи: " << this->filename << std::endl;
            return;
        }

        for (const auto& pair : memTableData) {
            outFile << pair.first << "=";
            
            std::visit([&outFile](const auto& value) {
                outFile << value;
            }, pair.second);
             
            outFile << "\n";
        }

        outFile.close();
        if (!outFile) {
            std::cerr << "Ошибка при записи файла: " << filename << std::endl;
        }
        sleep(1);
    };

    std::pair<bool, SimpleType> get(std::string& key) {
        std::ifstream file(this->filename);
        if (!file) {
            std::cerr << "Не удалось открыть файл.\n";
        }

        while (file.peek() != EOF) {
            std::string line;
            std::getline(file, line);

            std::pair<std::string, SimpleType> currentLine = this->getKeyValue(line);

            if (currentLine.first == key) {
                if (std::holds_alternative<std::string>(currentLine.second) && std::get<std::string>(currentLine.second) == TOMBSTONE) {
                    std::cerr <<  "Запрашиваемый ключ удален из базы!" << std::endl;
                }
                return std::pair<bool, SimpleType>{true, currentLine.second};
            }
        }
        return std::pair<bool, SimpleType>{false, "0"};

    };

    std::pair<std::string, SimpleType> getKeyValue(const std::string& line) {
        size_t pos = line.find('=');
        return {line.substr(0, pos), line.substr(pos+1)};
    }
    
private:
    std::string filename;
};
