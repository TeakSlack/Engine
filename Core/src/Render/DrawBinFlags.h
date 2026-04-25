#ifndef DRAW_BIN_FLAGS_H
#define DRAW_BIN_FLAGS_H

// Viewport / layer - main scene, shadow, mirror, UI, etc... ---------------
enum class DrawBinFlags : uint8_t
{
    None = 0,
    DepthPrepass = 1 << 1,
    ForwardOpaque = 1 << 2,
    Shadow = 1 << 3,
    Transparent = 1 << 4
};

inline DrawBinFlags operator|(DrawBinFlags a, DrawBinFlags b)
{
    return static_cast<DrawBinFlags>(static_cast<uint8_t>(a) | static_cast<uint8_t>(b));
}

#endif // DRAW_BIN_FLAGS_H