#include <unistd.h>
#include <stdarg.h>

/* String length */
int my_strlen(char *str)
{
    int len = 0;

    while (str[len] != '\0')
        len++;

    return len;
}

/* String to Integer */
int my_atoi(char *str)
{
    int i = 0;
    int sign = 1;
    int num = 0;

    if (str[0] == '-')
    {
        sign = -1;
        i++;
    }

    while (str[i] >= '0' && str[i] <= '9')
    {
        num = num * 10 + (str[i] - '0');
        i++;
    }

    return num * sign;
}

/* Mini scanf */
void my_scanf(const char *format, ...)
{
    char buffer[100];
    int bytes;

    va_list args;
    va_start(args, format);

    bytes = read(0, buffer, sizeof(buffer) - 1);
    buffer[bytes - 1] = '\0';     // Remove '\n'

    char *p = buffer;

    while (*format)
    {
        if (*format == '%')
        {
            format++;

            if (*format == 'd')
            {
                int *x = va_arg(args, int *);
                *x = my_atoi(p);

                while (*p != ' ' && *p != '\0')
                    p++;

                if (*p == ' ')
                    p++;
            }

            else if (*format == 's')
            {
                char *str = va_arg(args, char *);

                while (*p != ' ' && *p != '\0')
                {
                    *str = *p;
                    str++;
                    p++;
                }

                *str = '\0';

                if (*p == ' ')
                    p++;
            }

            else if (*format == 'c')
            {
                char *ch = va_arg(args, char *);
                *ch = *p;

                if (*p != '\0')
                    p++;
            }
        }

        format++;
    }

    va_end(args);
}

/* ---------- Print Functions ---------- */

void print_char(char c)
{
    write(1, &c, 1);
}

void print_string(char *str)
{
    while (*str)
    {
        write(1, str, 1);
        str++;
    }
}

void print_int(int num)
{
    char buf[12];
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
        buf[i++] = (num % 10) + '0';
        num /= 10;
    }

    while (i)
        print_char(buf[--i]);
}

int main()
{
    int age;
    char name[50];
    char grade;

    print_string("Enter Name : ");

    my_scanf("%s", name);
    print_string("Enter Age : ");
    my_scanf(" %d ", &age);


    print_string("Enter Grade : ");
    my_scanf("%c", &grade);

    print_string("\nName : ");
    print_string(name);

    print_string("\nAge : ");
    print_int(age);

    print_string("\nGrade : ");
    print_char(grade);

    print_char('\n');

    return 0;
}
