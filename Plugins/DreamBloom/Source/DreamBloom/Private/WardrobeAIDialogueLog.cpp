#include "WardrobeAIDialogueLog.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "HAL/IConsoleManager.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "Policies/CondensedJsonPrintPolicy.h"
#include "Serialization/JsonSerializer.h"

#if PLATFORM_WINDOWS
#include "Windows/WindowsHWrapper.h"
#endif

#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Misc/ScopeExit.h"
#include "WardrobeAISpeechTypes.h"
#endif

DEFINE_LOG_CATEGORY_STATIC(LogWardrobeAIDialogue, Log, All);

namespace WardrobeAIDialogueLog
{
namespace
{
TAutoConsoleVariable<int32> CVarDialogueLog(
    TEXT("Wardrobe.AI.DialogueLog"), 0,
    TEXT("0: off (default). 1: append conversation and API context to Saved/Logs/WardrobeAIDialogue.log. "
         "Records can contain private dialogue and memories. The setting lasts until this process exits."));

constexpr int64 MaxLogBytes = 5 * 1024 * 1024;

#if PLATFORM_WINDOWS
// AstrBot uses the same Windows mutex when appending to this JSONL file.
class FDialogueLogLock
{
public:
    FDialogueLogLock()
        : Handle(::CreateMutexW(nullptr, false, L"Local\\DreamBloom.WardrobeAIDialogueLog"))
    {
        if (Handle)
        {
            const uint32 Result = ::WaitForSingleObject(Handle, 250);
            bLocked = Result == WAIT_OBJECT_0 || Result == WAIT_ABANDONED;
        }
    }
    ~FDialogueLogLock()
    {
        if (bLocked) ::ReleaseMutex(Handle);
        if (Handle) ::CloseHandle(Handle);
    }
    bool IsLocked() const { return bLocked; }

private:
    HANDLE Handle = nullptr;
    bool bLocked = false;
};
#endif

TSharedRef<FJsonObject> MakeRecord(const FGuid& RequestId, const FString& EventId, const TCHAR* Kind,
                                  const TCHAR* Stage)
{
    auto Record = MakeShared<FJsonObject>();
    Record->SetStringField(TEXT("utc"), FDateTime::UtcNow().ToIso8601());
    if (RequestId.IsValid()) Record->SetStringField(TEXT("request_id"), RequestId.ToString(EGuidFormats::Digits));
    if (!EventId.IsEmpty()) Record->SetStringField(TEXT("event_id"), EventId);
    Record->SetStringField(TEXT("kind"), Kind);
    Record->SetStringField(TEXT("stage"), Stage);
    return Record;
}

bool AppendRecord(const FString& Path, const TSharedRef<FJsonObject>& Record)
{
    if (!IsEnabled()) return true;
    FString Line;
    FJsonSerializer::Serialize(Record, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Line));
    Line += TEXT("\n");

#if PLATFORM_WINDOWS
    const FDialogueLogLock Lock;
    if (!Lock.IsLocked()) return false;
#endif
    const int64 LineBytes = FTCHARToUTF8(*Line).Length();
    if (LineBytes > MaxLogBytes) return false;
    IFileManager& Files = IFileManager::Get();
    const FString Directory = FPaths::GetPath(Path);
    if (!Files.DirectoryExists(*Directory) && !Files.MakeDirectory(*Directory, true)) return false;

