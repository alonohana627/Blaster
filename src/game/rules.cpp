#include "rules.hpp"

namespace blaster::game {

namespace {

// helpers

char _held(const bool held, const bool pressed)
{
    return pressed ? 'p' : (held ? 'h' : '.');
}

} // namespace

std::string encodeInput(const Input& input)
{
    std::string text(6, '.');
    text[0] = input.move < 0 ? 'L' : (input.move > 0 ? 'R' : '.');
    text[1] = input.climb < 0 ? 'U' : (input.climb > 0 ? 'D' : '.');
    text[2] = _held(input.jumpHeld, input.jumpPressed);
    text[3] = _held(input.shootHeld, input.shootPressed);
    text[4] = input.slidePressed ? 's' : '.';
    text[5] = input.weaponPrev ? '<' : (input.weaponNext ? '>' : '.');
    return text;
}

std::optional<Input> decodeInput(const std::string_view text)
{
    if (text.size() != 6) {
        return std::nullopt;
    }
    Input in;
    switch (text[0]) {
    case 'L': in.move = -1; break;
    case 'R': in.move = 1; break;
    case '.': break;
    default: return std::nullopt;
    }
    switch (text[1]) {
    case 'U': in.climb = -1; break;
    case 'D': in.climb = 1; break;
    case '.': break;
    default: return std::nullopt;
    }
    const auto held = [](const char c, bool& isHeld, bool& isPressed) {
        isPressed = c == 'p';
        isHeld = c == 'p' || c == 'h';
        return c == 'p' || c == 'h' || c == '.';
    };
    if (!held(text[2], in.jumpHeld, in.jumpPressed) || !held(text[3], in.shootHeld, in.shootPressed)) {
        return std::nullopt;
    }
    if (text[4] != 's' && text[4] != '.') {
        return std::nullopt;
    }
    in.slidePressed = text[4] == 's';
    if (text[5] != '<' && text[5] != '>' && text[5] != '.') {
        return std::nullopt;
    }
    in.weaponPrev = text[5] == '<';
    in.weaponNext = text[5] == '>';
    return in;
}

gamelib::core::PlatformerTuning heroTuning(const bool icy)
{
    gamelib::core::PlatformerTuning t;
    t.maxSpeed = kRunSpeed;
    // Nearly instant starts and stops, on the ground and in the air: the
    // classic hero turns on a dime. Ice keeps the speed for a while.
    t.groundAccel = icy ? 0.045f : 0.7f;
    t.groundDecel = icy ? 0.025f : 0.9f;
    t.airAccel = 0.7f;
    t.airDecel = 0.9f;
    t.gravity = 0.25f;
    t.maxFallSpeed = 7.0f;
    // 3.5 tiles high (about 44 ticks in the air, 3.8 tiles across at run speed);
    // releasing jump while rising drops the speed to almost nothing.
    t.jumpSpeed = gamelib::core::PlatformerTuning::jumpSpeedFor(3.5f * kTile, t.gravity);
    t.jumpCut = 0.12f;
    t.coyoteTicks = 3;
    t.jumpBufferTicks = 4;
    t.cornerCorrection = 3.0f;
    t.climbSpeed = kClimbSpeed;
    t.climbJumpSpeed = {0.0f, 0.0f}; // jump on a ladder lets go
    t.hazardInset = 3.0f;
    return t;
}

gamelib::core::PlatformerTuning slideTuning()
{
    gamelib::core::PlatformerTuning t = heroTuning(false);
    t.maxSpeed = kSlideSpeed;
    t.groundAccel = 10.0f;
    t.groundDecel = 10.0f;
    t.airAccel = 0.0f;
    t.airDecel = 0.0f;
    t.climbSpeed = 0.0f;
    return t;
}

} // namespace blaster::game
