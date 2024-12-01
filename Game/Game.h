#pragma once
#include <chrono>
#include <thread>

#include "../Models/Project_path.h"
#include "Board.h"
#include "Config.h"
#include "Hand.h"
#include "Logic.h"

// Класс для управления игрой в шашки
class Game
{
  public:
    // Конструктор класса Game
    // Инициализирует объекты board, hand и logic с настройками из конфигурации
    Game() : board(config("WindowSize", "Width"), config("WindowSize", "Hight")), hand(&board), logic(&board, &config)
    {
        ofstream fout(project_path + "log.txt", ios_base::trunc);  // Создаёт новый файл для логирования
        fout.close();  // Закрывает файл сразу после создания
    }

    // Основная функция для начала игры
    // В этой функции запускается игровой цикл, обрабатываются ходы игроков и бота
    int play()
    {
        auto start = chrono::steady_clock::now();  // Засекаем время начала игры

        // Если игра начинается заново (replay), перезагружаем конфигурацию и перерисовываем доску
        if (is_replay)
        {
            logic = Logic(&board, &config);  // Перезапускаем логику игры
            config.reload();  // Перезагружаем настройки
            board.redraw();  // Перерисовываем игровую доску
        }
        else
        {
            board.start_draw();  // Инициализируем доску для новой игры
        }
        is_replay = false;

        int turn_num = -1;  // Номер хода
        bool is_quit = false;  // Флаг выхода из игры
        const int Max_turns = config("Game", "MaxNumTurns");  // Максимальное количество ходов, заданное в конфигурации

        // Игровой цикл
        while (++turn_num < Max_turns)
        {
            beat_series = 0;  // Сброс серии побеждённых фигур
            logic.find_turns(turn_num % 2);  // Находим доступные ходы для текущего игрока (0 — белые, 1 — чёрные)
            if (logic.turns.empty())  // Если нет доступных ходов, игра заканчивается
                break;

            // Настройка уровня сложности бота в зависимости от цвета
            logic.Max_depth = config("Bot", string((turn_num % 2) ? "Black" : "White") + string("BotLevel"));
            if (!config("Bot", string("Is") + string((turn_num % 2) ? "Black" : "White") + string("Bot")))  // Если это не бот
            {
                auto resp = player_turn(turn_num % 2);  // Ход игрока
                if (resp == Response::QUIT)  // Если игрок решил выйти
                {
                    is_quit = true;
                    break;
                }
                else if (resp == Response::REPLAY)  // Если игрок хочет перезапустить игру
                {
                    is_replay = true;
                    break;
                }
                else if (resp == Response::BACK)  // Если игрок хочет откатить ход
                {
                    if (config("Bot", string("Is") + string((1 - turn_num % 2) ? "Black" : "White") + string("Bot")) &&
                        !beat_series && board.history_mtx.size() > 2)
                    {
                        board.rollback();  // Откат хода
                        --turn_num;
                    }
                    if (!beat_series)  // Если не было побеждённых фигур
                        --turn_num;

                    board.rollback();  // Откат хода
                    --turn_num;
                    beat_series = 0;
                }
            }
            else  // Если ходит бот
                bot_turn(turn_num % 2);
        }

        auto end = chrono::steady_clock::now();  // Засекаем время окончания игры
        ofstream fout(project_path + "log.txt", ios_base::app);  // Открываем лог-файл для записи данных
        fout << "Game time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";  // Логируем время игры
        fout.close();

        if (is_replay)  // Если игра перезапускается
            return play();
        if (is_quit)  // Если игрок вышел из игры
            return 0;

        int res = 2;  // Изначально считаем ничью
        if (turn_num == Max_turns)  // Если превысили максимальное количество ходов
        {
            res = 0;
        }
        else if (turn_num % 2)  // Если ходят чёрные, то они выигрывают
        {
            res = 1;
        }
        board.show_final(res);  // Показываем результат игры
        auto resp = hand.wait();  // Ожидаем ввод от игрока (например, перезапуск игры)
        if (resp == Response::REPLAY)  // Если игрок хочет перезапустить
        {
            is_replay = true;
            return play();
        }
        return res;  // Возвращаем результат игры
    }

