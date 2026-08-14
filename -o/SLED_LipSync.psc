ScriptName SLED_LipSync Extends Quest

;-- Variables ---------------------------------------
Int _actorCount = 0
Actor[] _actors
Float[] _cooldownUntil
String _lastClip = "None"
Float _lastNow = 0.0
Float _lastPulse = 0.0
String _lastSkip = "init"
String _status = "Default OFF / Experimental"

;-- Properties --------------------------------------
slexpressionfaces Property Core Auto
FormList Property DefaultBuildTopics Auto
FormList Property DefaultClimaxTopics Auto
FormList Property DefaultPeakTopics Auto
FormList Property DefaultSustainTopics Auto
Int Property PHASE_BUILD = 0 AutoReadOnly hidden
Int Property PHASE_CLIMAX = 3 AutoReadOnly hidden
Int Property PHASE_PEAK = 2 AutoReadOnly hidden
Int Property PHASE_SUSTAIN = 1 AutoReadOnly hidden
FormList Property VoiceBuildTopicLists Auto
FormList Property VoiceClimaxTopicLists Auto
FormList Property VoicePeakTopicLists Auto
FormList Property VoiceSustainTopicLists Auto
FormList Property VoiceTypes Auto
Bool Property bDebug = False Auto
Bool Property bEnabled = False Auto
Bool Property bOwnVoice = False Auto
Bool Property bUseDefaultFallback = True Auto
Float Property fMouthHoldMargin = 0.349999994 AutoReadOnly hidden
Float Property fRate = 0.5 Auto
String Property sAddonName = "SLED - Lip-Sync" AutoReadOnly hidden
String Property sChannelName = "Mouth=Lip-Sync" AutoReadOnly hidden
String Property sMouthEvent = "SLED_LipSyncMouth" AutoReadOnly hidden
String Property sVersion = "0.2" AutoReadOnly hidden
String Property sYieldNoAsset = "NoAsset / yield to cadence" AutoReadOnly hidden

;-- Functions ---------------------------------------

; Skipped compiler generated GetState

; Skipped compiler generated GotoState

Event OnInit()
  Self.Maintenance() ; #DEBUG_LINE_NO:41
EndEvent

Function Maintenance()
  Self.InitState() ; #DEBUG_LINE_NO:45
  Self.RegisterForPulseBus() ; #DEBUG_LINE_NO:46
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:47
EndFunction

Function InitState()
  If _actors as Bool && _cooldownUntil as Bool ; #DEBUG_LINE_NO:51
    Return  ; #DEBUG_LINE_NO:52
  EndIf
  _actors = new Actor[5] ; #DEBUG_LINE_NO:54
  _cooldownUntil = new Float[5] ; #DEBUG_LINE_NO:55
EndFunction

Function RegisterForPulseBus()
  Self.UnregisterForModEvent("SLED_Beat") ; #DEBUG_LINE_NO:59
  Self.UnregisterForModEvent("SLED_ClimaxPeak") ; #DEBUG_LINE_NO:60
  Self.UnregisterForModEvent("SLED_Afterglow") ; #DEBUG_LINE_NO:61
  Self.UnregisterForModEvent("SLED_Distress") ; #DEBUG_LINE_NO:62
  Self.UnregisterForModEvent("SLED_ClearActor") ; #DEBUG_LINE_NO:63
  Self.UnregisterForModEvent("SLED_SceneEnd") ; #DEBUG_LINE_NO:64
  Self.RegisterForModEvent("SLED_Beat", "OnSLEDBeat") ; #DEBUG_LINE_NO:65
  Self.RegisterForModEvent("SLED_ClimaxPeak", "OnSLEDClimaxPeak") ; #DEBUG_LINE_NO:66
  Self.RegisterForModEvent("SLED_Afterglow", "OnSLEDAfterglow") ; #DEBUG_LINE_NO:67
  Self.RegisterForModEvent("SLED_Distress", "OnSLEDDistress") ; #DEBUG_LINE_NO:68
  Self.RegisterForModEvent("SLED_ClearActor", "OnSLEDClearActor") ; #DEBUG_LINE_NO:69
  Self.RegisterForModEvent("SLED_SceneEnd", "OnSLEDSceneEnd") ; #DEBUG_LINE_NO:70
EndFunction

