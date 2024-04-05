#pragma once

#define GHC_ENUM_ENABLE_BIT_OPERATIONS(Enum) \
constexpr Enum operator&(Enum X, Enum Y) { using underlying = typename std::underlying_type<Enum>::type; return static_cast<Enum>(static_cast<underlying>(X) & static_cast<underlying>(Y)); } \
constexpr Enum operator|(Enum X, Enum Y) { using underlying = typename std::underlying_type<Enum>::type; return static_cast<Enum>(static_cast<underlying>(X) | static_cast<underlying>(Y)); } \
constexpr Enum operator^(Enum X, Enum Y) { using underlying = typename std::underlying_type<Enum>::type; return static_cast<Enum>(static_cast<underlying>(X) ^ static_cast<underlying>(Y)); } \
constexpr Enum operator~(Enum X) { using underlying = typename std::underlying_type<Enum>::type; return static_cast<Enum>(~static_cast<underlying>(X)); } \
constexpr Enum& operator&=(Enum& X, Enum Y) { X = X & Y; return X; } \
constexpr Enum& operator|=(Enum& X, Enum Y) { X = X | Y; return X; } \
constexpr Enum& operator^=(Enum& X, Enum Y) { X = X ^ Y; return X; }
