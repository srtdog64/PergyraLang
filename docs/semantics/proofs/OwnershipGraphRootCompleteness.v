(* Root completeness for local cycle reclamation.

   OwnershipGraphCycleReclaim proves that the local check never returns a
   node reachable from a given root list. This file closes the premise: a
   small program language with values that contain links, liveness
   certificates, calls with suspended frames, handled errors and a reclaim
   statement, and a proof that reclaiming with roots from checked language
   certificates preserves every terminating reference run under its invariants.

   The guarantee is stated against the language, not against the compiler's
   roots. The reference semantics [rexec .. false] never reclaims. Anything
   it reads, follows, writes or deletes later is what must be kept; nothing
   in it depends on liveness. The reclaiming semantics [rexec .. true] runs
   the same program and, at each reclaim statement, takes its roots from the
   values of the certified live variables and of every suspended caller.

   Proved here:
   - [links_of_complete], [extractable_link_is_enumerated]: every link or
     view a projection path can extract from a value is in [links_of] of
     that value. Records, arrays, an enum's active payload, inout packets and
     closure captures are modelled as aggregates, so one structural
     definition covers them; the refinement from real type layouts to these
     aggregates is the producer's obligation.
   - [rcheck]: a liveness checker over the statement tree. Certificates are
     checked, not trusted: a loop head must contain the body's live-in, the
     exit set, the condition and the error target; a call's [keep] must
     contain everything live after it and the handler's live-in; a
     reclaim's root variables must contain everything live after it. Every
     statement that can fail keeps the handler's live-in.
   - [reclaim_simulates] (the main theorem): for every checked program
     whose routines are checked, every reference run from a state that
     satisfies [SimInv] is reproduced by the reclaiming run with the same
     final environment, trace and outcome, normal or handled error.
     [reclaim_preserves_every_reference_run] starts it from any state whose
     graph invariant holds, whose borrow table is empty and whose live links
     and edges name issued identities. The invariant it carries says every
     slot the reclaimer deleted is unreachable from the live roots at that
     statement boundary. So everything the rest of the reference run reads,
     follows, writes or deletes is still there. Missing roots would be a
     semantic failure, extra roots only a delay; the theorem needs only the
     inclusion.
   - Suspended frames: a call extends the frame roots with the caller's
     [keep] variables, so a callee's reclaim keeps the caller's live links
     and the live-in of the caller's handler; the callee's result variable is
     live to its end, and the caller binds it in the same step. An error
     carries no payload in this model; a payload would be a returned value,
     as CL6 packs it.
   - Cleanup authority: reclaim and explicit delete run only under the
     store's cleanup right ([reclaim_without_right_is_identity]); a value
     cannot carry that right, so copying links never grants it.
     [ledger_rights] projects the canonical authority's current permission;
     grant, transfer and consumption are connected at this boundary.
     [checked_ledger_reclaim_simulates] connects maintained counts and checked
     roots to the simulation. It does NOT couple the graph heap to the forest
     or apply its whole-unit lease/pin retirement checks.
   - [rexec_deterministic], and falsifiers that use the same semantics with a
     wrong root producer or a wrong certificate. Each turns a successful
     reference read into a refusal: a link held only inside an aggregate
     ([aggregate_only_link_needs_deep_roots]), a link held only by a
     suspended caller ([caller_frame_link_needs_frame_roots], and the
     checker refusing a [keep] that omits it), a backing held only by a
     derived view ([view_only_backing_needs_view_roots]), and a returned
     link dropped from the callee's roots before the caller holds it
     ([returned_link_must_stay_rooted]).

   Boundary. Allocation choices are inputs, as in OwnershipGraphLinks: an
   insert whose slot or blocks are refused has no transition in either run.
   Reclamation can only make more allocation choices admissible; the
   theorem covers every reference run that completes. Temporaries are named
   variables (ANF). A [GView] value roots its backing identity, but physical
   leases, pinning, addresses and range validity are not represented;
   the graph machine's borrow table is empty between statements. A node
   holds only store-relative edges, so links into other stores live in
   values. Weak links that may be cleared by reclamation are not provided.
   The production producer of liveness, frame maps and values, concurrency,
   counter overflow and the compiler/runtime refinement remain outside. *)
Require Import Stdlib.Lists.List Stdlib.Arith.PeanoNat Stdlib.Bool.Bool.
Require Import Stdlib.Sorting.Permutation Stdlib.micromega.Lia.
Require Import OwnershipCleanCore OwnershipGraphLinks OwnershipGraphCycleReclaim.
Require OwnershipTeardown OwnershipTeardownAuthority.
Import ListNotations.

(* ------------------------------------------------------------------ *)
(* Values and the links they hold                                       *)
(* ------------------------------------------------------------------ *)

(* An aggregate covers records, arrays, an enum's active variant with its
   payload, an inout result packet and a closure with its captures. *)
Inductive GVal : Type :=
| GNum (n : nat)
| GLnk (l : Link)
| GView (l : Link)
| GAgg (tag : nat) (vs : list GVal).

Fixpoint links_of (v : GVal) : list Link :=
  match v with
  | GNum _ => []
  | GLnk l => [l]
  | GView l => [l]
  | GAgg _ vs => (fix go (ws : list GVal) : list Link :=
                    match ws with [] => [] | w :: r => links_of w ++ go r end) vs
  end.

Lemma links_of_agg : forall t vs, links_of (GAgg t vs) = flat_map links_of vs.
Proof.
  intros t vs. induction vs as [|w r IH]; [reflexivity|].
  change (links_of (GAgg t (w :: r))) with (links_of w ++ links_of (GAgg t r)).
  rewrite IH. reflexivity.
Qed.

Fixpoint vat (v : GVal) (p : list nat) : option GVal :=
  match p with
  | [] => Some v
  | i :: r => match v with
              | GAgg _ vs => match nth_error vs i with Some w => vat w r | None => None end
              | _ => None
              end
  end.

Theorem links_of_complete : forall p v w, vat v p = Some w ->
  forall l, In l (links_of w) -> In l (links_of v).
Proof.
  induction p as [|i r IH]; intros v w H l Hl; simpl in H.
  - injection H as H. subst. exact Hl.
  - destruct v as [n|l0|l0|t vs]; try discriminate.
    destruct (nth_error vs i) as [w0|] eqn:E; [|discriminate].
    rewrite links_of_agg. apply in_flat_map. exists w0.
    split; [exact (nth_error_In _ _ E)| exact (IH w0 w H l Hl)].
Qed.

Definition as_link (v : GVal) : option Link :=
  match v with GLnk l | GView l => Some l | _ => None end.

Lemma as_link_in : forall v l, as_link v = Some l -> In l (links_of v).
Proof.
  intros [n|l0|l0|t vs] l H; simpl in H; try discriminate; injection H as H; subst; simpl; left; reflexivity.
Qed.

Theorem extractable_link_is_enumerated : forall v p w l,
  vat v p = Some w -> as_link w = Some l -> In l (links_of v).
Proof. intros v p w l Hp Hl. exact (links_of_complete p v w Hp l (as_link_in w l Hl)). Qed.

(* Wrong producers, used only by the falsifiers. *)
Definition top_links (v : GVal) : list Link := match v with GLnk l => [l] | _ => [] end.
Fixpoint links_no_views (v : GVal) : list Link :=
  match v with
  | GLnk l => [l]
  | GAgg _ vs => (fix go (ws : list GVal) : list Link :=
                    match ws with [] => [] | w :: r => links_no_views w ++ go r end) vs
  | _ => []
  end.

(* ------------------------------------------------------------------ *)
(* Environments and the language                                        *)
(* ------------------------------------------------------------------ *)

Definition REnv := Var -> option GVal.
Definition rempty : REnv := fun _ => None.
Definition rupd (rho : REnv) (x : Var) (v : GVal) : REnv :=
  fun y => if Nat.eqb y x then Some v else rho y.

Fixpoint rbind (ps : list Var) (vs : list GVal) : REnv :=
  match ps, vs with
  | p :: ps', v :: vs' => rupd (rbind ps' vs') p v
  | _, _ => rempty
  end.

Definition val_links (lk : GVal -> list Link) (rho : REnv) (x : Var) : list Link :=
  match rho x with Some v => lk v | None => [] end.
Definition vars_links (lk : GVal -> list Link) (rho : REnv) (xs : list Var) : list Link :=
  flat_map (val_links lk rho) xs.

Inductive RStmt : Type :=
| RSkip
| RNum (x : Var) (n : nat)
| RCopy (x y : Var)
| RPack (x : Var) (t : nat) (ys : list Var)
| RProj (x y : Var) (i : nat)
| RView (x y : Var)
| RRead (x y : Var)
| REdgeOf (x y : Var) (k : nat)
| RSetEdges (y : Var) (d : nat) (ys : list Var)
| RInsert (x : Var) (sid idx d : nat) (ys : list Var) (bs : list Block) (tb : Block)
| RDelete (y : Var)
| RSeq (a b : RStmt)
| RIf (c : Var) (a b : RStmt)
| RWhile (c : Var) (h : list Var) (b : RStmt)
| RTry (b h : RStmt)
| RCall (x : Var) (f : nat) (ys : list Var) (keep : list Var)
| RReclaim (sid : nat) (C : list nat) (fuel : nat) (R : list Var).

Inductive ROut := RNorm | RErr.

Definition RFun := (list Var * RStmt * Var)%type.

(* The liveness checker. [L] is live after the statement, [E] is the live-in
   of the handler an error would reach. Annotations are certificates. *)
Fixpoint rcheck (s : RStmt) (L E : list Var) : option (list Var) :=
  match s with
  | RSkip => Some L
  | RNum x _ => Some (vremove x L)
  | RCopy x y | RProj x y _ | RView x y | RRead x y | REdgeOf x y _ => Some (y :: vremove x L ++ E)
  | RPack x _ ys | RInsert x _ _ _ ys _ _ => Some (ys ++ vremove x L ++ E)
  | RSetEdges y _ ys => Some (y :: ys ++ L ++ E)
  | RDelete y => Some (y :: L ++ E)
  | RSeq a b => match rcheck b L E with Some Lb => rcheck a Lb E | None => None end
  | RIf c a b =>
      match rcheck a L E, rcheck b L E with
      | Some La, Some Lb => Some (c :: La ++ Lb ++ E)
      | _, _ => None
      end
  | RWhile c h b =>
      match rcheck b h E with
      | Some Lb => if inclb Lb h && inclb L h && vmem c h && inclb E h then Some h else None
      | None => None
      end
  | RTry b h => match rcheck h L E with Some Lh => rcheck b L Lh | None => None end
  | RCall x _ ys keep => if inclb (vremove x L) keep && inclb E keep then Some (ys ++ keep) else None
  | RReclaim _ _ _ R => if inclb L R then Some L else None
  end.

(* Every routine body is checked with only its result live at the end and
   nothing live on error; its live-in must be among its parameters. *)
Definition FunsOK (funs : nat -> option RFun) : Prop :=
  forall f ps body ret, funs f = Some (ps, body, ret) ->
    exists Lf, rcheck body [ret] [] = Some Lf /\ incl Lf ps.

Definition link_of (rho : REnv) (y : Var) : option Link :=
  match rho y with Some v => as_link v | None => None end.

Definition node_of (rho : REnv) (g : GS) (y : Var) : option (Link * Node) :=
  match link_of rho y with
  | Some l => match resolve g l with Found nd => Some (l, nd) | Missing _ => None end
  | None => None
  end.

Fixpoint vals_of (rho : REnv) (ys : list Var) : option (list GVal) :=
  match ys with
  | [] => Some []
  | y :: r => match rho y, vals_of rho r with Some v, Some vs => Some (v :: vs) | _, _ => None end
  end.

(* Edges are written from links of the same store. *)
Fixpoint edges_of (rho : REnv) (sid : nat) (ys : list Var) : option (list Edge) :=
  match ys with
  | [] => Some []
  | y :: r =>
      match link_of rho y, edges_of rho sid r with
      | Some l, Some es => if Nat.eqb (lsid l) sid then Some ((lidx l, lgen l) :: es) else None
      | _, _ => None
      end
  end.

Definition proj_of (rho : REnv) (y : Var) (i : nat) : option GVal :=
  match rho y with Some (GAgg _ vs) => nth_error vs i | _ => None end.

Definition edge_link_of (rho : REnv) (g : GS) (y : Var) (k : nat) : option Link :=
  match node_of rho g y with
  | Some (l, nd) => match nth_error (nedges nd) k with
                    | Some e => Some (mkLink (lsid l) (fst e) (snd e))
                    | None => None
                    end
  | None => None
  end.

Definition num_of (rho : REnv) (c : Var) : option nat :=
  match rho c with Some (GNum n) => Some n | _ => None end.

Definition unit_result (p : GS * GRes) : option GS :=
  match snd p with GUnit => Some (fst p) | _ => None end.

Section Lang.
Variable gmax : nat.
Variable funs : nat -> option RFun.
(* One authoritative cleanup-right cell per store, as in
   OwnershipTeardownAuthority; no value carries it. *)
Variable rights : nat -> option nat.
Variable ctx : nat.
(* The root producer: how a value is enumerated, and whether suspended
   frames contribute. The theorem fixes [links_of] and [true]. *)
Variable lk : GVal -> list Link.
Variable frames : bool.

Definition holds (sid : nat) : bool :=
  match rights sid with Some c => Nat.eqb c ctx | None => false end.

(* An edge write takes a write borrow, writes and ends it. *)
Definition write_link (g : GS) (l : Link) (d : nat) (es : list Edge) : option GS :=
  let r1 := gexec gmax g (OBegin l true) in
  match snd r1 with
  | GUnit =>
      let r2 := gexec gmax (fst r1) (OWrite 0 d es) in
      match snd r2 with
      | GUnit => unit_result (gexec gmax (fst r2) (OEnd 0))
      | _ => None
      end
  | _ => None
  end.

Definition set_result (rho : REnv) (g : GS) (y : Var) (d : nat) (ys : list Var) : option GS :=
  match node_of rho g y with
  | Some (l, _) => match edges_of rho (lsid l) ys with Some es => write_link g l d es | None => None end
  | None => None
  end.

Definition delete_result (rho : REnv) (g : GS) (y : Var) : option GS :=
  match link_of rho y with
  | Some l => if holds (lsid l) then unit_result (gexec gmax g (ODelete l)) else None
  | None => None
  end.

Definition reclaim_step (g : GS) (sid : nat) (C : list nat) (fuel : nat) (roots : list Link) : GS :=
  if holds sid then
    match find_store sid (gstores g) with
    | Some s => match trial_garbage fuel s roots C with
                | Some G => grun gmax g (reclaim_ops s G)
                | None => g
                end
    | None => g
    end
  else g.

Definition call_args (rho : REnv) (f : nat) (ys : list Var) : option (list Var * RStmt * Var * list GVal) :=
  match funs f with
  | Some (ps, body, ret) =>
      match vals_of rho ys with
      | Some vs => if Nat.eqb (length ps) (length vs) then Some (ps, body, ret, vs) else None
      | None => None
      end
  | None => None
  end.

Definition frame_roots (K : list Link) (rho : REnv) (keep : list Var) : list Link :=
  K ++ (if frames then vars_links lk rho keep else []).
Definition reclaim_roots (K : list Link) (rho : REnv) (R : list Var) : list Link :=
  vars_links lk rho R ++ (if frames then K else []).

(* [K] is the root list of the suspended frames. *)
Inductive rexec (real : bool) : list Link -> REnv -> GS -> RStmt -> REnv -> GS -> list nat -> ROut -> Prop :=
| X_Skip : forall K rho g, rexec real K rho g RSkip rho g [] RNorm
| X_Num : forall K rho g x n, rexec real K rho g (RNum x n) (rupd rho x (GNum n)) g [] RNorm
| X_Copy : forall K rho g x y v, rho y = Some v ->
    rexec real K rho g (RCopy x y) (rupd rho x v) g [] RNorm
| X_CopyErr : forall K rho g x y, rho y = None -> rexec real K rho g (RCopy x y) rho g [] RErr
| X_Pack : forall K rho g x t ys vs, vals_of rho ys = Some vs ->
    rexec real K rho g (RPack x t ys) (rupd rho x (GAgg t vs)) g [] RNorm
| X_PackErr : forall K rho g x t ys, vals_of rho ys = None -> rexec real K rho g (RPack x t ys) rho g [] RErr
| X_Proj : forall K rho g x y i w, proj_of rho y i = Some w ->
    rexec real K rho g (RProj x y i) (rupd rho x w) g [] RNorm
| X_ProjErr : forall K rho g x y i, proj_of rho y i = None -> rexec real K rho g (RProj x y i) rho g [] RErr
| X_View : forall K rho g x y l, link_of rho y = Some l ->
    rexec real K rho g (RView x y) (rupd rho x (GView l)) g [] RNorm
| X_ViewErr : forall K rho g x y, link_of rho y = None -> rexec real K rho g (RView x y) rho g [] RErr
| X_Read : forall K rho g x y l nd, node_of rho g y = Some (l, nd) ->
    rexec real K rho g (RRead x y) (rupd rho x (GNum (ndata nd))) g [ndata nd] RNorm
| X_ReadErr : forall K rho g x y, node_of rho g y = None -> rexec real K rho g (RRead x y) rho g [] RErr
| X_Edge : forall K rho g x y k l, edge_link_of rho g y k = Some l ->
    rexec real K rho g (REdgeOf x y k) (rupd rho x (GLnk l)) g [] RNorm
| X_EdgeErr : forall K rho g x y k, edge_link_of rho g y k = None ->
    rexec real K rho g (REdgeOf x y k) rho g [] RErr
| X_Set : forall K rho g y d ys g', set_result rho g y d ys = Some g' ->
    rexec real K rho g (RSetEdges y d ys) rho g' [] RNorm
| X_SetErr : forall K rho g y d ys, set_result rho g y d ys = None ->
    rexec real K rho g (RSetEdges y d ys) rho g [] RErr