Function OnSLEDBeat(String eventName, String strArg, Float numArg, Form sender)
  Self.NotePulse("beat") ; #DEBUG_LINE_NO:74
  If Utility.RandomFloat(0.0, 1.0) > fRate ; #DEBUG_LINE_NO:75
    Self.NoteSkip("beat: rate roll") ; #DEBUG_LINE_NO:76
    Return  ; #DEBUG_LINE_NO:77
  EndIf
  Actor a = Self.ActorFromPulse(strArg) ; #DEBUG_LINE_NO:79
  If numArg >= 0.800000012 ; #DEBUG_LINE_NO:80
    Self.TryClip(a, Self.PHASE_PEAK, "Peak") ; #DEBUG_LINE_NO:81
  ElseIf numArg >= 0.449999988 ; #DEBUG_LINE_NO:82
    Self.TryClip(a, Self.PHASE_SUSTAIN, "Sustain") ; #DEBUG_LINE_NO:83
  Else
    Self.TryClip(a, Self.PHASE_BUILD, "Build") ; #DEBUG_LINE_NO:85
  EndIf
EndFunction

Function OnSLEDClimaxPeak(String eventName, String strArg, Float numArg, Form sender)
  Self.NotePulse("climax") ; #DEBUG_LINE_NO:90
  Actor a = Self.ActorFromPulse(strArg) ; #DEBUG_LINE_NO:91
  Self.TryClip(a, Self.PHASE_CLIMAX, "climax") ; #DEBUG_LINE_NO:92
EndFunction

Function OnSLEDAfterglow(String eventName, String strArg, Float numArg, Form sender)
  Self.NotePulse("afterglow") ; #DEBUG_LINE_NO:96
  Actor a = Self.ActorFromPulse(strArg) ; #DEBUG_LINE_NO:97
  Self.ClearActor(a) ; #DEBUG_LINE_NO:98
EndFunction

Function OnSLEDDistress(String eventName, String strArg, Float numArg, Form sender)
  Self.NotePulse("distress") ; #DEBUG_LINE_NO:102
  Actor a = Self.ActorFromPulse(strArg) ; #DEBUG_LINE_NO:103
  Self.ClearActor(a) ; #DEBUG_LINE_NO:104
  _status = "Yielded(distress)" ; #DEBUG_LINE_NO:105
  Self.NoteSkip("distress: clear") ; #DEBUG_LINE_NO:106
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:107
EndFunction

Function OnSLEDClearActor(String eventName, String strArg, Float numArg, Form sender)
  Self.NotePulse("clear") ; #DEBUG_LINE_NO:111
  Actor a = Self.ActorFromPulse(strArg) ; #DEBUG_LINE_NO:112
  Self.ClearActor(a) ; #DEBUG_LINE_NO:113
EndFunction

Function OnSLEDSceneEnd(String eventName, String strArg, Float numArg, Form sender)
  Self.NotePulse("scene end") ; #DEBUG_LINE_NO:117
  Self.ClearAll() ; #DEBUG_LINE_NO:118
EndFunction

Function RegisterWithDoctor()
  If Core ; #DEBUG_LINE_NO:122
    Core.RegisterDoctorAddonDetailed(Self.sAddonName, Self.sChannelName, Self.sVersion, Self.GetStatusLine(), Self.GetReportStatusLine()) ; #DEBUG_LINE_NO:123
  EndIf
EndFunction

String Function GetStatusLine()
  If !bEnabled ; #DEBUG_LINE_NO:128
    Return "armed" ; #DEBUG_LINE_NO:129
  EndIf
  If !Self.HasAssets() ; #DEBUG_LINE_NO:131
    Return "NoAsset" ; #DEBUG_LINE_NO:132
  EndIf
  If stringutil.Find(_status, "NoVoice", 0) >= 0 ; #DEBUG_LINE_NO:134
    Return "NoVoice" ; #DEBUG_LINE_NO:135
  EndIf
  If stringutil.Find(_status, "NoAsset", 0) >= 0 || stringutil.Find(_status, "missing", 0) >= 0 ; #DEBUG_LINE_NO:137
    Return "NoAsset" ; #DEBUG_LINE_NO:138
  EndIf
  If stringutil.Find(_status, "Cooldown", 0) >= 0 ; #DEBUG_LINE_NO:140
    Return "Cooldown" ; #DEBUG_LINE_NO:141
  EndIf
  If stringutil.Find(_status, "Yield", 0) >= 0 || stringutil.Find(_status, "Yield", 0) >= 0 || stringutil.Find(_status, "Disabled", 0) >= 0 ; #DEBUG_LINE_NO:143
    Return "Yield" ; #DEBUG_LINE_NO:144
  EndIf
  If stringutil.Find(_status, "Actor table full", 0) >= 0 ; #DEBUG_LINE_NO:146
    Return "full" ; #DEBUG_LINE_NO:147
  EndIf
  If stringutil.Find(_status, "Panic", 0) >= 0 ; #DEBUG_LINE_NO:149
    Return "reset" ; #DEBUG_LINE_NO:150
  EndIf
  If stringutil.Find(_status, "clear", 0) >= 0 || stringutil.Find(_status, "clear", 0) >= 0 ; #DEBUG_LINE_NO:152
    Return "clear" ; #DEBUG_LINE_NO:153
  EndIf
  If stringutil.Find(_status, "Owner=", 0) >= 0 ; #DEBUG_LINE_NO:155
    Return "active" ; #DEBUG_LINE_NO:156
  EndIf
  Return "ON" ; #DEBUG_LINE_NO:158