    const int64 ExistingBytes = Files.FileSize(*Path);
    if (ExistingBytes >= 0 && ExistingBytes + LineBytes > MaxLogBytes)
    {
        const FString Previous = FPaths::ChangeExtension(Path, TEXT("previous.log"));
        if (!Files.Move(*Previous, *Path, true, false, false, true)) return false;
    }
    return FFileHelper::SaveStringToFile(Line, *Path, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM,
                                         &Files, FILEWRITE_Append | FILEWRITE_AllowRead | FILEWRITE_Silent);
}

bool AppendText(const FString& Path, const FGuid& RequestId, const FString& EventId, const TCHAR* Kind,
                const TCHAR* Stage, const FString& Text, FName Emotion)
{
    auto Record = MakeRecord(RequestId, EventId, Kind, Stage);
    Record->SetStringField(TEXT("text"), Text);
    if (!Emotion.IsNone()) Record->SetStringField(TEXT("emotion"), Emotion.ToString());
    return AppendRecord(Path, Record);
}

void RedactCredentialFields(TSharedPtr<FJsonValue>& Value, const FString& ApiKey)
{
    if (!Value) return;
    if (Value->Type == EJson::String && !ApiKey.IsEmpty())
    {
        FString Text = Value->AsString();
        if (Text.ReplaceInline(*ApiKey, TEXT("[redacted]"), ESearchCase::CaseSensitive) > 0)
        {
            Value = MakeShared<FJsonValueString>(MoveTemp(Text));
        }
    }
    else if (Value->Type == EJson::Array)
    {
        auto Items = Value->AsArray();
        for (auto& Item : Items) RedactCredentialFields(Item, ApiKey);
        Value = MakeShared<FJsonValueArray>(MoveTemp(Items));
    }
    else if (Value->Type == EJson::Object)
    {
        for (auto& Field : Value->AsObject()->Values)
        {
            const FString Key = FString(*Field.Key).ToLower().Replace(TEXT("-"), TEXT("_"));
            if (Key == TEXT("api_key") || Key == TEXT("apikey") || Key == TEXT("authorization")
                || Key == TEXT("proxy_authorization") || Key == TEXT("x_api_key")
                || Key == TEXT("access_token") || Key == TEXT("refresh_token") || Key == TEXT("password")
                || Key == TEXT("cookie") || Key == TEXT("set_cookie") || Key == TEXT("upstream"))
            {
                Field.Value = MakeShared<FJsonValueString>(TEXT("[redacted]"));
            }
            else
            {
                RedactCredentialFields(Field.Value, ApiKey);
            }
        }
    }
}

void SetExchangeBody(const TSharedRef<FJsonObject>& Record, FString Body, const FString& ApiKey)
{
    TSharedPtr<FJsonValue> Payload;
    if (FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Body), Payload) && Payload.IsValid())
    {
        RedactCredentialFields(Payload, ApiKey);
        Record->SetField(TEXT("payload"), MoveTemp(Payload));
    }
    else
    {
        if (!ApiKey.IsEmpty()) Body.ReplaceInline(*ApiKey, TEXT("[redacted]"), ESearchCase::CaseSensitive);
        Record->SetStringField(TEXT("body"), Body);
    }
}

void WarnWriteFailure(const FString& Path)
{
    UE_LOG(LogWardrobeAIDialogue, Warning, TEXT("Could not write dialogue debug log: %s"), *Path);
}
}

bool IsEnabled()
{
    return CVarDialogueLog.GetValueOnGameThread() == 1;
}

FString ReadUserText(const FJsonObject& Request)
{
    const TArray<TSharedPtr<FJsonValue>>* Messages = nullptr;
    if (!Request.TryGetArrayField(TEXT("messages"), Messages) || Messages->IsEmpty()) return {};

    const TSharedPtr<FJsonObject>* Message = nullptr;
    FString Role, Content;
    if (Messages->Last() && Messages->Last()->TryGetObject(Message) && *Message
        && (*Message)->TryGetStringField(TEXT("role"), Role) && Role == TEXT("user"))
    {
        (*Message)->TryGetStringField(TEXT("content"), Content);
    }
    return Content;
}

void Write(const FGuid& RequestId, const FString& EventId, const TCHAR* Kind, const TCHAR* Stage,
           const FString& Text, FName Emotion)
{
    if (!IsEnabled()) return;

    const FString Path = FPaths::ProjectLogDir() / TEXT("WardrobeAIDialogue.log");
    if (!AppendText(Path, RequestId, EventId, Kind, Stage, Text, Emotion)) WarnWriteFailure(Path);
}

void WriteExchange(const FGuid& RequestId, const FString& EventId, const TCHAR* Kind, const TCHAR* Stage,
                   const TCHAR* Peer, FString Body, const FString& ApiKey, int32 HttpStatus, bool bIncomplete)
{
    if (!IsEnabled()) return;

    auto Record = MakeRecord(RequestId, EventId, Kind, Stage);
    Record->SetStringField(TEXT("source"), TEXT("ue"));
    Record->SetStringField(TEXT("peer"), Peer);
    if (HttpStatus != INDEX_NONE) Record->SetNumberField(TEXT("http_status"), HttpStatus);
    if (bIncomplete) Record->SetBoolField(TEXT("incomplete"), true);
    SetExchangeBody(Record, MoveTemp(Body), ApiKey);
    const FString Path = FPaths::ProjectLogDir() / TEXT("WardrobeAIDialogue.log");
    if (!AppendRecord(Path, Record)) WarnWriteFailure(Path);
}
}

#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FWardrobeDialogueLogTest,"Wardrobe.AISpeech.DialogueDebugLog",
    EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)

