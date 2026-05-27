#include "lamusica/audio/AudioEngine.hpp"
#include "lamusica/session/Project.hpp"
#include "lamusica/session/ProjectDocument.hpp"

#include <iostream>

int main() {
    const lamusica::session::Project project{"Untitled"};
    const lamusica::audio::AudioEngine engine{{}};

    std::cout << "LaMusica DAW bootstrap: " << project.name() << " @ " << engine.config().sampleRate
              << " Hz\n";
    return 0;
}
