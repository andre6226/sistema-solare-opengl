// Coarse image check: compare_png REF.png NEW.png [MAX_MEAN_DIFF]
// Exit 0 when the mean absolute RGB difference (0-255 scale) is within the limit.
// shortcut: one global mean, blind to a small local change (a missing moon shadow
// is ~0.1); upgrade to a per-tile maximum if that ever matters.
#include <SFML/Graphics/Image.hpp>
#include <cstdio>
#include <cstdlib>
#include <cmath>

int main(int argc, char** argv) {
    if (argc < 3) { std::fprintf(stderr, "usage: %s REF.png NEW.png [MAX_MEAN_DIFF]\n", argv[0]); return 2; }
    const double limit = argc > 3 ? std::atof(argv[3]) : 3.0;
    sf::Image ref, img;
    if (!ref.loadFromFile(argv[1]) || !img.loadFromFile(argv[2])) return 2;
    if (ref.getSize() != img.getSize()) { std::printf("size differs\n"); return 1; }

    const std::uint8_t* a = ref.getPixelsPtr();
    const std::uint8_t* b = img.getPixelsPtr();
    const std::size_t n = std::size_t(ref.getSize().x) * ref.getSize().y;
    double sum = 0.0;
    for (std::size_t i = 0; i < n * 4; ++i) {
        if (i % 4 != 3) sum += std::abs(int(a[i]) - int(b[i]));
    }
    const double mean = sum / double(n * 3);
    std::printf("mean diff %.2f (limit %.2f)\n", mean, limit);
    return mean <= limit ? 0 : 1;
}
