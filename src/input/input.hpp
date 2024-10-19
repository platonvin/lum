#include "inputHandler.hpp"

// template <typename E>
// constexpr auto to_underlying(E e) noexcept {
//     return static_cast<std::underlying_type_t<E>>(e);
// }

// template <typename E, typename T>
// constexpr E from_underlying(T value) noexcept {
//     return static_cast<E>(value);
// }

enum class GameAction : int {
    MOVE_CAMERA_FORWARD,
    MOVE_CAMERA_BACKWARD,
    MOVE_CAMERA_LEFT,
    MOVE_CAMERA_RIGHT,
    TURN_CAMERA_LEFT,
    TURN_CAMERA_RIGHT,
    INCREASE_ZOOM,
    DECREASE_ZOOM,
    SHOOT,
    MOVE_TANK_FORWARD,
    MOVE_TANK_BACKWARD,
    TURN_TANK_LEFT,
    TURN_TANK_RIGHT,

    //TODO: i guess?..
    SET_BLOCK_1,
    SET_BLOCK_2,
    SET_BLOCK_3,
    SET_BLOCK_4,
    SET_BLOCK_5,
    SET_BLOCK_6,
    SET_BLOCK_7,
    SET_BLOCK_8,
    SET_BLOCK_9,
    SET_BLOCK_0,
    SET_BLOCK_F1,
    SET_BLOCK_F2,
    SET_BLOCK_F3,
    SET_BLOCK_F4,
    SET_BLOCK_F5,

    LAST_ACTION // MAKE SURE THIS IS THE LAST ITEM
};

constexpr GameAction operator+(GameAction lhs, int rhs) {
    using underlying = std::underlying_type_t<GameAction>;
    return static_cast<GameAction>(static_cast<underlying>(lhs) + rhs);
}
constexpr GameAction operator-(GameAction lhs, int rhs) {
    using underlying = std::underlying_type_t<GameAction>;
    return static_cast<GameAction>(static_cast<underlying>(lhs) - rhs);
}

constexpr GameAction& operator++(GameAction& action) {
    using underlying = std::underlying_type_t<GameAction>;
    action = static_cast<GameAction>(static_cast<underlying>(action) + 1);
    return action;
}
constexpr GameAction& operator--(GameAction& action) {
    using underlying = std::underlying_type_t<GameAction>;
    action = static_cast<GameAction>(static_cast<underlying>(action) - 1);
    return action;
}

class Input : public InputHandler<GameAction>{
public:
    Input(){};
    //  handler;
};
