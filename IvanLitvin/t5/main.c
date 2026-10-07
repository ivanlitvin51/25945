#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>

#define INITIAL_CAPACITY 64

typedef struct {
    off_t offset;
    int length;
} LineInfo;

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // открываем файл только для чтения (O_RDONLY)
    int fd = open(argv[1], O_RDONLY);
    if (fd == -1) {
        perror("Ошибка открытия файла");
        return 1;
    }

    int capacity = INITIAL_CAPACITY;
    int line_count = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));
    if (!table) {
        perror("Ошибка выделения памяти под таблицу");
        close(fd);
        return 1;
    }

    char ch; // буфер для побайтового чтения
    int current_len = 0; // длина текущей накапливаемой строки
    table[0].offset = 0; // первая строка всегда начинается с 0-го байта файла

    while (read(fd, &ch, 1) == 1) {
        if (ch == '\n') {
            // конец строки
            table[line_count].length = current_len;
            line_count++;
            current_len = 0;

            // если массив заполнен, увеличиваем его емкость в 2 раза через realloc
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

            // lseek(fd, 0L, SEEK_CUR) возвращает его текущую позицию курсора
            table[line_count].offset = lseek(fd, 0L, SEEK_CUR);
        } else {
            // Обычный символ внутри строки
            current_len++;
        }
    }

    // если файл не заканчивался символом '\n', сохраняем остаток последней строки
    if (current_len > 0) {
        table[line_count].length = current_len;
        line_count++;
    }

    printf("--- Таблица строк (Debug) ---\n");
    for (int i = 0; i < line_count; i++) {
        printf("Строка %d: Смещение = %ld, Длина = %d\n", 
               i + 1, (long)table[i].offset, table[i].length);
    }
    printf("Всего найдено строк: %d\n", line_count);
    printf("-----------------------------\n\n");

    int line_num;
    while (1) {
        printf("Введите номер строки (0 для выхода): ");
        if (scanf("%d", &line_num) != 1) {
            // защита от ввода мусора
            printf("Некорректный ввод!\n");
            break;
        }

        // выход по вводу 0
        if (line_num == 0) {
            break;
        }

        // проверка диапазона
        if (line_num < 1 || line_num > line_count) {
            printf("Строки с номером %d не существует!\n", line_num);
            continue;
        }

        // выделяем память ровно под длину строки + 1 байт для терминатора '\0'
        char *line_buf = malloc(table[line_num - 1].length + 1);
        if (!line_buf) {
            perror("Ошибка выделения буфера под строку");
            continue;
        }

        // lseek(fd, смещение, SEEK_SET) — идем на начало запрошенной строки
        lseek(fd, table[line_num - 1].offset, SEEK_SET);

        // считываем ровно столько байт, сколько длится эта строка
        read(fd, line_buf, table[line_num - 1].length);

        // так как read() не ставит '\0', поэтому добавляем его вручную в конец
        line_buf[table[line_num - 1].length] = '\0';

        printf("%s\n", line_buf);

        free(line_buf);
    }

    free(table);
    close(fd);

    return 0;
}