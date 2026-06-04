// v0.9.x — Niagara VFX command handlers.
//
// Goal: let the LLM own particle VFX end-to-end via the MCP, with the editor
// reserved only for authoring/exposing a base system's parameters.
//
// R&D (task #80) established the scriptable surface in UE 5.7:
//   - spawn + set-param are reachable from the Niagara C++/Python API;
//   - RENDERING a system at edit-time (so a screenshot shows it) requires
//     forcing the component solo and explicitly advancing its simulation:
//     SetForceSolo + SetCanRenderWhileSeeking + Activate + AdvanceSimulation.
//     SeekToDesiredAge alone does NOT simulate in a non-PIE editor world — the
//     world manager doesn't tick Niagara there, so the component reports
//     active/visible but spawns zero particles (corrected in #80 follow-up,
//     after the campfire-sparks bring-up surfaced empty preview screenshots);
//   - enumerating a system's User Parameters is NOT exposed to Python at all
//     (NiagaraSystem has no param-listing method there). It IS reachable in
//     C++ via FNiagaraUserRedirectionParameterStore — which is the whole
//     reason list_niagara_user_params lives here and not in a Python wrapper.
//
// Free functions in an anonymous namespace + REGISTER_MCP_COMMAND at the
// definition site, matching the v0.8.x level/editor command style.

#include "Commands/UnrealMCPCommonUtils.h"
#include "MCPRegistry.h"
#include "MCPParams.h"
#include "MCPResponse.h"

#include "Editor.h"
#include "Engine/World.h"
#include "EngineUtils.h"               // TActorIterator
#include "UObject/SoftObjectPath.h"

#include "NiagaraSystem.h"
#include "NiagaraComponent.h"
#include "NiagaraActor.h"
#include "NiagaraUserRedirectionParameterStore.h"

namespace
{

// ─── helpers ────────────────────────────────────────────────────────────────

/** Normalize a /Game/ package path to an object path (append .Leaf if absent). */
FString ToObjectPath(const FString& InPath)
{
    if (InPath.Contains(TEXT(".")))
    {
        return InPath;
    }
    FString Parent, Leaf;
    if (InPath.Split(TEXT("/"), &Parent, &Leaf, ESearchCase::IgnoreCase, ESearchDir::FromEnd))
    {
        return FString::Printf(TEXT("%s.%s"), *InPath, *Leaf);
    }
    return InPath;
}

UNiagaraSystem* LoadNiagaraSystem(const FString& SystemPath)
{
    return LoadObject<UNiagaraSystem>(nullptr, *ToObjectPath(SystemPath));
}

/** Two-pass actor resolve (display label, then internal name), then its NiagaraComponent. */
UNiagaraComponent* ResolveNiagaraComponent(const FString& ActorName, FString& OutError)
{
    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        OutError = TEXT("No editor world loaded");
        return nullptr;
    }

    AActor* Found = nullptr;
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        if (It->GetActorLabel() == ActorName) { Found = *It; break; }
    }
    if (!Found)
    {
        for (TActorIterator<AActor> It(World); It; ++It)
        {
            if (It->GetName() == ActorName) { Found = *It; break; }
        }
    }
    if (!Found)
    {
        OutError = FString::Printf(TEXT("Actor '%s' not found"), *ActorName);
        return nullptr;
    }

    UNiagaraComponent* Comp = Found->FindComponentByClass<UNiagaraComponent>();
    if (!Comp)
    {
        OutError = FString::Printf(TEXT("Actor '%s' has no NiagaraComponent"), *ActorName);
        return nullptr;
    }
    return Comp;
}

// ─── spawn_niagara ──────────────────────────────────────────────────────────

