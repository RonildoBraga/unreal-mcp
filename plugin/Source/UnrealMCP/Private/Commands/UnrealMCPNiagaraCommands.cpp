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

// Editor-only — baked module-input authoring (set_niagara_module_default).
// Drives the same view-model the Niagara asset editor sits on, headless.
#include "ViewModels/NiagaraSystemViewModel.h"
#include "ViewModels/NiagaraEmitterHandleViewModel.h"
#include "ViewModels/Stack/NiagaraStackViewModel.h"
#include "ViewModels/Stack/NiagaraStackEntry.h"
#include "ViewModels/Stack/NiagaraStackFunctionInput.h"
#include "ViewModels/Stack/NiagaraParameterHandle.h"
#include "NiagaraNodeFunctionCall.h"
#include "UObject/StructOnScope.h"

// add_niagara_module: add a whole module to a stack (graph surgery).
#include "ViewModels/Stack/NiagaraStackModuleItem.h"
#include "ViewModels/Stack/NiagaraStackGraphUtilities.h"
#include "NiagaraNodeOutput.h"
#include "NiagaraScript.h"
#include "NiagaraEditorUtilities.h"
#include "AssetRegistry/AssetData.h"

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

// ─── set_niagara_module_default (baked module-input authoring) ───────────────
//
// Edit a module input's baked default ON THE ASSET — not a per-instance
// override — then recompile so every future spawn uses it. No User Parameter
// required. This is the "edit the recipe" counterpart to set_niagara_param's
// "season one plate". Drives the same view-model the Niagara editor UI sits on
// (FNiagaraSystemViewModel + UNiagaraStackFunctionInput::SetLocalValue),
// headless via bIsForDataProcessingOnly. On no/ambiguous match it returns the
// full (emitter, module, input, type) list so a wrong guess self-corrects.
// R&D: #80 follow-up — proves VFX *authoring* (not just tuning) is scriptable.

