#include "SoundManager.hpp"
#include <iostream>

SoundManager& SoundManager::instance() {
    static SoundManager inst;
    return inst;
}

bool SoundManager::init(int freq, Uint16 format, int channels, int chunksize) {
    if (initialized) return true;

    if (SDL_WasInit(SDL_INIT_AUDIO) == 0) {
        if (SDL_InitSubSystem(SDL_INIT_AUDIO) != 0) {
            std::cerr << "SDL audio init failed: " << SDL_GetError() << std::endl;
            return false;
        }
    }

    int flags = MIX_INIT_OGG | MIX_INIT_MP3;
    int initted = Mix_Init(flags);
    if ((initted & flags) != flags) {
        // Not fatal, some formats may be missing
        std::cerr << "Mix_Init: could not init all loaders: " << Mix_GetError() << std::endl;
    }

    if (Mix_OpenAudio(freq, format, channels, chunksize) < 0) {
        std::cerr << "Mix_OpenAudio failed: " << Mix_GetError() << std::endl;
        return false;
    }

    Mix_AllocateChannels(32);
    initialized = true;
    return true;
}

void SoundManager::quit() {
    if (!initialized) return;

    for (auto& kv : sounds) {
        if (kv.second) Mix_FreeChunk(kv.second);
    }
    sounds.clear();

    for (auto& kv : musics) {
        if (kv.second) Mix_FreeMusic(kv.second);
    }
    musics.clear();

    Mix_CloseAudio();
    Mix_Quit();
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    initialized = false;
}

bool SoundManager::loadSound(const std::string& id, const std::string& path) {
    if (!initialized) return false;
    Mix_Chunk* chunk = Mix_LoadWAV(path.c_str());
    if (!chunk) {
        std::cerr << "Failed to load sound '" << path << "': " << Mix_GetError() << std::endl;
        return false;
    }
    sounds[id] = chunk;
    return true;
}

bool SoundManager::loadMusic(const std::string& id, const std::string& path) {
    if (!initialized) return false;
    Mix_Music* music = Mix_LoadMUS(path.c_str());
    if (!music) {
        std::cerr << "Failed to load music '" << path << "': " << Mix_GetError() << std::endl;
        return false;
    }
    musics[id] = music;
    return true;
}

int SoundManager::playSound(const std::string& id, int loops, int volume) {
    if (!initialized) return -1;
    auto it = sounds.find(id);
    if (it == sounds.end()) return -1;
    Mix_VolumeChunk(it->second, volume);
    int channel = Mix_PlayChannel(-1, it->second, loops);
    if (channel >= 0 && loops < 0) {
        soundChannels[id] = channel;
    }
    return channel;
}

void SoundManager::stopSound(const std::string& id) {
    if (!initialized) return;
    auto it = soundChannels.find(id);
    if (it == soundChannels.end()) return;
    int channel = it->second;
    Mix_HaltChannel(channel);
    soundChannels.erase(it);
}

void SoundManager::playMusic(const std::string& id, int loops, int volume) {
    if (!initialized) return;
    auto it = musics.find(id);
    if (it == musics.end()) return;
    Mix_VolumeMusic(volume);
    Mix_PlayMusic(it->second, loops);
}

void SoundManager::stopMusic() {
    if (!initialized) return;
    Mix_HaltMusic();
}

void SoundManager::setMusicVolume(int volume) {
    if (!initialized) return;
    Mix_VolumeMusic(volume);
}

SoundManager::~SoundManager() {
    quit();
}
