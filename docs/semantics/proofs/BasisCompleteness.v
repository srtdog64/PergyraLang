(*
  Pergyra Formal Semantics - Mechanized Sketch
  Target: docs/172 M2 (basis relative completeness) -- first fragment.
  Status: proof-sketch; not beta-closure evidence unless checked by CI (coqc).

  Reference frame: the *static structural* fragment of Milner's bigraphs
  ("The Space and Motion of Communicating Agents", 2009). A bigraph places
  agents in a forest of nested places (the place graph) and independently
  connects them (the link graph). This file fixes a bounded static version
  of that frame and shows, against Pergyra's spatial+connective axes:

    (1) Completeness  (encode_wf / encode_parent / encode_link):
        every well-formed place+link fragment is realized by a Pergyra
        axis configuration -- zones/worlds carry the place graph, channels
        carry the link graph. Nothing bigraph-expressible (in this
        fragment) is lost by the axis vocabulary.
    (2) Conservativity (decode_encode / encode_decode):
        the encoding is a bijection onto channel-only configurations --
        the spatial axis adds nothing beyond the place graph, i.e. the
        axis layer is exactly bigraph-shaped, not an ad-hoc superset.
    (3) Separation    (world_separation / cross_world_needs_channel):
        the contentful theorem. In ANY well-formed axis configuration,
        direct (containment-chain) references cannot cross world roots:
        every channel-free connectivity path stays inside one world tree.
        Cross-world flow is channel-only -- the static shadow of the
        AC-3 boundary discipline (docs/157 theorem T) and of the
        "Channel-only cross-World" design decision.

  Modeling notes (honest scope):
  - Places/zones are numbered so a parent has a smaller index than its
    child. Every finite forest admits such a topological numbering, so
    up to isomorphism no generality is lost; it buys terminating root
    computation without well-founded recursion machinery.
  - Only static structure is modeled. Bigraph *reaction* (dynamics)
    versus intent steps is the next rung, not claimed here.
  - "Direct edge" models a lexical/containment reference (legal only
    along the enclosure chain -- ax_wf); "channel" is the unrestricted
    connector. Theorems (1)-(2) are interface locks (definitional);
    theorem (3) carries the real proof content. Stated in the header so
    nobody mistakes the trivial ones for depth.
*)

Require Import Coq.Lists.List.
Require Import Coq.Arith.Wf_nat.
Require Import Lia.
Import ListNotations.

(* ====================================================== *)
(* 1. Reference frame: static bigraph fragment             *)
(* ====================================================== *)

Record Bigraph := {
  bg_parent : nat -> option nat;   (* place graph: enclosing place *)
  bg_links  : list (nat * nat)     (* link graph: connections      *)
}.

(* Topologically numbered forest: parents are smaller. *)
Definition bg_wf (B : Bigraph) : Prop :=
  forall c p, bg_parent B c = Some p -> p < c.

(* ====================================================== *)
(* 2. Pergyra axis configuration (spatial + connective)    *)
(* ====================================================== *)

Record AxisConfig := {
  ax_encl   : nat -> option nat;   (* zone/world enclosure (spatial axis)  *)
  ax_direct : list (nat * nat);    (* containment-chain direct references  *)
  ax_chans  : list (nat * nat)     (* channels (unrestricted connector)    *)
}.

(* Reflexive-transitive enclosure: y encloses x (or is x). *)
Inductive encl_star (P : nat -> option nat) : nat -> nat -> Prop :=
| encl_refl : forall x, encl_star P x x
| encl_step : forall x p y,
    P x = Some p -> encl_star P p y -> encl_star P x y.

Definition comparable (P : nat -> option nat) (a b : nat) : Prop :=
  encl_star P a b \/ encl_star P b a.

(* Well-formed axis configuration:
   - enclosure is a topologically numbered forest;
   - direct references stay on one enclosure chain (the AC-3-shaped
     restriction: no direct reference across containment). *)
Definition ax_wf (A : AxisConfig) : Prop :=
  (forall c p, ax_encl A c = Some p -> p < c) /\
  (forall a b, In (a, b) (ax_direct A) -> comparable (ax_encl A) a b).

(* ====================================================== *)
(* 3. Completeness: encoding the fragment into the axes    *)
(* ====================================================== *)

Definition encode (B : Bigraph) : AxisConfig :=
  {| ax_encl   := bg_parent B;
     ax_direct := [];
     ax_chans  := bg_links B |}.

Theorem encode_wf : forall B, bg_wf B -> ax_wf (encode B).
Proof.
  intros B H. split.
  - simpl. exact H.
  - simpl. intros a b Hin. destruct Hin.
Qed.

Theorem encode_parent : forall B c p,
  bg_parent B c = Some p <-> ax_encl (encode B) c = Some p.
Proof. intros. simpl. reflexivity. Qed.

Theorem encode_link : forall B a b,
  In (a, b) (bg_links B) <-> In (a, b) (ax_chans (encode B)).
Proof. intros. simpl. reflexivity. Qed.

(* Conservativity: encode is a bijection onto channel-only configs. *)
Definition decode (A : AxisConfig) : Bigraph :=
  {| bg_parent := ax_encl A; bg_links := ax_chans A |}.

Theorem decode_encode : forall B, decode (encode B) = B.
Proof. intros [P L]. reflexivity. Qed.

Theorem encode_decode : forall A,
  ax_direct A = [] -> encode (decode A) = A.
