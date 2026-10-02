export module core.buffer.manager;

import std;

import core.buffer;

namespace core::buffer {
    export struct BufferManager {
        private:
            std::unordered_map<std::string, buffer::Buffer> bufferMap;

            BufferManager(decltype(bufferMap)&& bmap)
                : bufferMap(bmap) { }

        public:
            static constexpr std::size_t Default_Overhead = 32;

            static auto create() -> std::optional<BufferManager> {
                return create(Default_Overhead);
            }

            static auto create(std::size_t const overhead) -> std::optional<BufferManager> {
                std::unordered_map<std::string, buffer::Buffer> bufferMap {overhead};
                return std::make_optional(BufferManager{
                    /* .bufferMap = */ std::move(bufferMap),
                });
            }

            auto buffer(std::string_view const name, std::size_t const bufSize) -> std::optional<std::string_view> {
                auto const buf = buffer::Buffer::create(bufSize);

                if (!buf) {
                    return std::make_optional("Failed to create buffer.");
                }

                auto const [_, append] = this->bufferMap.try_emplace(name.data(), std::move(buf.value()));

                if (!append) {
                    return std::make_optional("A buffer with the same name already exists.");
                }
                else {
                    return std::nullopt;
                }
            }

            inline auto get(std::string_view const name) -> std::optional<Buffer*> {
                if (this->bufferMap.contains(name)) {
                    return std::make_optional(&this->bufferMap.at(name))
                }

                return std::nullopt;
            }
    };
}
