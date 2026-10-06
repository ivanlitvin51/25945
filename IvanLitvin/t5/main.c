#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <windows.h>

// Начальный размер динамической таблицы строк
#define INITIAL_CAPACITY 64

// Структура для хранения метаданных о каждой строке файла
typedef struct {
    off_t offset; // Смещение: порядковый номер байта от начала файла, где строка начинается
    int length;   // Длина строки: сколько байт/символов она занимает
} LineInfo;

int main(int argc, char *argv[])
{
    SetConsoleOutputCP(65001);
    // 1. Проверяем, что пользователь передал имя файла при запуске: ./line_lookup test.txt
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // 2. Открываем файл системным вызовом open() только для чтения (O_RDONLY)
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Ошибка открытия файла");
        return 1;
    }

    // Инициализируем динамический массив структур под таблицу строк
    int capacity = INITIAL_CAPACITY;
    int line_count = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));
    if (!table) {
        perror("Ошибка выделения памяти под таблицу");
        close(fd);
        return 1;
    }

    char ch;                 // Буфер для побайтового чтения
    int current_len = 0;     // Длина текущей накапливаемой строки
    table[0].offset = 0;     // Первая строка всегда начинается с 0-го байта файла

    // 3. Этап сканирования: строим таблицу смещений и длин
    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n') {
            // Зафиксировали конец строки
            table[line_count].length = current_len;
            line_count++;
            current_len = 0;

            // Если массив заполнен, увеличиваем его емкость в 2 раза через realloc
            if (line_count >= capacity) {
                capacity *= 2;
                LineInfo *temp = realloc(table, capacity * sizeof(LineInfo));
                if (!temp) {
                    perror("Ошибка расширения памяти таблицы");
                    free(table);
                    close(fd);
                    return 1;
                }
                table = temp;
            }

            // lseek(fd, 0L, SEEK_CUR) не сдвигает курсор,
            // но возвращает его текущую позицию (т.е. адрес начала СЛЕДУЮЩЕЙ строки)
            table[line_count].offset = lseek(fd, 0L, SEEK_CUR);
        } else {
            // Обычный символ внутри строки
            current_len++;
        }
    }

    // Если файл не заканчивался символом '\n', сохраняем остаток последней строки
    if (current_len > 0) {
        table[line_count].length = current_len;
        line_count++;
    }

    // 4. Отладочный вывод таблицы (требование методички для проверки правильности отступов)
    printf("--- Таблица строк (Debug) ---\n");
    for (int i = 0; i < line_count; i++) {
        printf("Строка %d: Смещение = %ld, Длина = %d\n", 
               i + 1, (long)table[i].offset, table[i].length);
    }
    printf("Всего найдено строк: %d\n", line_count);
    printf("-----------------------------\n\n");

    // 5. Интерактивный цикл: запрос номеров строк у пользователя
    int line_num;
    while (1) {
        printf("Введите номер строки (0 для выхода): ");
        if (scanf("%d", &line_num) != 1) {
            // Защита от ввода мусора (например, если введут буквы)
            printf("Некорректный ввод!\n");
            break;
        }

        // Выход по вводу 0
        if (line_num == 0) {
            break;
        }

        // Проверка диапазона (строки для пользователя нумеруются с 1)
        if (line_num < 1 || line_num > line_count) {
            printf("Строки с номером %d не существует!\n", line_num);
            continue;
        }

        // Для пользователя номер строки line_num, а в массиве индекс [line_num - 1]
        int idx = line_num - 1;

        // Выделяем память ровно под длину строки + 1 байт для терминатора '\0'
        char *line_buf = malloc(table[idx].length + 1);
        if (!line_buf) {
            perror("Ошибка выделения буфера под строку");
            continue;
        }

        // lseek(fd, смещение, SEEK_SET) — прыгаем прямо на начало запрошенной строки
        lseek(fd, table[idx].offset, SEEK_SET);

        // Считываем ровно столько байт, сколько длится эта строка
        read(fd, line_buf, table[idx].length);

        // read() не ставит '\0', поэтому добавляем его вручную в конец,
        // чтобы printf понимал, где строка заканчивается
        line_buf[table[idx].length] = '\0';

        // Выводим полученную строку
        printf("%s\n", line_buf);

        // Освобождаем временный буфер строки
        free(line_buf);
    }

    // 6. Очистка ресурсов перед выходом
    free(table);
    close(fd);

    return 0;
}