// Wrapper for WASAPI written by mmozeiko
// https://gist.github.com/mmozeiko/5a5b168e61aff4c1eaec0381da62808f

#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <initguid.h>
#include <objbase.h>
#include <uuids.h>
#include <avrt.h>
#include <audioclient.h>
#include <mmdeviceapi.h>

#include <stddef.h>

// "count" means sample count (for example, 1 sample = 2 floats for stereo)
// "offset" or "size" means byte count

typedef struct {
	// public part
	
	// describes sampleBuffer format
	WAVEFORMATEX* bufferFormat;

	// use these values only between LockBuffer/UnlockBuffer calls
	void* sampleBuffer;  // ringbuffer for interleaved samples, no need to handle wrapping
	size_t sampleCount;  // how big is buffer in samples
	size_t playCount;    // how many samples were actually used for playback since previous LockBuffer call

	// private
	IAudioClient* client;
	HANDLE event;
	HANDLE thread;
	LONG stop;
	LONG lock;
	BYTE* buffer1;
	BYTE* buffer2;
	UINT32 outSize;              // output buffer size in bytes
	UINT32 rbSize;               // ringbuffer size, always power of 2
	UINT32 bufferUsed;           // how many samples are used from buffer
	BOOL bufferFirstLock;        // true when BufferLock is used at least once
	volatile LONG rbReadOffset;  // offset to read from buffer
	volatile LONG rbLockOffset;  // offset up to what buffer is currently being used
	volatile LONG rbWriteOffset; // offset up to what buffer is filled
} WasapiAudio;

//
// interface
//

// pass 0 for rate/count/mask to get default format of output device (use audio->bufferFormat)
// channelMask is bitmask of values from table here: https://learn.microsoft.com/en-us/windows/win32/api/mmreg/ns-mmreg-waveformatextensible#remarks
static void WA_Start(WasapiAudio* audio, size_t sampleRate, size_t channelCount, DWORD channelMask);

// stops the playback and releases resources
static void WA_Stop(WasapiAudio* audio);

// once locked, then you're allowed to write samples into the ringbuffer
// use only sampleBuffer, sampleCount and submittedCount members
static void WA_LockBuffer(WasapiAudio* audio);
static void WA_UnlockBuffer(WasapiAudio* audio, size_t writtenCount);

//
// implementation
//

// TODO: what's missing here:
// * proper error handling, like when no audio device is present (currently asserts)
// * automatically switch to new device when default audio device changes (IMMNotificationClient)

#pragma comment (lib, "avrt")
#pragma comment (lib, "ole32")
#pragma comment (lib, "onecore")

#include <assert.h>
#define HR(stmt) do { HRESULT _hr = stmt; assert(SUCCEEDED(_hr)); } while (0)

// why these are missing from windows libs? :(
DEFINE_GUID(CLSID_MMDeviceEnumerator, 0xbcde0395, 0xe52f, 0x467c, 0x8e, 0x3d, 0xc4, 0x57, 0x92, 0x91, 0x69, 0x2e);
DEFINE_GUID(IID_IMMDeviceEnumerator,  0xa95664d2, 0x9614, 0x4f35, 0xa7, 0x46, 0xde, 0x8d, 0xb6, 0x36, 0x17, 0xe6);
DEFINE_GUID(IID_IAudioClient,         0x1cb9ad4c, 0xdbfa, 0x4c32, 0xb1, 0x78, 0xc2, 0xf5, 0x68, 0xa7, 0x03, 0xb2);
DEFINE_GUID(IID_IAudioClient3,        0x7ed4ee07, 0x8e67, 0x4cd4, 0x8c, 0x1a, 0x2b, 0x7a, 0x59, 0x87, 0xad, 0x42);
DEFINE_GUID(IID_IAudioRenderClient,   0xf294acfc, 0x3146, 0x4483, 0xa7, 0xbf, 0xad, 0xdc, 0xa7, 0xc2, 0x60, 0xe2);

static void WA__Lock(WasapiAudio* audio)
{
	// loop while audio->lock != FALSE
	while (InterlockedCompareExchange(&audio->lock, TRUE, FALSE) != FALSE)
	{
		// wait while audio->lock == locked
		LONG locked = FALSE;
		WaitOnAddress(&audio->lock, &locked, sizeof(locked), INFINITE);
	}
	// now audio->lock == TRUE
}

