export module core.cli;

import std;

namespace core::cli {
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
        auto unwrap() -> std::conditional_t<Tag == Type::string, std::string_view, bool> {
            if constexpr (Tag == Type::string) {
                return this->payload.string;
            } else {
                return this->payload.boolean;
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
        int                                                                  size = 0;
        char const* const* const                                             _args;
        int const                                                            _argc;

        public:
            Args(char const* const* const args, int const argc)
                : _args(args),
                  _argc(argc) { }

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

            auto get(std::string_view const name) -> std::optional<Value*> {
                for (auto& [pname, value, _] : this->args) {
                    if (name == pname) {
                        return std::make_optional(&value);
                    }
                }

                return std::nullopt;
            }

            template<Value::Type Tag>
            auto get_unwrap(std::string_view const name) -> std::conditional_t<Tag == Value::Type::string, std::string_view, bool> {
                return this->get(name).value()->template unwrap<Tag>();
            }

            auto parse() -> std::optional<std::string_view> {
                for (int i = 1; i < this->_argc; i++) {
                    std::string_view const arg = this->_args[i];

                    if (arg[0] == '-') {
                        auto const maybeValueRef = this->get(arg.substr(1));

                        if (!maybeValueRef) {
                            std::println("Unknown command line argument '{}'", arg);
                            return std::make_optional("<unknown>");
                        }

                        switch (maybeValueRef.value()->tag) {
                            case Value::Type::string:
                                if (i == this->_argc - 1) {
                                    std::println("Expected a string value after '{}'.", arg);
                                    return std::make_optional("<missing argument>");
                                }
                                maybeValueRef.value()->payload.string = this->_args[(i++) + 1];
                                continue;

                            case Value::Type::boolean:
                                maybeValueRef.value()->payload.boolean = true;
                                continue;

                            case Value::Type::empty: continue;
                        }
                    }
                    else {
                        std::println("Unknown command line argument '{}'", arg);
                        return std::make_optional("<unknown>");
                    }
                }

                return std::nullopt;
            }
    };

    export template<int N>
    constexpr auto init(int const argc, char const* const* const args) -> Args<N> {
        return { args, argc };
    }
}
