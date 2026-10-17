#include "SimSpeech.h"

#include "HAL/Event.h"
#include "HAL/PlatformProcess.h"
#include "HAL/RunnableThread.h"
#include "Misc/CoreDelegates.h"
#include "Misc/ScopeLock.h"
#include "SAM3D.h"

#if PLATFORM_WINDOWS
#include "Windows/AllowWindowsPlatformTypes.h"
#include "Windows/PreWindowsApi.h"
#include <objbase.h>
#include <sapi.h>
#include "Windows/PostWindowsApi.h"
#include "Windows/HideWindowsPlatformTypes.h"
#endif

namespace
{
	constexpr int32 SampleRate = 22050;

	FString XmlEscape(const FString& S)
	{
		FString Out = S.Replace(TEXT("&"), TEXT("&amp;"));
		Out = Out.Replace(TEXT("<"), TEXT("&lt;"));
		return Out.Replace(TEXT(">"), TEXT("&gt;"));
	}
}

FSimSpeech& FSimSpeech::Get()
{
	static FSimSpeech* Instance = new FSimSpeech();  // lives to the end of the process; its thread stops before exit
	return *Instance;
}

FSimSpeech::FSimSpeech()
{
	Wake = FPlatformProcess::GetSynchEventFromPool(false);
	Thread = FRunnableThread::Create(this, TEXT("SAM3D crew speech"), 0, TPri_BelowNormal);
	FCoreDelegates::OnPreExit.AddLambda([this]()
	{
		if (Thread != nullptr)
		{
			Stop();
			Thread->WaitForCompletion();
		}
	});
}

FSimSpeech::~FSimSpeech()
{
	if (Thread != nullptr)
	{
		Stop();
		Thread->WaitForCompletion();
		delete Thread;
		Thread = nullptr;
	}
	if (Wake != nullptr)
	{
		FPlatformProcess::ReturnSynchEventToPool(Wake);
		Wake = nullptr;
	}
}

int32 FSimSpeech::NumVoices(const FString& Language) const
{
	FScopeLock Lock(&VoicesLock);
	int32 N = 0;
	for (const FVoice& V : Voices)
	{
		N += V.Language == Language ? 1 : 0;
	}
	return N;
}

TArray<FString> FSimSpeech::DescribeVoices() const
{
	FScopeLock Lock(&VoicesLock);
	TArray<FString> Out;
	for (const FVoice& V : Voices)
	{
		Out.Add(FString::Printf(TEXT("%s (%s, %s)"), *V.Name, *V.Language, V.bMale ? TEXT("male") : TEXT("female")));
	}
	return Out;
}

void FSimSpeech::Request(const FSimSpeechRequest& R)
{
	Requests.Enqueue(R);
	if (Wake != nullptr)
	{
		Wake->Trigger();
	}
}

bool FSimSpeech::TakeResult(FSimSpeechResult& Out)
{
	return Results.Dequeue(Out);
}

void FSimSpeech::Stop()
{
	bStop = true;
	if (Wake != nullptr)
	{
		Wake->Trigger();
	}
}

#if PLATFORM_WINDOWS

void FSimSpeech::ListVoices(void* CategoryId)
{
	ISpObjectTokenCategory* Cat = nullptr;
	if (FAILED(CoCreateInstance(CLSID_SpObjectTokenCategory, nullptr, CLSCTX_ALL, IID_ISpObjectTokenCategory, reinterpret_cast<void**>(&Cat))))
	{
		return;
	}
	if (FAILED(Cat->SetId(static_cast<const WCHAR*>(CategoryId), 0)))
	{
		Cat->Release();
		return;
	}
	IEnumSpObjectTokens* Enum = nullptr;
	if (SUCCEEDED(Cat->EnumTokens(nullptr, nullptr, &Enum)) && Enum != nullptr)
	{
		ULONG Count = 0;
		Enum->GetCount(&Count);
		for (ULONG I = 0; I < Count; ++I)
		{
			ISpObjectToken* Token = nullptr;
			if (FAILED(Enum->Next(1, &Token, nullptr)) || Token == nullptr)
			{
				continue;
			}
			FVoice V;
			uint32 Lcid = 0;
			ISpDataKey* Attr = nullptr;
			if (SUCCEEDED(Token->OpenKey(L"Attributes", &Attr)) && Attr != nullptr)
			{
				WCHAR* S = nullptr;
				if (SUCCEEDED(Attr->GetStringValue(L"Language", &S)) && S != nullptr)
				{
					Lcid = static_cast<uint32>(wcstoul(S, nullptr, 16));
					CoTaskMemFree(S);
				}
				if (SUCCEEDED(Attr->GetStringValue(L"Gender", &S)) && S != nullptr)
				{
					V.bMale = _wcsicmp(S, L"Female") != 0;
					CoTaskMemFree(S);
				}
				if (SUCCEEDED(Attr->GetStringValue(L"Name", &S)) && S != nullptr)
				{
					V.Name = FString(S).Replace(TEXT(" Desktop"), TEXT(""));
					CoTaskMemFree(S);
				}
				Attr->Release();
			}
			const uint32 Primary = Lcid & 0x3FF;
			V.Language = Primary == 0x19 ? TEXT("ru") : Primary == 0x09 ? TEXT("en") : FString();
			bool bDuplicate = false;
			{
				FScopeLock Lock(&VoicesLock);
				for (const FVoice& Other : Voices)
				{
					bDuplicate = bDuplicate || Other.Name == V.Name;  // the same voice in both lists
				}
			}
			if (V.Language.IsEmpty() || bDuplicate)
			{
				Token->Release();
				continue;
			}
			V.Token = Token;
			FScopeLock Lock(&VoicesLock);
			Voices.Add(V);
		}
		Enum->Release();
	}
	Cat->Release();
}