static void WA__Unlock(WasapiAudio* audio)
{
	// audio->lock = FALSE
	InterlockedExchange(&audio->lock, FALSE);
	WakeByAddressSingle(&audio->lock);
}

static DWORD CALLBACK WA__AudioThread(LPVOID arg)
{
	WasapiAudio* audio = (WasapiAudio*)arg;

	DWORD task = 0;
	HANDLE handle = AvSetMmThreadCharacteristicsW(L"Pro Audio", &task);
	assert(handle);

	IAudioClient* client = audio->client;

	IAudioRenderClient* playback;
	HR(client->GetService(IID_IAudioRenderClient, (LPVOID*)&playback));

	// get audio buffer size in samples
	UINT32 bufferSamples;
	HR(client->GetBufferSize(&bufferSamples));

	// start the playback
	HR(client->Start());

	UINT32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	UINT32 rbMask = audio->rbSize - 1;
	BYTE* input = audio->buffer1;

	while (WaitForSingleObject(audio->event, INFINITE) == WAIT_OBJECT_0)
	{
		if (InterlockedExchange(&audio->stop, FALSE))
		{
			break;
		}

		UINT32 paddingSamples;
		HR(client->GetCurrentPadding(&paddingSamples));

		// get output buffer from WASAPI
		BYTE* output;
		UINT32 maxOutputSamples = bufferSamples - paddingSamples;
		HR(playback->GetBuffer(maxOutputSamples, &output));

		WA__Lock(audio);

		UINT32 readOffset = audio->rbReadOffset;
		UINT32 writeOffset = audio->rbWriteOffset;

		// how many bytes available to read from ringbuffer
		UINT32 availableSize = writeOffset - readOffset;

		// how many samples available
		UINT32 availableSamples = availableSize / bytesPerSample;

		// will use up to max that's possible to output
		UINT32 useSamples = min(availableSamples, maxOutputSamples);

		// how many bytes to use
		UINT32 useSize = useSamples * bytesPerSample;

		// lock range [read, lock) that memcpy will read from below
		audio->rbLockOffset = readOffset + useSize;

		// will always submit required amount of samples, but if there's not enough to use, then submit silence
		UINT32 submitCount = useSamples ? useSamples : maxOutputSamples;
		DWORD flags = useSamples ? 0 : AUDCLNT_BUFFERFLAGS_SILENT;

		// remember how many samples are submitted
		audio->bufferUsed += submitCount;

		WA__Unlock(audio);

		// copy bytes to output
		// safe to do it outside WA__Lock/Unlock, because nobody will overwrite [read, lock) interval
		memcpy(output, input + (readOffset & rbMask), useSize);

		// advance read offset up to lock position, allows writing to [read, lock) interval
		InterlockedAdd(&audio->rbReadOffset, useSize);

		// submit output buffer to WASAPI
		HR(playback->ReleaseBuffer(submitCount, flags));
	}

	// stop the playback
	HR(client->Stop());
	playback->Release();

	AvRevertMmThreadCharacteristics(handle);
	return 0;
}

static DWORD RoundUpPow2(DWORD value)
{
	unsigned long index;
	_BitScanReverse(&index, value - 1);
	assert(index < 31);
	return 1U << (index + 1);
}

