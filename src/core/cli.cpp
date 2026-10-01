export module core.cli;

import std;

namespace cli {
    export struct Value {
        enum class Type {
            string,
            boolean,
            empty
        } tag = Type::empty;

        union Payload {
            std::string_view string;
            bool             boolean = false;
        } payload;

        auto type() const -> std::string_view {
            switch (this->tag) {
                case Type::string: return "string";
                case Type::boolean: return "bool";
                default: return "undefined";
            }
        }

        auto str() const -> std::string_view {
            switch (this->tag) {
                case Type::string: return this->payload.string;
                case Type::boolean: return this->payload.boolean ? "true" : "false";
                default: return "undefined";
            }
        }

        template<Type Tag>
        constexpr static auto make(std::conditional_t<Tag == Type::string, std::string_view, bool> const val) -> Value {
            if constexpr (Tag == Type::string) {
                return { .tag = Tag, .payload = { .string = val } };
            } else {
                return { .tag = Tag, .payload = { .boolean = val } };
            }
        }
    };

    export template<int N>
    class Args {
        std::array<std::tuple<std::string_view, Value, std::string_view>, N> args = {};
        int size = 0;

        public:
            constexpr auto option(std::string_view const name,
                                  bool             const defaultVal,
                                  std::string_view const desc) -> Args& {

                if (this->size >= N) {
                    return *this;
                }

                this->args[this->size++] = std::make_tuple(name, Value::make<Value::Type::boolean>(defaultVal), desc);
                return *this;
            }

            constexpr auto param(std::string_view const name,
                                 Value            const defaultVal,
                                 std::string_view const desc) -> Args& {
                if (this->size >= N) {
                    return *this;
                }

                this->args[this->size++] = std::make_tuple(name, defaultVal, desc);
                return *this;
            }

            auto help() -> void {
                for (auto const& [name, value, desc] : this->args) {
                    std::println("    {} <{}>: {}\n        (default = {})", name, value.type(), desc, value.str());
                }
            }
    };

    export template<int N>
    constexpr auto init() -> Args<N> {
        return {};
    }

    export inline constexpr auto args = init<2>();
}
