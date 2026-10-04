export module core.buffer;

import std;

namespace core::buffer {
    export struct Buffer {
        private:
            std::size_t used;
            std::size_t cap;
            std::unique_ptr<char[]> buffer;

            Buffer(std::size_t const cap, std::unique_ptr<char[]>&& buffer)
                : used(0),
                  cap(cap),
                  buffer(std::move(buffer)) { }

        public:
            Buffer(Buffer&& other) noexcept
                : used(std::exchange(other.used, 0)),
                  cap(std::exchange(other.cap, 0)),
                  buffer(std::move(other.buffer)) { }

            auto operator=(Buffer&& other) noexcept -> Buffer& {
                if (this != &other) {
                    used = std::exchange(other.used, 0);
                    cap = std::exchange(other.cap, 0);
                    buffer = std::move(other.buffer);
                }
                return *this;
            }

            static constexpr std::size_t DefaultOverhead = 255;

            static auto create(std::size_t const size) -> std::optional<Buffer> {
                auto const cap = size + DefaultOverhead;
                std::unique_ptr<char[]> mem{new (std::nothrow) char[cap]};

                if (!mem) {
                    return std::nullopt;
                }

                return Buffer{cap, std::move(mem)};
            }

            auto assign(std::string_view const buf) -> std::optional<std::string_view> {
                if (buf.size() > this->cap) {
                    return "Given data is too big for the buffer.";
                }

                std::memcpy(this->buffer.get(), buf.data(), buf.size());
                used = buf.size();
                return std::nullopt;
            }

            auto modify(std::size_t const off, char const data) -> std::optional<std::string_view> {
                if (off >= this->cap) {
                    return "Out of bounds";
                }

                if (off > this->used) {
                    this->used = off + 1;
                }

                this->buffer[off] = data;
                return std::nullopt;
            }

            auto view() const -> std::string_view {
                return {this->buffer.get(), this->used};
            }
    };
}
