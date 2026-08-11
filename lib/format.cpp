#include "lib/format.h"

namespace jixia::format {
namespace {

enum class Length : uint8_t {
    normal,
    char_width,
    short_width,
    long_width,
    long_long_width,
    size_width,
    ptrdiff_width,
};

struct Options {
    bool alternate;
    bool zero_pad;
    bool left_align;
    bool plus_sign;
    bool space_sign;
    bool uppercase;
    size_t width;
    Length length;
    char type;
};

[[nodiscard]]
constexpr bool is_digit(char ch)
{
    return (ch >= '0') && (ch <= '9');
}

void emit(Writer writer, char ch, size_t& count)
{
    writer.put(ch);
    ++count;
}

void emit_repeat(Writer writer, char ch, size_t amount, size_t& count)
{
    for (size_t index = 0; index < amount; ++index)
    {
        emit(writer, ch, count);
    }
}

void emit_text(Writer writer, const char* text, size_t& count)
{
    if (text == nullptr)
    {
        text = "(null)";
    }

    while (*text != '\0')
    {
        emit(writer, *text, count);
        ++text;
    }
}

[[nodiscard]]
Options parse_options(const char*& cursor)
{
    Options options{
        false,
        false,
        false,
        false,
        false,
        false,
        0U,
        Length::normal,
        '\0',
    };

    bool parsing_flags = true;
    while (parsing_flags)
    {
        switch (*cursor)
        {
            case '#':
                options.alternate = true;
                ++cursor;
                break;
            case '0':
                options.zero_pad = true;
                ++cursor;
                break;
            case '-':
                options.left_align = true;
                ++cursor;
                break;
            case '+':
                options.plus_sign = true;
                ++cursor;
                break;
            case ' ':
                options.space_sign = true;
                ++cursor;
                break;
            default:
                parsing_flags = false;
                break;
        }
    }

    while (is_digit(*cursor))
    {
        options.width =
            (options.width * 10U) +
            static_cast<size_t>(*cursor - '0');
        ++cursor;
    }

    if (*cursor == 'h')
    {
        ++cursor;
        if (*cursor == 'h')
        {
            options.length = Length::char_width;
            ++cursor;
        }
        else
        {
            options.length = Length::short_width;
        }
    }
    else if (*cursor == 'l')
    {
        ++cursor;
        if (*cursor == 'l')
        {
            options.length = Length::long_long_width;
            ++cursor;
        }
        else
        {
            options.length = Length::long_width;
        }
    }
    else if (*cursor == 'z')
    {
        options.length = Length::size_width;
        ++cursor;
    }
    else if (*cursor == 't')
    {
        options.length = Length::ptrdiff_width;
        ++cursor;
    }

    options.type = *cursor;
    if (*cursor != '\0')
    {
        ++cursor;
    }

    if (options.type == 'X')
    {
        options.uppercase = true;
    }

    return options;
}

[[nodiscard]]
size_t encode_unsigned(
    char* scratch,
    size_t capacity,
    uint64_t value,
    unsigned int base,
    bool uppercase)
{
    static constexpr char lower_digits[] = "0123456789abcdef";
    static constexpr char upper_digits[] = "0123456789ABCDEF";
    const char* const digits = uppercase ? upper_digits : lower_digits;

    size_t length = 0U;
    do
    {
        if (length >= capacity)
        {
            break;
        }

        const unsigned int digit =
            static_cast<unsigned int>(value % base);
        scratch[length++] = digits[digit];
        value /= base;
    } while (value != 0U);

    return length;
}

void emit_number(
    Writer writer,
    const Options& options,
    uint64_t magnitude,
    bool negative,
    unsigned int base,
    bool pointer,
    size_t& count)
{
    char scratch[sizeof(uint64_t) * 8U];
    const size_t digits_length =
        encode_unsigned(
            scratch,
            sizeof(scratch),
            magnitude,
            base,
            options.uppercase);

    char sign = '\0';
    if (negative)
    {
        sign = '-';
    }
    else if (options.plus_sign)
    {
        sign = '+';
    }
    else if (options.space_sign)
    {
        sign = ' ';
    }

    char prefix0 = '\0';
    char prefix1 = '\0';
    size_t prefix_length = 0U;

    if (pointer || options.alternate)
    {
        if (base == 16U)
        {
            prefix0 = '0';
            prefix1 = options.uppercase ? 'X' : 'x';
            prefix_length = 2U;
        }
        else if (base == 8U)
        {
            prefix0 = '0';
            prefix_length = 1U;
        }
        else if (base == 2U)
        {
            prefix0 = '0';
            prefix1 = options.uppercase ? 'B' : 'b';
            prefix_length = 2U;
        }
    }

    size_t minimum_digits = digits_length;
    if (pointer)
    {
        minimum_digits = sizeof(uintptr_t) * 2U;
    }

    const size_t total_length =
        minimum_digits +
        prefix_length +
        ((sign != '\0') ? 1U : 0U);

    if (!options.left_align && !options.zero_pad &&
        (options.width > total_length))
    {
        emit_repeat(
            writer,
            ' ',
            options.width - total_length,
            count);
    }

    if (sign != '\0')
    {
        emit(writer, sign, count);
    }

    if (prefix_length >= 1U)
    {
        emit(writer, prefix0, count);
    }
    if (prefix_length == 2U)
    {
        emit(writer, prefix1, count);
    }

    if (minimum_digits > digits_length)
    {
        emit_repeat(
            writer,
            '0',
            minimum_digits - digits_length,
            count);
    }

    if (!options.left_align && options.zero_pad &&
        (options.width > total_length))
    {
        emit_repeat(
            writer,
            '0',
            options.width - total_length,
            count);
    }

    for (size_t index = digits_length; index != 0U; --index)
    {
        emit(writer, scratch[index - 1U], count);
    }

    if (options.left_align && (options.width > total_length))
    {
        emit_repeat(
            writer,
            ' ',
            options.width - total_length,
            count);
    }
}

[[nodiscard]]
uint64_t read_unsigned(Length length, va_list& args)
{
    switch (length)
    {
        case Length::char_width:
            return static_cast<unsigned char>(
                va_arg(args, unsigned int));
        case Length::short_width:
            return static_cast<unsigned short>(
                va_arg(args, unsigned int));
        case Length::long_width:
            return static_cast<uint64_t>(
                va_arg(args, unsigned long));
        case Length::long_long_width:
            return static_cast<uint64_t>(
                va_arg(args, unsigned long long));
        case Length::size_width:
            return static_cast<uint64_t>(
                va_arg(args, size_t));
        case Length::ptrdiff_width:
            return static_cast<uint64_t>(
                va_arg(args, ptrdiff_t));
        case Length::normal:
        default:
            return static_cast<uint64_t>(
                va_arg(args, unsigned int));
    }
}

[[nodiscard]]
int64_t read_signed(Length length, va_list& args)
{
    switch (length)
    {
        case Length::char_width:
            return static_cast<signed char>(
                va_arg(args, int));
        case Length::short_width:
            return static_cast<short>(
                va_arg(args, int));
        case Length::long_width:
            return static_cast<int64_t>(
                va_arg(args, long));
        case Length::long_long_width:
            return static_cast<int64_t>(
                va_arg(args, long long));
        case Length::size_width:
            return static_cast<int64_t>(
                va_arg(args, ptrdiff_t));
        case Length::ptrdiff_width:
            return static_cast<int64_t>(
                va_arg(args, ptrdiff_t));
        case Length::normal:
        default:
            return static_cast<int64_t>(
                va_arg(args, int));
    }
}

} // namespace

size_t vformat(Writer writer, const char* format, va_list& args)
{
    if (!writer.valid() || (format == nullptr))
    {
        return 0U;
    }

    size_t count = 0U;
    const char* cursor = format;

    while (*cursor != '\0')
    {
        if (*cursor != '%')
        {
            emit(writer, *cursor, count);
            ++cursor;
            continue;
        }

        ++cursor;
        if (*cursor == '%')
        {
            emit(writer, '%', count);
            ++cursor;
            continue;
        }

        const Options options = parse_options(cursor);

        switch (options.type)
        {
            case 'c':
                emit(
                    writer,
                    static_cast<char>(va_arg(args, int)),
                    count);
                break;

            case 's':
                emit_text(
                    writer,
                    va_arg(args, const char*),
                    count);
                break;

            case 'd':
            case 'i':
            {
                const int64_t value =
                    read_signed(options.length, args);
                const bool negative = value < 0;
                const uint64_t magnitude =
                    negative
                        ? (0ULL - static_cast<uint64_t>(value))
                        : static_cast<uint64_t>(value);
                emit_number(
                    writer,
                    options,
                    magnitude,
                    negative,
                    10U,
                    false,
                    count);
                break;
            }

            case 'u':
                emit_number(
                    writer,
                    options,
                    read_unsigned(options.length, args),
                    false,
                    10U,
                    false,
                    count);
                break;

            case 'o':
                emit_number(
                    writer,
                    options,
                    read_unsigned(options.length, args),
                    false,
                    8U,
                    false,
                    count);
                break;

            case 'x':
            case 'X':
                emit_number(
                    writer,
                    options,
                    read_unsigned(options.length, args),
                    false,
                    16U,
                    false,
                    count);
                break;

            case 'b':
            case 'B':
            {
                Options binary_options = options;
                binary_options.uppercase =
                    options.type == 'B';
                emit_number(
                    writer,
                    binary_options,
                    read_unsigned(options.length, args),
                    false,
                    2U,
                    false,
                    count);
                break;
            }

            case 'p':
            {
                Options pointer_options = options;
                pointer_options.alternate = true;
                emit_number(
                    writer,
                    pointer_options,
                    reinterpret_cast<uintptr_t>(
                        va_arg(args, void*)),
                    false,
                    16U,
                    true,
                    count);
                break;
            }

            case '\0':
                emit(writer, '%', count);
                return count;

            default:
                emit(writer, '%', count);
                if (options.type != '\0')
                {
                    emit(writer, options.type, count);
                }
                break;
        }
    }

    return count;
}

} // namespace jixia::format
