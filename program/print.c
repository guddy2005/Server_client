#include <unistd.h>
#include <stdarg.h>

/* Print one character */
void print_char(char c)
{
    write(1, &c, 1);
}

/* Print string */
void print_string(char *str)
{
    if (str == 0)
    {
        write(1, "(null)", 6);
        return;
    }

    while (*str)
    {
        write(1, str, 1);
        str++;
    }
}

/* Print integer */
void print_int(int num)
{
    char buffer[12];
    int i = 0;

    if (num == 0)
    {
        print_char('0');
        return;
    }

    if (num < 0)
    {
        print_char('-');
        num = -num;
    }

    while (num > 0)
    {
        buffer[i++] = (num % 10) + '0';
        num = num / 10;
    }

    while (i > 0)
    {
        print_char(buffer[--i]);
    }
}

/* Our own printf */
void my_printf(const char *format, ...)
{
    va_list args;

    va_start(args, format);

    while (*format)
    {
        if (*format == '%')
        {
            format++;

            switch (*format)
            {
                case 'd':
                    print_int(va_arg(args, int));
                    break;

                case 's':
                    print_string(va_arg(args, char *));
                    break;

                case 'c':
                    print_char((char)va_arg(args, int));
                    break;

                case '%':
                    print_char('%');
                    break;

                default:
                    print_char('%');
                    print_char(*format);
            }
        }
        else
        {
            print_char(*format);
        }

        format++;
    }

    va_end(args);
}

int main()
{
    my_printf("Hello World\n");

    my_printf("Name : %s\n", "Guddy");

    my_printf("Age : %d\n", 21);

    my_printf("Letter : %c\n", 'A');

    my_printf("Marks : %d\n", 95);

    my_printf("100%% Complete\n");

    my_printf("Name : %s Age : %d Grade : %c\n",
              "Guddy",
              21,
              'A');

    return 0;
}
