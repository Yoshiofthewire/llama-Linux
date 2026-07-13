#pragma once
#include <QtGlobal>

struct SemanticColor
{
    quint32 rgb;   // 0xRRGGBB
    double alpha;  // 0.0-1.0, 1.0 for fully opaque
};

namespace SemanticColors
{
    inline constexpr SemanticColor danger        { 0xFF5F5F, 1.0 };
    inline constexpr SemanticColor dangerBorder   { 0xFFB4AB, 0.4 };
    inline constexpr SemanticColor dangerFill     { 0xFF5F5F, 0.12 };
    inline constexpr SemanticColor warning        { 0xFFD64D, 1.0 };
    inline constexpr SemanticColor successBorder  { 0x7BBF7B, 1.0 };
    inline constexpr SemanticColor successText    { 0xA5DCA5, 1.0 };
}