Proof. intros [P D C] H. simpl in H. subst. reflexivity. Qed.

(* ====================================================== *)
(* 4. World roots                                          *)
(* ====================================================== *)

(* Root computation; fuel (S x) suffices because parents are smaller. *)
Fixpoint rootof_fuel (fuel : nat) (P : nat -> option nat) (x : nat) : nat :=
  match fuel with
  | 0 => x
  | S f =>
      match P x with
      | None => x
      | Some p => rootof_fuel f P p
      end
  end.

Definition rootof (P : nat -> option nat) (x : nat) : nat :=
  rootof_fuel (S x) P x.

Lemma rootof_fuel_stable :
  forall (P : nat -> option nat),
    (forall c p, P c = Some p -> p < c) ->
    forall x f1 f2, x < f1 -> x < f2 ->
      rootof_fuel f1 P x = rootof_fuel f2 P x.
Proof.
  intros P HP x.
  induction x as [x IH] using lt_wf_ind.
  intros f1 f2 H1 H2.
  destruct f1 as [| f1]; [lia |].
  destruct f2 as [| f2]; [lia |].
  simpl.
  destruct (P x) as [p |] eqn:E; [| reflexivity].
  assert (Hpx : p < x) by (apply HP; exact E).
  apply IH; lia.
Qed.

Lemma rootof_step :
  forall (P : nat -> option nat),
    (forall c p, P c = Some p -> p < c) ->
    forall x p, P x = Some p -> rootof P x = rootof P p.
Proof.
  intros P HP x p E.
  unfold rootof.
  replace (rootof_fuel (S x) P x) with (rootof_fuel x P p).
  - apply rootof_fuel_stable; [exact HP | exact (HP x p E) | lia].
  - simpl. rewrite E. reflexivity.
Qed.

Lemma encl_star_root :
  forall (P : nat -> option nat),
    (forall c p, P c = Some p -> p < c) ->
    forall x y, encl_star P x y -> rootof P x = rootof P y.
Proof.
  intros P HP x y H. induction H.
  - reflexivity.
  - rewrite (rootof_step P HP x p H). exact IHencl_star.
Qed.

Lemma comparable_same_root :
  forall (P : nat -> option nat),
    (forall c p, P c = Some p -> p < c) ->
    forall a b, comparable P a b -> rootof P a = rootof P b.
Proof.
  intros P HP a b [H | H].
  - apply encl_star_root; assumption.
  - symmetry. apply encl_star_root; assumption.
Qed.

(* ====================================================== *)
(* 5. Separation: cross-world flow is channel-only         *)
(* ====================================================== *)

(* Undirected step over DIRECT edges only (channels excluded). *)
Inductive dstep (A : AxisConfig) : nat -> nat -> Prop :=
| dstep_fwd : forall a b, In (a, b) (ax_direct A) -> dstep A a b
| dstep_bwd : forall a b, In (a, b) (ax_direct A) -> dstep A b a.

(* Channel-free connectivity: reflexive-transitive closure of dstep. *)
Inductive dconn (A : AxisConfig) : nat -> nat -> Prop :=
| dconn_refl : forall x, dconn A x x
| dconn_step : forall x y z, dstep A x y -> dconn A y z -> dconn A x z.

(* THE theorem: a channel-free path never changes the world root. *)
Theorem world_separation :
  forall A, ax_wf A ->
  forall x y, dconn A x y ->
    rootof (ax_encl A) x = rootof (ax_encl A) y.
Proof.
  intros A [Hforest Hdir] x y Hconn.
  induction Hconn as [x | x y z Hstep Hconn IH].
  - reflexivity.
  - assert (Hxy : rootof (ax_encl A) x = rootof (ax_encl A) y).
    { inversion Hstep as [a b Hin Ha Hb | a b Hin Ha Hb]; subst.
      - apply comparable_same_root; [exact Hforest | apply Hdir; exact Hin].
      - symmetry.
        apply comparable_same_root; [exact Hforest | apply Hdir; exact Hin]. }
    rewrite Hxy. exact IH.
Qed.

(* Contrapositive, phrased as the design decision reads:
   crossing world roots REQUIRES a channel. *)
Corollary cross_world_needs_channel :
  forall A, ax_wf A ->
  forall x y,
    rootof (ax_encl A) x <> rootof (ax_encl A) y ->
    ~ dconn A x y.
Proof.
  intros A HA x y Hneq Hc. apply Hneq.
  eapply world_separation; eauto.
Qed.

(* Encoded bigraph fragments have no direct edges at all, so their
   channel-free connectivity is trivial -- all bigraph connectivity
   rides on channels, which are unrestricted (completeness side). *)
Lemma encode_dconn_trivial :
  forall B x y, dconn (encode B) x y -> x = y.
Proof.
  intros B x y H.
  induction H as [x | x y z Hs Hc IH].
  - reflexivity.
  - inversion Hs as [a b Hin | a b Hin]; subst; simpl in Hin; destruct Hin.
Qed.

(* The direct-edge restriction is not vacuous: a direct reference
   between two ROOT zones (incomparable) is ill-formed. *)
Example direct_cross_world_illformed :
  ~ ax_wf {| ax_encl := fun _ => None;
             ax_direct := [(0, 1)];
             ax_chans := [] |}.
Proof.
  intros [_ Hdir].
  specialize (Hdir 0 1 (or_introl eq_refl)).
  destruct Hdir as [H | H]; inversion H; subst; discriminate.
Qed.