  private:
    // Функция для хода бота
    void bot_turn(const bool color)
    {
        auto start = chrono::steady_clock::now();  // Засекаем время хода бота

        auto delay_ms = config("Bot", "BotDelayMS");  // Задержка между ходами бота (если есть)
        thread th(SDL_Delay, delay_ms);  // Поток для задержки, чтобы не блокировать основной процесс
        auto turns = logic.find_best_turns(color);  // Находим лучший ход для бота
        th.join();  // Ждём окончания потока

        bool is_first = true;  // Флаг для первого хода
        // Выполнение ходов бота
        for (auto turn : turns)
        {
            if (!is_first)
            {
                SDL_Delay(delay_ms);  // Задержка между ходами
            }
            is_first = false;
            beat_series += (turn.xb != -1);  // Увеличиваем серию побеждённых фигур
            board.move_piece(turn, beat_series);  // Выполняем ход
        }

        auto end = chrono::steady_clock::now();  // Засекаем время окончания хода бота
        ofstream fout(project_path + "log.txt", ios_base::app);  // Логируем время хода
        fout << "Bot turn time: " << (int)chrono::duration<double, milli>(end - start).count() << " millisec\n";
        fout.close();
    }

    // Функция для хода игрока
    Response player_turn(const bool color)
    {
        // Возвращает 1, если игрок решил выйти
        vector<pair<POS_T, POS_T>> cells;
        for (auto turn : logic.turns)
        {
            cells.emplace_back(turn.x, turn.y);  // Сохраняем все возможные клетки для хода
        }
        board.highlight_cells(cells);  // Подсвечиваем доступные клетки
        move_pos pos = {-1, -1, -1, -1};
        POS_T x = -1, y = -1;

        // Попытка совершить первый ход
        while (true)
        {
            auto resp = hand.get_cell();  // Ожидаем ввод от игрока
            if (get<0>(resp) != Response::CELL)
                return get<0>(resp);
            pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};

            bool is_correct = false;
            for (auto turn : logic.turns)
            {
                if (turn.x == cell.first && turn.y == cell.second)
                {
                    is_correct = true;
                    break;
                }
                if (turn == move_pos{x, y, cell.first, cell.second})
                {
                    pos = turn;
                    break;
                }
            }
            if (pos.x != -1)
                break;  // Если ход правильный, выходим из цикла
            if (!is_correct)
            {
                if (x != -1)
                {
                    board.clear_active();
                    board.clear_highlight();
                    board.highlight_cells(cells);  // Если ход неправильный, очищаем подсветку и повторяем
                }
                x = -1;
                y = -1;
                continue;
            }
            x = cell.first;
            y = cell.second;
            board.clear_highlight();
            board.set_active(x, y);  // Подсвечиваем текущую клетку
            vector<pair<POS_T, POS_T>> cells2;
            for (auto turn : logic.turns)
            {
                if (turn.x == x && turn.y == y)
                {
                    cells2.emplace_back(turn.x2, turn.y2);  // Подсвечиваем клетки, куда можно переместить фигуру
                }
            }
            board.highlight_cells(cells2);
        }
        board.clear_highlight();
        board.clear_active();
        board.move_piece(pos, pos.xb != -1);  // Перемещаем фигуру

        if (pos.xb == -1)
            return Response::OK;  // Если ход не завершился побеждением, возвращаем OK

        // Если побеждена фигура, продолжаем серию ударов
        beat_series = 1;
        while (true)
        {
            logic.find_turns(pos.x2, pos.y2);  // Ищем доступные ходы после удара
            if (!logic.have_beats)  // Если ударов больше нет, выходим
                break;

            vector<pair<POS_T, POS_T>> cells;
            for (auto turn : logic.turns)
            {
                cells.emplace_back(turn.x2, turn.y2);  // Подсвечиваем клетки для следующего удара
            }
            board.highlight_cells(cells);
            board.set_active(pos.x2, pos.y2);
            // Повторяем попытку удара
            while (true)
            {
                auto resp = hand.get_cell();
                if (get<0>(resp) != Response::CELL)
                    return get<0>(resp);
                pair<POS_T, POS_T> cell{get<1>(resp), get<2>(resp)};
                
                bool is_correct = false;
                for (auto turn : logic.turns)
                {
                    if (turn.x2 == cell.first && turn.y2 == cell.second)
                    {
                        is_correct = true;
                        pos = turn;
                        break;
                    }
                }
                if (!is_correct)
                    continue;

                board.clear_highlight();
                board.clear_active();
                beat_series += 1;
                board.move_piece(pos, beat_series);  // Выполняем следующий удар
                break;
            }
        }

        return Response::OK;  // Завершаем ход игрока
    }

  private:
    Config config;  // Объект конфигурации для получения настроек
    Board board;  // Игровая доска
    Hand hand;  // Объект для взаимодействия с игроком (например, для ввода хода)
    Logic logic;  // Логика игры (поиск ходов, определение побед)
    int beat_series;  // Счётчик ударов
    bool is_replay = false;  // Флаг перезапуска игры
};