EndFunction

String Function GetReportStatusLine()
  If !bEnabled ; #DEBUG_LINE_NO:162
    Return "Default OFF / Experimental / Say path idle / " + Self.DebugReport() ; #DEBUG_LINE_NO:163
  EndIf
  If !Self.HasAssets() ; #DEBUG_LINE_NO:165
    Return "Enabled / Lip-Sync=NotBaked / " + Self.DebugReport() ; #DEBUG_LINE_NO:166
  EndIf
  Return "Enabled / Experimental / " + _status + " / clip " + _lastClip + " / " + Self.DebugReport() ; #DEBUG_LINE_NO:168
EndFunction

String Function GetDoctorLine()
  Return Self.sChannelName + " | " + Self.GetReportStatusLine() ; #DEBUG_LINE_NO:172
EndFunction

String Function GetYieldReason()
  If !Self.HasAssets() ; #DEBUG_LINE_NO:176
    Return "NoAsset / wire FormLists and run the self-bake toolkit" ; #DEBUG_LINE_NO:177
  EndIf
  Return _status ; #DEBUG_LINE_NO:179
EndFunction

String Function GetTopicSummary()
  Return Self.ListSummary("DefaultBuild", DefaultBuildTopics) + " | " + Self.ListSummary("DefaultSustain", DefaultSustainTopics) + " | " + Self.ListSummary("DefaultPeak", DefaultPeakTopics) + " | " + Self.ListSummary("DefaultClimax", DefaultClimaxTopics) + " | VoiceTypes=" + Self.ListCountText(VoiceTypes) ; #DEBUG_LINE_NO:183
EndFunction

Function PanicReset()
  Self.ClearAll() ; #DEBUG_LINE_NO:187
  _status = "Panic reset: yielded" ; #DEBUG_LINE_NO:188
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:189
EndFunction

Function RequestClip(Actor akActor, Float afIntensity)
  If afIntensity >= 0.800000012 ; #DEBUG_LINE_NO:193
    Self.TryClip(akActor, Self.PHASE_CLIMAX, "ManualClimax") ; #DEBUG_LINE_NO:194
  ElseIf afIntensity >= 0.449999988 ; #DEBUG_LINE_NO:195
    Self.TryClip(akActor, Self.PHASE_SUSTAIN, "ManualSustain") ; #DEBUG_LINE_NO:196
  Else
    Self.TryClip(akActor, Self.PHASE_BUILD, "ManualBuild") ; #DEBUG_LINE_NO:198
  EndIf
EndFunction

Function TestClipOnSceneActor()
  If !bEnabled || !bOwnVoice || !Core || !Core.IsRunningScene() ; #DEBUG_LINE_NO:203
    Self.NoteSkip("test: disabled/owns voice/no scene") ; #DEBUG_LINE_NO:204
    Debug.Notification("SLED Lip-Sync: enable Lip-Sync and owns voice in a SexLab scene") ; #DEBUG_LINE_NO:205
    Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:206
    Return  ; #DEBUG_LINE_NO:207
  EndIf
  If !Self.HasAssets() ; #DEBUG_LINE_NO:209
    Self.NoteSkip("test: no assets") ; #DEBUG_LINE_NO:210
    Debug.Notification("SLED Lip-Sync: topic/FormList assets are not wired") ; #DEBUG_LINE_NO:211
    Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:212
    Return  ; #DEBUG_LINE_NO:213
  EndIf
  Int count = Core.SceneActorCount() ; #DEBUG_LINE_NO:215
  Int i = 0 ; #DEBUG_LINE_NO:216
  While i < count ; #DEBUG_LINE_NO:217
    Actor a = Core.SceneActor(i) ; #DEBUG_LINE_NO:218
    If a as Bool && Core.ActorConsentFloor(a) ; #DEBUG_LINE_NO:219
      Int idx = Self.TrackActor(a) ; #DEBUG_LINE_NO:220
      If idx >= 0 ; #DEBUG_LINE_NO:221
        _cooldownUntil[idx] = 0.0 ; #DEBUG_LINE_NO:222
        Self.TryClip(a, Self.PHASE_CLIMAX, "ManualClimax") ; #DEBUG_LINE_NO:223
        Return  ; #DEBUG_LINE_NO:224
      EndIf
    EndIf
    i += 1 ; #DEBUG_LINE_NO:227
  EndWhile
  Self.NoteSkip("test: no scene actor") ; #DEBUG_LINE_NO:229
  Debug.Notification("SLED Lip-Sync: no valid scene actor") ; #DEBUG_LINE_NO:230
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:231
EndFunction

