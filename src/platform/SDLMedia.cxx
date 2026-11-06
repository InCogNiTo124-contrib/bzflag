/* bzflag
 * Copyright (c) 1993-2025 Tim Riker
 *
 * This package is free software;  you can redistribute it and/or
 * modify it under the terms of the license found in the file
 * named COPYING that should have accompanied this file.
 *
 * THIS PACKAGE IS PROVIDED ``AS IS'' AND WITHOUT ANY EXPRESS OR
 * IMPLIED WARRANTIES, INCLUDING, WITHOUT LIMITATION, THE IMPLIED
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE.
 */

#include "common.h"

#include <stdlib.h>
#include <string>
#include <vector>
#include <sys/types.h>
#include <sys/stat.h>
#ifdef _WIN32
#  ifndef S_ISDIR
#    define S_ISDIR(m) ((m) & _S_IFDIR)
#  endif
#endif


#include "SDLMedia.h"
#include "ErrorHandler.h"

/* ugh, more mixing with libCommon */
#include "StateDatabase.h"


static const int defaultAudioRate=22050;

//
// SDLMedia
//

SDLMedia::SDLMedia() : BzfMedia(),
    audioReady(false),
    audioOutputRate(),
    cmdQueue(),
    cmdFill(0),
    userCallback(),
    audioStream(nullptr)
{
}

void SDLMedia::setMediaDirectory(const std::string& _dir)
{
    struct stat statbuf;
    const char *mdir = _dir.c_str();

#ifdef __APPLE__
    extern char *GetMacOSXDataPath(void);
    if ((stat(mdir, &statbuf) != 0) || !S_ISDIR(statbuf.st_mode))
    {
        /* try the Resource folder if invoked from a .app bundled */
        mdir = GetMacOSXDataPath();
        if (mdir)
        {
            BZDB.set("directory", mdir);
            BZDB.setPersistent("directory", false);
        }
    }
#endif
    if ((stat(mdir, &statbuf) != 0) || !S_ISDIR(statbuf.st_mode))
    {
        /* try a simple 'data' dir in current directory (source build invocation) */
        std::string defaultdir = DEFAULT_MEDIA_DIR;
        mdir = defaultdir.c_str();
        if (mdir)
        {
            BZDB.set("directory", mdir);
            BZDB.setPersistent("directory", false);
        }
    }
    if ((stat(mdir, &statbuf) != 0) || !S_ISDIR(statbuf.st_mode))
    {
        /* give up, revert to passed in directory */
        mdir = _dir.c_str();
    }

    mediaDir = mdir;
}

double          SDLMedia::stopwatch(bool start)
{
    Uint64 currentTick = SDL_GetPerformanceCounter();

    if (start)
    {
        stopwatchTime = currentTick;
        return 0.0;
    }
    if (currentTick >= stopwatchTime)
        return (double) (currentTick - stopwatchTime) /
               SDL_GetPerformanceFrequency(); // sec
    else
        //Clock is wrapped : happens after 49 days
        //Should be "wrap value" - stopwatchtime. Now approx.
        return (double) currentTick / SDL_GetPerformanceFrequency();
}

bool            SDLMedia::openAudio()
{
    // don't re-initialize
    if (audioReady) return false;

    if (!SDL_InitSubSystem(SDL_INIT_AUDIO))
    {
        printFatalError("Could not initialize SDL-Audio: %s.\n", SDL_GetError());
        exit(-1);
    };

    // what the frequency?
    audioOutputRate = defaultAudioRate;

    SDL_AudioSpec spec;
    SDL_zero(spec);
    spec.format = SDL_AUDIO_S16;
    spec.channels = 2;
    spec.freq = audioOutputRate;

    SDL_AudioDeviceID devId = SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK;
    if (!audioDevice.empty())
    {
        int count = 0;
        SDL_AudioDeviceID *devs = SDL_GetAudioPlaybackDevices(&count);
        if (devs)
        {
            for (int i = 0; i < count; i++)
            {
                const char *name = SDL_GetAudioDeviceName(devs[i]);
                if (name && (audioDevice == name || strcasecmp(audioDevice.c_str(), name) == 0))
                {
                    devId = devs[i];
                    break;
                }
            }
            SDL_free(devs);
        }
    }

    audioStream = SDL_OpenAudioDeviceStream(devId, &spec, &fillAudioWrapper, this);
    if (!audioStream)
    {
        fprintf(stderr, "Couldn't open audio stream: %s\n", SDL_GetError());
        return false;
    }

    // ready to go
    audioReady = true;

    return true;
}

void            SDLMedia::closeAudio()
{
    if (audioStream)
    {
        SDL_DestroyAudioStream(audioStream);
        audioStream = nullptr;
    }
    SDL_QuitSubSystem(SDL_INIT_AUDIO);
    audioReady    = false;
}

