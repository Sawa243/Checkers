#pragma once
#include <iostream>
#include <fstream>
#include <vector>

#include "../Models/Move.h"
#include "../Models/Project_path.h"

#ifdef __APPLE__
    #include <SDL2/SDL.h>
    #include <SDL2/SDL_image.h>
#else
    #include <SDL.h>
    #include <SDL_image.h>
#endif

using namespace std;

// Класс для работы с игровой доской
class Board
{
public:
    Board() = default;

    // Конструктор для создания доски с заданной шириной (W) и высотой (H)
    Board(const unsigned int W, const unsigned int H) : W(W), H(H)
    {
    }

    // Рисует начальное состояние доски (создаёт окно и загружает текстуры)
    int start_draw()
    {
        if (SDL_Init(SDL_INIT_EVERYTHING) != 0)  // Инициализация SDL
        {
            print_exception("SDL_Init can't init SDL2 lib");
            return 1;
        }
        if (W == 0 || H == 0)
        {
            SDL_DisplayMode dm;
            if (SDL_GetDesktopDisplayMode(0, &dm))
            {
                print_exception("SDL_GetDesktopDisplayMode can't get desktop display mode");
                return 1;
            }
            W = min(dm.w, dm.h);  // Устанавливаем размер окна в зависимости от разрешения экрана
            W -= W / 15;  // Уменьшаем размер для создания отступов
            H = W;  // Ширина и высота одинаковые для квадратной доски
        }

        // Создаём окно и рендерер для отрисовки
        win = SDL_CreateWindow("Checkers", 0, H / 30, W, H, SDL_WINDOW_RESIZABLE);
        if (win == nullptr)
        {
            print_exception("SDL_CreateWindow can't create window");
            return 1;
        }

        ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
        if (ren == nullptr)
        {
            print_exception("SDL_CreateRenderer can't create renderer");
            return 1;
        }

        // Загружаем текстуры для доски, фигур, кнопок и результата
        board = IMG_LoadTexture(ren, board_path.c_str());
        w_piece = IMG_LoadTexture(ren, piece_white_path.c_str());
        b_piece = IMG_LoadTexture(ren, piece_black_path.c_str());
        w_queen = IMG_LoadTexture(ren, queen_white_path.c_str());
        b_queen = IMG_LoadTexture(ren, queen_black_path.c_str());
        back = IMG_LoadTexture(ren, back_path.c_str());
        replay = IMG_LoadTexture(ren, replay_path.c_str());

        if (!board || !w_piece || !b_piece || !w_queen || !b_queen || !back || !replay)
        {
            print_exception("IMG_LoadTexture can't load main textures from " + textures_path);
            return 1;
        }

        SDL_GetRendererOutputSize(ren, &W, &H);  // Получаем фактические размеры окна
        make_start_mtx();  // Инициализируем начальную матрицу фигур
        rerender();  // Перерисовываем доску
        return 0;
    }

    // Перерисовывает доску, очищая её и сбрасывая все состояния
    void redraw()
    {
        game_results = -1;
        history_mtx.clear();
        history_beat_series.clear();
        make_start_mtx();  // Восстанавливаем начальное состояние
        clear_active();  // Убираем подсвеченную клетку
        clear_highlight();  // Убираем подсветку возможных ходов
    }

    // Выполняет перемещение фигуры на доске
    void move_piece(move_pos turn, const int beat_series = 0)
    {
        if (turn.xb != -1)  // Если фигура была побеждена
        {
            mtx[turn.xb][turn.yb] = 0;  // Убираем побеждённую фигуру с доски
        }
        move_piece(turn.x, turn.y, turn.x2, turn.y2, beat_series);  // Делаем ход
    }

    // Перемещает фигуру с одной клетки на другую
    void move_piece(const POS_T i, const POS_T j, const POS_T i2, const POS_T j2, const int beat_series = 0)
    {
        if (mtx[i2][j2])  // Если конечная клетка занята, выбрасываем ошибку
        {
            throw runtime_error("final position is not empty, can't move");
        }
        if (!mtx[i][j])  // Если начальная клетка пуста, выбрасываем ошибку
        {
            throw runtime_error("begin position is empty, can't move");
        }
        // Преобразуем фигуру в дамку, если она дошла до конца доски
        if ((mtx[i][j] == 1 && i2 == 0) || (mtx[i][j] == 2 && i2 == 7))
            mtx[i][j] += 2;  // Повышаем фигуру до дамки (3 для белых, 4 для чёрных)
        mtx[i2][j2] = mtx[i][j];  // Перемещаем фигуру
        drop_piece(i, j);  // Очищаем начальную клетку
        add_history(beat_series);  // Добавляем ход в историю
    }