Bool Function HasAssets()
  Return Self.HasTopics(DefaultBuildTopics) || Self.HasTopics(DefaultSustainTopics) || Self.HasTopics(DefaultPeakTopics) || Self.HasTopics(DefaultClimaxTopics) ; #DEBUG_LINE_NO:235
EndFunction

Function TryClip(Actor a, Int phaseId, String phase)
  Topic clipTopic = Self.PickTopic(a, phaseId) ; #DEBUG_LINE_NO:239
  If !Self.CanSpeak(a, clipTopic) ; #DEBUG_LINE_NO:240
    Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:241
    Return  ; #DEBUG_LINE_NO:242
  EndIf
  Int idx = Self.TrackActor(a) ; #DEBUG_LINE_NO:244
  If idx < 0 ; #DEBUG_LINE_NO:245
    _status = "Actor table full" ; #DEBUG_LINE_NO:246
    Self.NoteSkip("clip: actor table full") ; #DEBUG_LINE_NO:247
    Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:248
    Return  ; #DEBUG_LINE_NO:249
  EndIf
  Float now = Utility.GetCurrentRealTime() ; #DEBUG_LINE_NO:251
  If now < _cooldownUntil[idx] ; #DEBUG_LINE_NO:252
    _status = "Cooldown" ; #DEBUG_LINE_NO:253
    Self.NoteSkip("clip: cooldown") ; #DEBUG_LINE_NO:254
    Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:255
    Return  ; #DEBUG_LINE_NO:256
  EndIf
  _cooldownUntil[idx] = now + Self.CooldownSeconds(phase) ; #DEBUG_LINE_NO:258
  _lastNow = now ; #DEBUG_LINE_NO:259
  Self.SignalMouthOwner(a, now + Self.ClipMouthSeconds(phase) + Self.fMouthHoldMargin) ; #DEBUG_LINE_NO:260
  a.Say(clipTopic, None, False) ; #DEBUG_LINE_NO:261
  _status = "Owner=own pack / " + phase ; #DEBUG_LINE_NO:262
  _lastClip = phase ; #DEBUG_LINE_NO:263
  VoiceType vt = a.GetVoiceType() ; #DEBUG_LINE_NO:264
  If vt ; #DEBUG_LINE_NO:265
    Self.DebugNote("Say " + phase + " voice=" + vt.GetFormID() as String) ; #DEBUG_LINE_NO:266
  Else
    Self.DebugNote("Say " + phase + " voice=none") ; #DEBUG_LINE_NO:268
  EndIf
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:270
EndFunction

