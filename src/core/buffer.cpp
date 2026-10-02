export module core.buffer;

import std;

namespace buffer {
    export struct Buffer {
        private:
            std::size_t overhead;

            Buffer(std::size_t const overhead, std::size_t const size, char* const buffer)
                : overhead(overhead),
                  size(size),
                  buffer(buffer) { }

        public:
            std::size_t size;
            char*  buffer;

            static constexpr std::size_t Default_Overhead = 128;

            static auto create(std::size_t const size) -> std::optional<Buffer> {
                return create(size, Default_Overhead);
            }

            static auto create(std::size_t const size, std::size_t const overhead) -> std::optional<Buffer> {
                char* buffer = static_cast<char*>(std::calloc(size + overhead, sizeof(char)));

                if (buffer) {
                    return std::make_optional(Buffer{
                        /* .overhead = */ overhead, 
                        /* .size     = */ size,
                        /* .buffer   = */ buffer,
                    });
                }

                return std::nullopt;
            }
    };
}