bool FSimSpeech::Speak(const FSimSpeechRequest& R, FSimSpeechResult& Out)
{
	ISpVoice* Engine2 = static_cast<ISpVoice*>(Engine);
	if (Engine2 == nullptr)
	{
		return false;
	}
	// The language's voices, men first; the seat's index picks one.
	TArray<const FVoice*> Pick;
	{
		FScopeLock Lock(&VoicesLock);
		for (const FVoice& V : Voices)
		{
			if (V.Language == R.Language && V.bMale) Pick.Add(&V);
		}
		for (const FVoice& V : Voices)
		{
			if (V.Language == R.Language && !V.bMale) Pick.Add(&V);
		}
	}
	if (Pick.Num() == 0)
	{
		return false;
	}
	const int32 Men = Pick.FilterByPredicate([](const FVoice* V) { return V->bMale; }).Num();
	const int32 Choice = Men > 0 ? FMath::Abs(R.VoiceIndex) % Men : FMath::Abs(R.VoiceIndex) % Pick.Num();
	Engine2->SetVoice(static_cast<ISpObjectToken*>(Pick[Choice]->Token));
	IStream* Mem = nullptr;
	if (FAILED(CreateStreamOnHGlobal(nullptr, 1, &Mem)) || Mem == nullptr)
	{
		return false;
	}
	ISpStream* Stream = nullptr;
	if (FAILED(CoCreateInstance(CLSID_SpStream, nullptr, CLSCTX_ALL, IID_ISpStream, reinterpret_cast<void**>(&Stream))) || Stream == nullptr)
	{
		Mem->Release();
		return false;
	}
	WAVEFORMATEX Format = {};
	Format.wFormatTag = WAVE_FORMAT_PCM;
	Format.nChannels = 1;
	Format.nSamplesPerSec = SampleRate;
	Format.wBitsPerSample = 16;
	Format.nBlockAlign = 2;
	Format.nAvgBytesPerSec = SampleRate * 2;
	bool bOk = SUCCEEDED(Stream->SetBaseStream(Mem, SPDFID_WaveFormatEx, &Format)) && SUCCEEDED(Engine2->SetOutput(Stream, 1));
	if (bOk)
	{
		const FString Xml = FString::Printf(TEXT("<pitch absmiddle=\"%d\"><rate absspeed=\"%d\">%s</rate></pitch>"),
			FMath::Clamp(R.Pitch, -10, 10), FMath::Clamp(R.Rate, -10, 10), *XmlEscape(R.Text));
		bOk = SUCCEEDED(Engine2->Speak(*Xml, SPF_IS_XML, nullptr));
		Engine2->SetOutput(nullptr, 1);
	}
	if (bOk)
	{
		STATSTG Stat = {};
		Mem->Stat(&Stat, STATFLAG_NONAME);
		const uint64 Bytes = Stat.cbSize.QuadPart;
		LARGE_INTEGER Zero = {};
		Mem->Seek(Zero, STREAM_SEEK_SET, nullptr);
		Out.Pcm.SetNumZeroed(static_cast<int32>(Bytes / 2));
		ULONG Read = 0;
		Mem->Read(Out.Pcm.GetData(), static_cast<ULONG>(Out.Pcm.Num() * 2), &Read);
		Out.SampleRate = SampleRate;
		bOk = Out.Pcm.Num() > 0;
	}
	Stream->Release();
	Mem->Release();
	return bOk;
}

uint32 FSimSpeech::Run()
{
	if (FAILED(CoInitializeEx(nullptr, COINIT_MULTITHREADED)))
	{
		bReady = true;
		return 0;
	}
	ISpVoice* Voice = nullptr;
	if (SUCCEEDED(CoCreateInstance(CLSID_SpVoice, nullptr, CLSCTX_ALL, IID_ISpVoice, reinterpret_cast<void**>(&Voice))))
	{
		Engine = Voice;
	}
	// SAPI's own voices, and the "OneCore" ones that Windows' language packs install (Russian among them).
	ListVoices(const_cast<WCHAR*>(SPCAT_VOICES));
	ListVoices(const_cast<WCHAR*>(L"HKEY_LOCAL_MACHINE\\SOFTWARE\\Microsoft\\Speech_OneCore\\Voices"));
	bReady = true;
	while (!bStop)
	{
		FSimSpeechRequest R;
		if (!Requests.Dequeue(R))
		{
			Wake->Wait(200);
			continue;
		}
		FSimSpeechResult Out;
		Out.Id = R.Id;
		Out.bOk = Speak(R, Out);
		Results.Enqueue(MoveTemp(Out));
	}
	{
		FScopeLock Lock(&VoicesLock);
		for (FVoice& V : Voices)
		{
			static_cast<ISpObjectToken*>(V.Token)->Release();
			V.Token = nullptr;
		}
		Voices.Reset();
	}
	if (Voice != nullptr)
	{
		Voice->Release();
		Engine = nullptr;
	}
	CoUninitialize();
	return 0;
}

#else  // no speech engine on this platform: the crew is silent

void FSimSpeech::ListVoices(void*) {}

bool FSimSpeech::Speak(const FSimSpeechRequest&, FSimSpeechResult&) { return false; }

uint32 FSimSpeech::Run()
{
	bReady = true;
	while (!bStop)
	{
		FSimSpeechRequest R;
		if (!Requests.Dequeue(R))
		{
			Wake->Wait(200);
			continue;
		}
		FSimSpeechResult Out;
		Out.Id = R.Id;
		Results.Enqueue(MoveTemp(Out));
	}
	return 0;
}

#endif
