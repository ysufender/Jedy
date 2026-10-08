export module core.buffer;

import std;

namespace core::buffer {
    struct FreeDeleter {
        void operator()(char* p) const noexcept {
            std::free(p);
        }
    };

    export struct Buffer {
        private:
            std::size_t used;
            std::size_t cap;
            std::unique_ptr<char[], FreeDeleter> buffer;

            Buffer(std::size_t const cap, std::unique_ptr<char[], FreeDeleter>&& buffer)
                : used(0),
                  cap(cap),
                  buffer(std::move(buffer)) { }

            auto reserve(std::size_t const needed) -> bool {
                if (needed <= this->cap) {
                    return true;
                }

                auto const newCap = std::max(needed, this->cap * 2);
                char* raw = static_cast<char*>(std::realloc(this->buffer.get(), newCap));
                if (!raw) {
                    return false;
                }

                (void)this->buffer.release();
                this->buffer.reset(raw);
                this->cap = newCap;
                return true;
            }

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
                std::unique_ptr<char[], FreeDeleter> mem{static_cast<char*>(std::malloc(cap))};

                if (!mem) {
                    return std::nullopt;
                }

                return Buffer{cap, std::move(mem)};
            }

            auto assign(std::string_view const buf) -> std::optional<std::string_view> {
                if (!this->reserve(buf.size())) {
                    return "Out of memory.";
                }

                if (!buf.empty()) {
                    std::memcpy(this->buffer.get(), buf.data(), buf.size());
                }
                this->used = buf.size();
                return std::nullopt;
            }

            auto append(std::size_t const off, char const data) -> std::optional<std::string_view> {
                if (off > this->used) {
                    return "Offset is past the end of the buffer.";
                }

                if (!this->reserve(this->used + 1)) {
                    return "Out of memory.";
                }

                std::memmove(this->buffer.get() + off + 1, this->buffer.get() + off, this->used - off);
                this->buffer[off] = data;
                ++this->used;
                return std::nullopt;
            }

            auto modify(std::size_t const off, char const data) -> std::optional<std::string_view> {
                if (off >= this->used) {
                    return "Out of bounds";
                }

                this->buffer[off] = data;
                return std::nullopt;
            }

            auto get(std::size_t const off) -> std::optional<char> {
                if (off < this->view().size()) {
                    return this->view().at(off);
                }
                return std::nullopt;
            }

            auto view() const -> std::string_view {
                return {this->buffer.get(), this->used};
            }
    };
}