    // Убирает фигуру с доски
    void drop_piece(const POS_T i, const POS_T j)
    {
        mtx[i][j] = 0;
        rerender();  // Перерисовываем доску
    }

    // Преобразует фигуру в дамку
    void turn_into_queen(const POS_T i, const POS_T j)
    {
        if (mtx[i][j] == 0 || mtx[i][j] > 2)  // Если клетка пуста или фигура уже дамка
        {
            throw runtime_error("can't turn into queen in this position");
        }
        mtx[i][j] += 2;  // Преобразуем фигуру в дамку
        rerender();  // Перерисовываем доску
    }

    // Возвращает текущую матрицу доски
    vector<vector<POS_T>> get_board() const
    {
        return mtx;
    }

    // Подсвечивает клетки, куда можно переместить фигуру
    void highlight_cells(vector<pair<POS_T, POS_T>> cells)
    {
        for (auto pos : cells)
        {
            POS_T x = pos.first, y = pos.second;
            is_highlighted_[x][y] = 1;  // Устанавливаем подсветку для клеток
        }
        rerender();  // Перерисовываем доску
    }

    // Очищает подсветку всех клеток
    void clear_highlight()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            is_highlighted_[i].assign(8, 0);  // Сбрасываем подсветку
        }
        rerender();  // Перерисовываем доску
    }

    // Устанавливает активную клетку (клетка, на которую можно переместить фигуру)
    void set_active(const POS_T x, const POS_T y)
    {
        active_x = x;
        active_y = y;
        rerender();  // Перерисовываем доску
    }

    // Очищает активную клетку
    void clear_active()
    {
        active_x = -1;
        active_y = -1;
        rerender();  // Перерисовываем доску
    }

    // Проверяет, подсвечена ли клетка
    bool is_highlighted(const POS_T x, const POS_T y)
    {
        return is_highlighted_[x][y];
    }

    // Откатывает последние ходы
    void rollback()
    {
        auto beat_series = max(1, *(history_beat_series.rbegin()));
        while (beat_series-- && history_mtx.size() > 1)
        {
            history_mtx.pop_back();  // Убираем последний ход
            history_beat_series.pop_back();
        }
        mtx = *(history_mtx.rbegin());  // Восстанавливаем доску
        clear_highlight();  // Очищаем подсветку
        clear_active();  // Очищаем активную клетку
    }

    // Показывает финальный результат игры
    void show_final(const int res)
    {
        game_results = res;
        rerender();  // Перерисовываем доску для отображения результата
    }

    // Функция для изменения размера окна, если он был изменён
    void reset_window_size()
    {
        SDL_GetRendererOutputSize(ren, &W, &H);  // Получаем новые размеры
        rerender();  // Перерисовываем доску
    }

    // Закрытие всех ресурсов SDL
    void quit()
    {
        SDL_DestroyTexture(board);
        SDL_DestroyTexture(w_piece);
        SDL_DestroyTexture(b_piece);
        SDL_DestroyTexture(w_queen);
        SDL_DestroyTexture(b_queen);
        SDL_DestroyTexture(back);
        SDL_DestroyTexture(replay);
        SDL_DestroyRenderer(ren);
        SDL_DestroyWindow(win);
        SDL_Quit();
    }

    // Деструктор, который закрывает все ресурсы при удалении объекта
    ~Board()
    {
        if (win)
            quit();
    }