TSharedPtr<FJsonObject> HandleSetNiagaraModuleDefault(const TSharedPtr<FJsonObject>& Params)
{
    FMCPParams P(Params);

    FString SystemPath, InputName, Err;
    if (!P.GetString(TEXT("system_path"), SystemPath, Err)) { return FMCPResponse::Error(Err); }
    if (!P.GetString(TEXT("input"), InputName, Err))        { return FMCPResponse::Error(Err); }

    const FString ModuleFilter  = P.GetStringOr(TEXT("module"));
    const FString EmitterFilter = P.GetStringOr(TEXT("emitter"));

    // value: accept a JSON array of numbers, or a single number.
    TArray<float> Values;
    if (!P.GetFloatArray(TEXT("value"), Values, Err))
    {
        float Single;
        if (P.GetFloat(TEXT("value"), Single, Err)) { Values.Add(Single); }
        else { return FMCPResponse::Error(TEXT("'value' must be a number or array of numbers")); }
    }

    UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
    if (!System)
    {
        return FMCPResponse::Error(FString::Printf(TEXT("Could not load NiagaraSystem '%s'"), *SystemPath));
    }

    // Headless system view model — same object model the Niagara editor sits on.
    // bIsForDataProcessingOnly skips undo registration / sequencer / preview.
    FNiagaraSystemViewModelOptions Options;
    Options.bIsForDataProcessingOnly = true;
    Options.bCanSimulate = false;
    Options.bCanAutoCompile = true;
    Options.bCompileForEdit = true;
    Options.bCanModifyEmittersFromTimeline = false;
    // REQUIRED even headless: building the emitter stack creates an
    // EmitterProperties UNiagaraStackObject that subscribes to the Niagara
    // message manager, which asserts (crash) on an empty asset key. A fresh
    // unique GUID is a harmless private subscription topic that satisfies it.
    Options.MessageLogGuid.Emplace(FGuid::NewGuid());

    TSharedRef<FNiagaraSystemViewModel> SystemVM = MakeShared<FNiagaraSystemViewModel>();
    SystemVM->Initialize(*System, Options);

    TArray<UNiagaraStackFunctionInput*> Matches;
    TArray<TSharedPtr<FJsonValue>> Available;   // diagnostic dump for no/ambiguous match

    for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterVM : SystemVM->GetEmitterHandleViewModels())
    {
        const FString EmitterName = EmitterVM->GetName().ToString();
        if (!EmitterFilter.IsEmpty() && !EmitterName.Contains(EmitterFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        UNiagaraStackViewModel* StackVM = EmitterVM->GetEmitterStackViewModel();
        if (!StackVM || !StackVM->GetRootEntry()) { continue; }

        TArray<UNiagaraStackFunctionInput*> Inputs;
        StackVM->GetRootEntry()->GetUnfilteredChildrenOfType<UNiagaraStackFunctionInput>(Inputs, /*bRecursive*/ true);

        for (UNiagaraStackFunctionInput* In : Inputs)
        {
            if (!In || In->IsStaticParameter()) { continue; }

            const FString ModuleName = In->GetInputFunctionCallNode().GetFunctionName();
            const FString InName     = In->GetInputParameterHandle().GetName().ToString();
            const FString TypeName   = In->GetInputType().GetName();

            TSharedPtr<FJsonObject> Rec = MakeShared<FJsonObject>();
            Rec->SetStringField(TEXT("emitter"), EmitterName);
            Rec->SetStringField(TEXT("module"), ModuleName);
            Rec->SetStringField(TEXT("input"), InName);
            Rec->SetStringField(TEXT("type"), TypeName);
            Available.Add(MakeShared<FJsonValueObject>(Rec));

            const bool bInputMatch  = InName.Equals(InputName, ESearchCase::IgnoreCase);
            const bool bModuleMatch = ModuleFilter.IsEmpty() || ModuleName.Contains(ModuleFilter, ESearchCase::IgnoreCase);
            if (bInputMatch && bModuleMatch) { Matches.Add(In); }
        }
    }

    auto Fail = [&](const FString& Msg) -> TSharedPtr<FJsonObject>
    {
        TSharedPtr<FJsonObject> R = MakeShared<FJsonObject>();
        R->SetBoolField(TEXT("success"), false);
        R->SetStringField(TEXT("error"), Msg);
        R->SetArrayField(TEXT("available_inputs"), Available);
        return R;   // SystemVM tears down at scope exit (~FNiagaraSystemViewModel calls Cleanup)
    };

    if (Matches.Num() == 0)
    {
        return Fail(FString::Printf(
            TEXT("No module input matched input='%s' module='%s' emitter='%s' — see available_inputs."),
            *InputName, *ModuleFilter, *EmitterFilter));
    }
    if (Matches.Num() > 1)
    {
        return Fail(FString::Printf(
            TEXT("Ambiguous: %d inputs matched input='%s' — narrow with 'module'/'emitter'. See available_inputs."),
            Matches.Num(), *InputName));
    }

    UNiagaraStackFunctionInput* Target = Matches[0];
    const FNiagaraTypeDefinition& Type = Target->GetInputType();
    UScriptStruct* Struct = Cast<UScriptStruct>(Type.GetStruct());
    if (!Struct)
    {
        return Fail(FString::Printf(
            TEXT("Input '%s' (type '%s') is not a settable numeric struct"), *InputName, *Type.GetName()));
    }

    // Pack provided floats into the type's struct memory (float-backed numeric
    // types: float / vec2 / vec3 / vec4 / color). Zero-fill the remainder.
    const int32 TypeSize = Type.GetSize();
    TSharedRef<FStructOnScope> ValueStruct = MakeShared<FStructOnScope>(Struct);
    uint8* Mem = ValueStruct->GetStructMemory();
    FMemory::Memzero(Mem, TypeSize);
    const int32 NumFloats = FMath::Min(Values.Num(), TypeSize / (int32)sizeof(float));
    for (int32 i = 0; i < NumFloats; ++i)
    {
        FMemory::Memcpy(Mem + i * sizeof(float), &Values[i], sizeof(float));
    }

    const FString RModule = Target->GetInputFunctionCallNode().GetFunctionName();
    const FString RType   = Type.GetName();

    Target->SetLocalValue(ValueStruct);          // writes RapidIterationParameters + requests recompile
    System->WaitForCompilationComplete(false, false);
    System->MarkPackageDirty();
    // No explicit SystemVM->Cleanup(): that method isn't NIAGARAEDITOR_API-exported,
    // and ~FNiagaraSystemViewModel() calls Cleanup() anyway when the TSharedRef
    // drops at function scope exit.

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("system"), System->GetName());
    Result->SetStringField(TEXT("module"), RModule);
    Result->SetStringField(TEXT("input"), InputName);
    Result->SetStringField(TEXT("type"), RType);
    Result->SetNumberField(TEXT("floats_written"), NumFloats);
    Result->SetBoolField(TEXT("dirty"), true);   // not auto-saved; persist via save tool
    return FMCPResponse::Success(Result);
}