Bool Function CanSpeak(Actor a, Topic clipTopic)
  If !bEnabled || !bOwnVoice || !a || !Core ; #DEBUG_LINE_NO:274
    _status = "Disabled/yield" ; #DEBUG_LINE_NO:275
    Self.NoteSkip("speak: disabled/owns voice/no actor/core") ; #DEBUG_LINE_NO:276
    Return False ; #DEBUG_LINE_NO:277
  EndIf
  If a.IsDead() || a.IsChild() ; #DEBUG_LINE_NO:279
    _status = "Yielded(invalid actor)" ; #DEBUG_LINE_NO:280
    Self.NoteSkip("speak: invalid actor") ; #DEBUG_LINE_NO:281
    Return False ; #DEBUG_LINE_NO:282
  EndIf
  If !Core.IsRunningScene() || !Core.IsHuman(a) ; #DEBUG_LINE_NO:284
    _status = "Yielded(no scene)" ; #DEBUG_LINE_NO:285
    Self.NoteSkip("speak: no scene/not human") ; #DEBUG_LINE_NO:286
    Return False ; #DEBUG_LINE_NO:287
  EndIf
  If !Core.ActorConsentFloor(a) ; #DEBUG_LINE_NO:289
    _status = "Yielded(consent floor)" ; #DEBUG_LINE_NO:290
    Self.NoteSkip("speak: consent floor") ; #DEBUG_LINE_NO:291
    Return False ; #DEBUG_LINE_NO:292
  EndIf
  If clipTopic == None ; #DEBUG_LINE_NO:294
    If Self.HasAssets() ; #DEBUG_LINE_NO:295
      _status = "Lip-Sync=NoVoiceForType" ; #DEBUG_LINE_NO:296
      Self.NoteSkip("speak: no voice for type") ; #DEBUG_LINE_NO:297
    Else
      _status = "NoAsset / missing data list" ; #DEBUG_LINE_NO:299
      Self.NoteSkip("speak: missing data list") ; #DEBUG_LINE_NO:300
    EndIf
    Return False ; #DEBUG_LINE_NO:302
  EndIf
  String owner = Core.MouthYieldOwner(a, -1) ; #DEBUG_LINE_NO:304
  If owner != "" && owner != "Lip-Sync" ; #DEBUG_LINE_NO:305
    _status = "Yielded(" + owner + ")" ; #DEBUG_LINE_NO:306
    Self.NoteSkip("speak: yielded " + owner) ; #DEBUG_LINE_NO:307
    Return False ; #DEBUG_LINE_NO:308
  EndIf
  If mfgconsolefuncext.IsInDialogue(a) ; #DEBUG_LINE_NO:310
    _status = "Yielded(dialogue)" ; #DEBUG_LINE_NO:311
    Self.NoteSkip("speak: dialogue") ; #DEBUG_LINE_NO:312
    Return False ; #DEBUG_LINE_NO:313
  EndIf
  Return True ; #DEBUG_LINE_NO:315
EndFunction

Function SignalMouthOwner(Actor a, Float untilTime)
  If !a ; #DEBUG_LINE_NO:319
    Return  ; #DEBUG_LINE_NO:320
  EndIf
  Self.SendModEvent(Self.sMouthEvent, "" + a.GetFormID() as String, untilTime) ; #DEBUG_LINE_NO:322
EndFunction

Float Function ClipMouthSeconds(String phase)
  If phase == "climax" || phase == "ManualClimax" ; #DEBUG_LINE_NO:326
    Return 1.549999952 ; #DEBUG_LINE_NO:327
  ElseIf phase == "Peak" || phase == "ManualPeak" ; #DEBUG_LINE_NO:328
    Return 1.450000048 ; #DEBUG_LINE_NO:329
  ElseIf phase == "Sustain" || phase == "ManualSustain" ; #DEBUG_LINE_NO:330
    Return 1.0 ; #DEBUG_LINE_NO:331
  EndIf
  Return 0.850000024 ; #DEBUG_LINE_NO:333
EndFunction

Topic Function PickTopic(Actor a, Int phaseId)
  FormList phaseList = Self.PhaseListForActor(a, phaseId) ; #DEBUG_LINE_NO:337
  If !Self.HasTopics(phaseList) ; #DEBUG_LINE_NO:338
    Return None ; #DEBUG_LINE_NO:339
  EndIf
  Int count = phaseList.GetSize() ; #DEBUG_LINE_NO:341
  Int pick = Utility.RandomInt(0, count - 1) ; #DEBUG_LINE_NO:342
  Return phaseList.GetAt(pick) as Topic ; #DEBUG_LINE_NO:343
EndFunction

FormList Function PhaseListForActor(Actor a, Int phaseId)
  FormList voiceList = Self.VoicePhaseList(a, phaseId) ; #DEBUG_LINE_NO:347
  If Self.HasTopics(voiceList) ; #DEBUG_LINE_NO:348
    Return voiceList ; #DEBUG_LINE_NO:349
  EndIf
  If bUseDefaultFallback ; #DEBUG_LINE_NO:351
    Return Self.DefaultPhaseList(phaseId) ; #DEBUG_LINE_NO:352
  EndIf
  Return None ; #DEBUG_LINE_NO:354
EndFunction

