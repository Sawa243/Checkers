#pragma once
#include <tuple>

#include "../Models/Move.h"
#include "../Models/Response.h"
#include "Board.h"

// Класс для обработки взаимодействия с игроком (ввод хода)
class Hand
{
  public:
    // Конструктор класса Hand. Инициализирует объект доски, с которой будет происходить взаимодействие
    Hand(Board *board) : board(board)
    {
    }

    // Метод для получения координат клетки, по которой кликнул игрок
    // Возвращает кортеж: тип ответа, координаты x и y выбранной клетки
    tuple<Response, POS_T, POS_T> get_cell() const
    {
        SDL_Event windowEvent;  // События SDL
        Response resp = Response::OK;  // Изначально предполагаем, что все действия корректны
        int x = -1, y = -1;  // Координаты курсора
        int xc = -1, yc = -1;  // Индексы клетки на доске (по оси X и Y)

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))  // Проверяем все события SDL
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:  // Если игрок закрыл окно, завершаем игру
                    resp = Response::QUIT;
                    break;
                case SDL_MOUSEBUTTONDOWN:  // Если произошёл клик мыши
                    x = windowEvent.motion.x;  // Получаем координаты клика по экрану
                    y = windowEvent.motion.y;
                    // Переводим экранные координаты в индексы клетки на доске
                    xc = int(y / (board->H / 10) - 1);
                    yc = int(x / (board->W / 10) - 1);

                    // Обрабатываем различные зоны доски
                    if (xc == -1 && yc == -1 && board->history_mtx.size() > 1)  // Если кликнули за пределами доски
                    {
                        resp = Response::BACK;  // Игрок хочет откатить ход
                    }
                    else if (xc == -1 && yc == 8)  // Если кликнули на кнопку перезапуска
                    {
                        resp = Response::REPLAY;  // Игрок хочет перезапустить игру
                    }
                    else if (xc >= 0 && xc < 8 && yc >= 0 && yc < 8)  // Если кликнули на клетку доски
                    {
                        resp = Response::CELL;  // Игрок выбрал клетку для хода
                    }
                    else
                    {
                        xc = -1;
                        yc = -1;  // Если кликнули за пределами допустимой зоны
                    }
                    break;
                case SDL_WINDOWEVENT:
                    if (windowEvent.window.event == SDL_WINDOWEVENT_SIZE_CHANGED)  // Если изменён размер окна
                    {
                        board->reset_window_size();  // Перерисовываем доску
                        break;
                    }
                }
                if (resp != Response::OK)  // Если получен ответ (например, выход или откат), выходим из цикла
                    break;
            }
        }
        return {resp, xc, yc};  // Возвращаем результат: тип ответа и координаты клетки
    }

    // Метод, который ожидает действий игрока (например, клик по кнопке)
    // Возвращает тип ответа (QUIT, REPLAY, и т.д.)
    Response wait() const
    {
        SDL_Event windowEvent;  // События SDL
        Response resp = Response::OK;  // Изначально предполагаем, что всё в порядке

        while (true)
        {
            if (SDL_PollEvent(&windowEvent))  // Проверяем все события SDL
            {
                switch (windowEvent.type)
                {
                case SDL_QUIT:  // Если игрок закрыл окно
                    resp = Response::QUIT;
                    break;
                case SDL_WINDOWEVENT_SIZE_CHANGED:  // Если изменён размер окна
                    board->reset_window_size();  // Перерисовываем доску
                    break;
                case SDL_MOUSEBUTTONDOWN:  // Если произошёл клик мыши
                    int x = windowEvent.motion.x;
                    int y = windowEvent.motion.y;
                    int xc = int(y / (board->H / 10) - 1);  // Переводим координаты клика в индексы клетки
                    int yc = int(x / (board->W / 10) - 1);
                    if (xc == -1 && yc == 8)  // Если кликнули на кнопку перезапуска
                        resp = Response::REPLAY;  // Игрок хочет перезапустить игру
                break;
                }
                if (resp != Response::OK)  // Если получен ответ, выходим из цикла
                    break;
            }
        }
        return resp;  // Возвращаем ответ от игрока
    }

  private:
    Board *board;  // Указатель на объект доски
};
