#include <stdio.h>
#include <stdlib.h>
#include <fcntl.h>
#include <unistd.h>
#include <signal.h>

// Начальный размер динамической таблицы строк
#define INITIAL_CAPACITY 64

// Структура для хранения метаданных о каждой строке файла
typedef struct {
    off_t offset; // Смещение: порядковый номер байта от начала файла, где строка начинается
    int length;   // Длина строки: сколько байт/символов она занимает
} LineInfo;

// Глобальный дескриптор, чтобы обработчик сигнала мог читать из файла
int g_fd = -1;

// Функция-обработчик таймаута (вызывается ядром ОС через 5 секунд молчания)
void on_alarm(int sig)
{
    (void)sig; // Заглушка, чтобы компилятор не ругался на неиспользуемый параметр

    const char msg[] = "\n[Время вышло (5 сек)! Печать содержимого файла:]\n";
    write(1, msg, sizeof(msg) - 1);

    // Перематываем файл в самое начало
    lseek(g_fd, 0, SEEK_SET);

    char buf[256];
    int bytes;
    // Считываем весь файл кусками и выводим в терминал (дескриптор 1 = stdout)
    while ((bytes = read(g_fd, buf, sizeof(buf))) > 0) {
        write(1, buf, bytes);
    }

    close(g_fd);
    _exit(0); // Безопасный асинхронный выход из сигнала
}

int main(int argc, char *argv[])
{
    // 1. Проверяем, что пользователь передал имя файла при запуске: ./line_lookup test.txt
    if (argc != 2) {
        fprintf(stderr, "Использование: %s <имя_файла>\n", argv[0]);
        return 1;
    }

    // 2. Открываем файл системным вызовом open() только для чтения (O_RDONLY)
    g_fd = open(argv[1], O_RDONLY);
    if (g_fd == -1) {
        perror("Ошибка открытия файла");
        return 1;
    }

    // Инициализируем динамический массив структур под таблицу строк
    int capacity = INITIAL_CAPACITY;
    int line_count = 0;
    LineInfo *table = malloc(capacity * sizeof(LineInfo));
    if (!table) {
        perror("Ошибка выделения памяти под таблицу");
        close(g_fd);
        return 1;
    }

    char ch;                 // Буфер для побайтового чтения
    int current_len = 0;     // Длина текущей накапливаемой строки
    table[0].offset = 0;     // Первая строка всегда начинается с 0-го байта файла

    // 3. Этап сканирования: строим таблицу смещений и длин
    while (read(g_fd, &ch, 1) == 1) {
        if (ch == '\r') {
            // Пропускаем символ возврата каретки Windows
            continue;
        }

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

            // lseek(g_fd, 0L, SEEK_CUR) не сдвигает курсор,
            // но возвращает его текущую позицию (т.е. адрес начала СЛЕДУЮЩЕЙ строки)
            table[line_count].offset = lseek(g_fd, 0L, SEEK_CUR);
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

    // Регистрируем наш обработчик сигнала SIGALRM
    signal(SIGALRM, on_alarm);

    // 5. Интерактивный цикл: запрос номеров строк у пользователя
    int line_num;
    while (1) {
        printf("Введите номер строки (0 для выхода, 5 сек таймаут): ");
        // Обязательно сбрасываем буфер stdout, чтобы надпись гарантированно появилась до запуска таймера
        fflush(stdout);

        // Взводим таймер на 5 секунд
        alarm(5);

        int res = scanf("%d", &line_num);

        // Успели получить ввод — немедленно выключаем таймер
        alarm(0);

        if (res != 1) {
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