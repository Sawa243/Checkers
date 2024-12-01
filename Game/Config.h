#pragma once
#include <fstream>
#include <nlohmann/json.hpp>
using json = nlohmann::json;

#include "../Models/Project_path.h"

// Класс для работы с настройками игры, загружаемыми из JSON-файла
class Config
{
  public:
    // Конструктор класса, который сразу вызывает функцию reload для загрузки настроек
    Config()
    {
        reload();
    }

    // Функция для перезагрузки конфигурации из файла settings.json
    // Открывает файл, читает JSON-данные и сохраняет их в объект config
    void reload()
    {
        // Открываем файл настроек
        std::ifstream fin(project_path + "settings.json");
        fin >> config;   // Загружаем содержимое файла в объект config
        fin.close();     // Закрываем файл
    }

    // Оператор вызова для доступа к данным конфигурации
    // Позволяет обратиться к настройке по пути и имени
    auto operator()(const string &setting_dir, const string &setting_name) const
    {
        return config[setting_dir][setting_name];  // Возвращаем значение, найденное в JSON
    }

  private:
    json config;  // Объект для хранения данных конфигурации в формате JSON
};
