#include "sound.h"

// loads any supported sound file, and resamples to mono 16-bit audio with specified sample rate
static Sound S_Load(const WCHAR* path, size_t sampleRate)
{
	Sound sound = { NULL, 0, 0, false };
	HR(MFStartup(MF_VERSION, MFSTARTUP_LITE));

	IMFSourceReader* reader;
	HR(MFCreateSourceReaderFromURL(path, NULL, &reader));

	// read only first audio stream
	HR(reader->SetStreamSelection((DWORD)MF_SOURCE_READER_ALL_STREAMS, FALSE));
	HR(reader->SetStreamSelection((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, TRUE));

	size_t kChannelCount = 1;
	WAVEFORMATEXTENSIBLE format = {};
	format.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	format.Format.nChannels = (WORD)kChannelCount;
	format.Format.nSamplesPerSec = (WORD)sampleRate;
	format.Format.nAvgBytesPerSec = (DWORD)(sampleRate * kChannelCount * sizeof(short));
	format.Format.nBlockAlign = (WORD)(kChannelCount * sizeof(short));
	format.Format.wBitsPerSample = (WORD)(8 * sizeof(short));
	format.Format.cbSize = sizeof(format) - sizeof(format.Format);
	format.Samples.wValidBitsPerSample = 8 * sizeof(short);
	format.dwChannelMask = SPEAKER_FRONT_CENTER;
	format.SubFormat = MEDIASUBTYPE_PCM;

	// Media Foundation in Windows 8+ allows reader to convert output to different format than native
	IMFMediaType* type;
	HR(MFCreateMediaType(&type));
	HR(MFInitMediaTypeFromWaveFormatEx(type, &format.Format, sizeof(format)));
	HR(reader->SetCurrentMediaType((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, NULL, type));
	type->Release();

	size_t used = 0;
	size_t capacity = 0;

	for (;;)
	{
		IMFSample* sample;
		DWORD flags = 0;
		HRESULT hr = reader->ReadSample((DWORD)MF_SOURCE_READER_FIRST_AUDIO_STREAM, 0, NULL, &flags, NULL, &sample);
		if (FAILED(hr))
		{
			break;
		}

		if (flags & MF_SOURCE_READERF_ENDOFSTREAM)
		{
			break;
		}
		assert(flags == 0);

		IMFMediaBuffer* buffer;
		HR(sample->ConvertToContiguousBuffer(&buffer));

		BYTE* data;
		DWORD size;
		HR(buffer->Lock(&data, NULL, &size));
		{
			size_t avail = capacity - used;
			if (avail < size)
			{
				sound.samples = (short *)realloc(sound.samples, capacity += 64 * 1024);
			}
			memcpy((char*)sound.samples + used, data, size);
			used += size;
		}
		HR(buffer->Unlock());

		buffer->Release();
		sample->Release();
	}

	reader->Release();

	HR(MFShutdown());

	sound.pos = sound.count = used / format.Format.nBlockAlign;
	return sound;
}

static void S_Update(Sound* sound, size_t samples)
{
	sound->pos += samples;
	if (sound->loop)
	{
		sound->pos %= sound->count;
	}
	else
	{
		sound->pos = min(sound->pos, sound->count);
	}
}

static void S_Mix(float* outSamples, size_t outSampleCount, float volume, const Sound* sound)
{
	const short* inSamples = sound->samples;
	size_t inPos = sound->pos;
	size_t inCount = sound->count;
	bool inLoop = sound->loop;

	for (size_t i = 0; i < outSampleCount; i++)
	{
		if (inLoop)
		{
			if (inPos == inCount)
			{
				// reset looping sound back to start
				inPos = 0;
			}
		}
		else
		{
			if (inPos >= inCount)
			{
				// non-looping sounds stops playback when done
				break;
			}
		}

		float sample = inSamples[inPos++] * (1.f / 32768.f);
		outSamples[0] += volume * sample;
		outSamples[1] += volume * sample;
		outSamples += 2;
	}
}
