export module core.buffer.manager;

import std;

import core.buffer;

namespace core::buffer {
    export struct BufferManager {
        private:
            std::unordered_map<std::string_view, buffer::Buffer> bufferMap;

        public:
            BufferManager() : bufferMap() { }

            auto buffer(std::string_view const name, std::size_t const bufSize) -> std::optional<std::string_view> {
                auto buf = buffer::Buffer::create(bufSize);

                if (!buf) {
                    return std::make_optional("Failed to create buffer.");
                }

                auto const [_, append] = this->bufferMap.try_emplace(name.data(), std::move(buf.value()));

                std::println("Log: Adding buffer {}", name);

                if (!append) {
                    return std::make_optional("A buffer with the same name already exists.");
                }
                else {
                    return std::nullopt;
                }
            }

            inline auto get(std::string_view const name) -> std::optional<Buffer*> {
                if (this->bufferMap.contains(name)) {
                    return std::make_optional(&this->bufferMap.at(name));
                }

                std::println("Error: No buffer with name {}", name);
                return std::nullopt;
            }
    };
}