FormList Function VoicePhaseList(Actor a, Int phaseId)
  If !a || !VoiceTypes ; #DEBUG_LINE_NO:358
    Return None ; #DEBUG_LINE_NO:359
  EndIf
  VoiceType actorVoice = a.GetVoiceType() ; #DEBUG_LINE_NO:361
  If !actorVoice ; #DEBUG_LINE_NO:362
    Return None ; #DEBUG_LINE_NO:363
  EndIf
  Int idx = Self.IndexOfForm(VoiceTypes, actorVoice as Form) ; #DEBUG_LINE_NO:365
  If idx < 0 ; #DEBUG_LINE_NO:366
    Return None ; #DEBUG_LINE_NO:367
  EndIf
  FormList bucket = Self.VoiceBucketList(phaseId) ; #DEBUG_LINE_NO:369
  If !bucket || idx >= bucket.GetSize() ; #DEBUG_LINE_NO:370
    Return None ; #DEBUG_LINE_NO:371
  EndIf
  Return bucket.GetAt(idx) as FormList ; #DEBUG_LINE_NO:373
EndFunction

FormList Function VoiceBucketList(Int phaseId)
  If phaseId == Self.PHASE_BUILD ; #DEBUG_LINE_NO:377
    Return VoiceBuildTopicLists ; #DEBUG_LINE_NO:378
  ElseIf phaseId == Self.PHASE_SUSTAIN ; #DEBUG_LINE_NO:379
    Return VoiceSustainTopicLists ; #DEBUG_LINE_NO:380
  ElseIf phaseId == Self.PHASE_PEAK ; #DEBUG_LINE_NO:381
    Return VoicePeakTopicLists ; #DEBUG_LINE_NO:382
  ElseIf phaseId == Self.PHASE_CLIMAX ; #DEBUG_LINE_NO:383
    Return VoiceClimaxTopicLists ; #DEBUG_LINE_NO:384
  EndIf
  Return None ; #DEBUG_LINE_NO:386
EndFunction

FormList Function DefaultPhaseList(Int phaseId)
  If phaseId == Self.PHASE_BUILD ; #DEBUG_LINE_NO:390
    Return DefaultBuildTopics ; #DEBUG_LINE_NO:391
  ElseIf phaseId == Self.PHASE_SUSTAIN ; #DEBUG_LINE_NO:392
    Return DefaultSustainTopics ; #DEBUG_LINE_NO:393
  ElseIf phaseId == Self.PHASE_PEAK ; #DEBUG_LINE_NO:394
    Return DefaultPeakTopics ; #DEBUG_LINE_NO:395
  ElseIf phaseId == Self.PHASE_CLIMAX ; #DEBUG_LINE_NO:396
    Return DefaultClimaxTopics ; #DEBUG_LINE_NO:397
  EndIf
  Return None ; #DEBUG_LINE_NO:399
EndFunction

Int Function IndexOfForm(FormList listRef, Form needle)
  If !listRef || !needle ; #DEBUG_LINE_NO:403
    Return -1 ; #DEBUG_LINE_NO:404
  EndIf
  Int i = 0 ; #DEBUG_LINE_NO:406
  Int count = listRef.GetSize() ; #DEBUG_LINE_NO:407
  While i < count ; #DEBUG_LINE_NO:408
    If listRef.GetAt(i) == needle ; #DEBUG_LINE_NO:409
      Return i ; #DEBUG_LINE_NO:410
    EndIf
    i += 1 ; #DEBUG_LINE_NO:412
  EndWhile
  Return -1 ; #DEBUG_LINE_NO:414
EndFunction

Bool Function HasTopics(FormList listRef)
  Return listRef != None && listRef.GetSize() > 0 ; #DEBUG_LINE_NO:418
EndFunction

String Function ListSummary(String label, FormList listRef)
  Return label + "=" + Self.ListCountText(listRef) ; #DEBUG_LINE_NO:422
EndFunction

String Function ListCountText(FormList listRef)
  If !listRef ; #DEBUG_LINE_NO:426
    Return "missing" ; #DEBUG_LINE_NO:427
  EndIf
  Return "" + listRef.GetSize() as String ; #DEBUG_LINE_NO:429
EndFunction

Float Function CooldownSeconds(String phase)
  If phase == "climax" || phase == "ManualClimax" ; #DEBUG_LINE_NO:433
    Return 4.0 ; #DEBUG_LINE_NO:434
  ElseIf phase == "Peak" || phase == "ManualPeak" ; #DEBUG_LINE_NO:435
    Return 5.0 ; #DEBUG_LINE_NO:436
  ElseIf phase == "Sustain" || phase == "ManualSustain" ; #DEBUG_LINE_NO:437
    Return 5.5 ; #DEBUG_LINE_NO:438
  EndIf
  Return 6.0 - Self.ClampF(fRate, 0.0, 1.0) * 3.0 ; #DEBUG_LINE_NO:440
EndFunction

