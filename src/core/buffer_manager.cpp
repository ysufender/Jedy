export module core.buffer.manager;

import std;

import core.buffer;

struct StringHash {
    using is_transparent = void;

    auto operator()(std::string_view const s) const noexcept -> std::size_t {
        return std::hash<std::string_view>{}(s);
    }
};

namespace core::buffer {
    export struct BufferManager {
        private:
            std::unordered_map<std::string, buffer::Buffer, StringHash, std::equal_to<>> bufferMap;

        public:
            BufferManager() : bufferMap() { }

            auto buffer(std::string_view const name, std::size_t const bufSize) -> std::optional<std::string_view> {
                auto buf = buffer::Buffer::create(bufSize);
                if (!buf) {
                    return "Failed to create buffer.";
                }

                auto const [_, inserted] = this->bufferMap.try_emplace(std::string{name}, std::move(buf.value()));
                if (!inserted) {
                    return "A buffer with the same name already exists.";
                }
                return std::nullopt;
            }

            inline auto get(std::string_view const name) -> std::optional<Buffer*> {
                auto const it = this->bufferMap.find(name);
                if (it == this->bufferMap.end()) {
                    return std::nullopt;
                }
                return &it->second;
            }

            auto flush(std::string_view const name) -> std::optional<std::string_view> {
                auto const maybe = this->get(name);
                if (!maybe) {
                    return "No buffer found with given name";
                }

                std::ofstream out{std::string{name}};
                out << maybe.value()->view();
                return std::nullopt;
            }
    };
}
