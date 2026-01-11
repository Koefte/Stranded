#pragma once

#include <SDL.h>
#include <SDL_mixer.h>
#include <string>
#include <unordered_map>

class SoundManager {
public:
    static SoundManager& instance();

    // Initialize audio subsystem. Returns true on success.
    bool init(int freq = 44100, Uint16 format = MIX_DEFAULT_FORMAT, int channels = 2, int chunksize = 1024);
    void quit();

    bool loadSound(const std::string& id, const std::string& path);
    bool loadMusic(const std::string& id, const std::string& path);

    // Play a sound. Returns channel used or -1 on error.
    int playSound(const std::string& id, int loops = 0, int volume = MIX_MAX_VOLUME);
    void playMusic(const std::string& id, int loops = -1, int volume = MIX_MAX_VOLUME);
    void stopMusic();
    void setMusicVolume(int volume);
    // Stop a looping sound previously played via playSound
    void stopSound(const std::string& id);

private:
    SoundManager() = default;
    ~SoundManager();

    std::unordered_map<std::string, Mix_Chunk*> sounds;
    std::unordered_map<std::string, Mix_Music*> musics;
    std::unordered_map<std::string, int> soundChannels; // maps id -> channel
    bool initialized = false;
};
