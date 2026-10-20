// SFML frontend: window, keyboard, beep, and the 60 Hz frame loop.

#include <SFML/Graphics.hpp>
#include <SFML/Audio.hpp>
#include <SFML/Window.hpp>

#include <cstdio>
#include <cstdlib>
#include <vector>

#include "chip8.h"

// static constexpr unsigned int VIDEO_W = 64;
// static constexpr unsigned int VIDEO_H = 32;
static constexpr float SCALE = 12.0f;
static constexpr double FRAME_MS = 1000.0 / 60.0;

// CHIP-8 keypad -> keyboard, using physical key positions.
static const sf::Keyboard::Scancode KEYMAP[16] = {
    sf::Keyboard::Scancode::X,
    sf::Keyboard::Scancode::Num1,
    sf::Keyboard::Scancode::Num2,
    sf::Keyboard::Scancode::Num3,
    sf::Keyboard::Scancode::Q,
    sf::Keyboard::Scancode::W,
    sf::Keyboard::Scancode::E,
    sf::Keyboard::Scancode::A,
    sf::Keyboard::Scancode::S,
    sf::Keyboard::Scancode::D,
    sf::Keyboard::Scancode::Z,
    sf::Keyboard::Scancode::C,
    sf::Keyboard::Scancode::Num4,
    sf::Keyboard::Scancode::R,
    sf::Keyboard::Scancode::F,
    sf::Keyboard::Scancode::V
};

int main(int argc, char** argv) {
    if (argc < 2) {
        std::fprintf(
            stderr,
            "usage: %s <rom> [instructions-per-frame (default 10 = ~600 Hz)]\n",
            argv[0]
        );
        return 2;
    }

    const int perFrame = argc > 2 ? std::atoi(argv[2]) : 10;

    if (perFrame <= 0) {
        std::fprintf(stderr, "instructions-per-frame must be positive\n");
        return 2;
    }

    Chip8 chip;

    if (!chip.loadFile(argv[1])) {
        std::fprintf(stderr, "could not load ROM '%s'\n", argv[1]);
        return 1;
    }

    // Window setup: SFML 3 uses Vector2 values for its dimensions.
    sf::RenderWindow window(
        sf::VideoMode({VIDEO_W * 12, VIDEO_H * 12}),
        "CHIP-8"
    );

    window.setFramerateLimit(60);

    // Texture and sprite.
    sf::Texture texture({VIDEO_W, VIDEO_H});
    sf::Sprite sprite(texture);
    sprite.setScale({SCALE, SCALE});

    // Generate a one-second square-wave beep.
    constexpr unsigned int SAMPLE_RATE = 44100;
    std::vector<std::int16_t> audioSamples(SAMPLE_RATE);

    for (unsigned int i = 0; i < SAMPLE_RATE; ++i) {
        audioSamples[i] = ((i / 50) % 2) ? 3000 : -3000;
    }

    sf::SoundBuffer beepBuffer;

    const std::vector<sf::SoundChannel> monoChannelMap = {
        sf::SoundChannel::Mono
    };

    if (!beepBuffer.loadFromSamples(
            audioSamples.data(),
            audioSamples.size(),
            1,
            SAMPLE_RATE,
            monoChannelMap)) {
        std::fprintf(stderr, "could not initialize beep audio\n");
        return 1;
    }

    sf::Sound beepSound(beepBuffer);
    beepSound.setLooping(true);

    // Timing state.
    sf::Clock clock;
    double owed = 0.0;

    while (window.isOpen()) {
        // SFML 3: pollEvent() returns an optional event.
        while (const std::optional event = window.pollEvent()) {
            if (event->is<sf::Event::Closed>()) {
                window.close();
            } else if (
                const auto* keyPressed =
                    event->getIf<sf::Event::KeyPressed>()
            ) {
                if (keyPressed->scancode ==
                    sf::Keyboard::Scancode::Escape) {
                    window.close();
                }
            }
        }

        // Read the physical keyboard positions.
        for (int k = 0; k < 16; ++k) {
            chip.keys[k] = sf::Keyboard::isKeyPressed(KEYMAP[k]);
        }

        // Fixed 60 Hz emulation steps.
        owed += clock.restart().asSeconds() * 1000.0;

        // Avoid a huge catch-up after a long stall.
        if (owed > 100.0) {
            owed = 100.0;
        }

        bool redraw = false;

        while (owed >= FRAME_MS) {
            for (int i = 0; i < perFrame; ++i) {
                chip.cycle();
            }

            chip.tick();  // CHIP-8 timers advance at 60 Hz.

            owed -= FRAME_MS;
            redraw = true;
        }

        // Audio state update.
        if (chip.soundTimer > 0 &&
            beepSound.getStatus() != sf::SoundSource::Status::Playing) {
            beepSound.play();
        } else if (
            chip.soundTimer == 0 &&
            beepSound.getStatus() == sf::SoundSource::Status::Playing
        ) {
            beepSound.pause();
        }

        // Render only when an emulation step has occurred.
        if (redraw) {
            // Assumes chip.video contains RGBA pixels.
            texture.update(
                reinterpret_cast<const std::uint8_t*>(chip.video)
            );

            window.clear();
            window.draw(sprite);
            window.display();
        }
    }

    return 0;
}