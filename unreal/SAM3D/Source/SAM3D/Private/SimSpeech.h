#pragma once

#include "CoreMinimal.h"
#include "Containers/Queue.h"
#include "HAL/Runnable.h"
#include "HAL/ThreadSafeBool.h"

class FRunnableThread;
class FEvent;

/** A line to speak (roadmap 8.10): the words, the language, the voice's pitch and rate (-10..10) and which voice of the
 *  language (seats take different ones where there are several). */
struct FSimSpeechRequest
{
	int32 Id = 0;
	FString Text;
	FString Language;  // "ru" or "en"
	int32 Pitch = 0;
	int32 Rate = 0;
	int32 VoiceIndex = 0;
};

/** The spoken line: 16-bit mono PCM. */
struct FSimSpeechResult
{
	int32 Id = 0;
	bool bOk = false;
	int32 SampleRate = 22050;
	TArray<int16> Pcm;
};

/**
 * The crew's voices from the speech engine installed with the operating system (Windows: SAPI, its own and the
 * "OneCore" voices of the language packs). Lines are synthesised on a worker thread so the game never waits; the game
 * thread queues requests and collects the results. Without an engine (other platforms) every request fails and the
 * crew is silent (the HUD still shows its words).
 */
class FSimSpeech : public FRunnable
{
public:
	/** The one engine for the process, started on first use. */
	static FSimSpeech& Get();

	virtual ~FSimSpeech() override;

	/** The voices have been listed (a moment after start). */
	bool IsReady() const { return bReady; }
	/** How many voices speak a language ("ru", "en"). */
	int32 NumVoices(const FString& Language) const;
	/** The voices, as "name (language, gender)", for the log. */
	TArray<FString> DescribeVoices() const;

	void Request(const FSimSpeechRequest& R);
	bool TakeResult(FSimSpeechResult& Out);

	// FRunnable
	virtual uint32 Run() override;
	virtual void Stop() override;

private:
	FSimSpeech();

	struct FVoice
	{
		FString Name;
		FString Language;
		bool bMale = true;
		void* Token = nullptr;  // ISpObjectToken*, owned by the worker thread
	};

	void ListVoices(void* Category);
	bool Speak(const FSimSpeechRequest& R, FSimSpeechResult& Out);

	TQueue<FSimSpeechRequest, EQueueMode::Mpsc> Requests;
	TQueue<FSimSpeechResult, EQueueMode::Spsc> Results;
	FEvent* Wake = nullptr;
	FRunnableThread* Thread = nullptr;
	FThreadSafeBool bStop = false;
	FThreadSafeBool bReady = false;
	mutable FCriticalSection VoicesLock;
	TArray<FVoice> Voices;
	void* Engine = nullptr;  // ISpVoice*, worker thread only
};