(* An inadmissible allocation choice has no transition. *)
| X_Insert : forall K rho g x sid idx d ys bs tb es g' l, edges_of rho sid ys = Some es ->
    gexec gmax g (OInsert sid idx d es bs tb) = (g', GLink l) ->
    rexec real K rho g (RInsert x sid idx d ys bs tb) (rupd rho x (GLnk l)) g' [] RNorm
| X_InsertErr : forall K rho g x sid idx d ys bs tb, edges_of rho sid ys = None ->
    rexec real K rho g (RInsert x sid idx d ys bs tb) rho g [] RErr
| X_Delete : forall K rho g y g', delete_result rho g y = Some g' ->
    rexec real K rho g (RDelete y) rho g' [] RNorm
| X_DeleteErr : forall K rho g y, delete_result rho g y = None -> rexec real K rho g (RDelete y) rho g [] RErr
| X_SeqN : forall K rho g a b rho1 g1 tr1 rho2 g2 tr2 o,
    rexec real K rho g a rho1 g1 tr1 RNorm -> rexec real K rho1 g1 b rho2 g2 tr2 o ->
    rexec real K rho g (RSeq a b) rho2 g2 (tr1 ++ tr2) o
| X_SeqE : forall K rho g a b rho1 g1 tr1,
    rexec real K rho g a rho1 g1 tr1 RErr -> rexec real K rho g (RSeq a b) rho1 g1 tr1 RErr
| X_IfT : forall K rho g c a b n rho1 g1 tr o, num_of rho c = Some (S n) ->
    rexec real K rho g a rho1 g1 tr o -> rexec real K rho g (RIf c a b) rho1 g1 tr o
| X_IfF : forall K rho g c a b rho1 g1 tr o, num_of rho c = Some 0 ->
    rexec real K rho g b rho1 g1 tr o -> rexec real K rho g (RIf c a b) rho1 g1 tr o
| X_IfErr : forall K rho g c a b, num_of rho c = None -> rexec real K rho g (RIf c a b) rho g [] RErr
| X_WhileF : forall K rho g c h b, num_of rho c = Some 0 -> rexec real K rho g (RWhile c h b) rho g [] RNorm
| X_WhileT : forall K rho g c h b n rho1 g1 tr1 rho2 g2 tr2 o, num_of rho c = Some (S n) ->
    rexec real K rho g b rho1 g1 tr1 RNorm -> rexec real K rho1 g1 (RWhile c h b) rho2 g2 tr2 o ->
    rexec real K rho g (RWhile c h b) rho2 g2 (tr1 ++ tr2) o
| X_WhileE : forall K rho g c h b n rho1 g1 tr1, num_of rho c = Some (S n) ->
    rexec real K rho g b rho1 g1 tr1 RErr -> rexec real K rho g (RWhile c h b) rho1 g1 tr1 RErr
| X_WhileErr : forall K rho g c h b, num_of rho c = None -> rexec real K rho g (RWhile c h b) rho g [] RErr
| X_TryN : forall K rho g b h rho1 g1 tr1,
    rexec real K rho g b rho1 g1 tr1 RNorm -> rexec real K rho g (RTry b h) rho1 g1 tr1 RNorm
| X_TryE : forall K rho g b h rho1 g1 tr1 rho2 g2 tr2 o,
    rexec real K rho g b rho1 g1 tr1 RErr -> rexec real K rho1 g1 h rho2 g2 tr2 o ->
    rexec real K rho g (RTry b h) rho2 g2 (tr1 ++ tr2) o
| X_Call : forall K rho g x f ys keep ps body ret vs rhof g1 tr v,
    call_args rho f ys = Some (ps, body, ret, vs) ->
    rexec real (frame_roots K rho keep) (rbind ps vs) g body rhof g1 tr RNorm -> rhof ret = Some v ->
    rexec real K rho g (RCall x f ys keep) (rupd rho x v) g1 tr RNorm
| X_CallNoRet : forall K rho g x f ys keep ps body ret vs rhof g1 tr,
    call_args rho f ys = Some (ps, body, ret, vs) ->
    rexec real (frame_roots K rho keep) (rbind ps vs) g body rhof g1 tr RNorm -> rhof ret = None ->
    rexec real K rho g (RCall x f ys keep) rho g1 tr RErr
| X_CallErr : forall K rho g x f ys keep ps body ret vs rhof g1 tr,
    call_args rho f ys = Some (ps, body, ret, vs) ->
    rexec real (frame_roots K rho keep) (rbind ps vs) g body rhof g1 tr RErr ->
    rexec real K rho g (RCall x f ys keep) rho g1 tr RErr
| X_CallBad : forall K rho g x f ys keep, call_args rho f ys = None ->
    rexec real K rho g (RCall x f ys keep) rho g [] RErr
| X_ReclaimRef : forall K rho g sid C fuel R, real = false ->
    rexec real K rho g (RReclaim sid C fuel R) rho g [] RNorm
| X_Reclaim : forall K rho g sid C fuel R, real = true ->
    rexec real K rho g (RReclaim sid C fuel R) rho (reclaim_step g sid C fuel (reclaim_roots K rho R)) [] RNorm.

(* A context without the store's cleanup right reclaims nothing. *)
Theorem reclaim_without_right_is_identity : forall g sid C fuel roots,
  holds sid = false -> reclaim_step g sid C fuel roots = g.
Proof. intros g sid C fuel roots H. unfold reclaim_step. rewrite H. reflexivity. Qed.

Theorem delete_without_right_is_refused : forall rho g y l,
  link_of rho y = Some l -> holds (lsid l) = false -> delete_result rho g y = None.
Proof. intros rho g y l Hl Hh. unfold delete_result. rewrite Hl, Hh. reflexivity. Qed.

End Lang.

(* ------------------------------------------------------------------ *)
(* The simulation invariant                                             *)
(* ------------------------------------------------------------------ *)

(* The live roots of a point: every link held by a live variable, plus the
   roots of the suspended frames. *)
Definition live_roots (rho : REnv) (L : list Var) (K : list Link) : list Link :=
  vars_links links_of rho L ++ K.

(* A reclaiming store differs from the reference store only in deleted
   nodes that the reference roots do not reach. *)
Definition SlotRel (R : list Link) (s sr : Store) : Prop :=
  ssid sr = ssid s /\ stab sr = stab s /\
  forall j, nth_error (sslots sr) j = nth_error (sslots s) j \/
    (exists gen nd, nth_error (sslots s) j = Some (mkSlot gen (Some nd)) /\
                   nth_error (sslots sr) j = Some (mkSlot (S gen) None) /\ ~ reach s R j).

Definition GSRel (R : list Link) (g r : GS) : Prop :=
  gsid r = gsid g /\ gbor r = gbor g /\ gissued r = gissued g /\
  map ssid (gstores r) = map ssid (gstores g) /\
  (forall sid s, find_store sid (gstores g) = Some s ->
     exists sr, find_store sid (gstores r) = Some sr /\ SlotRel R s sr).

(* Every edge names an identity some insert issued. *)
Definition EdgesFromIssued (g : GS) : Prop :=
  forall s j x e, In s (gstores g) -> nth_error (sslots s) j = Some x -> In e (slot_edges x) ->
    In (mkLink (ssid s) (fst e) (snd e)) (gissued g).

Record SimInv (gmax : nat) (K : list Link) (rho : REnv) (L : list Var) (g r : GS) : Prop := {
  si_ref : GInv gmax g;
  si_real : GInv gmax r;
  si_rel : GSRel (live_roots rho L K) g r;
  si_bor : gbor g = [];
  si_iss : forall l, In l (live_roots rho L K) -> In l (gissued g);
  si_edges : EdgesFromIssued g }.

(* ------------------------------------------------------------------ *)
(* Roots of environments                                                *)
(* ------------------------------------------------------------------ *)

Lemma vars_links_in : forall lk rho xs l, In l (vars_links lk rho xs) <->
  exists x v, In x xs /\ rho x = Some v /\ In l (lk v).
Proof.
  intros lk rho xs l. unfold vars_links. rewrite in_flat_map. split.
  - intros [x [Hx Hl]]. unfold val_links in Hl. destruct (rho x) as [v|] eqn:E; [|contradiction].
    exists x, v. auto.
  - intros [x [v [Hx [E Hl]]]]. exists x. split; [exact Hx|]. unfold val_links. rewrite E. exact Hl.
Qed.

Lemma vars_links_incl : forall lk rho xs ys, incl xs ys -> incl (vars_links lk rho xs) (vars_links lk rho ys).
Proof.
  intros lk rho xs ys H l Hl. apply vars_links_in in Hl. destruct Hl as [x [v [Hx [E Hv]]]].
  apply vars_links_in. exists x, v. auto.
Qed.

Lemma link_of_root : forall rho y l xs K, In y xs -> link_of rho y = Some l -> In l (live_roots rho xs K).
Proof.
  intros rho y l xs K Hy Hl. unfold link_of in Hl. destruct (rho y) as [v|] eqn:E; [|discriminate].
  apply in_or_app. left. apply vars_links_in. exists y, v. split; [exact Hy|]. split; [exact E| exact (as_link_in v l Hl)].
Qed.

Lemma rupd_same : forall rho x v, rupd rho x v x = Some v.
Proof. intros. unfold rupd. rewrite Nat.eqb_refl. reflexivity. Qed.

Lemma rupd_other : forall rho x v y, y <> x -> rupd rho x v y = rho y.
Proof. intros rho x v y H. unfold rupd. apply Nat.eqb_neq in H. rewrite H. reflexivity. Qed.

Lemma roots_after_def : forall rho x v L Lin K l, incl (vremove x L) Lin ->
  In l (live_roots (rupd rho x v) L K) -> In l (links_of v) \/ In l (live_roots rho Lin K).
Proof.
  intros rho x v L Lin K l Hi Hl. unfold live_roots in *. apply in_app_or in Hl. destruct Hl as [Hl|Hl].
  - apply vars_links_in in Hl. destruct Hl as [z [w [Hz [E Hw]]]].
    destruct (Nat.eq_dec z x) as [->|Hne].
    + rewrite rupd_same in E. injection E as E. subst w. left. exact Hw.
    + rewrite (rupd_other _ _ _ _ Hne) in E. right. apply in_or_app. left. apply vars_links_in.
      exists z, w. split; [apply Hi; apply vremove_In; split; assumption|]. split; assumption.
  - right. apply in_or_app. right. exact Hl.
Qed.

Lemma roots_weaken : forall rho L L' K, incl L' L -> forall l, In l (live_roots rho L' K) -> In l (live_roots rho L K).
Proof.
  intros rho L L' K H l Hl. unfold live_roots in *. apply in_app_or in Hl. apply in_or_app.
  destruct Hl as [Hl|Hl]; [left; exact (vars_links_incl _ _ _ _ H l Hl)| right; exact Hl].
Qed.

Lemma vals_of_in : forall rho ys vs v, vals_of rho ys = Some vs -> In v vs -> exists y, In y ys /\ rho y = Some v.
Proof.
  intros rho ys. induction ys as [|y r IH]; intros vs v H Hv; simpl in H.
  - injection H as H. subst. contradiction.
  - destruct (rho y) as [w|] eqn:Ey; [|discriminate]. destruct (vals_of rho r) as [ws|] eqn:Er; [|discriminate].
    injection H as H. subst vs. destruct Hv as [->|Hv].
    + exists y. split; [left; reflexivity| exact Ey].
    + destruct (IH ws v eq_refl Hv) as [z [Hz Ez]]. exists z. split; [right; exact Hz| exact Ez].
Qed.

Lemma vals_links : forall rho ys vs, vals_of rho ys = Some vs ->
  forall l, In l (flat_map links_of vs) -> In l (vars_links links_of rho ys).
Proof.
  intros rho ys vs H l Hl. apply in_flat_map in Hl. destruct Hl as [v [Hv Hl]].
  destruct (vals_of_in _ _ _ _ H Hv) as [y [Hy Ey]]. apply vars_links_in. exists y, v. auto.
Qed.

Lemma rbind_in : forall ps vs z v, rbind ps vs z = Some v -> In v vs.
Proof.
  induction ps as [|p ps IH]; intros vs z v H; simpl in H; [discriminate|].
  destruct vs as [|w ws]; [discriminate|]. unfold rupd in H.
  destruct (Nat.eqb z p); [injection H as H; subst; left; reflexivity| right; exact (IH ws z v H)].
Qed.

Lemma edges_of_spec : forall rho sid ys es e, edges_of rho sid ys = Some es -> In e es ->
  exists y l, In y ys /\ link_of rho y = Some l /\ lsid l = sid /\ e = (lidx l, lgen l).
Proof.
  intros rho sid ys. induction ys as [|y r IH]; intros es e H He; simpl in H.
  - injection H as H. subst. contradiction.
  - destruct (link_of rho y) as [l|] eqn:El; [|discriminate].
    destruct (edges_of rho sid r) as [es'|] eqn:Er; [|discriminate].
    destruct (Nat.eqb (lsid l) sid) eqn:Es; [|discriminate]. injection H as H. subst es.
    destruct He as [<-|He].
    + exists y, l. split; [left; reflexivity|]. split; [exact El|]. split; [apply Nat.eqb_eq; exact Es| reflexivity].
    + destruct (IH es' e eq_refl He) as [z [l' [Hz [El' [Hs Ee]]]]].
      exists z, l'. split; [right; exact Hz| auto].
Qed.

Lemma link_eta : forall l, mkLink (lsid l) (lidx l) (lgen l) = l.
Proof. intros [a b c]. reflexivity. Qed.

Lemma proj_links : forall rho y i w, proj_of rho y i = Some w ->
  forall l, In l (links_of w) -> In l (vars_links links_of rho [y]).
Proof.
  intros rho y i w H l Hl. unfold proj_of in H. destruct (rho y) as [[n|l0|l0|t vs]|] eqn:E; try discriminate.
  apply vars_links_in. exists y, (GAgg t vs). split; [left; reflexivity|]. split; [exact E|].
  rewrite links_of_agg. apply in_flat_map. exists w. split; [exact (nth_error_In _ _ H)| exact Hl].
Qed.

(* ------------------------------------------------------------------ *)
(* Store lists                                                          *)
(* ------------------------------------------------------------------ *)

Lemma find_none_iff : forall sid ss, find_store sid ss = None <-> ~ In sid (map ssid ss).
Proof.
  intros sid ss. induction ss as [|a r IH]; simpl; [tauto|].
  destruct (Nat.eqb (ssid a) sid) eqn:E.
  - apply Nat.eqb_eq in E. split; [discriminate| intros H; exfalso; apply H; left; exact E].
  - apply Nat.eqb_neq in E. rewrite IH. split.
    + intros H [H'|H']; [exact (E H')| exact (H H')].
    + intros H H'. apply H. right. exact H'.
Qed.

Lemma find_store_in : forall ss s, NoDup (map ssid ss) -> In s ss -> find_store (ssid s) ss = Some s.
Proof.
  induction ss as [|a r IH]; intros s Hn Hs; [contradiction|]. simpl in Hn |- *.
  inversion Hn as [|? ? Ha Hr]; subst.
  destruct Hs as [->|Hs]; [rewrite Nat.eqb_refl; reflexivity|].
  destruct (Nat.eqb (ssid a) (ssid s)) eqn:E.
  - apply Nat.eqb_eq in E. exfalso. apply Ha. rewrite E. apply in_map. exact Hs.
  - exact (IH s Hr Hs).
Qed.

Lemma find_store_unique : forall gmax g s s0, GInv gmax g -> In s (gstores g) ->
  find_store (ssid s) (gstores g) = Some s0 -> s0 = s.
Proof.
  intros gmax g s s0 Hg Hs E. rewrite (find_store_in _ _ (gi_sids _ _ Hg) Hs) in E. injection E as E. symmetry. exact E.
Qed.

(* ------------------------------------------------------------------ *)
(* The relation                                                         *)
(* ------------------------------------------------------------------ *)

Lemma slotrel_none : forall R s sr j, SlotRel R s sr ->
  (nth_error (sslots sr) j = None <-> nth_error (sslots s) j = None).
Proof.
  intros R s sr j [_ [_ H]]. destruct (H j) as [E|[g0 [n0 [E0 [E1 _]]]]].
  - rewrite E. tauto.
  - rewrite E0, E1. split; discriminate.
Qed.

Lemma slotrel_length : forall R s sr, SlotRel R s sr -> length (sslots sr) = length (sslots s).
Proof.
  intros R s sr H.
  assert (A : forall j, length (sslots sr) <= j <-> length (sslots s) <= j).
  { intros j. rewrite <- !nth_error_None. exact (slotrel_none R s sr j H). }
  pose proof (proj1 (A (length (sslots sr))) (le_n _)). pose proof (proj2 (A (length (sslots s))) (le_n _)). lia.
Qed.

Lemma slotrel_follow : forall R s sr l, SlotRel R s sr -> In l R -> lsid l = ssid s ->
  follow sr (lidx l, lgen l) = follow s (lidx l, lgen l).
Proof.
  intros R s sr l [_ [_ H]] Hl Hs. destruct (H (lidx l)) as [E|[g0 [n0 [E0 [E1 Hn]]]]].
  - unfold follow. simpl. rewrite E. reflexivity.
  - rewrite (follow_vacant sr (lidx l, lgen l) (S g0) E1).
    destruct (follow s (lidx l, lgen l)) as [nd|] eqn:Ef; [|reflexivity].
    exfalso. apply Hn. exact (RRoot s R l nd Hl Hs Ef).
Qed.

(* What the reference roots reach is unchanged in the reclaiming store. *)
Lemma slotrel_forward : forall R s sr, SlotRel R s sr ->
  forall j, reach s R j -> reach sr R j /\ nth_error (sslots sr) j = nth_error (sslots s) j.
Proof.
  intros R s sr Hr j Hj.
  assert (Slot : forall j, reach s R j -> nth_error (sslots sr) j = nth_error (sslots s) j).
  { intros j0 H0. pose proof Hr as [_ [_ H]]. destruct (H j0) as [E|[g0 [n0 [_ [_ Hn]]]]]; [exact E| contradiction]. }
  split; [|exact (Slot j Hj)].
  induction Hj as [l nd Hl Hs Hf| j gen nd e nd' Hj IH Hn He Hf].
  - apply (RRoot sr R l nd Hl); [destruct Hr as [Es _]; rewrite Es; exact Hs|].
    rewrite (slotrel_follow R s sr l Hr Hl Hs). exact Hf.
  - apply (REdge sr R j gen nd e nd' IH); [rewrite (Slot j Hj); exact Hn| exact He|].
    assert (Hk : reach s R (fst e)) by exact (REdge s R j gen nd e nd' Hj Hn He Hf).
    unfold follow in *. rewrite (Slot (fst e) Hk). exact Hf.
Qed.

Lemma slotrel_reroot : forall R R' s sr, SlotRel R s sr ->
  (forall j, reach s R' j -> reach s R j) -> SlotRel R' s sr.
Proof.
  intros R R' s sr [Es [Et H]] Hm. split; [exact Es|]. split; [exact Et|]. intros j.
  destruct (H j) as [E|[g0 [n0 [E0 [E1 Hn]]]]]; [left; exact E|].
  right. exists g0, n0. split; [exact E0|]. split; [exact E1|]. intro Hj. exact (Hn (Hm j Hj)).
Qed.

Lemma gsrel_reroot : forall R R' g r, GSRel R g r ->
  (forall s, In s (gstores g) -> forall j, reach s R' j -> reach s R j) -> GSRel R' g r.
Proof.
  intros R R' g r [H1 [H2 [H3 [H4 H5]]]] Hm. split; [exact H1|]. split; [exact H2|]. split; [exact H3|].
  split; [exact H4|]. intros sid s Ef. destruct (H5 sid s Ef) as [sr [Er Hs]].
  exists sr. split; [exact Er|]. apply (slotrel_reroot R R' s sr Hs). apply Hm.
  exact (proj1 (find_store_some _ _ _ Ef)).
Qed.

Lemma gsrel_refl : forall R g, GSRel R g g.
Proof.
  intros R g. split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|].
  intros sid s Ef. exists s. split; [exact Ef|]. split; [reflexivity|]. split; [reflexivity|].
  intros j. left. reflexivity.
Qed.

Lemma gsrel_find : forall R g r sid, GSRel R g r -> find_store sid (gstores g) = None ->
  find_store sid (gstores r) = None.
Proof.
  intros R g r sid [_ [_ [_ [Hm _]]]] E. apply find_none_iff. rewrite Hm. apply find_none_iff. exact E.
Qed.

Lemma resolve_rel : forall R g r l, GSRel R g r -> In l R -> resolve r l = resolve g l.
Proof.
  intros R g r l HR Hl. unfold resolve.
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef.
  - destruct HR as [_ [_ [_ [_ Hf]]]]. destruct (Hf _ _ Ef) as [sr [Er Hs]]. rewrite Er.
    destruct (find_store_some _ _ _ Ef) as [_ Hsid].
    rewrite (slotrel_follow R s sr l Hs Hl (eq_sym Hsid)). reflexivity.
  - rewrite (gsrel_find R g r _ HR Ef). reflexivity.
Qed.

Lemma node_of_rel : forall R g r rho y, GSRel R g r ->
  (forall l, link_of rho y = Some l -> In l R) -> node_of rho r y = node_of rho g y.
Proof.
  intros R g r rho y HR Hl. unfold node_of. destruct (link_of rho y) as [l|] eqn:E; [|reflexivity].
  rewrite (resolve_rel R g r l HR (Hl l eq_refl)). reflexivity.
Qed.

Lemma slots_fp_rel : forall R s sr, SlotRel R s sr -> incl (slots_fp (sslots sr)) (slots_fp (sslots s)).
Proof.
  intros R s sr [_ [_ H]] b Hb. unfold slots_fp in *. apply in_flat_map in Hb. destruct Hb as [x [Hx Hb]].
  destruct (In_nth_error _ _ Hx) as [j Ej]. apply in_flat_map.
  destruct (H j) as [E|[g0 [n0 [E0 [E1 _]]]]].
  - rewrite E in Ej. exists x. split; [exact (nth_error_In _ _ Ej)| exact Hb].
  - rewrite E1 in Ej. injection Ej as Ej. subst x. simpl in Hb. contradiction.
Qed.

(* The reclaiming heap holds no block the reference heap lacks. *)
Lemma heap_incl : forall gmax R g r, GInv gmax g -> GInv gmax r -> GSRel R g r -> incl (gheap r) (gheap g).
Proof.
  intros gmax R g r Hg Hr [_ [_ [_ [Hm Hf]]]] b Hb.
  apply (Permutation_in _ (Permutation_sym (gi_perm _ _ Hg))).
  pose proof (Permutation_in _ (gi_perm _ _ Hr) Hb) as Hb'. unfold gfp in *.
  apply in_flat_map in Hb'. destruct Hb' as [sr [Hsr Hb']].
  assert (Er : find_store (ssid sr) (gstores r) = Some sr) by exact (find_store_in _ _ (gi_sids _ _ Hr) Hsr).
  destruct (find_store (ssid sr) (gstores g)) as [s|] eqn:Ef.
  - destruct (Hf _ _ Ef) as [sr' [Er' Hs]]. rewrite Er in Er'. injection Er' as Er'. subst sr'.
    apply in_flat_map. exists s. split; [exact (proj1 (find_store_some _ _ _ Ef))|].
    pose proof Hs as [_ [Et _]]. unfold store_fp in *. simpl in Hb' |- *. destruct Hb' as [Hb'|Hb'].
    + left. rewrite <- Et. exact Hb'.
    + right. exact (slots_fp_rel R s sr Hs b Hb').
  - exfalso. apply find_none_iff in Ef. apply Ef. rewrite <- Hm. apply in_map. exact Hsr.
Qed.

Lemma bfree_incl : forall bs H H', incl H' H -> bfree bs H = true -> bfree bs H' = true.
Proof.
  intros bs H H' Hi Hf. unfold bfree in *. rewrite forallb_forall in *. intros b Hb.
  pose proof (Hf b Hb) as E. apply negb_true_iff in E. apply negb_true_iff.
  destruct (bmem b H') eqn:E'; [|reflexivity]. apply bmem_true in E'. apply Hi in E'.
  apply bmem_true in E'. congruence.
Qed.

Lemma free_incl : forall xs H H', incl H' H -> incl (free xs H') (free xs H).
Proof.
  intros xs H H' Hi b Hb. unfold free in *. apply filter_In in Hb. apply filter_In.
  destruct Hb as [Hb E]. split; [exact (Hi b Hb)| exact E].
Qed.

(* Updating one store on both sides keeps the relation. *)
Lemma stores_rel_set : forall R R' ss rs s s' sr sr',
  map ssid rs = map ssid ss ->
  (forall sid s0, find_store sid ss = Some s0 -> exists sr0, find_store sid rs = Some sr0 /\ SlotRel R s0 sr0) ->
  find_store (ssid s') ss = Some s -> find_store (ssid s') rs = Some sr -> ssid sr' = ssid s' ->
  SlotRel R' s' sr' ->
  (forall s0, In s0 ss -> ssid s0 <> ssid s' -> forall j, reach s0 R' j -> reach s0 R j) ->
  map ssid (set_store sr' rs) = map ssid (set_store s' ss) /\
  (forall sid s0, find_store sid (set_store s' ss) = Some s0 ->
     exists sr0, find_store sid (set_store sr' rs) = Some sr0 /\ SlotRel R' s0 sr0).
Proof.
  intros R R' ss rs s s' sr sr' Hm Hf Es Er Hid Hrel Ho. split.
  - rewrite !map_ssid_set. exact Hm.
  - intros sid s0 E0. rewrite (find_after_set _ _ s sid Es) in E0.
    assert (Er' : find_store (ssid sr') rs = Some sr) by (rewrite Hid; exact Er).
    rewrite (find_after_set _ _ sr sid Er'). rewrite Hid.
    destruct (Nat.eqb sid (ssid s')) eqn:E.
    + injection E0 as E0. subst s0. exists sr'. split; [reflexivity| exact Hrel].
    + destruct (Hf sid s0 E0) as [sr0 [Er0 Hs0]]. exists sr0. split; [exact Er0|].
      apply (slotrel_reroot R R' s0 sr0 Hs0). apply Ho; [exact (proj1 (find_store_some _ _ _ E0))|].
      apply Nat.eqb_neq in E. destruct (find_store_some _ _ _ E0) as [_ Hs]. congruence.
Qed.

(* ------------------------------------------------------------------ *)
(* Reachability after one store operation                               *)
(* ------------------------------------------------------------------ *)

Lemma reach_other_store : forall s R R', (forall l, In l R' -> lsid l = ssid s -> In l R) ->
  forall j, reach s R' j -> reach s R j.
Proof.
  intros s R R' H j Hr. induction Hr as [l nd Hl Hs Hf| j gen nd e nd' Hr IH Hj He Hf].
  - exact (RRoot s R l nd (H l Hl Hs) Hs Hf).
  - exact (REdge s R j gen nd e nd' IH Hj He Hf).
Qed.

(* Writing new edges that only name reachable nodes reaches nothing new. *)
Lemma reach_after_write : forall s w gen nd nd' R R',
  nth_error (sslots s) w = Some (mkSlot gen (Some nd)) ->
  (forall l, In l R' -> lsid l = ssid s -> In l R) ->
  (forall e n, In e (nedges nd') -> follow s e = Some n -> reach s R (fst e)) ->
  forall j, reach (mkStore (ssid s) (stab s) (upd (sslots s) w (mkSlot gen (Some nd')))) R' j -> reach s R j.
Proof.
  intros s w gen nd nd' R R' Hw HR He j Hr.
  assert (Hlen : w < length (sslots s)) by (apply nth_error_Some; congruence).
  assert (Fw : forall e n, follow (mkStore (ssid s) (stab s) (upd (sslots s) w (mkSlot gen (Some nd')))) e = Some n ->
                 exists n0, follow s e = Some n0).
  { intros e n Hf. unfold follow in *. simpl in Hf. destruct (Nat.eq_dec (fst e) w) as [E|Hne].
    - rewrite E, nth_upd_same in Hf by exact Hlen. rewrite E, Hw. simpl in Hf |- *.
      destruct (Nat.eqb gen (snd e)); [eexists; reflexivity| discriminate].
    - rewrite (nth_upd_other _ _ _ _ _ Hne) in Hf. exists n. exact Hf. }
  induction Hr as [l n Hl Hs Hf| j gen0 nd0 e n Hr IH Hj He' Hf].
  - destruct (Fw _ _ Hf) as [n0 Hf0]. exact (RRoot s R l n0 (HR l Hl Hs) Hs Hf0).
  - destruct (Fw _ _ Hf) as [n0 Hf0]. simpl in Hj. destruct (Nat.eq_dec j w) as [->|Hne].
    + rewrite nth_upd_same in Hj by exact Hlen. injection Hj as Eg En. subst gen0 nd0.
      exact (He e n0 He' Hf0).
    + rewrite (nth_upd_other _ _ _ _ _ Hne) in Hj. exact (REdge s R j gen0 nd0 e n0 IH Hj He' Hf0).
Qed.

Lemma reach_after_delete : forall s w gen nd R R',
  nth_error (sslots s) w = Some (mkSlot gen (Some nd)) ->
  (forall l, In l R' -> lsid l = ssid s -> In l R) ->
  forall j, reach (mkStore (ssid s) (stab s) (upd (sslots s) w (mkSlot (S gen) None))) R' j -> reach s R j.
Proof.
  intros s w gen nd R R' Hw HR j Hr. apply (reach_other_store s R R' HR).
  apply (reclaimed_backward [w] s (mkStore (ssid s) (stab s) (upd (sslots s) w (mkSlot (S gen) None))) R'); [|exact Hr].
  split; [reflexivity|]. split; [reflexivity|]. intros k. simpl. destruct (Nat.eq_dec k w) as [->|Hne].
  - right. split; [left; reflexivity|]. exists gen, nd. split; [exact Hw|].
    apply nth_upd_same. apply nth_error_Some. congruence.
  - left. apply nth_upd_other. exact Hne.
Qed.

(* A new node is reached only through its own link. *)
Lemma reach_after_insert : forall s s' idx g0 nd R R',
  ssid s' = ssid s ->
  nth_error (sslots s') idx = Some (mkSlot g0 (Some nd)) ->
  (forall j, j <> idx -> nth_error (sslots s') j = nth_error (sslots s) j) ->
  (forall l, In l R' -> lsid l = ssid s -> l = mkLink (ssid s) idx g0 \/ In l R) ->
  (forall l, In l R -> lsid l = ssid s -> lidx l = idx -> lgen l <> g0) ->
  (forall j x e, nth_error (sslots s) j = Some x -> In e (slot_edges x) -> fst e = idx -> snd e <> g0) ->
  (forall e, In e (nedges nd) -> (fst e = idx -> snd e <> g0) /\ (forall n, follow s e = Some n -> reach s R (fst e))) ->
  forall j, reach s' R' j -> reach s R j \/ j = idx.
Proof.
  intros s s' idx g0 nd R R' Hid Hi Ho HR' HR Hold Hnew j Hr.
  assert (Fo : forall e n, follow s' e = Some n -> fst e <> idx -> follow s e = Some n).
  { intros e n Hf Hne. unfold follow in *. rewrite (Ho _ Hne) in Hf. exact Hf. }
  assert (Fi : forall e n, follow s' e = Some n -> fst e = idx -> snd e = g0).
  { intros e n Hf E. destruct (follow_gen s' e n Hf) as [t [Ht [Hg _]]]. rewrite E, Hi in Ht.
    injection Ht as Ht. subst t. simpl in Hg. symmetry. exact Hg. }
  induction Hr as [l n Hl Hs Hf| j gen0 nd0 e n Hr IH Hj He Hf].
  - rewrite Hid in Hs. destruct (HR' l Hl Hs) as [El|HlR].
    + right. rewrite El. reflexivity.
    + left. destruct (Nat.eq_dec (lidx l) idx) as [E|Hne].
      * exfalso. exact (HR l HlR Hs E (Fi _ _ Hf E)).
      * exact (RRoot s R l n HlR Hs (Fo _ _ Hf Hne)).
  - left. destruct (Nat.eq_dec j idx) as [->|Hne].
    + rewrite Hi in Hj. injection Hj as Eg En. subst gen0 nd0.
      destruct (Hnew e He) as [Hself Hreach].
      destruct (Nat.eq_dec (fst e) idx) as [E|Hne2]; [exfalso; exact (Hself E (Fi _ _ Hf E))|].
      exact (Hreach n (Fo _ _ Hf Hne2)).
    + rewrite (Ho _ Hne) in Hj. destruct IH as [IH|IH]; [|contradiction].
      destruct (Nat.eq_dec (fst e) idx) as [E|Hne2].
      * exfalso. exact (Hold j (mkSlot gen0 (Some nd0)) e Hj He E (Fi _ _ Hf E)).
      * exact (REdge s R j gen0 nd0 e n IH Hj He (Fo _ _ Hf Hne2)).
Qed.

(* An issued link names an existing identity: never a vacant current
   generation, never a slot that does not exist yet. *)
Lemma issued_current : forall gmax g l s, GInv gmax g -> In l (gissued g) ->
  find_store (lsid l) (gstores g) = Some s ->
  exists t, nth_error (sslots s) (lidx l) = Some t /\ (lgen l < sgen t \/ (lgen l = sgen t /\ snode t <> None)).
Proof.
  intros gmax g l s Hg Hl Ef. destruct (gi_iss _ _ Hg l Hl) as [_ Hat]. exact (Hat s Ef).
Qed.

(* ------------------------------------------------------------------ *)
(* Machine operations used by the language                              *)
(* ------------------------------------------------------------------ *)

Lemma node_of_link : forall rho g y l nd, node_of rho g y = Some (l, nd) ->
  link_of rho y = Some l /\ resolve g l = Found nd.
Proof.
  intros rho g y l nd H. unfold node_of in H. destruct (link_of rho y) as [l0|]; [|discriminate].
  destruct (resolve g l0) as [nd0|r0] eqn:E; [|discriminate]. injection H as <- <-. split; [reflexivity| exact E].
Qed.

Lemma resolve_found : forall g l nd, resolve g l = Found nd ->
  exists s, find_store (lsid l) (gstores g) = Some s /\ nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)).
Proof.
  intros g l nd H. unfold resolve in H. destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; [|discriminate].
  destruct (follow s (lidx l, lgen l)) as [n|] eqn:Efo; [|discriminate]. injection H as <-.
  exists s. split; [reflexivity|]. destruct (follow_gen s _ n Efo) as [t [Et [Eg En]]]. simpl in Et, Eg.
  rewrite Et. destruct t as [tg tn]. simpl in Eg, En. subst. reflexivity.
Qed.

Lemma follow_slot : forall s l nd, nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)) ->
  follow s (lidx l, lgen l) = Some nd.
Proof. intros s l nd H. unfold follow. simpl. rewrite H. simpl. rewrite Nat.eqb_refl. reflexivity. Qed.

Lemma write_link_exec : forall gmax g l d es s nd, gbor g = [] -> find_store (lsid l) (gstores g) = Some s ->
  nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)) ->
  write_link gmax g l d es = Some (mkGS (set_store (mkStore (ssid s) (stab s)
      (upd (sslots s) (lidx l) (mkSlot (lgen l) (Some (mkNode d es (nblocks nd)))))) (gstores g))
    (gheap g) (gsid g) [] (gissued g)).
Proof.
  intros gmax g l d es s nd Hb Ef Es. unfold write_link, unit_result. cbv zeta. cbn [gexec]. unfold g_begin.
  rewrite Ef, (follow_slot s l nd Es), Hb. cbn [conflicts existsb snd fst gbor gstores gheap gsid gissued].
  cbn [gexec]. unfold g_write. cbn [nth_error bwr negb bsid bidx gbor gstores].
  rewrite Ef, Es. cbn [snd fst]. cbn [gexec]. unfold g_end. reflexivity.
Qed.

Lemma write_link_shape : forall gmax g l d es g', gbor g = [] -> write_link gmax g l d es = Some g' ->
  exists s nd, find_store (lsid l) (gstores g) = Some s /\
    nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)) /\
    g' = mkGS (set_store (mkStore (ssid s) (stab s)
                (upd (sslots s) (lidx l) (mkSlot (lgen l) (Some (mkNode d es (nblocks nd)))))) (gstores g))
              (gheap g) (gsid g) [] (gissued g).
Proof.
  intros gmax g l d es g' Hb H.
  assert (Hr : exists nd, resolve g l = Found nd).
  { unfold write_link, unit_result in H. cbv zeta in H. cbn [gexec] in H. unfold g_begin in H.
    unfold resolve. destruct (find_store (lsid l) (gstores g)) as [s|]; [|discriminate].
    destruct (follow s (lidx l, lgen l)) as [n|]; [exists n; reflexivity| discriminate]. }
  destruct Hr as [nd Hr]. destruct (resolve_found g l nd Hr) as [s [Ef Es]].
  rewrite (write_link_exec gmax g l d es s nd Hb Ef Es) in H. injection H as <-.
  exists s, nd. split; [exact Ef|]. split; [exact Es| reflexivity].
Qed.

Lemma write_link_ginv : forall gmax g l d es g', GInv gmax g -> write_link gmax g l d es = Some g' -> GInv gmax g'.
Proof.
  intros gmax g l d es g' Hg H. unfold write_link, unit_result in H. cbv zeta in H.
  destruct (snd (gexec gmax g (OBegin l true))); try discriminate.
  destruct (snd (gexec gmax (fst (gexec gmax g (OBegin l true))) (OWrite 0 d es))); try discriminate.
  destruct (snd (gexec gmax (fst (gexec gmax (fst (gexec gmax g (OBegin l true))) (OWrite 0 d es))) (OEnd 0)));
    try discriminate.
  injection H as <-.
  exact (ginv_step gmax _ (OEnd 0) (ginv_step gmax _ (OWrite 0 d es) (ginv_step gmax g (OBegin l true) Hg))).
Qed.

Lemma insert_shape : forall gmax g sid idx d es bs tb g' l,
  gexec gmax g (OInsert sid idx d es bs tb) = (g', GLink l) ->
  exists s, find_store sid (gstores g) = Some s /\
  ((exists gen, nth_error (sslots s) idx = Some (mkSlot gen None) /\ Nat.ltb gen gmax = true /\
      bnodup bs && bfree bs (gheap g) = true /\ l = mkLink sid idx gen /\
      g' = mkGS (set_store (mkStore sid (stab s) (upd (sslots s) idx (mkSlot gen (Some (mkNode d es bs))))) (gstores g))
                (bs ++ gheap g) (gsid g) (gbor g) (l :: gissued g)) \/
   (nth_error (sslots s) idx = None /\ store_borrowed sid (gbor g) = false /\
      Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax = true /\
      bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g)) = true /\ l = mkLink sid idx 0 /\
      g' = mkGS (set_store (mkStore sid tb (sslots s ++ [mkSlot 0 (Some (mkNode d es bs))])) (gstores g))
                (bs ++ tb :: free [stab s] (gheap g)) (gsid g) (gbor g) (l :: gissued g))).
Proof.
  intros gmax g sid idx d es bs tb g' l H. cbn [gexec] in H. unfold g_insert in H. cbv zeta in H.
  destruct (find_store sid (gstores g)) as [s|] eqn:Ef; [|discriminate]. exists s. split; [reflexivity|].
  destruct (nth_error (sslots s) idx) as [sl|] eqn:Ei.
  - destruct sl as [gen [nd0|]]; cbn [snode sgen] in H; [discriminate|].
    destruct (Nat.ltb gen gmax) eqn:Eg; [|discriminate].
    destruct (bnodup bs && bfree bs (gheap g)) eqn:Eb; [|discriminate].
    injection H as Hg Hl. subst g' l. left. exists gen. split; [reflexivity|]. split; [assumption || reflexivity|].
    split; [assumption || reflexivity|]. split; reflexivity.
  - right. destruct (store_borrowed sid (gbor g)) eqn:Eb; [discriminate|].
    destruct (Nat.eqb idx (length (sslots s)) && Nat.ltb 0 gmax) eqn:Ea; [|discriminate].
    destruct (bnodup (tb :: bs) && bfree (tb :: bs) (free [stab s] (gheap g))) eqn:Ec; [|discriminate].
    injection H as Hg Hl. subst g' l. split; [reflexivity|]. split; [reflexivity|]. split; [assumption || reflexivity|].
    split; [assumption || reflexivity|]. split; reflexivity.
Qed.

Lemma insert_fill_exec : forall gmax r sid idx d es bs tb sr gen,
  find_store sid (gstores r) = Some sr -> nth_error (sslots sr) idx = Some (mkSlot gen None) ->
  Nat.ltb gen gmax = true -> bnodup bs && bfree bs (gheap r) = true ->
  gexec gmax r (OInsert sid idx d es bs tb) =
    (mkGS (set_store (mkStore sid (stab sr) (upd (sslots sr) idx (mkSlot gen (Some (mkNode d es bs))))) (gstores r))
          (bs ++ gheap r) (gsid r) (gbor r) (mkLink sid idx gen :: gissued r), GLink (mkLink sid idx gen)).
Proof.
  intros gmax r sid idx d es bs tb sr gen Ef Ei Eg Eb. cbn [gexec]. unfold g_insert. cbv zeta.
  rewrite Ef, Ei. cbn [snode sgen]. rewrite Eg, Eb. reflexivity.
Qed.

Lemma insert_append_exec : forall gmax r sid idx d es bs tb sr,
  find_store sid (gstores r) = Some sr -> nth_error (sslots sr) idx = None ->
  store_borrowed sid (gbor r) = false -> Nat.eqb idx (length (sslots sr)) && Nat.ltb 0 gmax = true ->
  bnodup (tb :: bs) && bfree (tb :: bs) (free [stab sr] (gheap r)) = true ->
  gexec gmax r (OInsert sid idx d es bs tb) =
    (mkGS (set_store (mkStore sid tb (sslots sr ++ [mkSlot 0 (Some (mkNode d es bs))])) (gstores r))
          (bs ++ tb :: free [stab sr] (gheap r)) (gsid r) (gbor r) (mkLink sid idx 0 :: gissued r),
     GLink (mkLink sid idx 0)).
Proof.
  intros gmax r sid idx d es bs tb sr Ef Ei Eb Ea Ec. cbn [gexec]. unfold g_insert. cbv zeta.
  rewrite Ef, Ei, Eb, Ea, Ec. reflexivity.
Qed.

Lemma delete_shape : forall gmax g l g', gexec gmax g (ODelete l) = (g', GUnit) ->
  exists s nd, find_store (lsid l) (gstores g) = Some s /\
    nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)) /\
    existsb (same_place (lsid l) (lidx l)) (gbor g) = false /\
    g' = mkGS (set_store (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S (lgen l)) None))) (gstores g))
              (free (nblocks nd) (gheap g)) (gsid g) (gbor g) (gissued g).
Proof.
  intros gmax g l g' H. cbn [gexec] in H. unfold g_delete in H.
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef; [|discriminate].
  destruct (nth_error (sslots s) (lidx l)) as [[gen [nd|]]|] eqn:Ei; try discriminate.
  destruct (Nat.eqb gen (lgen l)) eqn:Eg; [|discriminate]. apply Nat.eqb_eq in Eg. subst gen.
  destruct (existsb (same_place (lsid l) (lidx l)) (gbor g)) eqn:Eb; [discriminate|].
  injection H as <-. exists s, nd. repeat split; first [assumption | reflexivity].
Qed.

Lemma delete_exec : forall gmax r l sr nd, find_store (lsid l) (gstores r) = Some sr ->
  nth_error (sslots sr) (lidx l) = Some (mkSlot (lgen l) (Some nd)) ->
  existsb (same_place (lsid l) (lidx l)) (gbor r) = false ->
  gexec gmax r (ODelete l) =
    (mkGS (set_store (mkStore (ssid sr) (stab sr) (upd (sslots sr) (lidx l) (mkSlot (S (lgen l)) None))) (gstores r))
          (free (nblocks nd) (gheap r)) (gsid r) (gbor r) (gissued r), GUnit).
Proof.
  intros gmax r l sr nd Ef Ei Eb. cbn [gexec]. unfold g_delete. rewrite Ef, Ei, Nat.eqb_refl, Eb. reflexivity.
Qed.

(* A delete that succeeds in the reclaiming state succeeds in the reference. *)
Lemma delete_ok_back : forall gmax R g r l, GSRel R g r ->
  snd (gexec gmax r (ODelete l)) = GUnit -> snd (gexec gmax g (ODelete l)) = GUnit.
Proof.
  intros gmax R g r l HR H. destruct (gexec gmax r (ODelete l)) as [r' res] eqn:E. simpl in H. subst res.
  destruct (delete_shape gmax r l r' E) as [sr [nd [Er [Es [Eb _]]]]].
  pose proof HR as [F1 [F2 [F3 [F4 F5]]]].
  destruct (find_store (lsid l) (gstores g)) as [s|] eqn:Ef.
  - destruct (F5 _ _ Ef) as [sr0 [Er0 Hs]]. rewrite Er in Er0. injection Er0 as <-.
    pose proof Hs as [_ [_ Hj]]. destruct (Hj (lidx l)) as [E1|[g0 [n0 [_ [E2 _]]]]].
    + rewrite E1 in Es. rewrite F2 in Eb. rewrite (delete_exec gmax g l s nd Ef Es Eb). reflexivity.
    + rewrite E2 in Es. discriminate.
  - rewrite (gsrel_find R g r _ HR Ef) in Er. discriminate.
Qed.

Lemma grun_deletes_fields : forall gmax ops r, (forall o, In o ops -> exists l, o = ODelete l) ->
  gsid (grun gmax r ops) = gsid r /\ gbor (grun gmax r ops) = gbor r /\
  gissued (grun gmax r ops) = gissued r /\ map ssid (gstores (grun gmax r ops)) = map ssid (gstores r).
Proof.
  intros gmax ops. induction ops as [|o rest IH]; intros r Hops; [repeat split; reflexivity|].
  destruct (Hops o (or_introl eq_refl)) as [l ->]. cbn [grun].
  destruct (IH (fst (gexec gmax r (ODelete l)))) as [A [B [C D]]]; [intros o Ho; apply Hops; right; exact Ho|].
  rewrite A, B, C, D. cbn [gexec]. unfold g_delete.
  destruct (find_store (lsid l) (gstores r)) as [s|] eqn:Ef; [|repeat split; reflexivity].
  destruct (nth_error (sslots s) (lidx l)) as [[g0 [nd|]]|]; try (repeat split; reflexivity).
  destruct (Nat.eqb g0 (lgen l)); [|repeat split; reflexivity].
  destruct (existsb (same_place (lsid l) (lidx l)) (gbor r)); [repeat split; reflexivity|].
  cbn [fst gsid gbor gissued gstores]. split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|].
  apply map_ssid_set.
Qed.

(* Deleting nodes the reclaimer proved unreachable keeps the relation. *)
Lemma slotrel_after_reclaim : forall R R2 G s sr sr', SlotRel R s sr -> reclaimed G sr sr' ->
  (forall j, reach sr R2 j -> ~ In j G) -> (forall l, In l R -> In l R2) -> SlotRel R s sr'.
Proof.
  intros R R2 G s sr sr' Hs [Es [Et Rc]] U Hi. pose proof Hs as [Es0 [Et0 H]].
  split; [congruence|]. split; [congruence|]. intros j. destruct (Rc j) as [E|[Hin [gen [nd [E0 E1]]]]].
  - rewrite E. exact (H j).
  - right. exists gen, nd. destruct (H j) as [E2|[g1 [n1 [_ [E3 _]]]]].
    + split; [rewrite <- E2; exact E0|]. split; [exact E1|]. intro Hr.
      apply (U j); [|exact Hin]. apply (reach_mono sr R R2 Hi). exact (proj1 (slotrel_forward R s sr Hs j Hr)).
    + rewrite E3 in E0. discriminate.
Qed.

(* ------------------------------------------------------------------ *)
(* Invariant steps                                                      *)
(* ------------------------------------------------------------------ *)

Lemma siminv_change : forall gmax K rho L K' rho' L' g r, SimInv gmax K rho L g r ->
  (forall l, In l (live_roots rho' L' K') -> In l (live_roots rho L K)) -> SimInv gmax K' rho' L' g r.
Proof.
  intros gmax K rho L K' rho' L' g r H Hi. destruct H as [H1 H2 H3 H4 H5 H6]. constructor; try assumption.
  - apply (gsrel_reroot _ _ g r H3). intros s Hs j Hj. exact (reach_mono s _ _ Hi j Hj).
  - intros l Hl. apply H5. exact (Hi l Hl).
Qed.

Lemma siminv_weaken : forall gmax K rho L L' g r, SimInv gmax K rho L g r -> incl L' L -> SimInv gmax K rho L' g r.
Proof. intros gmax K rho L L' g r H Hi. apply (siminv_change gmax K rho L K rho L' g r H). apply roots_weaken. exact Hi. Qed.

Lemma siminv_def : forall gmax K rho L Lin g r x v, SimInv gmax K rho Lin g r -> incl (vremove x L) Lin ->
  (forall l, In l (links_of v) -> In l (live_roots rho Lin K)) -> SimInv gmax K (rupd rho x v) L g r.
Proof.
  intros gmax K rho L Lin g r x v H Hi Hv. apply (siminv_change gmax K rho Lin K (rupd rho x v) L g r H).
  intros l Hl. destruct (roots_after_def rho x v L Lin K l Hi Hl) as [A|A]; [exact (Hv l A)| exact A].
Qed.

Lemma edge_link_shape : forall rho g y k l', edge_link_of rho g y k = Some l' ->
  exists l nd e s, link_of rho y = Some l /\ find_store (lsid l) (gstores g) = Some s /\
    nth_error (sslots s) (lidx l) = Some (mkSlot (lgen l) (Some nd)) /\ nth_error (nedges nd) k = Some e /\
    l' = mkLink (lsid l) (fst e) (snd e).
Proof.
  intros rho g y k l' H. unfold edge_link_of in H.
  destruct (node_of rho g y) as [[l nd]|] eqn:En; [|discriminate].
  destruct (nth_error (nedges nd) k) as [e|] eqn:Ee; [|discriminate]. injection H as <-.
  destruct (node_of_link _ _ _ _ _ En) as [El Er]. destruct (resolve_found g l nd Er) as [s [Ef Es]].
  exists l, nd, e, s. repeat split; assumption.
Qed.

(* Following an edge of a reachable node roots nothing new. *)
Lemma siminv_edge : forall gmax K rho L Lin g r x y k l', SimInv gmax K rho Lin g r -> In y Lin ->
  incl (vremove x L) Lin -> edge_link_of rho g y k = Some l' -> SimInv gmax K (rupd rho x (GLnk l')) L g r.
Proof.
  intros gmax K rho L Lin g r x y k l' HS Hy Hi He.
  destruct (edge_link_shape rho g y k l' He) as [l [nd [e [s0 [El [Ef [Es0 [Ee ->]]]]]]]].
  assert (HlR : In l (live_roots rho Lin K)) by exact (link_of_root rho y l Lin K Hy El).
  destruct (find_store_some _ _ _ Ef) as [Hs0 Hsid].
  destruct HS as [H1 H2 H3 H4 H5 H6]. constructor; try assumption.
  - apply (gsrel_reroot _ _ g r H3). intros s Hs j Hj.
    apply (reach_cover s (live_roots rho Lin K) (live_roots (rupd rho x (GLnk (mkLink (lsid l) (fst e) (snd e)))) L K));
      [|exact Hj].
    intros l0 n Hl0 Hls Hf.
    destruct (roots_after_def rho x (GLnk (mkLink (lsid l) (fst e) (snd e))) L Lin K l0 Hi Hl0) as [A|A].
    + simpl in A. destruct A as [<-|[]]. simpl in Hls.
      assert (Es : s0 = s).
      { pose proof (find_store_in _ _ (gi_sids _ _ H1) Hs) as F. rewrite <- Hls in F. rewrite Ef in F.
        injection F as F. exact F. }
      subst s0.
      exact (REdge s _ (lidx l) (lgen l) nd e n
               (RRoot s _ l nd HlR (eq_sym Hsid) (follow_slot s l nd Es0)) Es0 (nth_error_In _ _ Ee) Hf).
    + exact (RRoot s _ l0 n A Hls Hf).
  - intros l0 Hl0.
    destruct (roots_after_def rho x (GLnk (mkLink (lsid l) (fst e) (snd e))) L Lin K l0 Hi Hl0) as [A|A].
    + simpl in A. destruct A as [<-|[]]. rewrite <- Hsid.
      exact (H6 s0 (lidx l) _ e Hs0 Es0 (nth_error_In _ _ Ee)).
    + exact (H5 l0 A).
Qed.

Lemma siminv_reclaim : forall gmax rights ctx K rho L R g r sid C fuel, SimInv gmax K rho L g r -> incl L R ->
  SimInv gmax K rho L g (reclaim_step gmax rights ctx r sid C fuel (vars_links links_of rho R ++ K)).
Proof.
  intros gmax rights ctx K rho L R g r sid C fuel HS Hi. unfold reclaim_step.
  destruct (holds rights ctx sid); [|exact HS].
  destruct (find_store sid (gstores r)) as [sr|] eqn:Er; [|exact HS].
  destruct (trial_garbage fuel sr (vars_links links_of rho R ++ K) C) as [G|] eqn:Et; [|exact HS].
  destruct HS as [H1 H2 H3 H4 H5 H6]. constructor; try assumption.
  - apply ginv_run. exact H2.
  - destruct (find_store_some _ _ _ Er) as [_ Hsid].
    destruct (grun_deletes_fields gmax (reclaim_ops sr G) r) as [A [B [C' D]]].
    { intros o Ho. unfold reclaim_ops in Ho. apply in_map_iff in Ho. destruct Ho as [i [<- _]]. eexists; reflexivity. }
    destruct H3 as [F1 [F2 [F3 [F4 F5]]]].
    split; [congruence|]. split; [congruence|]. split; [congruence|]. split; [congruence|].
    intros sid0 s0 E0. destruct (F5 sid0 s0 E0) as [sr0 [Er0 Hs0]].
    destruct (Nat.eq_dec sid0 sid) as [->|Hne].
    + rewrite Er in Er0. injection Er0 as <-.
      assert (Er2 : find_store (ssid sr) (gstores r) = Some sr) by (rewrite Hsid; exact Er).
      destruct (reclaim_run gmax G sr G r sr (fun i Hi => Hi) Er2 (reclaimed_refl G sr)) as [sr' [Er' Rc]].
      exists sr'. split; [rewrite <- Hsid; exact Er'|].
      apply (slotrel_after_reclaim _ (vars_links links_of rho R ++ K) G s0 sr sr' Hs0 Rc).
      * exact (trial_garbage_unreachable fuel sr _ C G Et).
      * intros l Hl. unfold live_roots in Hl. apply in_app_or in Hl. apply in_or_app.
        destruct Hl as [Hl|Hl]; [left; exact (vars_links_incl _ _ _ _ Hi l Hl)| right; exact Hl].
    + exists sr0. split; [|exact Hs0]. rewrite reclaim_run_other; [exact Er0|]. rewrite Hsid. exact Hne.
Qed.

(* ------------------------------------------------------------------ *)
(* Store-changing statements                                            *)
(* ------------------------------------------------------------------ *)

Lemma sim_set : forall gmax K rho y ys L E g r d g',
  SimInv gmax K rho (y :: ys ++ L ++ E) g r -> set_result gmax rho g y d ys = Some g' ->
  exists r', set_result gmax rho r y d ys = Some r' /\ SimInv gmax K rho L g' r'.
Proof.
  intros gmax K rho y ys L E g r d g' HS Hs.
  remember (y :: ys ++ L ++ E) as Lin eqn:HLin.
  pose proof HS as [H1 H2 H3 H4 H5 H6]. pose proof H3 as [F1 [F2 [F3 [F4 F5]]]].
  unfold set_result in Hs. destruct (node_of rho g y) as [[l nd0]|] eqn:En; [|discriminate].
  destruct (edges_of rho (lsid l) ys) as [es|] eqn:Ees; [|discriminate].
  destruct (node_of_link _ _ _ _ _ En) as [El Er].
  assert (HlR : In l (live_roots rho Lin K)) by (apply (link_of_root rho y l Lin K); [subst; left; reflexivity| exact El]).
  destruct (write_link_shape gmax g l d es g' H4 Hs) as [s [nd [Ef [Es Eg']]]].
  destruct (find_store_some _ _ _ Ef) as [Hs_in Hsid].
  destruct (F5 _ _ Ef) as [sr [Er' Hsr]].
  assert (Hreach : reach s (live_roots rho Lin K) (lidx l))
    by exact (RRoot s _ l nd HlR (eq_sym Hsid) (follow_slot s l nd Es)).
  destruct (slotrel_forward _ s sr Hsr _ Hreach) as [_ Eslot].
  assert (Esr : nth_error (sslots sr) (lidx l) = Some (mkSlot (lgen l) (Some nd))) by (rewrite Eslot; exact Es).
  assert (Hbr : gbor r = []) by (rewrite F2; exact H4).
  pose proof (write_link_exec gmax r l d es sr nd Hbr Er' Esr) as Wr.
  exists (mkGS (set_store (mkStore (ssid sr) (stab sr) (upd (sslots sr) (lidx l)
            (mkSlot (lgen l) (Some (mkNode d es (nblocks nd)))))) (gstores r)) (gheap r) (gsid r) [] (gissued r)).
  split.
  { unfold set_result.
    rewrite (node_of_rel _ g r rho y H3 (fun l0 Hl0 => link_of_root rho y l0 Lin K ltac:(subst; left; reflexivity) Hl0)).
    rewrite En, Ees. exact Wr. }
  assert (HRR : forall l0, In l0 (live_roots rho L K) -> In l0 (live_roots rho Lin K)).
  { apply roots_weaken. intros z Hz. subst Lin. simpl. right. rewrite !in_app_iff. tauto. }
  assert (Hnew : forall e0, In e0 es -> exists l0, In l0 (live_roots rho Lin K) /\ lsid l0 = lsid l /\ e0 = (lidx l0, lgen l0)).
  { intros e0 He0. destruct (edges_of_spec rho (lsid l) ys es e0 Ees He0) as [y0 [l0 [Hy0 [El0 [Hl0 Ee0]]]]].
    exists l0. split; [|split; assumption]. apply (link_of_root rho y0 l0 Lin K); [|exact El0].
    subst Lin. simpl. right. rewrite !in_app_iff. tauto. }
  assert (Ls : lidx l < length (sslots s)) by (apply nth_error_Some; congruence).
  assert (Lr : lidx l < length (sslots sr)) by (apply nth_error_Some; congruence).
  assert (SR : SlotRel (live_roots rho L K)
                 (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (lgen l) (Some (mkNode d es (nblocks nd))))))
                 (mkStore (ssid sr) (stab sr) (upd (sslots sr) (lidx l) (mkSlot (lgen l) (Some (mkNode d es (nblocks nd))))))).
  { destruct Hsr as [E1 [E2 E3]]. split; [exact E1|]. split; [exact E2|]. intros j. simpl.
    destruct (Nat.eq_dec j (lidx l)) as [->|Hne].
    - left. rewrite !nth_upd_same by assumption. reflexivity.
    - rewrite !(nth_upd_other _ _ _ _ _ Hne). destruct (E3 j) as [Ej|[g0 [n0 [Ej0 [Ej1 Hn]]]]]; [left; exact Ej|].
      right. exists g0, n0. split; [exact Ej0|]. split; [exact Ej1|]. intro Hr. apply Hn.
      apply (reach_after_write s (lidx l) (lgen l) nd (mkNode d es (nblocks nd)) _ (live_roots rho L K) Es);
        [intros l0 Hl0 _; exact (HRR l0 Hl0)| |exact Hr].
      intros e0 n He0 Hf. destruct (Hnew e0 He0) as [l0 [Hl0 [Hsl0 ->]]].
      exact (RRoot s _ l0 n Hl0 (eq_trans Hsl0 (eq_sym Hsid)) Hf). }
  assert (Efs : find_store (ssid (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l)
                   (mkSlot (lgen l) (Some (mkNode d es (nblocks nd))))))) (gstores g) = Some s)
    by (simpl; rewrite Hsid; exact Ef).
  assert (Ers : find_store (ssid (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l)
                   (mkSlot (lgen l) (Some (mkNode d es (nblocks nd))))))) (gstores r) = Some sr)
    by (simpl; rewrite Hsid; exact Er').
  assert (Hid : ssid (mkStore (ssid sr) (stab sr) (upd (sslots sr) (lidx l) (mkSlot (lgen l) (Some (mkNode d es (nblocks nd)))))) =
                ssid (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (lgen l) (Some (mkNode d es (nblocks nd)))))))
    by (simpl; destruct Hsr as [E1 _]; exact E1).
  destruct (stores_rel_set _ (live_roots rho L K) (gstores g) (gstores r) s _ sr _ F4 F5 Efs Ers Hid SR
    (fun s0 _ _ j Hj => reach_mono s0 _ _ HRR j Hj)) as [M Fs].
  subst g'. constructor.
  - exact (write_link_ginv gmax g l d es _ H1 Hs).
  - exact (write_link_ginv gmax r l d es _ H2 Wr).
  - split; [exact F1|]. split; [reflexivity|]. split; [exact F3|]. split; [exact M| exact Fs].
  - reflexivity.
  - intros l0 Hl0. exact (H5 l0 (HRR l0 Hl0)).
  - intros s0 j x0 e0 Hs0 Ej He0. simpl in Hs0 |- *. apply in_set in Hs0. destruct Hs0 as [->|Hs0].
    + simpl in Ej. destruct (Nat.eq_dec j (lidx l)) as [->|Hne].
      * rewrite nth_upd_same in Ej by exact Ls. injection Ej as <-. simpl in He0.
        destruct (Hnew e0 He0) as [l0 [Hl0 [Hsl0 ->]]]. simpl. rewrite Hsid, <- Hsl0, link_eta. exact (H5 l0 Hl0).
      * rewrite (nth_upd_other _ _ _ _ _ Hne) in Ej. exact (H6 s j x0 e0 Hs_in Ej He0).
    + exact (H6 s0 j x0 e0 Hs0 Ej He0).
Qed.

Lemma sim_set_err : forall gmax K rho Lin y ys g r d,
  SimInv gmax K rho Lin g r -> In y Lin -> set_result gmax rho g y d ys = None -> set_result gmax rho r y d ys = None.
Proof.
  intros gmax K rho Lin y ys g r d HS Hy Hs. pose proof HS as [H1 H2 H3 H4 H5 H6].
  unfold set_result in *.
  rewrite (node_of_rel _ g r rho y H3 (fun l0 Hl0 => link_of_root rho y l0 Lin K Hy Hl0)).
  destruct (node_of rho g y) as [[l nd0]|] eqn:En; [|reflexivity].
  destruct (edges_of rho (lsid l) ys) as [es|]; [|reflexivity].
  destruct (node_of_link _ _ _ _ _ En) as [_ Er]. destruct (resolve_found g l nd0 Er) as [s [Ef Es]].
  rewrite (write_link_exec gmax g l d es s nd0 H4 Ef Es) in Hs. discriminate.
Qed.

Lemma nth_snoc_other : forall (A : Type) (sl : list A) x j, j <> length sl -> nth_error (sl ++ [x]) j = nth_error sl j.
Proof.
  intros A sl x j H. destruct (Nat.lt_ge_cases j (length sl)) as [Hl|Hg].
  - apply nth_error_app1. exact Hl.
  - rewrite nth_error_app2 by exact Hg. rewrite (proj2 (nth_error_None sl j) Hg).
    destruct (j - length sl) as [|m] eqn:E; [lia|]. destruct m; reflexivity.
Qed.

Lemma nth_snoc_same : forall (A : Type) (sl : list A) x, nth_error (sl ++ [x]) (length sl) = Some x.
Proof. intros A sl x. rewrite nth_error_app2 by lia. rewrite Nat.sub_diag. reflexivity. Qed.

(* The common part of the two insert shapes: one new slot at idx with
   generation g0, every other slot unchanged on both sides. *)
Lemma sim_insert_core : forall gmax K rho x L Lin g r l s sr s' sr' idx g0 nd es g' r',
  SimInv gmax K rho Lin g r -> incl (vremove x L) Lin ->
  find_store (lsid l) (gstores g) = Some s -> find_store (lsid l) (gstores r) = Some sr ->
  SlotRel (live_roots rho Lin K) s sr ->
  l = mkLink (ssid s) idx g0 -> nedges nd = es ->
  ssid s' = ssid s -> ssid sr' = ssid s -> stab sr' = stab s' ->
  nth_error (sslots s') idx = Some (mkSlot g0 (Some nd)) -> nth_error (sslots sr') idx = Some (mkSlot g0 (Some nd)) ->
  (forall j, j <> idx -> nth_error (sslots s') j = nth_error (sslots s) j) ->
  (forall j, j <> idx -> nth_error (sslots sr') j = nth_error (sslots sr) j) ->
  (forall t, nth_error (sslots s) idx = Some t -> sgen t = g0 /\ snode t = None) ->
  (forall e, In e es -> exists l0, In l0 (live_roots rho Lin K) /\ lsid l0 = lsid l /\ e = (lidx l0, lgen l0)) ->
  GInv gmax g' -> GInv gmax r' ->
  gstores g' = set_store s' (gstores g) -> gstores r' = set_store sr' (gstores r) ->
  gsid g' = gsid g -> gsid r' = gsid r -> gbor g' = gbor g -> gbor r' = gbor r ->
  gissued g' = l :: gissued g -> gissued r' = l :: gissued r ->
  SimInv gmax K (rupd rho x (GLnk l)) L g' r'.
Proof.
  intros gmax K rho x L Lin g r l s sr s' sr' idx g0 nd es g' r' HS Hi Ef Er Hsr El Hes Hs' Hsr' Ht'
    Ei' Eir' Ho' Hor' Hvac Hnew Gg Gr Sg Sr Ig Ir Bg Br Ug Ur.
  pose proof HS as [H1 H2 H3 H4 H5 H6]. pose proof H3 as [F1 [F2 [F3 [F4 F5]]]].
  destruct (find_store_some _ _ _ Ef) as [Hs_in Hsid].
  set (R := live_roots rho Lin K) in *. set (R' := live_roots (rupd rho x (GLnk l)) L K).
  assert (Split : forall l0, In l0 R' -> l0 = l \/ In l0 R).
  { intros l0 Hl0. destruct (roots_after_def rho x (GLnk l) L Lin K l0 Hi Hl0) as [A|A].
    - left. simpl in A. destruct A as [<-|[]]. reflexivity.
    - right. exact A. }
  (* No current identity names the new slot before the insert. *)
  assert (HR : forall l0, In l0 R -> lsid l0 = ssid s -> lidx l0 = idx -> lgen l0 <> g0).
  { intros l0 Hl0 Hls Hli Hlg. rewrite <- Hsid in Ef. rewrite <- Hls in Ef.
    destruct (issued_current gmax g l0 s H1 (H5 l0 Hl0) Ef) as [t [Et Ht]]. rewrite Hli in Et.
    destruct (Hvac t Et) as [Eg En]. rewrite Hlg, <- Eg in Ht. destruct Ht as [Hl|[_ Hn]]; [lia| contradiction]. }
  assert (Hold : forall j x0 e, nth_error (sslots s) j = Some x0 -> In e (slot_edges x0) -> fst e = idx -> snd e <> g0).
  { intros j x0 e Ej He Hfe Hse. pose proof (H6 s j x0 e Hs_in Ej He) as Hiss.
    assert (Ef2 : find_store (lsid (mkLink (ssid s) (fst e) (snd e))) (gstores g) = Some s) by (simpl; rewrite Hsid; exact Ef).
    destruct (issued_current gmax g _ s H1 Hiss Ef2) as [t [Et Ht]]. simpl in Et, Ht. rewrite Hfe in Et.
    destruct (Hvac t Et) as [Eg En]. rewrite Hse, <- Eg in Ht. destruct Ht as [Hl|[_ Hn]]; [lia| contradiction]. }
  assert (Reach : forall j, reach s' R' j -> reach s R j \/ j = idx).
  { intros j Hj. apply (reach_after_insert s s' idx g0 nd R R' Hs' Ei' Ho'); [| exact HR | exact Hold | | exact Hj].
    - intros l0 Hl0 Hls. destruct (Split l0 Hl0) as [->|A]; [left; exact El| right; exact A].
    - intros e He. rewrite Hes in He. destruct (Hnew e He) as [l0 [Hl0 [Hls ->]]]. split.
      + intros Hfe. simpl in Hfe |- *. apply (HR l0 Hl0); [rewrite Hls, El; reflexivity| exact Hfe].
      + intros n Hf. exact (RRoot s R l0 n Hl0 (eq_trans Hls (eq_trans (f_equal lsid El) eq_refl)) Hf). }
  assert (SR : SlotRel R' s' sr').
  { destruct Hsr as [E1 [E2 E3]]. split; [congruence|]. split; [rewrite Ht'; reflexivity|]. intros j.
    destruct (Nat.eq_dec j idx) as [->|Hne].
    - left. rewrite Ei', Eir'. reflexivity.
    - rewrite (Ho' j Hne), (Hor' j Hne). destruct (E3 j) as [Ej|[gg [nn [Ej0 [Ej1 Hn]]]]]; [left; exact Ej|].
      right. exists gg, nn. split; [exact Ej0|]. split; [exact Ej1|]. intro Hr.
      destruct (Reach j Hr) as [A|A]; [exact (Hn A)| exact (Hne A)]. }
  assert (Hsl : lsid l = ssid s) by (rewrite El; reflexivity).
  assert (Efs : find_store (ssid s') (gstores g) = Some s) by (rewrite Hs', <- Hsl; exact Ef).
  assert (Ers : find_store (ssid s') (gstores r) = Some sr) by (rewrite Hs', <- Hsl; exact Er).
  assert (Ho : forall s0, In s0 (gstores g) -> ssid s0 <> ssid s' -> forall j, reach s0 R' j -> reach s0 R j).
  { intros s0 Hs0 Hne j Hj. apply (reach_other_store s0 R R'); [|exact Hj].
    intros l0 Hl0 Hls0. destruct (Split l0 Hl0) as [->|A]; [|exact A].
    exfalso. apply Hne. rewrite Hs', <- Hls0, Hsl. reflexivity. }
  destruct (stores_rel_set R R' (gstores g) (gstores r) s s' sr sr' F4 F5 Efs Ers (eq_trans Hsr' (eq_sym Hs')) SR Ho)
    as [M Fs].
  constructor.
  - exact Gg.
  - exact Gr.
  - split; [congruence|]. split; [congruence|]. split; [congruence|]. rewrite Sg, Sr. split; [exact M| exact Fs].
  - rewrite Bg. exact H4.
  - intros l0 Hl0. rewrite Ug. destruct (Split l0 Hl0) as [->|A]; [left; reflexivity| right; exact (H5 l0 A)].
  - intros s0 j x0 e0 Hs0 Ej He0. rewrite Ug. rewrite Sg in Hs0. apply in_set in Hs0. destruct Hs0 as [->|Hs0].
    + destruct (Nat.eq_dec j idx) as [->|Hne].
      * rewrite Ei' in Ej. injection Ej as <-. change (In e0 (nedges nd)) in He0. rewrite Hes in He0.
        destruct (Hnew e0 He0) as [l0 [Hl0 [Hls ->]]]. right. simpl. rewrite Hs', <- Hsl, <- Hls, link_eta.
        exact (H5 l0 Hl0).
      * rewrite (Ho' j Hne) in Ej. right. rewrite Hs'. exact (H6 s j x0 e0 Hs_in Ej He0).
    + right. exact (H6 s0 j x0 e0 Hs0 Ej He0).
Qed.

Lemma sim_insert : forall gmax K rho x L Lin g r sid idx d es bs tb g' l ys,
  SimInv gmax K rho Lin g r -> incl (vremove x L) Lin -> incl ys Lin ->
  edges_of rho sid ys = Some es -> gexec gmax g (OInsert sid idx d es bs tb) = (g', GLink l) ->
  exists r', gexec gmax r (OInsert sid idx d es bs tb) = (r', GLink l) /\ SimInv gmax K (rupd rho x (GLnk l)) L g' r'.
Proof.
  intros gmax K rho x L Lin g r sid idx d es bs tb g' l ys HS Hi Hys Ees Hins.
  pose proof HS as [H1 H2 H3 H4 H5 H6]. pose proof H3 as [F1 [F2 [F3 [F4 F5]]]].
  pose proof (ginv_step gmax g (OInsert sid idx d es bs tb) H1) as Gg. rewrite Hins in Gg. simpl in Gg.
  destruct (insert_shape gmax g sid idx d es bs tb g' l Hins) as [s [Ef Hcase]].
  destruct (find_store_some _ _ _ Ef) as [_ Hsid].
  destruct (F5 _ _ Ef) as [sr [Er Hsr]].
  assert (Hnew : forall e, In e es -> exists l0, In l0 (live_roots rho Lin K) /\ lsid l0 = sid /\ e = (lidx l0, lgen l0)).
  { intros e He. destruct (edges_of_spec rho sid ys es e Ees He) as [y0 [l0 [Hy0 [El0 [Hl0 Ee0]]]]].
    exists l0. split; [exact (link_of_root rho y0 l0 Lin K (Hys y0 Hy0) El0)| split; assumption]. }
  pose proof (heap_incl gmax _ g r H1 H2 H3) as Hheap.
  destruct Hcase as [[gen [Ei [Eg [Eb [El Eg']]]]]|[Ei [Ebr [Ea [Ec [El Eg']]]]]].
  - (* reuse of a vacant slot *)
    assert (Eir : nth_error (sslots sr) idx = Some (mkSlot gen None)).
    { destruct Hsr as [_ [_ E3]]. destruct (E3 idx) as [E|[g0 [n0 [E0 _]]]]; [rewrite E; exact Ei|].
      rewrite Ei in E0. discriminate. }
    assert (Ebr : bnodup bs && bfree bs (gheap r) = true).
    { apply andb_true_iff in Eb. destruct Eb as [Eb1 Eb2]. apply andb_true_iff. split; [exact Eb1|].
      exact (bfree_incl bs _ _ Hheap Eb2). }
    pose proof (insert_fill_exec gmax r sid idx d es bs tb sr gen Er Eir Eg Ebr) as Xr.
    rewrite <- El in Xr. eexists. split; [exact Xr|].
    pose proof (ginv_step gmax r (OInsert sid idx d es bs tb) H2) as Gr. rewrite Xr in Gr. simpl in Gr.
    assert (Ls : idx < length (sslots s)) by (apply nth_error_Some; congruence).
    assert (Lr : idx < length (sslots sr)) by (apply nth_error_Some; congruence).
    subst g'.
    refine (sim_insert_core gmax K rho x L Lin g r l s sr
      (mkStore sid (stab s) (upd (sslots s) idx (mkSlot gen (Some (mkNode d es bs)))))
      (mkStore sid (stab sr) (upd (sslots sr) idx (mkSlot gen (Some (mkNode d es bs)))))
      idx gen (mkNode d es bs) es _ _ HS Hi _ _ Hsr _ eq_refl _ _ _ _ _ _ _ _ _ Gg Gr
      eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl).
    + rewrite El. exact Ef.
    + rewrite El. exact Er.
    + rewrite El, Hsid. reflexivity.
    + simpl. symmetry. exact Hsid.
    + simpl. symmetry. exact Hsid.
    + simpl. destruct Hsr as [_ [E2 _]]. exact E2.
    + simpl. apply nth_upd_same. exact Ls.
    + simpl. apply nth_upd_same. exact Lr.
    + intros j Hne. simpl. apply nth_upd_other. exact Hne.
    + intros j Hne. simpl. apply nth_upd_other. exact Hne.
    + intros t Et. rewrite Ei in Et. injection Et as <-. split; reflexivity.
    + intros e He. destruct (Hnew e He) as [l0 [Hl0 [Hls Ee]]]. exists l0. split; [exact Hl0|].
      split; [rewrite El; exact Hls| exact Ee].
  - (* a growing insert *)
    assert (Eir : nth_error (sslots sr) idx = None) by exact (proj2 (slotrel_none _ s sr idx Hsr) Ei).
    assert (Ebr' : store_borrowed sid (gbor r) = false) by (rewrite F2; exact Ebr).
    assert (Ea' : Nat.eqb idx (length (sslots sr)) && Nat.ltb 0 gmax = true)
      by (rewrite (slotrel_length _ s sr Hsr); exact Ea).
    assert (Ec' : bnodup (tb :: bs) && bfree (tb :: bs) (free [stab sr] (gheap r)) = true).
    { apply andb_true_iff in Ec. destruct Ec as [Ec1 Ec2]. apply andb_true_iff. split; [exact Ec1|].
      destruct Hsr as [_ [E2 _]]. rewrite E2. exact (bfree_incl _ _ _ (free_incl _ _ _ Hheap) Ec2). }
    pose proof (insert_append_exec gmax r sid idx d es bs tb sr Er Eir Ebr' Ea' Ec') as Xr.
    rewrite <- El in Xr. eexists. split; [exact Xr|].
    pose proof (ginv_step gmax r (OInsert sid idx d es bs tb) H2) as Gr. rewrite Xr in Gr. simpl in Gr.
    apply andb_true_iff in Ea. destruct Ea as [Ea1 _]. apply Nat.eqb_eq in Ea1.
    assert (Lr : idx = length (sslots sr)) by (rewrite (slotrel_length _ s sr Hsr); exact Ea1).
    subst g'.
    refine (sim_insert_core gmax K rho x L Lin g r l s sr
      (mkStore sid tb (sslots s ++ [mkSlot 0 (Some (mkNode d es bs))]))
      (mkStore sid tb (sslots sr ++ [mkSlot 0 (Some (mkNode d es bs))]))
      idx 0 (mkNode d es bs) es _ _ HS Hi _ _ Hsr _ eq_refl _ _ eq_refl _ _ _ _ _ _ Gg Gr
      eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl eq_refl).
    + rewrite El. exact Ef.
    + rewrite El. exact Er.
    + rewrite El, Hsid. reflexivity.
    + simpl. symmetry. exact Hsid.
    + simpl. symmetry. exact Hsid.
    + simpl. rewrite Ea1. apply nth_snoc_same.
    + simpl. rewrite Lr. apply nth_snoc_same.
    + intros j Hne. simpl. apply nth_snoc_other. rewrite <- Ea1. exact Hne.
    + intros j Hne. simpl. apply nth_snoc_other. rewrite <- Lr. exact Hne.
    + intros t Et. rewrite Ei in Et. discriminate.
    + intros e He. destruct (Hnew e He) as [l0 [Hl0 [Hls Ee]]]. exists l0. split; [exact Hl0|].
      split; [rewrite El; exact Hls| exact Ee].
Qed.

Lemma sim_delete : forall gmax rights ctx K rho y L E g r g',
  SimInv gmax K rho (y :: L ++ E) g r -> delete_result gmax rights ctx rho g y = Some g' ->
  exists r', delete_result gmax rights ctx rho r y = Some r' /\ SimInv gmax K rho L g' r'.
Proof.
  intros gmax rights ctx K rho y L E g r g' HS Hd.
  remember (y :: L ++ E) as Lin eqn:HLin.
  pose proof HS as [H1 H2 H3 H4 H5 H6]. pose proof H3 as [F1 [F2 [F3 [F4 F5]]]].
  unfold delete_result in Hd. destruct (link_of rho y) as [l|] eqn:El; [|discriminate].
  destruct (holds rights ctx (lsid l)) eqn:Eh; [|discriminate].
  unfold unit_result in Hd. destruct (gexec gmax g (ODelete l)) as [g0 res] eqn:Ex. simpl in Hd.
  destruct res; try discriminate. injection Hd as <-.
  pose proof (ginv_step gmax g (ODelete l) H1) as Gg. rewrite Ex in Gg. simpl in Gg.
  destruct (delete_shape gmax g l g0 Ex) as [s [nd [Ef [Es [Eb Eg']]]]].
  destruct (find_store_some _ _ _ Ef) as [Hs_in Hsid].
  destruct (F5 _ _ Ef) as [sr [Er Hsr]].
  assert (HlR : In l (live_roots rho Lin K)) by (apply (link_of_root rho y l Lin K); [subst; left; reflexivity| exact El]).
  assert (Hreach : reach s (live_roots rho Lin K) (lidx l))
    by exact (RRoot s _ l nd HlR (eq_sym Hsid) (follow_slot s l nd Es)).
  destruct (slotrel_forward _ s sr Hsr _ Hreach) as [_ Eslot].
  assert (Esr : nth_error (sslots sr) (lidx l) = Some (mkSlot (lgen l) (Some nd))) by (rewrite Eslot; exact Es).
  assert (Ebr : existsb (same_place (lsid l) (lidx l)) (gbor r) = false) by (rewrite F2; exact Eb).
  pose proof (delete_exec gmax r l sr nd Er Esr Ebr) as Xr.
  pose proof (ginv_step gmax r (ODelete l) H2) as Gr. rewrite Xr in Gr. simpl in Gr.
  eexists. split.
  { unfold delete_result. rewrite El, Eh. unfold unit_result. rewrite Xr. reflexivity. }
  assert (HRR : forall l0, In l0 (live_roots rho L K) -> In l0 (live_roots rho Lin K)).
  { apply roots_weaken. intros z Hz. subst Lin. simpl. right. rewrite in_app_iff. tauto. }
  assert (Ls : lidx l < length (sslots s)) by (apply nth_error_Some; congruence).
  assert (Lr : lidx l < length (sslots sr)) by (apply nth_error_Some; congruence).
  assert (SR : SlotRel (live_roots rho L K)
                 (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S (lgen l)) None)))
                 (mkStore (ssid sr) (stab sr) (upd (sslots sr) (lidx l) (mkSlot (S (lgen l)) None)))).
  { destruct Hsr as [E1 [E2 E3]]. split; [exact E1|]. split; [exact E2|]. intros j. simpl.
    destruct (Nat.eq_dec j (lidx l)) as [->|Hne].
    - left. rewrite !nth_upd_same by assumption. reflexivity.
    - rewrite !(nth_upd_other _ _ _ _ _ Hne). destruct (E3 j) as [Ej|[gg [nn [Ej0 [Ej1 Hn]]]]]; [left; exact Ej|].
      right. exists gg, nn. split; [exact Ej0|]. split; [exact Ej1|]. intro Hr. apply Hn.
      exact (reach_after_delete s (lidx l) (lgen l) nd _ _ Es (fun l0 Hl0 _ => HRR l0 Hl0) j Hr). }
  assert (Efs : find_store (ssid (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S (lgen l)) None))))
                  (gstores g) = Some s) by (simpl; rewrite Hsid; exact Ef).
  assert (Ers : find_store (ssid (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S (lgen l)) None))))
                  (gstores r) = Some sr) by (simpl; rewrite Hsid; exact Er).
  assert (Hid : ssid (mkStore (ssid sr) (stab sr) (upd (sslots sr) (lidx l) (mkSlot (S (lgen l)) None))) =
                ssid (mkStore (ssid s) (stab s) (upd (sslots s) (lidx l) (mkSlot (S (lgen l)) None))))
    by (simpl; destruct Hsr as [E1 _]; exact E1).
  destruct (stores_rel_set _ (live_roots rho L K) (gstores g) (gstores r) s _ sr _ F4 F5 Efs Ers Hid SR
    (fun s0 _ _ j Hj => reach_mono s0 _ _ HRR j Hj)) as [M Fs].
  subst g0. constructor.
  - exact Gg.
  - exact Gr.
  - split; [exact F1|]. split; [exact F2|]. split; [exact F3|]. split; [exact M| exact Fs].
  - exact H4.
  - intros l0 Hl0. exact (H5 l0 (HRR l0 Hl0)).
  - intros s0 j x0 e0 Hs0 Ej He0. simpl in Hs0 |- *. apply in_set in Hs0. destruct Hs0 as [->|Hs0].
    + simpl in Ej. destruct (Nat.eq_dec j (lidx l)) as [->|Hne].
      * rewrite nth_upd_same in Ej by exact Ls. injection Ej as <-. simpl in He0. contradiction.
      * rewrite (nth_upd_other _ _ _ _ _ Hne) in Ej. exact (H6 s j x0 e0 Hs_in Ej He0).
    + exact (H6 s0 j x0 e0 Hs0 Ej He0).
Qed.

Lemma sim_delete_err : forall gmax rights ctx K rho Lin y g r,
  SimInv gmax K rho Lin g r -> delete_result gmax rights ctx rho g y = None ->
  delete_result gmax rights ctx rho r y = None.
Proof.
  intros gmax rights ctx K rho Lin y g r HS Hd. pose proof HS as [_ _ H3 _ _ _].
  unfold delete_result, unit_result in *. destruct (link_of rho y) as [l|]; [|reflexivity].
  destruct (holds rights ctx (lsid l)); [|reflexivity].
  destruct (snd (gexec gmax r (ODelete l))) eqn:Er; try reflexivity.
  rewrite (delete_ok_back gmax _ g r l H3 Er) in Hd. discriminate.
Qed.

(* ------------------------------------------------------------------ *)
(* Reclaiming never changes a reference run                             *)
(* ------------------------------------------------------------------ *)

Definition out_live (o : ROut) (L E : list Var) : list Var := match o with RNorm => L | RErr => E end.

Ltac in_solve := simpl; repeat rewrite in_app_iff; tauto.

Lemma call_args_spec : forall funs rho f ys ps body ret vs, call_args funs rho f ys = Some (ps, body, ret, vs) ->
  funs f = Some (ps, body, ret) /\ vals_of rho ys = Some vs.
Proof.
  intros funs rho f ys ps body ret vs H. unfold call_args in H.
  destruct (funs f) as [[[ps0 body0] ret0]|]; [|discriminate].
  destruct (vals_of rho ys) as [vs0|]; [|discriminate].
  destruct (Nat.eqb (length ps0) (length vs0)); [|discriminate]. injection H as <- <- <- <-. split; reflexivity.
Qed.

(* The callee's live-in roots are among the caller's call roots. *)
Lemma siminv_call_entry : forall gmax K rho ys keep ps vs Lf g r, SimInv gmax K rho (ys ++ keep) g r ->
  vals_of rho ys = Some vs -> SimInv gmax (frame_roots links_of true K rho keep) (rbind ps vs) Lf g r.
Proof.
  intros gmax K rho ys keep ps vs Lf g r HS Ev. apply (siminv_change gmax K rho (ys ++ keep) _ _ _ g r HS).
  intros l0 Hl0. unfold live_roots in Hl0 |- *. apply in_app_or in Hl0. apply in_or_app.
  destruct Hl0 as [Hl0|Hl0].
  - apply vars_links_in in Hl0. destruct Hl0 as [z [v' [_ [E' Hl']]]].
    left. apply (vars_links_incl links_of rho ys (ys ++ keep)); [intros q Hq; apply in_or_app; left; exact Hq|].
    apply (vals_links rho ys vs Ev l0). apply in_flat_map. exists v'. split; [exact (rbind_in ps vs z v' E')| exact Hl'].
  - unfold frame_roots in Hl0. apply in_app_or in Hl0. destruct Hl0 as [Hl0|Hl0]; [right; exact Hl0|].
    left. apply (vars_links_incl links_of rho keep (ys ++ keep)); [intros q Hq; apply in_or_app; right; exact Hq| exact Hl0].
Qed.

(* After the call the caller's roots are among the callee's exit roots. *)
Lemma siminv_call_exit : forall gmax K rho keep rhof Lr g r K2 rho2 L2,
  SimInv gmax (frame_roots links_of true K rho keep) rhof Lr g r ->
  (forall l, In l (vars_links links_of rho2 L2) -> In l (vars_links links_of rhof Lr) \/ In l (vars_links links_of rho keep)) ->
  K2 = K -> SimInv gmax K2 rho2 L2 g r.
Proof.
  intros gmax K rho keep rhof Lr g r K2 rho2 L2 HS Hi ->. apply (siminv_change gmax _ rhof Lr K rho2 L2 g r HS).
  intros l0 Hl0. unfold live_roots in Hl0 |- *. apply in_app_or in Hl0. apply in_or_app.
  destruct Hl0 as [Hl0|Hl0].
  - destruct (Hi l0 Hl0) as [A|A]; [left; exact A|]. right. unfold frame_roots. apply in_or_app. right. exact A.
  - right. unfold frame_roots. apply in_or_app. left. exact Hl0.
Qed.

Lemma vars_after_def : forall rho x v L keep l, incl (vremove x L) keep ->
  In l (vars_links links_of (rupd rho x v) L) -> In l (links_of v) \/ In l (vars_links links_of rho keep).
Proof.
  intros rho x v L keep l Hi Hl. apply vars_links_in in Hl. destruct Hl as [z [w [Hz [E Hw]]]].
  destruct (Nat.eq_dec z x) as [->|Hne].
  - rewrite rupd_same in E. injection E as <-. left. exact Hw.
  - rewrite (rupd_other _ _ _ _ Hne) in E. right. apply vars_links_in. exists z, w.
    split; [apply Hi; apply vremove_In; split; assumption| split; assumption].
Qed.

Section Sim.
Variable gmax : nat.
Variable funs : nat -> option RFun.
Variable rights : nat -> option nat.
Variable ctx : nat.
Hypothesis funs_ok : FunsOK funs.

(* The main theorem. A checked program's reference run is reproduced by the
   reclaiming run: same final environment, trace and outcome. The invariant
   at the end says every node the reclaimer deleted is unreachable from the
   live roots of that point. *)
Theorem reclaim_simulates : forall K rho g s rho' g' tr o,
  rexec gmax funs rights ctx links_of true false K rho g s rho' g' tr o ->
  forall L E Lin r, rcheck s L E = Some Lin -> SimInv gmax K rho Lin g r ->
  exists r', rexec gmax funs rights ctx links_of true true K rho r s rho' r' tr o /\ SimInv gmax K rho' (out_live o L E) g' r'.
Proof.
  intros K rho g s rho' g' tr o H.
  induction H as
    [ K rho g
    | K rho g x n
    | K rho g x y v Hy
    | K rho g x y Hy
    | K rho g x t ys vs Hv
    | K rho g x t ys Hv
    | K rho g x y i w Hp
    | K rho g x y i Hp
    | K rho g x y l Hl
    | K rho g x y Hl
    | K rho g x y l nd Hn
    | K rho g x y Hn
    | K rho g x y k l He
    | K rho g x y k He
    | K rho g y d ys g' Hs
    | K rho g y d ys Hs
    | K rho g x sid idx d ys bs tb es g' l Hes Hins
    | K rho g x sid idx d ys bs tb Hes
    | K rho g y g' Hd
    | K rho g y Hd
    | K rho g a b rho1 g1 tr1 rho2 g2 tr2 o Ha IHa Hb IHb
    | K rho g a b rho1 g1 tr1 Ha IHa
    | K rho g c a b n rho1 g1 tr o Hc0 Ha IHa
    | K rho g c a b rho1 g1 tr o Hc0 Hb IHb
    | K rho g c a b Hc0
    | K rho g c h b Hc0
    | K rho g c h b n rho1 g1 tr1 rho2 g2 tr2 o Hc0 Hb IHb Hw IHw
    | K rho g c h b n rho1 g1 tr1 Hc0 Hb IHb
    | K rho g c h b Hc0
    | K rho g b h rho1 g1 tr1 Hb IHb
    | K rho g b h rho1 g1 tr1 rho2 g2 tr2 o Hb IHb Hh IHh
    | K rho g x f ys keep ps body ret vs rhof g1 tr v Ha Hb IHb Hr
    | K rho g x f ys keep ps body ret vs rhof g1 tr Ha Hb IHb Hr
    | K rho g x f ys keep ps body ret vs rhof g1 tr Ha Hb IHb
    | K rho g x f ys keep Ha
    | K rho g sid C fuel R Hreal
    | K rho g sid C fuel R Hreal ];
  intros L0 E0 Lin r Hc HS.
  - (* Skip *)
    simpl in Hc. injection Hc as <-. exists r. split; [constructor| exact HS].
  - (* Num *)
    simpl in Hc. injection Hc as <-. exists r. split; [constructor|].
    apply (siminv_def gmax K rho L0 (vremove x L0) g r x (GNum n) HS); [intros z Hz; exact Hz| simpl; intros l0 []].
  - (* Copy *)
    simpl in Hc. injection Hc as <-. exists r. split; [apply X_Copy; exact Hy|].
    apply (siminv_def gmax K rho L0 _ g r x v HS); [intros z Hz; in_solve|].
    intros l0 Hl0. apply in_or_app. left. apply vars_links_in. exists y, v. split; [left; reflexivity| split; assumption].
  - simpl in Hc. injection Hc as <-. exists r. split; [apply X_CopyErr; exact Hy|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Pack *)
    simpl in Hc. injection Hc as <-. exists r. split; [apply X_Pack; exact Hv|].
    apply (siminv_def gmax K rho L0 _ g r x (GAgg t vs) HS); [intros z Hz; in_solve|].
    intros l0 Hl0. rewrite links_of_agg in Hl0. apply in_or_app. left.
    apply (vars_links_incl links_of rho ys); [intros z Hz; in_solve| exact (vals_links rho ys vs Hv l0 Hl0)].
  - simpl in Hc. injection Hc as <-. exists r. split; [apply X_PackErr; exact Hv|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Proj *)
    simpl in Hc. injection Hc as <-. exists r. split; [apply X_Proj; exact Hp|].
    apply (siminv_def gmax K rho L0 _ g r x w HS); [intros z Hz; in_solve|].
    intros l0 Hl0. apply in_or_app. left. apply (vars_links_incl links_of rho [y]);
      [intros z [<-|[]]; left; reflexivity| exact (proj_links rho y i w Hp l0 Hl0)].
  - simpl in Hc. injection Hc as <-. exists r. split; [apply X_ProjErr; exact Hp|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* View *)
    simpl in Hc. injection Hc as <-. exists r. split; [apply X_View; exact Hl|].
    apply (siminv_def gmax K rho L0 _ g r x (GView l) HS); [intros z Hz; in_solve|].
    intros l0 [<-|[]]. exact (link_of_root rho y l (y :: vremove x L0 ++ E0) K (or_introl eq_refl) Hl).
  - simpl in Hc. injection Hc as <-. exists r. split; [apply X_ViewErr; exact Hl|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Read *)
    simpl in Hc. injection Hc as <-. exists r. split.
    + apply X_Read with (l := l).
      rewrite (node_of_rel _ g r rho y (si_rel _ _ _ _ _ _ HS)
                 (fun l0 Hl0 => link_of_root rho y l0 (y :: vremove x L0 ++ E0) K (or_introl eq_refl) Hl0)). exact Hn.
    + apply (siminv_def gmax K rho L0 _ g r x (GNum (ndata nd)) HS); [intros z Hz; in_solve| simpl; intros l0 []].
  - simpl in Hc. injection Hc as <-. exists r. split.
    + apply X_ReadErr.
      rewrite (node_of_rel _ g r rho y (si_rel _ _ _ _ _ _ HS)
                 (fun l0 Hl0 => link_of_root rho y l0 (y :: vremove x L0 ++ E0) K (or_introl eq_refl) Hl0)). exact Hn.
    + simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Edge *)
    simpl in Hc. injection Hc as <-. exists r. split.
    + apply X_Edge. unfold edge_link_of.
      rewrite (node_of_rel _ g r rho y (si_rel _ _ _ _ _ _ HS)
                 (fun l0 Hl0 => link_of_root rho y l0 (y :: vremove x L0 ++ E0) K (or_introl eq_refl) Hl0)). exact He.
    + apply (siminv_edge gmax K rho L0 _ g r x y k l HS); [left; reflexivity| intros z Hz; in_solve| exact He].
  - simpl in Hc. injection Hc as <-. exists r. split.
    + apply X_EdgeErr. unfold edge_link_of.
      rewrite (node_of_rel _ g r rho y (si_rel _ _ _ _ _ _ HS)
                 (fun l0 Hl0 => link_of_root rho y l0 (y :: vremove x L0 ++ E0) K (or_introl eq_refl) Hl0)). exact He.
    + simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* SetEdges *)
    simpl in Hc. injection Hc as <-. destruct (sim_set gmax K rho y ys L0 E0 g r d g' HS Hs) as [r' [Hr' S']].
    exists r'. split; [apply X_Set; exact Hr'| exact S'].
  - simpl in Hc. injection Hc as <-. exists r. split.
    + apply X_SetErr. exact (sim_set_err gmax K rho _ y ys g r d HS (or_introl eq_refl) Hs).
    + simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Insert *)
    simpl in Hc. injection Hc as <-.
    destruct (sim_insert gmax K rho x L0 _ g r sid idx d es bs tb g' l ys HS
                ltac:(intros z Hz; in_solve) ltac:(intros z Hz; in_solve) Hes Hins) as [r' [Xr S']].
    exists r'. split; [eapply X_Insert; [exact Hes| exact Xr]| exact S'].
  - simpl in Hc. injection Hc as <-. exists r. split; [apply X_InsertErr; exact Hes|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Delete *)
    simpl in Hc. injection Hc as <-. destruct (sim_delete gmax rights ctx K rho y L0 E0 g r g' HS Hd) as [r' [Dr S']].
    exists r'. split; [apply X_Delete; exact Dr| exact S'].
  - simpl in Hc. injection Hc as <-. exists r. split.
    + apply X_DeleteErr. exact (sim_delete_err gmax rights ctx K rho _ y g r HS Hd).
    + simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* Seq *)
    simpl in Hc. destruct (rcheck b L0 E0) as [Lb|] eqn:Eb; [|discriminate].
    destruct (IHa Lb E0 Lin r Hc HS) as [r1 [Ra S1]]. simpl in S1.
    destruct (IHb L0 E0 Lb r1 Eb S1) as [r2 [Rb S2]].
    exists r2. split; [eapply X_SeqN; [exact Ra| exact Rb]| exact S2].
  - simpl in Hc. destruct (rcheck b L0 E0) as [Lb|] eqn:Eb; [|discriminate].
    destruct (IHa Lb E0 Lin r Hc HS) as [r1 [Ra S1]].
    exists r1. split; [apply X_SeqE; exact Ra| exact S1].
  - (* If *)
    simpl in Hc. destruct (rcheck a L0 E0) as [La|] eqn:Ea; [|discriminate].
    destruct (rcheck b L0 E0) as [Lb|] eqn:Eb; [|discriminate]. injection Hc as <-.
    destruct (IHa L0 E0 La r Ea (siminv_weaken gmax K rho _ La g r HS ltac:(intros z Hz; in_solve))) as [r1 [Ra S1]].
    exists r1. split; [eapply X_IfT; [exact Hc0| exact Ra]| exact S1].
  - simpl in Hc. destruct (rcheck a L0 E0) as [La|] eqn:Ea; [|discriminate].
    destruct (rcheck b L0 E0) as [Lb|] eqn:Eb; [|discriminate]. injection Hc as <-.
    destruct (IHb L0 E0 Lb r Eb (siminv_weaken gmax K rho _ Lb g r HS ltac:(intros z Hz; in_solve))) as [r1 [Rb S1]].
    exists r1. split; [eapply X_IfF; [exact Hc0| exact Rb]| exact S1].
  - simpl in Hc. destruct (rcheck a L0 E0) as [La|]; [|discriminate].
    destruct (rcheck b L0 E0) as [Lb|]; [|discriminate]. injection Hc as <-.
    exists r. split; [apply X_IfErr; exact Hc0|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz; in_solve.
  - (* While: exit *)
    simpl in Hc. destruct (rcheck b h E0) as [Lb|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L0 h && vmem c h && inclb E0 h) eqn:Ei; [|discriminate]. injection Hc as <-.
    repeat rewrite andb_true_iff in Ei. destruct Ei as [[[I1 I2] I3] I4].
    exists r. split; [apply X_WhileF; exact Hc0|]. exact (siminv_weaken _ _ _ _ _ _ _ HS (inclb_spec _ _ I2)).
  - (* While: one more round *)
    assert (Hw0 : rcheck (RWhile c h b) L0 E0 = Some Lin) by exact Hc.
    simpl in Hc. destruct (rcheck b h E0) as [Lb|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L0 h && vmem c h && inclb E0 h) eqn:Ei; [|discriminate]. injection Hc as <-.
    repeat rewrite andb_true_iff in Ei. destruct Ei as [[[I1 I2] I3] I4].
    destruct (IHb h E0 Lb r Eb (siminv_weaken _ _ _ _ _ _ _ HS (inclb_spec _ _ I1))) as [r1 [Rb S1]]. simpl in S1.
    destruct (IHw L0 E0 h r1 Hw0 S1) as [r2 [Rw S2]].
    exists r2. split; [eapply X_WhileT; [exact Hc0| exact Rb| exact Rw]| exact S2].
  - simpl in Hc. destruct (rcheck b h E0) as [Lb|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L0 h && vmem c h && inclb E0 h) eqn:Ei; [|discriminate]. injection Hc as <-.
    repeat rewrite andb_true_iff in Ei. destruct Ei as [[[I1 I2] I3] I4].
    destruct (IHb h E0 Lb r Eb (siminv_weaken _ _ _ _ _ _ _ HS (inclb_spec _ _ I1))) as [r1 [Rb S1]].
    exists r1. split; [eapply X_WhileE; [exact Hc0| exact Rb]| exact S1].
  - simpl in Hc. destruct (rcheck b h E0) as [Lb|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L0 h && vmem c h && inclb E0 h) eqn:Ei; [|discriminate]. injection Hc as <-.
    repeat rewrite andb_true_iff in Ei. destruct Ei as [[[I1 I2] I3] I4].
    exists r. split; [apply X_WhileErr; exact Hc0|]. exact (siminv_weaken _ _ _ _ _ _ _ HS (inclb_spec _ _ I4)).
  - (* Try *)
    simpl in Hc. destruct (rcheck h L0 E0) as [Lh|] eqn:Eh; [|discriminate].
    destruct (IHb L0 Lh Lin r Hc HS) as [r1 [Rb S1]].
    exists r1. split; [apply X_TryN; exact Rb| exact S1].
  - simpl in Hc. destruct (rcheck h L0 E0) as [Lh|] eqn:Eh; [|discriminate].
    destruct (IHb L0 Lh Lin r Hc HS) as [r1 [Rb S1]]. simpl in S1.
    destruct (IHh L0 E0 Lh r1 Eh S1) as [r2 [Rh S2]].
    exists r2. split; [eapply X_TryE; [exact Rb| exact Rh]| exact S2].
  - (* Call, normal return *)
    simpl in Hc. destruct (inclb (vremove x L0) keep && inclb E0 keep) eqn:Ek; [|discriminate]. injection Hc as <-.
    apply andb_true_iff in Ek. destruct Ek as [K1 K2]. apply inclb_spec in K1. apply inclb_spec in K2.
    destruct (call_args_spec funs rho f ys ps body ret vs Ha) as [Ef Ev].
    destruct (funs_ok f ps body ret Ef) as [Lf [Ecb _]].
    destruct (IHb [ret] [] Lf r Ecb (siminv_call_entry gmax K rho ys keep ps vs Lf g r HS Ev)) as [r1 [Rb S1]].
    simpl in S1. exists r1. split; [eapply X_Call; [exact Ha| exact Rb| exact Hr]|].
    apply (siminv_call_exit gmax K rho keep rhof [ret] g1 r1 K (rupd rho x v) L0 S1); [|reflexivity].
    intros l0 Hl0. destruct (vars_after_def rho x v L0 keep l0 K1 Hl0) as [B|B].
    + left. apply vars_links_in. exists ret, v. split; [left; reflexivity| split; assumption].
    + right. exact B.
  - (* Call, no result bound *)
    simpl in Hc. destruct (inclb (vremove x L0) keep && inclb E0 keep) eqn:Ek; [|discriminate]. injection Hc as <-.
    apply andb_true_iff in Ek. destruct Ek as [K1 K2]. apply inclb_spec in K2.
    destruct (call_args_spec funs rho f ys ps body ret vs Ha) as [Ef Ev].
    destruct (funs_ok f ps body ret Ef) as [Lf [Ecb _]].
    destruct (IHb [ret] [] Lf r Ecb (siminv_call_entry gmax K rho ys keep ps vs Lf g r HS Ev)) as [r1 [Rb S1]].
    simpl in S1. exists r1. split; [eapply X_CallNoRet; [exact Ha| exact Rb| exact Hr]|].
    apply (siminv_call_exit gmax K rho keep rhof [ret] g1 r1 K rho E0 S1); [|reflexivity].
    intros l0 Hl0. right. exact (vars_links_incl links_of rho E0 keep K2 l0 Hl0).
  - (* Call, error in the callee *)
    simpl in Hc. destruct (inclb (vremove x L0) keep && inclb E0 keep) eqn:Ek; [|discriminate]. injection Hc as <-.
    apply andb_true_iff in Ek. destruct Ek as [K1 K2]. apply inclb_spec in K2.
    destruct (call_args_spec funs rho f ys ps body ret vs Ha) as [Ef Ev].
    destruct (funs_ok f ps body ret Ef) as [Lf [Ecb _]].
    destruct (IHb [ret] [] Lf r Ecb (siminv_call_entry gmax K rho ys keep ps vs Lf g r HS Ev)) as [r1 [Rb S1]].
    simpl in S1. exists r1. split; [eapply X_CallErr; [exact Ha| exact Rb]|].
    apply (siminv_call_exit gmax K rho keep rhof [] g1 r1 K rho E0 S1); [|reflexivity].
    intros l0 Hl0. right. exact (vars_links_incl links_of rho E0 keep K2 l0 Hl0).
  - (* Call that cannot start *)
    simpl in Hc. destruct (inclb (vremove x L0) keep && inclb E0 keep) eqn:Ek; [|discriminate]. injection Hc as <-.
    apply andb_true_iff in Ek. destruct Ek as [K1 K2]. apply inclb_spec in K2.
    exists r. split; [apply X_CallBad; exact Ha|].
    simpl. apply (siminv_weaken _ _ _ _ _ _ _ HS). intros z Hz. apply in_or_app. right. exact (K2 z Hz).
  - (* Reclaim: the reference skips it, the reclaiming run deletes *)
    simpl in Hc. destruct (inclb L0 R) eqn:Ei; [|discriminate]. injection Hc as <-.
    exists (reclaim_step gmax rights ctx r sid C fuel (reclaim_roots links_of true K rho R)). split.
    + apply X_Reclaim. reflexivity.
    + exact (siminv_reclaim gmax rights ctx K rho L0 R g r sid C fuel HS (inclb_spec _ _ Ei)).
  - discriminate Hreal.
Qed.

(* From any start state that satisfies the issuing invariants. *)
Theorem reclaim_preserves_every_reference_run : forall K rho g s L E Lin rho' g' tr o,
  rcheck s L E = Some Lin -> GInv gmax g -> gbor g = [] ->
  (forall l, In l (live_roots rho Lin K) -> In l (gissued g)) -> EdgesFromIssued g ->
  rexec gmax funs rights ctx links_of true false K rho g s rho' g' tr o ->
  exists r', rexec gmax funs rights ctx links_of true true K rho g s rho' r' tr o /\ SimInv gmax K rho' (out_live o L E) g' r'.
Proof.
  intros K rho g s L E Lin rho' g' tr o Hc Hg Hb Hi He Hx.
  apply (reclaim_simulates K rho g s rho' g' tr o Hx L E Lin g Hc).
  constructor; try assumption. apply gsrel_refl.
Qed.

End Sim.

(* ------------------------------------------------------------------ *)
(* Maintained-count and canonical-ledger admission at a reclaim boundary *)
(* ------------------------------------------------------------------ *)

(* Binding issuance and graph/forest footprint coherence are not manufactured
   here. A binding identifies the canonical root (including its epoch); the
   ledger, not a value or a cached boolean, decides who currently holds it. *)
Definition StoreRootBindings := nat -> option OwnershipTeardown.Link.

Definition ledger_rights (a : OwnershipTeardownAuthority.AuthorityState)
    (bindings : StoreRootBindings) : nat -> option nat :=
  fun sid => match bindings sid with
             | Some root => if OwnershipTeardownAuthority.holds_cleanup a root
                            then Some (OwnershipTeardownAuthority.acting_context a)
                            else None
             | None => None
             end.

Theorem ledger_holds_matches : forall a bindings sid,
  holds (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a) sid =
  match bindings sid with
  | Some root => OwnershipTeardownAuthority.holds_cleanup a root
  | None => false
  end.
Proof.
  intros a bindings sid. unfold holds, ledger_rights.
  destruct (bindings sid) as [root|]; [|reflexivity].
  destruct (OwnershipTeardownAuthority.holds_cleanup a root);
    [apply Nat.eqb_refl|reflexivity].
Qed.

Theorem ledger_grant_admits : forall a after bindings sid r,
  OwnershipTeardownAuthority.create_root a r = OwnershipTeardownAuthority.Accepted after ->
  bindings sid = Some (r, OwnershipTeardown.root_gen (OwnershipTeardownAuthority.forest a) r) ->
  holds (ledger_rights after bindings) (OwnershipTeardownAuthority.acting_context after) sid = true.
Proof.
  intros a after bindings sid r H HB. rewrite ledger_holds_matches, HB.
  exact (proj2 (proj2 (OwnershipTeardownAuthority.root_creation_issues_to_acting_owner a r after H))).
Qed.

Theorem ledger_transfer_revokes : forall a after bindings sid r epoch new_holder,
  OwnershipTeardownAuthority.transfer_cleanup a (r, epoch) new_holder =
    OwnershipTeardownAuthority.Accepted after ->
  new_holder <> OwnershipTeardownAuthority.acting_context a ->
  bindings sid = Some (r, epoch) ->
  holds (ledger_rights after bindings) (OwnershipTeardownAuthority.acting_context after) sid = false.
Proof.
  intros a after bindings sid r epoch new_holder H Hne HB.
  rewrite ledger_holds_matches, HB.
  exact (proj1 (proj2 (proj2
    (OwnershipTeardownAuthority.transfer_moves_not_copies a r epoch new_holder after H))) Hne).
Qed.

Theorem ledger_consumption_revokes : forall a after bindings sid r epoch unit,
  OwnershipTeardownAuthority.retire a (OwnershipTeardownAuthority.RetireRoot (r, epoch)) unit =
    OwnershipTeardownAuthority.Accepted after -> bindings sid = Some (r, epoch) ->
  holds (ledger_rights after bindings) (OwnershipTeardownAuthority.acting_context after) sid = false.
Proof.
  intros a after bindings sid r epoch unit H HB. rewrite ledger_holds_matches, HB.
  apply OwnershipTeardownAuthority.copied_reference_is_not_cleanup_authority.
  rewrite (OwnershipTeardownAuthority.root_retirement_consumes_responsibility a r epoch unit after H).
  discriminate.
Qed.

Definition ledger_counted_reclaim (gmax : nat)
    (a : OwnershipTeardownAuthority.AuthorityState) (bindings : StoreRootBindings)
    (c : CountedGraph) (sid : nat) (C : list nat) (fuel : nat) (roots : list Link) : CountedBatchResult :=
  if holds (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a) sid
  then counted_reclaim gmax c sid C fuel roots
  else CountedBatchRefused c BatchAuthorityDenied.

Theorem ledger_reclaim_denied_is_unchanged : forall gmax a bindings c sid C fuel roots,
  holds (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a) sid = false ->
  ledger_counted_reclaim gmax a bindings c sid C fuel roots =
    CountedBatchRefused c BatchAuthorityDenied.
Proof. intros. unfold ledger_counted_reclaim. rewrite H. reflexivity. Qed.

(* Accepted batches refine the old sequential reference semantics. A refusal
   is not projected onto that runner or turned into its silent no-op branch. *)
Theorem ledger_counted_reclaim_agrees : forall gmax a bindings c sid C fuel roots after,
  CountsExact (counted_counts c) (counted_graph c) ->
  ledger_counted_reclaim gmax a bindings c sid C fuel roots = CountedBatchAccepted after ->
  counted_graph after =
  reclaim_step gmax (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a)
    (counted_graph c) sid C fuel roots.
Proof.
  intros gmax a bindings c sid C fuel roots after HC H.
  unfold ledger_counted_reclaim in H.
  destruct (holds (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a) sid)
    eqn:HA; [|discriminate].
  destruct (counted_reclaim_projects _ _ _ _ _ _ _ HC H) as [s [G [HS [HG E]]]].
  unfold reclaim_step. rewrite HA, HS, HG. exact E.
Qed.

Theorem ledger_counted_reclaim_refused_unchanged : forall gmax a bindings c sid C fuel roots original why,
  ledger_counted_reclaim gmax a bindings c sid C fuel roots = CountedBatchRefused original why ->
  original = c.
Proof.
  intros gmax a bindings c sid C fuel roots original why H. unfold ledger_counted_reclaim in H.
  destruct (holds (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a) sid).
  - eapply counted_reclaim_refused_unchanged; exact H.
  - injection H; auto.
Qed.

Definition checked_ledger_reclaim (gmax : nat)
    (a : OwnershipTeardownAuthority.AuthorityState) (bindings : StoreRootBindings)
    (c : CountedGraph) (K : list Link) (rho : REnv) (L R : list Var)
    (sid : nat) (C : list nat) (fuel : nat) : CountedBatchResult :=
  if inclb L R then ledger_counted_reclaim gmax a bindings c sid C fuel
    (reclaim_roots links_of true K rho R) else CountedBatchRefused c BatchIncompleteRoots.

Theorem incomplete_reclaim_roots_refused : forall gmax a bindings c K rho L R sid C fuel,
  inclb L R = false -> checked_ledger_reclaim gmax a bindings c K rho L R sid C fuel =
    CountedBatchRefused c BatchIncompleteRoots.
Proof. intros. unfold checked_ledger_reclaim. rewrite H. reflexivity. Qed.

Theorem checked_ledger_reclaim_refused_unchanged : forall gmax a bindings c K rho L R sid C fuel original why,
  checked_ledger_reclaim gmax a bindings c K rho L R sid C fuel = CountedBatchRefused original why ->
  original = c.
Proof.
  intros gmax a bindings c K rho L R sid C fuel original why H.
  unfold checked_ledger_reclaim in H. destruct (inclb L R).
  - eapply ledger_counted_reclaim_refused_unchanged; exact H.
  - injection H; auto.
Qed.

(* SimInv is established by the checked language's simulation, not by an
   arbitrary caller choosing L. This is a boundary refinement, not a proof of
   the real MIR producer or a synchronized graph/forest retirement algorithm. *)
Theorem checked_ledger_reclaim_simulates : forall gmax a bindings c c' K rho L R sid C fuel g,
  CountsExact (counted_counts c) (counted_graph c) -> AllEdgesIssued (counted_graph c) ->
  SimInv gmax K rho L g (counted_graph c) ->
  checked_ledger_reclaim gmax a bindings c K rho L R sid C fuel = CountedBatchAccepted c' ->
  SimInv gmax K rho L g (counted_graph c') /\
  CountsExact (counted_counts c') (counted_graph c') /\ AllEdgesIssued (counted_graph c').
Proof.
  intros gmax a bindings c c' K rho L R sid C fuel g HC HE HI H.
  unfold checked_ledger_reclaim in H. destruct (inclb L R) eqn:HL; [|discriminate].
  split.
  - rewrite (ledger_counted_reclaim_agrees gmax a bindings c sid C fuel _ c' HC H).
    exact (siminv_reclaim gmax (ledger_rights a bindings)
      (OwnershipTeardownAuthority.acting_context a) K rho L R g (counted_graph c)
      sid C fuel HI (inclb_spec L R HL)).
  - unfold ledger_counted_reclaim in H.
    destruct (holds (ledger_rights a bindings) (OwnershipTeardownAuthority.acting_context a) sid);
      [|discriminate].
    eapply counted_reclaim_preserves_counts; eassumption.
Qed.

(* ------------------------------------------------------------------ *)
(* Determinism                                                          *)
(* ------------------------------------------------------------------ *)

Theorem rexec_deterministic : forall gmax funs rights ctx lk frames real K rho g s rho1 g1 tr1 o1,
  rexec gmax funs rights ctx lk frames real K rho g s rho1 g1 tr1 o1 ->
  forall rho2 g2 tr2 o2, rexec gmax funs rights ctx lk frames real K rho g s rho2 g2 tr2 o2 ->
  rho1 = rho2 /\ g1 = g2 /\ tr1 = tr2 /\ o1 = o2.
Proof.
  intros gmax funs rights ctx lk frames real K rho g s rho1 g1 tr1 o1 H.
  induction H; intros rho2' g2' tr2' o2' H2; inversion H2; subst;
  repeat match goal with
  | H1 : ?a = Some _, H3 : ?a = Some _ |- _ =>
      rewrite H1 in H3; first [discriminate H3 | injection H3; clear H3; intros; subst]
  | H1 : ?a = Some _, H3 : ?a = None |- _ => rewrite H1 in H3; discriminate H3
  | H1 : ?a = (_, _), H3 : ?a = (_, _) |- _ =>
      rewrite H1 in H3; first [discriminate H3 | injection H3; clear H3; intros; subst]
  | IH : forall _ _ _ _, rexec _ _ _ _ _ _ _ ?K0 ?r0 ?g0 ?s0 _ _ _ _ -> _,
    Hx : rexec _ _ _ _ _ _ _ ?K0 ?r0 ?g0 ?s0 _ _ _ _ |- _ =>
      destruct (IH _ _ _ _ Hx) as [? [? [? ?]]]; clear IH; subst
  end;
  try discriminate; repeat split; reflexivity.
Qed.

Lemma determined_outcome : forall gmax funs rights ctx lk frames real K rho g s rho1 g1 tr1 o1,
  rexec gmax funs rights ctx lk frames real K rho g s rho1 g1 tr1 o1 ->
  forall rho2 g2 tr2 o2, rexec gmax funs rights ctx lk frames real K rho g s rho2 g2 tr2 o2 -> o2 = o1 /\ tr2 = tr1.
Proof.
  intros gmax funs rights ctx lk frames real K rho g s rho1 g1 tr1 o1 H1 rho2 g2 tr2 o2 H2.
  destruct (rexec_deterministic _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ H1 _ _ _ _ H2) as [_ [_ [E1 E2]]].
  split; symmetry; assumption.
Qed.

(* ------------------------------------------------------------------ *)
(* Witnesses and falsifiers                                             *)
(* ------------------------------------------------------------------ *)

(* One store with one node holding data 7. Variable 1 holds its link. *)
Definition ex_g : GS := grun 3 gempty [ONew (0, 0); OInsert 0 0 7 [] [(2, 0)] (1, 0)].
Definition ex_link : Link := mkLink 0 0 0.
Definition ex_node : Node := mkNode 7 [] [(2, 0)].
Definition ex_rho : REnv := rupd rempty 1 (GLnk ex_link).
Definition ex_rights : nat -> option nat := fun _ => Some 0.
Definition ex_nofuns : nat -> option RFun := fun _ => None.

Lemma ex_nofuns_ok : FunsOK ex_nofuns.
Proof. intros f ps body ret H. discriminate. Qed.

Lemma ex_ginv : GInv 3 ex_g.
Proof. apply ginv_run. apply ginv_empty. Qed.

Lemma ex_issued : forall K, K = [] -> forall l, In l (live_roots ex_rho [1] K) -> In l (gissued ex_g).
Proof. intros K -> l Hl. vm_compute in Hl. destruct Hl as [<-|[]]. vm_compute. left. reflexivity. Qed.

Lemma ex_edges : EdgesFromIssued ex_g.
Proof.
  intros s j x e Hs Ej He. vm_compute in Hs. destruct Hs as [<-|[]].
  destruct j as [|[|j]]; vm_compute in Ej; [injection Ej as <-; vm_compute in He; contradiction| discriminate| discriminate].
Qed.

(* A dead link really is reclaimed: the reclaiming run drops the node block
   the reference run keeps. *)
Example dead_node_is_reclaimed :
  rcheck (RReclaim 0 [0] 2 []) [] [] = Some [] /\
  length (gheap ex_g) = 2 /\
  length (gheap (reclaim_step 3 ex_rights 0 ex_g 0 [0] 2 (reclaim_roots links_of true [] ex_rho []))) = 1.
Proof. repeat split; vm_compute; reflexivity. Qed.

(* 1. A link held only inside an aggregate. *)
Definition agg_prog : RStmt :=
  RSeq (RPack 2 0 [1]) (RSeq (RReclaim 0 [0] 2 [2]) (RSeq (RProj 3 2 0) (RRead 4 3))).

Lemma agg_ref_run : exists rho' g',
  rexec 3 ex_nofuns ex_rights 0 links_of true false [] ex_rho ex_g agg_prog rho' g' [7] RNorm.
Proof.
  eexists. eexists. unfold agg_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := [7]); [apply X_Pack; reflexivity|].
  eapply X_SeqN with (tr1 := []) (tr2 := [7]); [apply X_ReclaimRef; reflexivity|].
  eapply X_SeqN with (tr1 := []) (tr2 := [7]); [apply X_Proj; reflexivity|].
  apply (X_Read _ _ _ _ _ _ _ _ _ _ _ _ ex_link ex_node). reflexivity.
Qed.

Lemma agg_shallow_run : exists rho' g',
  rexec 3 ex_nofuns ex_rights 0 top_links true true [] ex_rho ex_g agg_prog rho' g' [] RErr.
Proof.
  eexists. eexists. unfold agg_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Pack; reflexivity|].
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Reclaim; reflexivity|].
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Proj; reflexivity|].
  apply X_ReadErr. vm_compute. reflexivity.
Qed.

Theorem aggregate_only_link_needs_deep_roots :
  rcheck agg_prog [] [] = Some [1] /\
  (exists rho' g', rexec 3 ex_nofuns ex_rights 0 links_of true true [] ex_rho ex_g agg_prog rho' g' [7] RNorm) /\
  (forall rho' g' tr o, rexec 3 ex_nofuns ex_rights 0 top_links true true [] ex_rho ex_g agg_prog rho' g' tr o ->
     o = RErr /\ tr = []).
Proof.
  split; [reflexivity|]. split.
  - destruct agg_ref_run as [rho' [g' Href]].
    destruct (reclaim_preserves_every_reference_run 3 ex_nofuns ex_rights 0 ex_nofuns_ok [] ex_rho ex_g agg_prog
                [] [] [1] rho' g' [7] RNorm eq_refl ex_ginv eq_refl (ex_issued [] eq_refl) ex_edges Href)
      as [r' [Hr' _]].
    exists rho', r'. exact Hr'.
  - intros rho' g' tr o H. destruct agg_shallow_run as [r1 [g1 H1]]. exact (determined_outcome _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ H1 _ _ _ _ H).
Qed.

(* 2. A link held only by a suspended caller. The callee reclaims with no
   local root and returns its numeric argument. *)
Definition frame_body : RStmt := RReclaim 0 [0] 2 [5].
Definition frame_funs : nat -> option RFun := fun f => if Nat.eqb f 0 then Some ([5], frame_body, 5) else None.
Definition frame_prog (keep : list Var) : RStmt := RSeq (RNum 6 0) (RSeq (RCall 7 0 [6] keep) (RRead 8 1)).

Lemma frame_funs_ok : FunsOK frame_funs.
Proof.
  intros f ps body ret H. unfold frame_funs in H. destruct (Nat.eqb f 0); [|discriminate].
  injection H as <- <- <-. exists [5]. split; [reflexivity| intros z Hz; exact Hz].
Qed.

Lemma frame_ref_run : exists rho' g',
  rexec 3 frame_funs ex_rights 0 links_of true false [] ex_rho ex_g (frame_prog [1]) rho' g' [7] RNorm.
Proof.
  eexists. eexists. unfold frame_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := [7]); [apply X_Num|].
  eapply X_SeqN with (tr1 := []) (tr2 := [7]).
  - eapply X_Call with (ps := [5]) (body := frame_body) (ret := 5) (vs := [GNum 0]) (v := GNum 0);
      [reflexivity| apply X_ReclaimRef; reflexivity| reflexivity].
  - apply (X_Read _ _ _ _ _ _ _ _ _ _ _ _ ex_link ex_node). reflexivity.
Qed.

Lemma frame_without_frames_run : exists rho' g',
  rexec 3 frame_funs ex_rights 0 links_of false true [] ex_rho ex_g (frame_prog [1]) rho' g' [] RErr.
Proof.
  eexists. eexists. unfold frame_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Num|].
  eapply X_SeqN with (tr1 := []) (tr2 := []).
  - eapply X_Call with (ps := [5]) (body := frame_body) (ret := 5) (vs := [GNum 0]) (v := GNum 0);
      [reflexivity| apply X_Reclaim; reflexivity| reflexivity].
  - apply X_ReadErr. vm_compute. reflexivity.
Qed.

Lemma frame_wrong_keep_run : exists rho' g',
  rexec 3 frame_funs ex_rights 0 links_of true true [] ex_rho ex_g (frame_prog []) rho' g' [] RErr.
Proof.
  eexists. eexists. unfold frame_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Num|].
  eapply X_SeqN with (tr1 := []) (tr2 := []).
  - eapply X_Call with (ps := [5]) (body := frame_body) (ret := 5) (vs := [GNum 0]) (v := GNum 0);
      [reflexivity| apply X_Reclaim; reflexivity| reflexivity].
  - apply X_ReadErr. vm_compute. reflexivity.
Qed.

Theorem caller_frame_link_needs_frame_roots :
  rcheck (frame_prog [1]) [] [] = Some [1] /\
  (exists rho' g', rexec 3 frame_funs ex_rights 0 links_of true true [] ex_rho ex_g (frame_prog [1]) rho' g' [7] RNorm) /\
  (forall rho' g' tr o, rexec 3 frame_funs ex_rights 0 links_of false true [] ex_rho ex_g (frame_prog [1]) rho' g' tr o ->
     o = RErr /\ tr = []) /\
  rcheck (frame_prog []) [] [] = None /\
  (forall rho' g' tr o, rexec 3 frame_funs ex_rights 0 links_of true true [] ex_rho ex_g (frame_prog []) rho' g' tr o ->
     o = RErr /\ tr = []).
Proof.
  split; [reflexivity|]. split.
  { destruct frame_ref_run as [rho' [g' Href]].
    destruct (reclaim_preserves_every_reference_run 3 frame_funs ex_rights 0 frame_funs_ok [] ex_rho ex_g (frame_prog [1])
                [] [] [1] rho' g' [7] RNorm eq_refl ex_ginv eq_refl (ex_issued [] eq_refl) ex_edges Href)
      as [r' [Hr' _]].
    exists rho', r'. exact Hr'. }
  split.
  { intros rho' g' tr o H. destruct frame_without_frames_run as [r1 [g1 H1]].
    exact (determined_outcome _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ H1 _ _ _ _ H). }
  split; [reflexivity|].
  intros rho' g' tr o H. destruct frame_wrong_keep_run as [r1 [g1 H1]].
  exact (determined_outcome _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ H1 _ _ _ _ H).
Qed.

(* 3. A backing held only by a derived view. *)
Definition view_prog : RStmt := RSeq (RView 2 1) (RSeq (RReclaim 0 [0] 2 [2]) (RRead 3 2)).

Lemma view_ref_run : exists rho' g',
  rexec 3 ex_nofuns ex_rights 0 links_of true false [] ex_rho ex_g view_prog rho' g' [7] RNorm.
Proof.
  eexists. eexists. unfold view_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := [7]); [apply X_View; reflexivity|].
  eapply X_SeqN with (tr1 := []) (tr2 := [7]); [apply X_ReclaimRef; reflexivity|].
  apply (X_Read _ _ _ _ _ _ _ _ _ _ _ _ ex_link ex_node). reflexivity.
Qed.

Lemma view_blind_run : exists rho' g',
  rexec 3 ex_nofuns ex_rights 0 links_no_views true true [] ex_rho ex_g view_prog rho' g' [] RErr.
Proof.
  eexists. eexists. unfold view_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_View; reflexivity|].
  eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Reclaim; reflexivity|].
  apply X_ReadErr. vm_compute. reflexivity.
Qed.

Theorem view_only_backing_needs_view_roots :
  rcheck view_prog [] [] = Some [1] /\
  (exists rho' g', rexec 3 ex_nofuns ex_rights 0 links_of true true [] ex_rho ex_g view_prog rho' g' [7] RNorm) /\
  (forall rho' g' tr o, rexec 3 ex_nofuns ex_rights 0 links_no_views true true [] ex_rho ex_g view_prog rho' g' tr o ->
     o = RErr /\ tr = []).
Proof.
  split; [reflexivity|]. split.
  - destruct view_ref_run as [rho' [g' Href]].
    destruct (reclaim_preserves_every_reference_run 3 ex_nofuns ex_rights 0 ex_nofuns_ok [] ex_rho ex_g view_prog
                [] [] [1] rho' g' [7] RNorm eq_refl ex_ginv eq_refl (ex_issued [] eq_refl) ex_edges Href)
      as [r' [Hr' _]].
    exists rho', r'. exact Hr'.
  - intros rho' g' tr o H. destruct view_blind_run as [r1 [g1 H1]].
    exact (determined_outcome _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ H1 _ _ _ _ H).
Qed.

(* 4. A returned link. The callee copies its argument into its result and
   reclaims before returning. A certificate that treats the result as
   already handed to the caller drops it from the callee's roots. *)
Definition ret_body (R : list Var) : RStmt := RSeq (RCopy 6 5) (RReclaim 0 [0] 2 R).
Definition ret_funs (R : list Var) : nat -> option RFun :=
  fun f => if Nat.eqb f 1 then Some ([5], ret_body R, 6) else None.
Definition ret_prog : RStmt := RSeq (RCall 7 1 [1] []) (RRead 8 7).

Lemma ret_funs_ok : FunsOK (ret_funs [6]).
Proof.
  intros f ps body ret H. unfold ret_funs in H. destruct (Nat.eqb f 1); [|discriminate].
  injection H as <- <- <-. exists [5]. split; [reflexivity| intros z Hz; exact Hz].
Qed.

Lemma ret_ref_run : exists rho' g',
  rexec 3 (ret_funs [6]) ex_rights 0 links_of true false [] ex_rho ex_g ret_prog rho' g' [7] RNorm.
Proof.
  eexists. eexists. unfold ret_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := [7]).
  - eapply X_Call with (ps := [5]) (body := ret_body [6]) (ret := 6) (vs := [GLnk ex_link]) (v := GLnk ex_link);
      [reflexivity| eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Copy; reflexivity| apply X_ReclaimRef; reflexivity]
      | reflexivity].
  - apply (X_Read _ _ _ _ _ _ _ _ _ _ _ _ ex_link ex_node). reflexivity.
Qed.

Lemma ret_dropped_run : exists rho' g',
  rexec 3 (ret_funs []) ex_rights 0 links_of true true [] ex_rho ex_g ret_prog rho' g' [] RErr.
Proof.
  eexists. eexists. unfold ret_prog.
  eapply X_SeqN with (tr1 := []) (tr2 := []).
  - eapply X_Call with (ps := [5]) (body := ret_body []) (ret := 6) (vs := [GLnk ex_link]) (v := GLnk ex_link);
      [reflexivity| eapply X_SeqN with (tr1 := []) (tr2 := []); [apply X_Copy; reflexivity| apply X_Reclaim; reflexivity]
      | reflexivity].
  - apply X_ReadErr. vm_compute. reflexivity.
Qed.

Theorem returned_link_must_stay_rooted :
  rcheck ret_prog [] [] = Some [1] /\
  (exists rho' g', rexec 3 (ret_funs [6]) ex_rights 0 links_of true true [] ex_rho ex_g ret_prog rho' g' [7] RNorm) /\
  rcheck (ret_body []) [6] [] = None /\
  (forall rho' g' tr o, rexec 3 (ret_funs []) ex_rights 0 links_of true true [] ex_rho ex_g ret_prog rho' g' tr o ->
     o = RErr /\ tr = []).
Proof.
  split; [reflexivity|]. split.
  { destruct ret_ref_run as [rho' [g' Href]].
    destruct (reclaim_preserves_every_reference_run 3 (ret_funs [6]) ex_rights 0 ret_funs_ok [] ex_rho ex_g ret_prog
                [] [] [1] rho' g' [7] RNorm eq_refl ex_ginv eq_refl (ex_issued [] eq_refl) ex_edges Href)
      as [r' [Hr' _]].
    exists rho', r'. exact Hr'. }
  split; [reflexivity|].
  intros rho' g' tr o H. destruct ret_dropped_run as [r1 [g1 H1]].
  exact (determined_outcome _ _ _ _ _ _ _ _ _ _ _ _ _ _ _ H1 _ _ _ _ H).
Qed.

(* Without the store's cleanup right nothing is reclaimed, even with a dead
   node and a candidate naming it. *)
Example reclaim_needs_the_right :
  reclaim_step 3 (fun _ => Some 1) 0 ex_g 0 [0] 2 [] = ex_g.
Proof. reflexivity. Qed.