// ─── add_niagara_module (stack module authoring) ─────────────────────────────
//
// ADD a whole module to an emitter's stack (e.g. a Curl Noise Force into the
// Particle Update stage for turbulence), then recompile. The complement to
// set_niagara_module_default (which edits an existing module's input). Same
// headless FNiagaraSystemViewModel; the add is
// FNiagaraStackGraphUtilities::AddScriptModuleToStack — the UNiagaraScript*
// overload, since the FAssetData / args-struct overloads aren't
// NIAGARAEDITOR_API-exported. `before` controls insert order, which matters for
// forces: a Curl Noise Force must precede SolveForcesAndVelocity or its force
// is integrated a frame late / not at all. R&D: #80 follow-up.

ENiagaraScriptUsage ParseStageUsage(const FString& Stage, bool& bOk)
{
    bOk = true;
    const FString S = Stage.Replace(TEXT(" "), TEXT("")).ToLower();
    if (S.Contains(TEXT("particleupdate")) || S == TEXT("update")) return ENiagaraScriptUsage::ParticleUpdateScript;
    if (S.Contains(TEXT("particlespawn")) || S == TEXT("spawn"))   return ENiagaraScriptUsage::ParticleSpawnScript;
    if (S.Contains(TEXT("emitterupdate")))                         return ENiagaraScriptUsage::EmitterUpdateScript;
    if (S.Contains(TEXT("emitterspawn")))                          return ENiagaraScriptUsage::EmitterSpawnScript;
    if (S.Contains(TEXT("systemupdate")))                          return ENiagaraScriptUsage::SystemUpdateScript;
    if (S.Contains(TEXT("systemspawn")))                           return ENiagaraScriptUsage::SystemSpawnScript;
    bOk = false;
    return ENiagaraScriptUsage::ParticleUpdateScript;
}

