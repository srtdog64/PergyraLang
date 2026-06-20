(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/semantics/01_intent_world_zone.md
          "Theorem: Intent Step Progress" and "Theorem: Authority Soundness"
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Scope: this mechanizes progress + preservation for a SMALL FRAGMENT -- a
  linear sequence of authority-guarded actions (one intent, run in order). It
  is the canonical soundness shape (a well-typed/well-authorized configuration
  never gets stuck) for that fragment only.

  Negative scope (read this): this is NOT whole-language soundness. It does NOT
  cover types, generics, world/zone nesting, effects, relations, projections,
  slots, async, or modules. "Pergyra is proven sound" remains FALSE; this adds
  one more mechanized fragment (intent-step progress under authority) to the
  proof surface, it does not close the whole-language obligation. See
  AxisOwnership.md section 7.1.
*)

Require Import Coq.Init.Nat.
Require Import Coq.Lists.List.
Import ListNotations.

(* ========================================== *)
(* 1. A minimal intent model                  *)
(* ========================================== *)

(* An authority is an abstract name. *)
Definition Authority := nat.

(* An action fires only if its required authority is granted. *)
Record Action : Type := mkAction { act_needs : Authority }.

(* An intent is a linear list of actions, executed head-first. *)
Definition Intent := list Action.

(* The ambient grant set: which authorities are held. *)
Definition Grants := Authority -> bool.

(* Well-authorized: every action in the intent has its authority granted.
   This is the static premise behind docs/01 "authorized_by". *)
Definition WellAuthorized (g : Grants) (i : Intent) : Prop :=
  forall a, In a i -> g (act_needs a) = true.

(* Small-step: fire the head action when its authority is granted. A step that
   is not authorized simply does not exist (no rule), i.e. it is rejected, not
   silently taken. *)
Inductive IntentStep (g : Grants) : Intent -> Intent -> Prop :=
  | StepFire : forall a rest,
      g (act_needs a) = true ->
      IntentStep g (a :: rest) rest.

(* A configuration is complete when nothing remains to run. *)
Definition Complete (i : Intent) : Prop := i = [].

(* ========================================== *)
(* 2. Intent Step Progress                    *)
(* docs/01 "Theorem: Intent Step Progress"     *)
(* ========================================== *)

(* A well-authorized intent is either complete or can take a step: it never
   gets stuck on an action it is allowed to run. *)
Theorem intent_step_progress :
  forall g i, WellAuthorized g i -> Complete i \/ exists i', IntentStep g i i'.
Proof.
  intros g i Hwa. destruct i as [| a rest].
  - left. reflexivity.
  - right. exists rest. apply StepFire. apply Hwa. left. reflexivity.
Qed.

(* ========================================== *)
(* 3. Authority Soundness (preservation)       *)
(* docs/01 "Theorem: Authority Soundness"      *)
(* ========================================== *)

(* Stepping preserves well-authorization: a step never exposes an un-granted
   action. (Preservation for this fragment.) *)
Theorem intent_step_preservation :
  forall g i i', WellAuthorized g i -> IntentStep g i i' -> WellAuthorized g i'.
Proof.
  intros g i i' Hwa Hstep. inversion Hstep; subst.
  intros b Hin. apply Hwa. right. exact Hin.
Qed.

(* Every firing step was authorized -- the step relation cannot fire an action
   whose authority is not granted. This is the "no un-authorized action runs"
   half of Authority Soundness, read directly off the step relation. *)
Theorem intent_step_was_authorized :
  forall g a rest i', IntentStep g (a :: rest) i' -> g (act_needs a) = true.
Proof.
  intros g a rest i' Hstep. inversion Hstep; subst. assumption.
Qed.

(* ========================================== *)
(* 4. Combined: no stuck non-complete state    *)
(* ========================================== *)

(* Progress + preservation over one step: a well-authorized intent is complete,
   or steps to an intent that is itself well-authorized. Iterating this, a
   well-authorized intent runs to completion without ever getting stuck. *)
Corollary intent_no_stuck :
  forall g i, WellAuthorized g i ->
    Complete i \/ exists i', IntentStep g i i' /\ WellAuthorized g i'.
Proof.
  intros g i Hwa. destruct (intent_step_progress g i Hwa) as [Hc | [i' Hs]].
  - left. exact Hc.
  - right. exists i'. split.
    + exact Hs.
    + exact (intent_step_preservation g i i' Hwa Hs).
Qed.

(* ========================================== *)
(* 5. Not done here (honest)                  *)
(* ------------------------------------------ *)
(* - Whole-language soundness (types + generics + world/zone + effects + slots  *)
(*   + async). This fragment is authority-guarded linear intents only.          *)
(* - World/Zone Frontier Termination (docs/01 third theorem) -- needs the       *)
(*   nested world/zone model, not present here.                                 *)
(* ========================================== *)
