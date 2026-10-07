#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

#define INITIAL_CAPACITY 64

typedef struct {
    off_t offset;
    int length;
} LineInfo;

// глобальный дескриптор, чтобы обработчик сигнала мог читать из файла
int g_fd = -1;

// функция-обработчик таймаута
void on_alarm(int sig)
{
    (void)sig;

    const char msg[] = "\n[Время вышло (5 сек)! Печать содержимого файла:]\n";
    write(1, msg, sizeof(msg) - 1);

    // перематываем файл в самое начало
    lseek(g_fd, 0, SEEK_SET);

    char buf[256];
    int bytes;
    // cчитываем весь файл кусками и выводим в терминал (дескриптор 1 = stdout)
    while ((bytes = read(g_fd, buf, sizeof(buf))) > 0) {
        write(1, buf, bytes);
    }

    close(g_fd);
    _exit(0); // асинхронный выход из сигнала
}

int main(int argc, char *argv[])
{
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // открываем файл системным только для чтения (O_RDONLY)
    g_fd = open(argv[1], O_RDONLY);
    if (g_fd == -1) {
        perror("Ошибка открытия файла");
        return 1;
    }

    int capacity = INITIAL_CAPACITY;
    int line_count = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));

    char ch; // буфер для побайтового чтения
    int current_len = 0; // длина текущей накапливаемой строки
    table[0].offset = 0; // первая строка всегда начинается с 0-го байта файла

    while (read(g_fd, &ch, 1) == 1) {

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
                    close(g_fd);
                    return 1;
                }
                table = temp;
            }

            table[line_count].offset = lseek(g_fd, 0L, SEEK_CUR);
        } else {
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

    signal(SIGALRM, on_alarm);

    int line_num;
    while (1) {
        printf("Введите номер строки (0 для выхода, 5 сек таймаут): ");
        // сбрасываем буфер stdout, чтобы надпись гарантированно появилась до запуска таймера
        fflush(stdout);

        alarm(5);

        int res = scanf("%d", &line_num);

        // получили ввод — немедленно выключаем таймер
        alarm(0);

        if (res != 1) {
            printf("Некорректный ввод!\n");
            break;
        }

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

        // lseek(g_fd, смещение, SEEK_SET) — прыгаем прямо на начало запрошенной строки
        lseek(g_fd, table[idx].offset, SEEK_SET);

        // Считываем ровно столько байт, сколько длится эта строка
        read(g_fd, line_buf, table[idx].length);

        // read() не ставит '\0', поэтому добавляем его вручную в конец
        line_buf[table[idx].length] = '\0';

        // Выводим полученную строку
        printf("%s\n", line_buf);

        // Освобождаем временный буфер строки
        free(line_buf);
    }

    // 6. Очистка ресурсов перед выходом
    free(table);
    close(g_fd);

    return 0;
}