TSharedPtr<FJsonObject> HandleAddNiagaraModule(const TSharedPtr<FJsonObject>& Params)
{
    FMCPParams P(Params);

    FString SystemPath, ModuleRef, Err;
    if (!P.GetString(TEXT("system_path"), SystemPath, Err)) { return FMCPResponse::Error(Err); }
    if (!P.GetString(TEXT("module"), ModuleRef, Err))       { return FMCPResponse::Error(Err); }

    const FString StageStr      = P.GetStringOr(TEXT("stage"), TEXT("ParticleUpdate"));
    const FString EmitterFilter = P.GetStringOr(TEXT("emitter"));
    const FString BeforeModule  = P.GetStringOr(TEXT("before"));

    bool bStageOk = false;
    const ENiagaraScriptUsage Usage = ParseStageUsage(StageStr, bStageOk);
    if (!bStageOk)
    {
        return FMCPResponse::Error(FString::Printf(
            TEXT("Unknown stage '%s' (ParticleSpawn|ParticleUpdate|EmitterSpawn|EmitterUpdate|SystemSpawn|SystemUpdate)"),
            *StageStr));
    }

    UNiagaraSystem* System = LoadNiagaraSystem(SystemPath);
    if (!System)
    {
        return FMCPResponse::Error(FString::Printf(TEXT("Could not load NiagaraSystem '%s'"), *SystemPath));
    }

    // Resolve the module script: a "/"-path loads directly; a bare name is
    // matched against the module-script asset registry for this stage.
    UNiagaraScript* ModuleScript = nullptr;
    if (ModuleRef.Contains(TEXT("/")))
    {
        ModuleScript = LoadObject<UNiagaraScript>(nullptr, *ToObjectPath(ModuleRef));
    }
    else
    {
        FNiagaraEditorUtilities::FGetFilteredScriptAssetsOptions Opt;
        Opt.ScriptUsageToInclude = ENiagaraScriptUsage::Module;
        Opt.TargetUsageToMatch   = Usage;
        TArray<FAssetData> Assets;
        FNiagaraEditorUtilities::GetFilteredScriptAssets(Opt, Assets);
        for (const FAssetData& A : Assets)
        {
            if (A.AssetName.ToString().Contains(ModuleRef, ESearchCase::IgnoreCase))
            {
                ModuleScript = Cast<UNiagaraScript>(A.GetAsset());
                if (ModuleScript) { break; }
            }
        }
    }
    if (!ModuleScript)
    {
        return FMCPResponse::Error(FString::Printf(TEXT("Could not resolve module script '%s'"), *ModuleRef));
    }

    // Headless view-model (MessageLogGuid required — see set_niagara_module_default).
    FNiagaraSystemViewModelOptions Options;
    Options.bIsForDataProcessingOnly = true;
    Options.bCanSimulate = false;
    Options.bCanAutoCompile = true;
    Options.bCompileForEdit = true;
    Options.bCanModifyEmittersFromTimeline = false;
    Options.MessageLogGuid.Emplace(FGuid::NewGuid());

    TSharedRef<FNiagaraSystemViewModel> SystemVM = MakeShared<FNiagaraSystemViewModel>();
    SystemVM->Initialize(*System, Options);

    // Collect the target stage's existing modules (execution order) + output node.
    UNiagaraNodeOutput* TargetOutput = nullptr;
    TArray<UNiagaraStackModuleItem*> StageModules;
    FString ResolvedEmitter;

    for (const TSharedRef<FNiagaraEmitterHandleViewModel>& EmitterVM : SystemVM->GetEmitterHandleViewModels())
    {
        const FString EName = EmitterVM->GetName().ToString();
        if (!EmitterFilter.IsEmpty() && !EName.Contains(EmitterFilter, ESearchCase::IgnoreCase)) { continue; }

        UNiagaraStackViewModel* StackVM = EmitterVM->GetEmitterStackViewModel();
        if (!StackVM || !StackVM->GetRootEntry()) { continue; }

        TArray<UNiagaraStackModuleItem*> Items;
        StackVM->GetRootEntry()->GetUnfilteredChildrenOfType<UNiagaraStackModuleItem>(Items, /*bRecursive*/ true);
        for (UNiagaraStackModuleItem* It : Items)
        {
            if (!It) { continue; }
            if (FNiagaraStackGraphUtilities::GetOutputNodeUsage(It->GetModuleNode()) == Usage)
            {
                StageModules.Add(It);
                if (!TargetOutput) { TargetOutput = It->GetOutputNode(); }
            }
        }
        if (TargetOutput) { ResolvedEmitter = EName; break; }
    }

    if (!TargetOutput)
    {
        return FMCPResponse::Error(FString::Printf(
            TEXT("No '%s' stage found on emitter '%s'"), *StageStr, *EmitterFilter));
    }

    // Insert index: before the named module if given, else append (INDEX_NONE).
    int32 TargetIndex = INDEX_NONE;
    if (!BeforeModule.IsEmpty())
    {
        for (int32 i = 0; i < StageModules.Num(); ++i)
        {
            if (StageModules[i]->GetModuleNode().GetFunctionName().Contains(BeforeModule, ESearchCase::IgnoreCase))
            {
                TargetIndex = i;
                break;
            }
        }
    }

    UNiagaraNodeFunctionCall* NewNode =
        FNiagaraStackGraphUtilities::AddScriptModuleToStack(ModuleScript, *TargetOutput, TargetIndex);
    if (!NewNode)
    {
        return FMCPResponse::Error(TEXT("AddScriptModuleToStack returned null — module not added"));
    }
    const FString AddedName = NewNode->GetFunctionName();

    System->RequestCompile(true);
    System->WaitForCompilationComplete(false, false);
    System->MarkPackageDirty();

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("system"), System->GetName());
    Result->SetStringField(TEXT("emitter"), ResolvedEmitter);
    Result->SetStringField(TEXT("module"), ModuleScript->GetName());
    Result->SetStringField(TEXT("added_node"), AddedName);
    Result->SetStringField(TEXT("stage"), StageStr);
    Result->SetNumberField(TEXT("index"), TargetIndex);
    Result->SetBoolField(TEXT("dirty"), true);
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
REGISTER_MCP_COMMAND("set_niagara_module_default", &HandleSetNiagaraModuleDefault);
REGISTER_MCP_COMMAND("add_niagara_module", &HandleAddNiagaraModule);
REGISTER_MCP_COMMAND("list_niagara_user_params", &HandleListNiagaraUserParams);

} // namespace
