Scriptname IsekaiHeroCraft Hidden
{Natives exported by IsekaiHeroSKSE.dll for other mods' scripts.}

; At a forge-family bench Isekai Hero lends one of each stored material to the
; player and takes the leftovers back to the Dimensional Storage when the menu
; closes. Every one of those moves raises OnItemAdded / OnItemRemoved on scripts
; listening on the player. A listener that reacts to "the player spent an item"
; (a refund, a recycle chance, a cost) should ignore these moves:
;
;   Event OnItemRemoved(Form akBaseItem, int aiItemCount, ObjectReference akItemReference, ObjectReference akDestContainer)
;       if IsekaiHeroCraft.IsStorageTransfer(akDestContainer)
;           return
;       endif
;       ...
;
; Pass akDestContainer from OnItemRemoved, akSourceContainer from OnItemAdded.
; Keyed on the event's own container, so it stays correct however late the
; queued event runs. Without IsekaiHeroSKSE.dll the call logs an unbound-native
; error and returns False, i.e. the listener behaves as it did before.

; True when akOtherContainer is the Dimensional Storage chest.
Bool Function IsStorageTransfer(ObjectReference akOtherContainer) global native

; Run Isekai Hero's self-test now, as it runs after every game load: the report
; goes to IsekaiHeroSKSE.log, including the machine-readable lines
;   [SELFTEST] isekai.<test-id> PASS|FAIL <detail>
;   [SELFTEST] ENDE
; The run is queued for the next frame, so the lines appear shortly after the
; call returns. True when it was queued. No on-screen panel.
Bool Function RunSelfTest() global native

; Test runs only: pay out the next milestone the character is still owed, exactly as
; finishing its quest would (gold panel, rewards, saved in the co-save). Does nothing
; before reincarnation. Queued for the next frame; True when it was queued.
Bool Function DebugGrantNext() global native