Int Function TrackActor(Actor a)
  Self.InitState() ; #DEBUG_LINE_NO:444
  Int empty = -1 ; #DEBUG_LINE_NO:445
  Int i = 0 ; #DEBUG_LINE_NO:446
  While i < _actors.Length ; #DEBUG_LINE_NO:447
    If _actors[i] == a ; #DEBUG_LINE_NO:448
      Return i ; #DEBUG_LINE_NO:449
    ElseIf !_actors[i] && empty < 0 ; #DEBUG_LINE_NO:450
      empty = i ; #DEBUG_LINE_NO:451
    EndIf
    i += 1 ; #DEBUG_LINE_NO:453
  EndWhile
  If empty >= 0 ; #DEBUG_LINE_NO:455
    _actors[empty] = a ; #DEBUG_LINE_NO:456
    _cooldownUntil[empty] = 0.0 ; #DEBUG_LINE_NO:457
    _actorCount = Self.CountTrackedActors() ; #DEBUG_LINE_NO:458
    Self.DebugNote("actor tracked " + _actorCount as String) ; #DEBUG_LINE_NO:459
  EndIf
  Return empty ; #DEBUG_LINE_NO:461
EndFunction

Function ClearActor(Actor a)
  If !_actors || !a ; #DEBUG_LINE_NO:465
    Return  ; #DEBUG_LINE_NO:466
  EndIf
  Int i = 0 ; #DEBUG_LINE_NO:468
  While i < _actors.Length ; #DEBUG_LINE_NO:469
    If _actors[i] == a ; #DEBUG_LINE_NO:470
      Self.SignalMouthOwner(a, Utility.GetCurrentRealTime()) ; #DEBUG_LINE_NO:471
      _actors[i] = None ; #DEBUG_LINE_NO:472
      _cooldownUntil[i] = 0.0 ; #DEBUG_LINE_NO:473
      _actorCount = Self.CountTrackedActors() ; #DEBUG_LINE_NO:474
    EndIf
    i += 1 ; #DEBUG_LINE_NO:476
  EndWhile
  _status = "Yielded/cleared" ; #DEBUG_LINE_NO:478
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:479
EndFunction

Function ClearAll()
  If _actors ; #DEBUG_LINE_NO:483
    Int i = 0 ; #DEBUG_LINE_NO:484
    While i < _actors.Length ; #DEBUG_LINE_NO:485
      If _actors[i] ; #DEBUG_LINE_NO:486
        Self.SignalMouthOwner(_actors[i], Utility.GetCurrentRealTime()) ; #DEBUG_LINE_NO:487
      EndIf
      _actors[i] = None ; #DEBUG_LINE_NO:489
      _cooldownUntil[i] = 0.0 ; #DEBUG_LINE_NO:490
      i += 1 ; #DEBUG_LINE_NO:491
    EndWhile
  EndIf
  _actorCount = Self.CountTrackedActors() ; #DEBUG_LINE_NO:494
  _status = "All clear" ; #DEBUG_LINE_NO:495
  Self.RegisterWithDoctor() ; #DEBUG_LINE_NO:496
EndFunction

Function NotePulse(String label)
  _lastPulse = Utility.GetCurrentRealTime() ; #DEBUG_LINE_NO:500
  Self.DebugNote("pulse " + label) ; #DEBUG_LINE_NO:501
EndFunction

Function NoteSkip(String reason)
  _lastSkip = reason ; #DEBUG_LINE_NO:505
  Self.DebugNote("skip " + reason) ; #DEBUG_LINE_NO:506
EndFunction

Function DebugNote(String text)
  If bDebug ; #DEBUG_LINE_NO:510
    Debug.Notification("SLED Lip-Sync: " + text) ; #DEBUG_LINE_NO:511
  EndIf
EndFunction

Int Function CountTrackedActors()
  If !_actors ; #DEBUG_LINE_NO:516
    Return 0 ; #DEBUG_LINE_NO:517
  EndIf
  Int count = 0 ; #DEBUG_LINE_NO:519
  Int i = 0 ; #DEBUG_LINE_NO:520
  While i < _actors.Length ; #DEBUG_LINE_NO:521
    If _actors[i] ; #DEBUG_LINE_NO:522
      count += 1 ; #DEBUG_LINE_NO:523
    EndIf
    i += 1 ; #DEBUG_LINE_NO:525
  EndWhile
  Return count ; #DEBUG_LINE_NO:527
EndFunction