TSharedPtr<FJsonObject> HandleSpawnNiagara(const TSharedPtr<FJsonObject>& Params)
{
    FMCPParams P(Params);

    FString SystemPath, Err;
    if (!P.GetString(TEXT("system_path"), SystemPath, Err))
    {
        return FMCPResponse::Error(Err);
    }

    UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
    if (!System)
    {
        return FMCPResponse::Error(FString::Printf(TEXT("Could not load NiagaraSystem '%s'"), *SystemPath));
    }

    UWorld* World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!World)
    {
        return FMCPResponse::Error(TEXT("No editor world loaded"));
    }

    FVector Location(0, 0, 0);
    FString VErr;
    P.GetVector(TEXT("location"), Location, VErr);   // optional; default origin
    FRotator Rotation(0, 0, 0);
    P.GetRotator(TEXT("rotation"), Rotation, VErr);  // optional

    ANiagaraActor* Actor = World->SpawnActor<ANiagaraActor>(Location, Rotation);
    if (!Actor)
    {
        return FMCPResponse::Error(TEXT("Failed to spawn NiagaraActor"));
    }

    UNiagaraComponent* Comp = Actor->GetNiagaraComponent();
    if (!Comp)
    {
        return FMCPResponse::Error(TEXT("Spawned NiagaraActor has no NiagaraComponent"));
    }
    Comp->SetAsset(System);
    Comp->Activate(true);

    const FString Label = P.GetStringOr(TEXT("label"));
    if (!Label.IsEmpty())
    {
        Actor->SetActorLabel(Label);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Actor->GetActorLabel());
    Result->SetStringField(TEXT("internal_name"), Actor->GetName());
    Result->SetStringField(TEXT("system"), System->GetName());
    return FMCPResponse::Success(Result);
}

// ─── seek_niagara (edit-time preview render) ────────────────────────────────