void            SDLMedia::startAudioCallback(bool (*proc)(void))
{
    userCallback = proc;
    // Stop sending silence and start calling audio callback
    if (audioStream)
        SDL_ResumeAudioStreamDevice(audioStream);
}

void            SDLMedia::writeSoundCommand(const void* cmd, int len)
{
    if (!audioReady) return;

    if (audioStream)
        SDL_LockAudioStream(audioStream);

    // Discard command if full
    if ((cmdFill + len) < 2048)
    {
        memcpy(&cmdQueue[cmdFill], cmd, len);
        // We should awake audioSleep - but game become unplayable
        // using here an SDL_CondSignal(wakeCond)
        cmdFill += len;
    }

    if (audioStream)
        SDL_UnlockAudioStream(audioStream);
}

bool            SDLMedia::readSoundCommand(void* cmd, int len)
{
    bool result = false;

    if (cmdFill >= len)
    {
        memcpy(cmd, cmdQueue, len);
        // repack list of command waiting to be processed
        memmove(cmdQueue, &cmdQueue[len], cmdFill - len);
        cmdFill -= len;
        result = true;
    }
    return result;
}

int         SDLMedia::getAudioOutputRate() const
{
    return audioOutputRate;
}

int         SDLMedia::getAudioBufferSize() const
{
    return 1024;
}

int         SDLMedia::getAudioBufferChunkSize() const
{
    return 512;
}

void SDLMedia::fillAudio(int additional_amount)
{
    if (!userCallback || !audioStream)
        return;
    const int bytesPerChunk = getAudioBufferSize() * sizeof(int16_t);
    while (additional_amount > 0)
    {
        userCallback();
        additional_amount -= bytesPerChunk;
    }
}

void SDLCALL SDLMedia::fillAudioWrapper(void *userdata, SDL_AudioStream *, int additional_amount, int)
{
    SDLMedia *me = static_cast<SDLMedia *>(userdata);
    me->fillAudio(additional_amount);
}

void SDLMedia::writeAudioFrames(const float* samples, int numFrames)
{
    if (!audioStream || numFrames <= 0)
        return;
    const int numSamples = numFrames * 2;
    std::vector<int16_t> buffer(numSamples);
    for (int j = 0; j < numSamples; j++)
    {
        if (samples[j] < -32767.0f)
            buffer[j] = -32767;
        else if (samples[j] > 32767.0f)
            buffer[j] = 32767;
        else
            buffer[j] = static_cast<int16_t>(samples[j]);
    }
    SDL_PutAudioStreamData(audioStream, buffer.data(), numSamples * sizeof(int16_t));
}

// Setting Audio Driver
void    SDLMedia::setDriver(std::string driverName)
{
    SDL_SetHint(SDL_HINT_AUDIO_DRIVER, driverName.c_str());
}

// Setting Audio Device
void    SDLMedia::setDevice(std::string deviceName)
{
    audioDevice = std::move(deviceName);
}

float*      SDLMedia::doReadSound(const std::string &filename, int &numFrames,
                                  int &rate) const
{
    SDL_AudioSpec wav_spec;
    Uint32 wav_length = 0;
    Uint8 *wav_buffer = nullptr;

    rate = defaultAudioRate;
    if (!SDL_LoadWAV(filename.c_str(), &wav_spec, &wav_buffer, &wav_length))
        return NULL;

    SDL_AudioSpec target_spec;
    SDL_zero(target_spec);
    target_spec.format = SDL_AUDIO_S16;
    target_spec.channels = 2;
    target_spec.freq = audioOutputRate > 0 ? audioOutputRate : defaultAudioRate;

    Uint8 *cvt_buf = nullptr;
    int cvt_len = 0;
    if (!SDL_ConvertAudioSamples(&wav_spec, wav_buffer, (int)wav_length, &target_spec, &cvt_buf, &cvt_len))
    {
        printFatalError("Could not convert Wav file %s: %s.\n",
                        filename.c_str(), SDL_GetError());
        SDL_free(wav_buffer);
        return NULL;
    }
    SDL_free(wav_buffer);

    numFrames = cvt_len / 4;
    const int16_t *cvt16 = reinterpret_cast<const int16_t *>(cvt_buf);
    float *data = new float[numFrames * 2];
    for (int i = 0; i < numFrames * 2; i++)
        data[i] = cvt16[i];
    SDL_free(cvt_buf);
    return data;
}

void SDLMedia::audioDriver(std::string& driverName)
{
    const char *driver = SDL_GetCurrentAudioDriver();
    if (driver)
        driverName = driver;
    else
        driverName = "audio not available";
}

// Local Variables: ***
// mode: C++ ***
// tab-width: 4 ***
// c-basic-offset: 4 ***
// indent-tabs-mode: nil ***
// End: ***
// ex: shiftwidth=4 tabstop=4