static void WA_Start(WasapiAudio* audio, size_t sampleRate, size_t channelCount, DWORD channelMask)
{
	// initialize COM
	HR(CoInitializeEx(NULL, COINIT_APARTMENTTHREADED));

	// create enumerator to get audio device
	IMMDeviceEnumerator* enumerator;
	HR(CoCreateInstance(CLSID_MMDeviceEnumerator, NULL, CLSCTX_ALL, IID_IMMDeviceEnumerator, (LPVOID*)&enumerator));

	// get default playback device
	IMMDevice* device;
	HR(enumerator->GetDefaultAudioEndpoint(eRender, eConsole, &device));
	enumerator->Release();

	// create audio client for device
	HR(device->Activate(IID_IAudioClient, CLSCTX_ALL, NULL, (LPVOID*)&audio->client));
	device->Release();

	WAVEFORMATEXTENSIBLE formatEx = {};
	formatEx.Format.wFormatTag = WAVE_FORMAT_EXTENSIBLE;
	formatEx.Format.nChannels = (WORD)channelCount;
	formatEx.Format.nSamplesPerSec = (WORD)sampleRate;
	formatEx.Format.nAvgBytesPerSec = (DWORD)(sampleRate * channelCount * sizeof(float));
	formatEx.Format.nBlockAlign = (WORD)(channelCount * sizeof(float));
	formatEx.Format.wBitsPerSample = (WORD)(8 * sizeof(float));
	formatEx.Format.cbSize = sizeof(formatEx) - sizeof(formatEx.Format);
	formatEx.Samples.wValidBitsPerSample = 8 * sizeof(float);
	formatEx.dwChannelMask = channelMask;
	formatEx.SubFormat = MEDIASUBTYPE_IEEE_FLOAT;

	WAVEFORMATEX* wfx;
	if (sampleRate == 0 || channelCount == 0 || channelMask == 0)
	{
		// use native mixing format
		HR(audio->client->GetMixFormat(&wfx));
		audio->bufferFormat = wfx;
	}
	else
	{
		// will use our format
		wfx = &formatEx.Format;
	}
	
	BOOL clientInitialized = FALSE;

	// try to initialize client with newer functionality in Windows 10, no AUTOCONVERTPCM allowed
	IAudioClient3* client3;
	if (SUCCEEDED(audio->client->QueryInterface(IID_IAudioClient3, (LPVOID*)&client3)))
	{
		// minimum buffer size will typically be 480 samples (10msec @ 48khz)
		// but it can be 128 samples (2.66 msec @ 48khz) if driver is properly installed
		// see bullet-point instructions here: https://learn.microsoft.com/en-us/windows-hardware/drivers/audio/low-latency-audio#measurement-tools
		UINT32 defaultPeriodSamples, fundamentalPeriodSamples, minPeriodSamples, maxPeriodSamples;
		HR(client3->GetSharedModeEnginePeriod(wfx, &defaultPeriodSamples, &fundamentalPeriodSamples, &minPeriodSamples, &maxPeriodSamples));

		const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK;
		if (SUCCEEDED(client3->InitializeSharedAudioStream(flags, minPeriodSamples, wfx, NULL)))
		{
			clientInitialized = TRUE;
		}

		client3->Release();
	}

	if (!clientInitialized)
	{
		// get duration for shared-mode streams, this will typically be 480 samples (10msec @ 48khz)
		REFERENCE_TIME duration;
		HR(audio->client->GetDevicePeriod(&duration, NULL));

		// initialize audio playback
		const DWORD flags = AUDCLNT_STREAMFLAGS_EVENTCALLBACK | AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM | AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;
		HR(audio->client->Initialize(AUDCLNT_SHAREMODE_SHARED, flags, duration, 0, wfx, NULL));
	}

	if (wfx == &formatEx.Format)
	{
		HR(audio->client->GetMixFormat(&wfx));
		audio->bufferFormat = wfx;
	}

	UINT32 bufferSamples;
	HR(audio->client->GetBufferSize(&bufferSamples));
	audio->outSize = bufferSamples * audio->bufferFormat->nBlockAlign;

	// setup event handle to wait on
	audio->event = CreateEventW(NULL, FALSE, FALSE, NULL);
	HR(audio->client->SetEventHandle(audio->event));

	// use at least 64KB or 1 second whichever is larger, and round upwards to pow2 for ringbuffer
	DWORD rbSize = RoundUpPow2(max(64 * 1024, audio->bufferFormat->nAvgBytesPerSec));

	// reserve virtual address placeholder for 2x size for magic ringbuffer
	char* placeholder1 = (char *)VirtualAlloc2(NULL, NULL, 2 * rbSize, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER, PAGE_NOACCESS, NULL, 0);
	char* placeholder2 = placeholder1 + rbSize;
	assert(placeholder1);

	// split allocated address space in half
	BOOL ok = VirtualFree(placeholder1, rbSize, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);
	assert(ok);

	// create page-file backed section for buffer
	HANDLE section = CreateFileMappingW(INVALID_HANDLE_VALUE, NULL, PAGE_READWRITE, 0, rbSize, NULL);
	assert(section);

	// map same section into both addresses
	void* view1 = MapViewOfFile3(section, NULL, placeholder1, 0, rbSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
	void* view2 = MapViewOfFile3(section, NULL, placeholder2, 0, rbSize, MEM_REPLACE_PLACEHOLDER, PAGE_READWRITE, NULL, 0);
	assert(view1 && view2);

	audio->sampleBuffer = NULL;
	audio->sampleCount = 0;
	audio->playCount = 0;
	audio->buffer1 = (BYTE *)view1;
	audio->buffer2 = (BYTE *)view2;
	audio->rbSize = rbSize;
	audio->bufferUsed = 0;
	audio->bufferFirstLock = TRUE;
	audio->rbReadOffset = 0;
	audio->rbLockOffset = 0;
	audio->rbWriteOffset = 0;
	InterlockedExchange(&audio->stop, FALSE);
	InterlockedExchange(&audio->lock, FALSE);
	audio->thread = CreateThread(NULL, 0, &WA__AudioThread, audio, 0, NULL);

	// this is ok, actual memory will be freed only when it is unmapped
	VirtualFree(placeholder1, 0, MEM_RELEASE);
	VirtualFree(placeholder2, 0, MEM_RELEASE);
	CloseHandle(section);
}

static void WA_Stop(WasapiAudio* audio)
{
	// notify thread to stop
	InterlockedExchange(&audio->stop, TRUE);
	SetEvent(audio->event);

	// wait for thread to finish
	WaitForSingleObject(audio->thread, INFINITE);
	CloseHandle(audio->thread);
	CloseHandle(audio->event);

	// release ringbuffer
	UnmapViewOfFileEx(audio->buffer1, 0);
	UnmapViewOfFileEx(audio->buffer2, 0);

	// release audio client
	CoTaskMemFree(audio->bufferFormat);
	audio->client->Release();

	// done with COM
	CoUninitialize();
}

static void WA_LockBuffer(WasapiAudio* audio)
{
	UINT32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	UINT32 rbSize = audio->rbSize;
	UINT32 outSize = audio->outSize;

	WA__Lock(audio);

	UINT32 readOffset = audio->rbReadOffset;
	UINT32 lockOffset = audio->rbLockOffset;
	UINT32 writeOffset = audio->rbWriteOffset;

	// how many bytes are used in buffer by reader = [read, lock) range
	UINT32 usedSize = lockOffset - readOffset;

	// make sure there are samples available for one wasapi buffer submission
	// so in case audio thread needs samples before UnlockBuffer is called, it can get some 
	if (usedSize < outSize)
	{
		// how many bytes available in current buffer = [read, write) range
		UINT32 availSize = writeOffset - readOffset;

		// if [read, lock) is smaller than outSize buffer, then increase lock to [read, read+outSize) range
		usedSize = min(outSize, availSize);
		audio->rbLockOffset = lockOffset = readOffset + usedSize;
	}

	// how many bytes can be written to buffer
	UINT32 writeSize = rbSize - usedSize;

	// reset write marker to beginning of lock offset (can start writing there)
	audio->rbWriteOffset = lockOffset;

	// reset play sample count, use 0 for playCount when LockBuffer is called first time
	audio->playCount = audio->bufferFirstLock ? 0 : audio->bufferUsed;
	audio->bufferFirstLock = FALSE;
	audio->bufferUsed = 0;

	WA__Unlock(audio);

	// buffer offset/size where to write
	// safe to write in [write, read) range, because reading happen in [read, lock) range (lock==write)
	audio->sampleBuffer = audio->buffer1 + (lockOffset & (rbSize - 1));
	audio->sampleCount = writeSize / bytesPerSample;
}

static void WA_UnlockBuffer(WasapiAudio* audio, size_t writtenSamples)
{
	UINT32 bytesPerSample = audio->bufferFormat->nBlockAlign;
	size_t writeSize = writtenSamples * bytesPerSample;

	// advance write offset to allow reading new samples
	InterlockedAdd(&audio->rbWriteOffset, (LONG)writeSize);
}

#ifdef __cplusplus
}
#endif