TSharedPtr<FJsonObject> HandleSeekNiagara(const TSharedPtr<FJsonObject>& Params)
{
    FMCPParams P(Params);

    FString ActorName, Err;
    if (!P.GetString(TEXT("actor"), ActorName, Err))
    {
        return FMCPResponse::Error(Err);
    }

    UNiagaraComponent* Comp = ResolveNiagaraComponent(ActorName, Err);
    if (!Comp)
    {
        return FMCPResponse::Error(Err);
    }

    // live=true restores normal gameplay ticking (call before saving the level
    // so the system isn't frozen in-game). Default false = preview at `age`.
    const bool bLive = P.GetBoolOr(TEXT("live"), false);
    if (bLive)
    {
        // Restore normal batched gameplay ticking. Clear ForceSolo in case a
        // prior preview set it, else the system stays solo (works, but wasteful).
        Comp->SetForceSolo(false);
        Comp->SetAgeUpdateMode(ENiagaraAgeUpdateMode::TickDeltaTime);
        Comp->Activate(true);
    }
    else
    {
        // In a non-PIE editor world the world manager does not tick Niagara, so
        // SeekToDesiredAge alone reports active/visible but spawns NOTHING.
        // Forcing the component solo and explicitly advancing the sim is what
        // actually simulates + renders it for a screenshot.
        const float Age = P.GetFloatOr(TEXT("age"), 1.0f);
        Comp->SetForceSolo(true);
        Comp->SetCanRenderWhileSeeking(true);
        Comp->Activate(true);   // reset + activate
        const float Dt = 1.0f / 30.0f;
        const int32 Ticks = FMath::Max(1, FMath::CeilToInt(Age / Dt));
        Comp->AdvanceSimulation(Ticks, Dt);
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor"), ActorName);
    Result->SetBoolField(TEXT("live"), bLive);
    return FMCPResponse::Success(Result);
}

// ─── set_niagara_param ──────────────────────────────────────────────────────

TSharedPtr<FJsonObject> HandleSetNiagaraParam(const TSharedPtr<FJsonObject>& Params)
{
    FMCPParams P(Params);

    FString ActorName, Name, Type, Err;
    if (!P.GetString(TEXT("actor"), ActorName, Err)) { return FMCPResponse::Error(Err); }
    if (!P.GetString(TEXT("name"),  Name,      Err)) { return FMCPResponse::Error(Err); }
    if (!P.GetString(TEXT("type"),  Type,      Err)) { return FMCPResponse::Error(Err); }

    UNiagaraComponent* Comp = ResolveNiagaraComponent(ActorName, Err);
    if (!Comp) { return FMCPResponse::Error(Err); }

    const FName Var(*Name);
    const FString T = Type.ToLower();

    if (T == TEXT("float"))
    {
        float V; if (!P.GetFloat(TEXT("value"), V, Err)) return FMCPResponse::Error(Err);
        Comp->SetVariableFloat(Var, V);
    }
    else if (T == TEXT("int"))
    {
        int32 V; if (!P.GetInt(TEXT("value"), V, Err)) return FMCPResponse::Error(Err);
        Comp->SetVariableInt(Var, V);
    }
    else if (T == TEXT("bool"))
    {
        bool V; if (!P.GetBool(TEXT("value"), V, Err)) return FMCPResponse::Error(Err);
        Comp->SetVariableBool(Var, V);
    }
    else if (T == TEXT("vec2"))
    {
        const FVector2D V = FUnrealMCPCommonUtils::GetVector2DFromJson(P.Raw(), TEXT("value"));
        Comp->SetVariableVec2(Var, V);
    }
    else if (T == TEXT("vec3"))
    {
        FVector V; if (!P.GetVector(TEXT("value"), V, Err)) return FMCPResponse::Error(Err);
        Comp->SetVariableVec3(Var, V);
    }
    else if (T == TEXT("position"))
    {
        FVector V; if (!P.GetVector(TEXT("value"), V, Err)) return FMCPResponse::Error(Err);
        Comp->SetVariablePosition(Var, FVector(V));
    }
    else if (T == TEXT("color") || T == TEXT("linear_color"))
    {
        TArray<float> C; if (!P.GetFloatArray(TEXT("value"), C, Err)) return FMCPResponse::Error(Err);
        const FLinearColor Col(
            C.IsValidIndex(0) ? C[0] : 0.0f,
            C.IsValidIndex(1) ? C[1] : 0.0f,
            C.IsValidIndex(2) ? C[2] : 0.0f,
            C.IsValidIndex(3) ? C[3] : 1.0f);
        Comp->SetVariableLinearColor(Var, Col);
    }
    else
    {
        return FMCPResponse::Error(FString::Printf(
            TEXT("Unsupported param type '%s' (use float|int|bool|vec2|vec3|position|color)"), *Type));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor"), ActorName);
    Result->SetStringField(TEXT("name"), Name);
    Result->SetStringField(TEXT("type"), Type);
    return FMCPResponse::Success(Result);
}

// ─── list_niagara_user_params (C++-only keystone) ───────────────────────────

TSharedPtr<FJsonObject> HandleListNiagaraUserParams(const TSharedPtr<FJsonObject>& Params)
{
    FMCPParams P(Params);

    FString SystemPath, Err;
    if (!P.GetString(TEXT("system_path"), SystemPath, Err))
    {
        return FMCPResponse::Error(Err);
    }

    UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
    if (!System)
    {
        return FMCPResponse::Error(FString::Printf(TEXT("Could not load NiagaraSystem '%s'"), *SystemPath));
    }

    // The exposed-parameter store is the LLM-tunable surface. Reachable in C++,
    // not Python — this is the whole point of the tool.
    const FNiagaraUserRedirectionParameterStore& Store = System->GetExposedParameters();
    TArray<FNiagaraVariable> UserParams;
    Store.GetUserParameters(UserParams);

    TArray<TSharedPtr<FJsonValue>> ParamArr;
    for (const FNiagaraVariable& V : UserParams)
    {
        // GetUserParameters returns redirected (User.-stripped) names, but strip
        // defensively so the name is exactly what set_niagara_param expects.
        FString PName = V.GetName().ToString();
        PName.RemoveFromStart(TEXT("User."));

        TSharedPtr<FJsonObject> Obj = MakeShared<FJsonObject>();
        Obj->SetStringField(TEXT("name"), PName);
        Obj->SetStringField(TEXT("type"), V.GetType().GetName());
        ParamArr.Add(MakeShared<FJsonValueObject>(Obj));
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("system"), System->GetName());
    Result->SetArrayField(TEXT("user_parameters"), ParamArr);
    Result->SetNumberField(TEXT("count"), ParamArr.Num());
    return FMCPResponse::Success(Result);
}

REGISTER_MCP_COMMAND("spawn_niagara", &HandleSpawnNiagara);
REGISTER_MCP_COMMAND("seek_niagara", &HandleSeekNiagara);
REGISTER_MCP_COMMAND("set_niagara_param", &HandleSetNiagaraParam);
REGISTER_MCP_COMMAND("list_niagara_user_params", &HandleListNiagaraUserParams);

} // namespace