private:
    // Добавление хода в историю
    void add_history(const int beat_series = 0)
    {
        history_mtx.push_back(mtx);  // Добавляем текущую матрицу в историю
        history_beat_series.push_back(beat_series);  // Добавляем серию побеждённых фигур
    }

    // Создание начальной матрицы для игры
    void make_start_mtx()
    {
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                mtx[i][j] = 0;  // Инициализируем клетку пустой
                if (i < 3 && (i + j) % 2 == 1)  // Расставляем чёрные фигуры на первых строках
                    mtx[i][j] = 2;
                if (i > 4 && (i + j) % 2 == 1)  // Расставляем белые фигуры на последних строках
                    mtx[i][j] = 1;
            }
        }
        add_history();  // Добавляем начальную матрицу в историю
    }

    // Функция для перерисовки всех текстур
    void rerender()
    {
        // Очистка экрана
        SDL_RenderClear(ren);
        SDL_RenderCopy(ren, board, NULL, NULL);

        // Отображение фигур
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!mtx[i][j])  // Если клетка пустая, пропускаем
                    continue;

                // Вычисляем позицию фигуры на экране
                int wpos = W * (j + 1) / 10 + W / 120;
                int hpos = H * (i + 1) / 10 + H / 120;
                SDL_Rect rect{ wpos, hpos, W / 12, H / 12 };

                SDL_Texture* piece_texture;
                // Выбираем текстуру в зависимости от типа фигуры
                if (mtx[i][j] == 1)
                    piece_texture = w_piece;
                else if (mtx[i][j] == 2)
                    piece_texture = b_piece;
                else if (mtx[i][j] == 3)
                    piece_texture = w_queen;
                else
                    piece_texture = b_queen;

                SDL_RenderCopy(ren, piece_texture, NULL, &rect);  // Отображаем фигуру
            }
        }

        // Отображение подсветки
        SDL_SetRenderDrawColor(ren, 0, 255, 0, 0);
        const double scale = 2.5;
        SDL_RenderSetScale(ren, scale, scale);
        for (POS_T i = 0; i < 8; ++i)
        {
            for (POS_T j = 0; j < 8; ++j)
            {
                if (!is_highlighted_[i][j])
                    continue;
                SDL_Rect cell{ int(W * (j + 1) / 10 / scale), int(H * (i + 1) / 10 / scale), int(W / 10 / scale),
                              int(H / 10 / scale) };
                SDL_RenderDrawRect(ren, &cell);
            }
        }

        // Отображение активной клетки
        if (active_x != -1)
        {
            SDL_SetRenderDrawColor(ren, 255, 0, 0, 0);
            SDL_Rect active_cell{ int(W * (active_y + 1) / 10 / scale), int(H * (active_x + 1) / 10 / scale),
                                 int(W / 10 / scale), int(H / 10 / scale) };
            SDL_RenderDrawRect(ren, &active_cell);
        }
        SDL_RenderSetScale(ren, 1, 1);

        // Отображение кнопок
        SDL_Rect rect_left{ W / 40, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, back, NULL, &rect_left);
        SDL_Rect replay_rect{ W * 109 / 120, H / 40, W / 15, H / 15 };
        SDL_RenderCopy(ren, replay, NULL, &replay_rect);

        // Отображение результата игры
        if (game_results != -1)
        {
            string result_path = draw_path;
            if (game_results == 1)
                result_path = white_path;
            else if (game_results == 2)
                result_path = black_path;

            SDL_Texture* result_texture = IMG_LoadTexture(ren, result_path.c_str());
            if (result_texture == nullptr)
            {
                print_exception("IMG_LoadTexture can't load game result picture from " + result_path);
                return;
            }

            SDL_Rect res_rect{ W / 5, H * 3 / 10, W * 3 / 5, H * 2 / 5 };
            SDL_RenderCopy(ren, result_texture, NULL, &res_rect);
            SDL_DestroyTexture(result_texture);
        }

        SDL_RenderPresent(ren);
        // Ожидаем перед следующими действиями
        SDL_Delay(10);
        SDL_Event windowEvent;
        SDL_PollEvent(&windowEvent);
    }

    // Запись ошибки в лог
    void print_exception(const string& text) {
        ofstream fout(project_path + "log.txt", ios_base::app);
        fout << "Error: " << text << ". "<< SDL_GetError() << endl;
        fout.close();
    }

public:
    int W = 0;  // Ширина окна
    int H = 0;  // Высота окна

    // История состояний доски
    vector<vector<vector<POS_T>>> history_mtx;

private:
    SDL_Window *win = nullptr;  // Окно
    SDL_Renderer *ren = nullptr;  // Рендерер
    // Текстуры для доски, фигур и кнопок
    SDL_Texture *board = nullptr;
    SDL_Texture *w_piece = nullptr;
    SDL_Texture *b_piece = nullptr;
    SDL_Texture *w_queen = nullptr;
    SDL_Texture *b_queen = nullptr;
    SDL_Texture *back = nullptr;
    SDL_Texture *replay = nullptr;

    // Пути к изображениям для текстур
    const string textures_path = project_path + "Textures/";
    const string board_path = textures_path + "board.png";
    const string piece_white_path = textures_path + "piece_white.png";
    const string piece_black_path = textures_path + "piece_black.png";
    const string queen_white_path = textures_path + "queen_white.png";
    const string queen_black_path = textures_path + "queen_black.png";
    const string white_path = textures_path + "white_wins.png";
    const string black_path = textures_path + "black_wins.png";
    const string draw_path = textures_path + "draw.png";
    const string back_path = textures_path + "back.png";
    const string replay_path = textures_path + "replay.png";

    // Координаты активной клетки
    int active_x = -1, active_y = -1;
    // Результат игры
    int game_results = -1;
    // Матрица подсвеченных клеток
    vector<vector<bool>> is_highlighted_ = vector<vector<bool>>(8, vector<bool>(8, 0));
    // Матрица для хранения состояния доски (фигуры, пустые клетки)
    vector<vector<POS_T>> mtx = vector<vector<POS_T>>(8, vector<POS_T>(8, 0));
    // История серии побеждённых фигур
    vector<int> history_beat_series;
};
