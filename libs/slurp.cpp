#include "slurp.hpp"

#include <cstdio> // for printf, read/seek etc..
#include <cstdlib> // for free and calloc

unique_cstr slurp(const char* filepath)
{
        auto file = std::fopen(filepath, "rb");
        if (!file) {
                return { nullptr, std::free };
        }

        std::fseek(file, 0, SEEK_END);
        auto fsize = std::ftell(file);
        std::fseek(file, 0, SEEK_SET);

        auto content = unique_cstr { static_cast<char*> (std::calloc(1, 1 + fsize)), std::free };
        auto dest = content.get();
        auto remaining = fsize;
        long n;
        while (n = std::fread(dest, 1, remaining, file), n != 0) {
                dest += n;
                remaining -= n;
        }
        std::fclose(file);

        return content;
}