bool FWardrobeDialogueLogTest::RunTest(const FString&)
{
    using namespace WardrobeAIDialogueLog;
    const FGuid RequestId = FGuid::NewGuid();
    const FString Path = FPaths::ProjectIntermediateDir() / (RequestId.ToString() + TEXT(".log"));
    const FString Previous = FPaths::ChangeExtension(Path, TEXT("previous.log"));
    const int32 OriginalValue = CVarDialogueLog.GetValueOnGameThread();
    auto* Variable = CVarDialogueLog.AsVariable();
    const auto OriginalSource = static_cast<EConsoleVariableFlags>(Variable->GetFlags() & ECVF_SetByMask);
    ON_SCOPE_EXIT
    {
        Variable->Unset(ECVF_SetByConsole);
        if (OriginalSource == ECVF_SetByConsole) Variable->Set(OriginalValue, ECVF_SetByConsole);
        IFileManager::Get().Delete(*Path);
        IFileManager::Get().Delete(*Previous);
    };

    IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Wardrobe.AI.DialogueLog 0"), *GLog, nullptr);
    TestFalse(TEXT("Console off disables logging"), IsEnabled());
    AppendText(Path, RequestId, {}, TEXT("manual"), TEXT("request"), TEXT("not written"), NAME_None);
    TestFalse(TEXT("Disabled logging does not create a file"), IFileManager::Get().FileExists(*Path));

    IConsoleManager::Get().ProcessUserConsoleInput(TEXT("Wardrobe.AI.DialogueLog 1"), *GLog, nullptr);
    if (!TestTrue(TEXT("Console on enables logging"), IsEnabled())) return false;

    FWardrobeSpeechJob Job;
    Job.PlayerMessage = TEXT("第一行\n第二行");
    const FString User = ReadUserText(*WardrobeAISpeech::BuildChatRequest(Job));
    TestEqual(TEXT("Player input is preserved"), User, Job.PlayerMessage);
    FString RawReply, Error;
    const FString Reply = TEXT("【平静】你好呀。");
    const FString Envelope = TEXT("{\"choices\":[{\"message\":{\"content\":\"") + Reply + TEXT("\"}}]}");
    if (!TestTrue(TEXT("Parse reply"), WardrobeAISpeech::ParseCompanionReply(Envelope, Job, Error, &RawReply))) return false;
    TestEqual(TEXT("Capture retains the original emotion markup"), RawReply, Reply);

    AppendText(Path, RequestId, {}, TEXT("manual"), TEXT("request"), User, NAME_None);
    AppendText(Path, RequestId, {}, TEXT("manual"), TEXT("reply"), RawReply, NAME_None);
    const FString Spoken = WardrobeAISpeech::BuildVoiceRequest(Job)->GetStringField(TEXT("text"));
    AppendText(Path, RequestId, {}, TEXT("manual"), TEXT("tts"), Spoken, Job.Emotion);
    FString File;
    if (!TestTrue(TEXT("Read independent UTF-8 log"), FFileHelper::LoadFileToString(File, *Path))) return false;
    TArray<FString> Lines;
    File.ParseIntoArrayLines(Lines);
    if (!TestEqual(TEXT("Multiline input still occupies a single JSON record"), Lines.Num(), 3)) return false;
    const FString ExpectedText[] = {Job.PlayerMessage, Reply, TEXT("你好呀。")};
    for (int32 Index = 0; Index < Lines.Num(); ++Index)
    {
        TSharedPtr<FJsonObject> Record;
        if (!TestTrue(TEXT("Each line is valid JSON"), FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Lines[Index]), Record) && Record.IsValid())) return false;
        TestEqual(TEXT("Exact text round-trips"), Record->GetStringField(TEXT("text")), ExpectedText[Index]);
        TestEqual(TEXT("All stages share a request ID"), Record->GetStringField(TEXT("request_id")), RequestId.ToString(EGuidFormats::Digits));
    }

    auto Exchange = MakeRecord(RequestId, {}, TEXT("manual"), TEXT("ue_request"));
    SetExchangeBody(Exchange, TEXT("{\"messages\":[{\"role\":\"system\",\"content\":\"character persona\"}],\"api_key\":\"TEST_KEY\",\"echo\":\"TEST_KEY\"}"), TEXT("TEST_KEY"));
    TestTrue(TEXT("Write the full request as structured JSON"), AppendRecord(Path, Exchange));
    TestTrue(TEXT("Read full request"), FFileHelper::LoadFileToString(File, *Path));
    TestTrue(TEXT("System persona survives"), File.Contains(TEXT("character persona")));
    TestFalse(TEXT("Credentials and echoes are redacted"), File.Contains(TEXT("TEST_KEY")));

    const FString FullLog = FString::ChrN(static_cast<int32>(MaxLogBytes), TEXT('x'));
    if (!TestTrue(TEXT("Seed rotation threshold"), FFileHelper::SaveStringToFile(FullLog, *Path))) return false;
    TestTrue(TEXT("Record is appended after rotation"), AppendText(Path, RequestId, {}, TEXT("manual"), TEXT("request"), User, NAME_None));
    TestEqual(TEXT("Previous log retains its data"), IFileManager::Get().FileSize(*Previous), MaxLogBytes);
    TestTrue(TEXT("Active log starts a new bounded file"), IFileManager::Get().FileSize(*Path) < MaxLogBytes);
    return true;
}
#endif
