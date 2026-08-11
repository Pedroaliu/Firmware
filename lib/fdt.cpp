//
// Created by pedroa on 2026/8/10.
//

#include "lib/fdt.h"


namespace {


constexpr uint32_t kFdtMagic = 0xd00dfeedU;

constexpr uint32_t kFdtBeginNode = 0x00000001U;
constexpr uint32_t kFdtEndNode   = 0x00000002U;
constexpr uint32_t kFdtProp      = 0x00000003U;
constexpr uint32_t kFdtNop       = 0x00000004U;
constexpr uint32_t kFdtEnd       = 0x00000009U;

constexpr uint32_t kFdtHeaderSize = 40U;


[[nodiscard]]
uint32_t read_be32(const uint8_t* address)
{
    return
        (static_cast<uint32_t>(address[0]) << 24U)
        |
        (static_cast<uint32_t>(address[1]) << 16U)
        |
        (static_cast<uint32_t>(address[2]) << 8U)
        |
        static_cast<uint32_t>(address[3]);
}


[[nodiscard]]
bool align4(
    uint32_t value,
    uint32_t limit,
    uint32_t& result)
{
    if (value > limit)
    {
        return false;
    }


    const uint32_t padding =
        (4U - (value & 3U)) & 3U;


    if (padding > (limit - value))
    {
        return false;
    }


    result = value + padding;

    return true;
}


[[nodiscard]]
bool string_equals(
    const char* left,
    const char* right)
{
    for (;;)
    {
        if (*left != *right)
        {
            return false;
        }


        if (*left == '\0')
        {
            return true;
        }


        ++left;
        ++right;
    }
}


[[nodiscard]]
const char* string_at(
    const uint8_t* strings,
    uint32_t strings_size,
    uint32_t offset)
{
    if (offset >= strings_size)
    {
        return nullptr;
    }


    for (uint32_t index = offset;
         index < strings_size;
         ++index)
    {
        if (strings[index] == 0U)
        {
            return reinterpret_cast<const char*>(
                strings + offset);
        }
    }


    return nullptr;
}


[[nodiscard]]
bool property_string_equals(
    const uint8_t* value,
    uint32_t length,
    const char* expected)
{
    uint32_t index = 0;


    while (expected[index] != '\0')
    {
        if (index >= length)
        {
            return false;
        }


        if (value[index] !=
            static_cast<uint8_t>(expected[index]))
        {
            return false;
        }


        ++index;
    }


    /*
     * DT string properties contain the terminating NUL.
     */
    if (index >= length)
    {
        return false;
    }


    if (value[index] != 0U)
    {
        return false;
    }


    return length == index + 1U;
}


} // namespace


namespace jixia::fdt {


CpuCountResult cpu_count(uintptr_t dtb_address)
{
    CpuCountResult result{
        .count = 0,
        .valid = false,
    };


    if (dtb_address == 0U)
    {
        return result;
    }


    const auto* blob =
        reinterpret_cast<const uint8_t*>(dtb_address);


    /*
     * FDT header:
     *
     *  0  magic
     *  4  totalsize
     *  8  off_dt_struct
     * 12  off_dt_strings
     * ...
     * 32  size_dt_strings
     * 36  size_dt_struct
     */
    if (read_be32(blob) != kFdtMagic)
    {
        return result;
    }


    const uint32_t total_size =
        read_be32(blob + 4U);

    const uint32_t structure_offset =
        read_be32(blob + 8U);

    const uint32_t strings_offset =
        read_be32(blob + 12U);

    const uint32_t strings_size =
        read_be32(blob + 32U);

    const uint32_t structure_size =
        read_be32(blob + 36U);


    if (total_size < kFdtHeaderSize)
    {
        return result;
    }


    if (structure_offset > total_size)
    {
        return result;
    }


    if (structure_size >
        total_size - structure_offset)
    {
        return result;
    }


    if (strings_offset > total_size)
    {
        return result;
    }


    if (strings_size >
        total_size - strings_offset)
    {
        return result;
    }


    const uint8_t* structure =
        blob + structure_offset;

    const uint8_t* strings =
        blob + strings_offset;


    uint32_t cursor = 0;

    /*
     * Root node becomes depth 0.
     */
    int32_t depth = -1;

    /*
     * -1 means that /cpus has not been entered.
     */
    int32_t cpus_depth = -1;

    bool current_cpu_counted = false;


    while (cursor + 4U <= structure_size)
    {
        const uint32_t token =
            read_be32(structure + cursor);

        cursor += 4U;


        switch (token)
        {
        case kFdtBeginNode:
        {
            const uint32_t name_offset = cursor;


            while (cursor < structure_size &&
                   structure[cursor] != 0U)
            {
                ++cursor;
            }


            if (cursor >= structure_size)
            {
                return result;
            }


            const char* node_name =
                reinterpret_cast<const char*>(
                    structure + name_offset);


            /*
             * Skip terminating NUL.
             */
            ++cursor;


            uint32_t aligned_cursor = 0;

            if (!align4(
                    cursor,
                    structure_size,
                    aligned_cursor))
            {
                return result;
            }


            cursor = aligned_cursor;

            ++depth;


            /*
             * /cpus is a direct child of the root.
             */
            if (depth == 1 &&
                string_equals(node_name, "cpus"))
            {
                cpus_depth = depth;
            }


            /*
             * A new direct child of /cpus.
             */
            if (cpus_depth >= 0 &&
                depth == cpus_depth + 1)
            {
                current_cpu_counted = false;
            }


            break;
        }


        case kFdtEndNode:
        {
            if (depth < 0)
            {
                return result;
            }


            if (depth == cpus_depth)
            {
                cpus_depth = -1;
            }


            --depth;

            break;
        }


        case kFdtProp:
        {
            /*
             * FDT_PROP payload starts with:
             *
             *   uint32_t len;
             *   uint32_t nameoff;
             */
            if (structure_size - cursor < 8U)
            {
                return result;
            }


            const uint32_t length =
                read_be32(structure + cursor);

            const uint32_t name_offset =
                read_be32(structure + cursor + 4U);

            cursor += 8U;


            if (length >
                structure_size - cursor)
            {
                return result;
            }


            const uint8_t* value =
                structure + cursor;


            const char* property_name =
                string_at(
                    strings,
                    strings_size,
                    name_offset);


            if (property_name == nullptr)
            {
                return result;
            }


            /*
             * Only count direct children of /cpus whose
             * device_type property is exactly "cpu".
             */
            if (cpus_depth >= 0 &&
                depth == cpus_depth + 1 &&
                !current_cpu_counted &&
                string_equals(
                    property_name,
                    "device_type") &&
                property_string_equals(
                    value,
                    length,
                    "cpu"))
            {
                ++result.count;

                current_cpu_counted = true;
            }


            cursor += length;


            uint32_t aligned_cursor = 0;

            if (!align4(
                    cursor,
                    structure_size,
                    aligned_cursor))
            {
                return result;
            }


            cursor = aligned_cursor;

            break;
        }


        case kFdtNop:
            break;


        case kFdtEnd:
            /*
             * A structurally complete tree has already
             * closed the root node.
             */
            if (depth != -1)
            {
                return result;
            }


            result.valid = true;

            return result;


        default:
            return result;
        }
    }


    return result;
}


} // namespace jixia::fdt