String Function DebugReport()
  String pulse = "never" ; #DEBUG_LINE_NO:531
  If _lastPulse > 0.0 ; #DEBUG_LINE_NO:532
    pulse = "" + ((Utility.GetCurrentRealTime() - _lastPulse) as Int) as String ; #DEBUG_LINE_NO:533
  EndIf
  Return ("pulse " + pulse + "s ago | actors " + _actorCount as String) + " | last: " + _lastSkip ; #DEBUG_LINE_NO:535
EndFunction

String Function TopicName(Topic t, String fallback)
  If t ; #DEBUG_LINE_NO:539
    Return fallback + "=wired" ; #DEBUG_LINE_NO:540
  EndIf
  Return fallback + "=missing" ; #DEBUG_LINE_NO:542
EndFunction

Actor Function ActorFromPulse(String strArg)
  Int bar = stringutil.Find(strArg, "|", 0) ; #DEBUG_LINE_NO:546
  If bar < 0 ; #DEBUG_LINE_NO:547
    Return None ; #DEBUG_LINE_NO:548
  EndIf
  String idText = stringutil.Substring(strArg, bar + 1, 0) ; #DEBUG_LINE_NO:550
  Int id = Self.ParseInt(idText) ; #DEBUG_LINE_NO:551
  If id == 0 ; #DEBUG_LINE_NO:552
    Return None ; #DEBUG_LINE_NO:553
  EndIf
  Return Game.GetFormEx(id) as Actor ; #DEBUG_LINE_NO:555
EndFunction

Int Function ParseInt(String value)
  Int len = stringutil.GetLength(value) ; #DEBUG_LINE_NO:559
  If len <= 0 ; #DEBUG_LINE_NO:560
    Return 0 ; #DEBUG_LINE_NO:561
  EndIf
  Int sign = 1 ; #DEBUG_LINE_NO:563
  Int i = 0 ; #DEBUG_LINE_NO:564
  If stringutil.GetNthChar(value, 0) == "-" ; #DEBUG_LINE_NO:565
    sign = -1 ; #DEBUG_LINE_NO:566
    i = 1 ; #DEBUG_LINE_NO:567
  EndIf
  Int out = 0 ; #DEBUG_LINE_NO:569
  While i < len ; #DEBUG_LINE_NO:570
    Int d = Self.DigitValue(stringutil.GetNthChar(value, i)) ; #DEBUG_LINE_NO:571
    If d < 0 ; #DEBUG_LINE_NO:572
      Return 0 ; #DEBUG_LINE_NO:573
    EndIf
    out = out * 10 + d ; #DEBUG_LINE_NO:575
    i += 1 ; #DEBUG_LINE_NO:576
  EndWhile
  Return out * sign ; #DEBUG_LINE_NO:578
EndFunction

Int Function DigitValue(String c)
  If c == "0" ; #DEBUG_LINE_NO:582
    Return 0 ; #DEBUG_LINE_NO:583
  ElseIf c == "1" ; #DEBUG_LINE_NO:584
    Return 1 ; #DEBUG_LINE_NO:585
  ElseIf c == "2" ; #DEBUG_LINE_NO:586
    Return 2 ; #DEBUG_LINE_NO:587
  ElseIf c == "3" ; #DEBUG_LINE_NO:588
    Return 3 ; #DEBUG_LINE_NO:589
  ElseIf c == "4" ; #DEBUG_LINE_NO:590
    Return 4 ; #DEBUG_LINE_NO:591
  ElseIf c == "5" ; #DEBUG_LINE_NO:592
    Return 5 ; #DEBUG_LINE_NO:593
  ElseIf c == "6" ; #DEBUG_LINE_NO:594
    Return 6 ; #DEBUG_LINE_NO:595
  ElseIf c == "7" ; #DEBUG_LINE_NO:596
    Return 7 ; #DEBUG_LINE_NO:597
  ElseIf c == "8" ; #DEBUG_LINE_NO:598
    Return 8 ; #DEBUG_LINE_NO:599
  ElseIf c == "9" ; #DEBUG_LINE_NO:600
    Return 9 ; #DEBUG_LINE_NO:601
  EndIf
  Return -1 ; #DEBUG_LINE_NO:603
EndFunction

Float Function ClampF(Float v, Float lo, Float hi)
  If v < lo ; #DEBUG_LINE_NO:607
    Return lo ; #DEBUG_LINE_NO:608
  ElseIf v > hi ; #DEBUG_LINE_NO:609
    Return hi ; #DEBUG_LINE_NO:610
  EndIf
  Return v ; #DEBUG_LINE_NO:612
EndFunction
