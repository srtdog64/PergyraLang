(*
  OwnershipCleanCore.v  --  compiler-owned ownership cleanup: the compiler,
  not the programmer, decides move/copy and inserts every release, and the
  result is memory safe, garbage free, and observably equal to plain value
  semantics.

  Companion to docs/semantics/27_ownership_clean.md. The language direction
  (AGENTS.md, 2026-10-08) is compiler-owned ownership cleanup rather than a
  tracing GC, manual deep drop, field carrier/restoration choreography, or
  more own/ref annotations. This file fixes the algorithm and proves it.

  Source language. Programs are written with value semantics only: every
  read of a variable observes an independent value, nothing is ever freed,
  and there is no move, drop, own, or ref in the syntax.

    SDef x f ys     x := f(values of ys)          fresh computed value
    SCopy x y       x := y                         value copy
    SPack x ys      x := Node [ys...]              aggregate construction
    SPush x y       x := x ++ [y]                  element push
    SField x y i    x := child i of y              field/element read
    SEmit y         observe y
    SCall x g ys    x := g(ys)                     function call
    SCallIO g z ys  g(inout z, ys)                 value-result call
    SFocus t y p s  focus t on y.p { s }           work on a part in place
    SSeq, SIf c, SWhile c head body               structured control

  Target language. Values own lists of heap blocks, one block per node of
  the value in preorder; an allocation step creates all of them at once. A
  frame has OWNED
  bindings and BORROWED bindings; a borrowed binding can be read but never
  moved, mutated, or freed, and its blocks belong to an enclosing frame (the
  frame heap R). The machine keeps an explicit live-block heap, and every
  unsafe step is refused (stuck): reading or freeing a block that is not
  live, freeing a block twice, freeing through a borrow, binding over a live
  owner, or allocating a block that is still live.

  The algorithm [elab]. Given the set L of variables live after a statement
  (a backward liveness pass) and the set B of borrowed variables (the
  enclosing function's readonly parameters), it rewrites the statement:

    - a use of y that is still needed later (y in L) or that is borrowed
      (y in B) is a COPY; otherwise it is a MOVE (the source is unbound);
    - Pack/Push take moved elements; a kept element is first copied into a
      compiler temporary [Tmp y] and the temporary is moved;
    - after the statement, every variable that was live before but is dead
      after is released ([settle]): an owned one is DROPPED at its last use,
      a borrowed one only ENDS (no heap effect);
    - at a branch, each arm first releases what that arm no longer needs;
    - a loop carries a head live set [head] as a certificate; [elab] only
      checks it, so the compiler's liveness solver is untrusted;
    - a call follows the callee's parameter modes [M]. A BORROWED parameter
      is lent: the callee reads it and copies out of it but cannot release
      caller storage. A SINK parameter is consumed: the caller moves the
      argument into the callee frame at its last use, or first copies it
      into a temporary if it is still needed. The result's ownership returns
      to the caller. An inout call is a call whose first sink argument is
      also its result: the argument moves in and the updated value moves
      back, with no copy.

  Places. A focus moves the part at a path out of an owned root into a
  temporary, with exactly that part's blocks, runs a body on it while the
  rest of the root is suspended, and moves the result back. It covers an
  inout call on a member path, an update of a part from itself, and a
  read-only view of a part, all without a copy. The body must not touch the
  root; [elab] refuses it otherwise.

  Dead aggregates and regions. An unpack moves each part of a record that
  is dead afterwards into its own variable and frees only the record's own
  node; a region keeps its value live to its end and releases it there.

  Parameter modes. [elab] is sound for every mode table, so the table is a
  certificate, like the loop head. A table entry is a resolved summary or
  none: a call through a missing summary, or one of the wrong length, is
  refused and never read as borrowed. [infer_modes] marks a parameter sink
  when the body stores, moves, mutates, redefines, or returns it, or passes
  it on to a sink parameter. That inference is outside the trusted base,
  and its rounds ascend from [no_summaries] ([infer_ascends]).

  Theorems.
    [elab_sound]          for every mode table, and every function table
                          whose bodies elaborate under it, the elaborated
                          program runs without any refused
                          step, emits exactly the source trace, and at every
                          statement boundary the bound variables are exactly
                          the live ones, with the source's contents.
    [heap_is_live_footprint]  at every boundary the live heap is a
                          permutation of the live owners' footprints plus the
                          frame heap: no block is shared and none is unowned.
    [closed_program_frees_everything]  a closed program ends with an empty
                          heap and empty environments: no leak.
    [elab_copy_moves_dead_source] / [elab_copy_copies_live_source] /
    [elab_copy_copies_borrowed_source]  the move/copy decision table.
    [settle_tail], [elab_def_releases_operands],
    [elab_call_releases_operands]  the releases after a statement depend
                          only on the operands it mentions, so an
                          implementation need not walk the live set.

  Refutations (why each rule is needed).
    [alias_copy_double_free]   a shallow alias copy (the current copy-out
                          aliasing, C2) makes the two automatic drops free
                          one block twice: refused.
    [early_drop_use_after_free]  dropping before the last use leaves the use
                          stuck: the drop point must follow liveness.
    [borrowed_drop_refused]    a callee cannot free a borrowed parameter.
    [missing_settle_leaks]     elaborating without the releases ends with a
                          live block that no variable owns.

  Witnesses. [gui_program_runs_clean] and [gui_calls_sink_runs_clean] run
  the Array<GuiDraw> + Label(String) shape that blocked the GUI work, inline
  and through a constructor function plus an inout append procedure. With
  inferred modes, the call version copies nothing
  ([gui_calls_sink_moves_only]); with borrow-only modes it copies twice
  ([gui_calls_copy_twice]). [gui_calls_sink_reuse_copies_once] shows that a
  caller that still needs the argument pays one copy, at the call site.
  [owned_update_needs_sink] shows that an owned functional update,
  Append(xs, y) returning xs, is refused with borrow-only modes and runs
  with zero copies once its parameters are inferred sink.
  [gui_state_runs_clean] does the same through a GuiState record with an
  inout call on its member path state.draws: no copy and no field copy
  ([gui_state_focus_copies_nothing]), where detaching and restoring the
  field costs one field copy ([gui_state_detach_copies_the_field]).
  [gui_state_update_copies_nothing] updates state.draws from itself through
  a sink Append; [projection_view_copies_nothing] reads a part in place;
  [pack_child_segment] shows that a focus moves exactly the blocks the
  part had before it was packed.

  Negative scope. Big-step semantics: divergent runs are not covered.
  Exits (break, continue, return, throw, try) are the layer in
  OwnershipCleanExits.v, not statements of this file. A part of a borrowed
  root read into a variable (SField) is still a copy; focus and unpack need
  an owned root. A drop has no observable effect. The theorems take the
  source execution as a premise and check no types. A call that passes one
  variable to two sink parameters is refused; an implementation binds a
  temporary first. Slots, async, FFI, and the physical allocator are not
  modelled (docs/semantics/27_ownership_clean.md section 4). This file is a
  model of the algorithm; it does not prove that the compiler implements
  it.
*)

Require Import Stdlib.Lists.List.
Require Import Stdlib.Arith.PeanoNat.
Require Import Stdlib.Bool.Bool.
Require Import Stdlib.Sorting.Permutation.
Require Import Stdlib.micromega.Lia.
Import ListNotations.

Definition Var := nat.
(* A block is (allocation step, node index). One allocation step creates one
   block per node of the value it builds, so a footprint has exactly one
   block per node, in preorder; a field's blocks are found by its path. *)
Definition Block := (nat * nat)%type.
Definition FName := nat.

(* ------------------------------------------------------------------ *)
(* Values and the source language (pure value semantics)               *)
(* ------------------------------------------------------------------ *)

Inductive SVal : Type :=
| SLeaf (k : nat)
| SNode (cs : list SVal).

(* Number of nodes of a value. *)
Fixpoint vsize (v : SVal) : nat :=
  match v with
  | SLeaf _ => 1
  | SNode cs => S (list_sum (map vsize cs))
  end.

(* Places. A path is a list of child indices. [vget] reads the part at a
   path, [vset] replaces it, and [voff] is the part's offset in the
   preorder footprint: a node's own block comes first, then each child's
   blocks in order. *)
Fixpoint vget (v : SVal) (p : list nat) : option SVal :=
  match p with
  | [] => Some v
  | i :: q =>
      match v with
      | SLeaf _ => None
      | SNode cs => match nth_error cs i with Some c => vget c q | None => None end
      end
  end.

Fixpoint replace_nth {A : Type} (l : list A) (i : nat) (x : A) : list A :=
  match l, i with
  | [], _ => []
  | _ :: r, 0 => x :: r
  | a :: r, S j => a :: replace_nth r j x
  end.

Fixpoint vset (v : SVal) (p : list nat) (w : SVal) : option SVal :=
  match p with
  | [] => Some w
  | i :: q =>
      match v with
      | SLeaf _ => None
      | SNode cs =>
          match nth_error cs i with
          | Some c => match vset c q w with
                      | Some c' => Some (SNode (replace_nth cs i c'))
                      | None => None
                      end
          | None => None
          end
      end
  end.

Fixpoint voff (v : SVal) (p : list nat) : option nat :=
  match p with
  | [] => Some 0
  | i :: q =>
      match v with
      | SLeaf _ => None
      | SNode cs =>
          match nth_error cs i with
          | Some c => match voff c q with
                      | Some o => Some (1 + list_sum (map vsize (firstn i cs)) + o)
                      | None => None
                      end
          | None => None
          end
      end
  end.

Definition truthy (v : SVal) : bool :=
  match v with
  | SLeaf k => negb (Nat.eqb k 0)
  | SNode _ => true
  end.

Inductive SStmt : Type :=
| SSkip
| SDef (x : Var) (f : list SVal -> SVal) (ys : list Var)
| SCopy (x y : Var)
| SPack (x : Var) (ys : list Var)
| SPush (x y : Var)
| SField (x y : Var) (i : nat)
| SEmit (y : Var)
| SSeq (s1 s2 : SStmt)
| SIf (c : Var) (s1 s2 : SStmt)
| SWhile (c : Var) (head : list Var) (body : SStmt)
| SCall (x : Var) (g : FName) (ys : list Var)
| SCallIO (g : FName) (z : Var) (ys : list Var)
| SFocus (t y : Var) (p : list nat) (s : SStmt)
| SUnpack (y : Var) (xs : list Var)
| SRegion (x : Var) (f : list SVal -> SVal) (ys : list Var) (s : SStmt).

(* A function: parameters, body, result variable. A procedure: the inout
   parameter, the readonly parameters, body. *)
Definition SFunTable := FName -> option (list Var * SStmt * Var).
Definition SProcTable := FName -> option (Var * list Var * SStmt).

Definition SEnv := Var -> option SVal.

Definition sempty : SEnv := fun _ => None.

Definition supd (sg : SEnv) (x : Var) (v : SVal) : SEnv :=
  fun z => if Nat.eq_dec x z then Some v else sg z.

Fixpoint sbind (ps : list Var) (vs : list SVal) : SEnv :=
  match ps, vs with
  | p :: ps', v :: vs' => supd (sbind ps' vs') p v
  | _, _ => sempty
  end.

Fixpoint supd_list (sg : SEnv) (xs : list Var) (cs : list SVal) : SEnv :=
  match xs, cs with
  | x :: xr, c :: cr => supd (supd_list sg xr cr) x c
  | _, _ => sg
  end.

Fixpoint slookup_all (sg : SEnv) (ys : list Var) : option (list SVal) :=
  match ys with
  | [] => Some []
  | y :: r =>
      match sg y, slookup_all sg r with
      | Some v, Some vs => Some (v :: vs)
      | _, _ => None
      end
  end.

Section SourceSemantics.
Variable funs : SFunTable.
Variable procs : SProcTable.

Inductive sexec : SEnv -> SStmt -> SEnv -> list SVal -> Prop :=
| SE_Skip : forall sg, sexec sg SSkip sg []
| SE_Def : forall sg x f ys vs,
    slookup_all sg ys = Some vs ->
    sexec sg (SDef x f ys) (supd sg x (f vs)) []
| SE_Copy : forall sg x y v,
    sg y = Some v ->
    sexec sg (SCopy x y) (supd sg x v) []
| SE_Pack : forall sg x ys vs,
    slookup_all sg ys = Some vs ->
    sexec sg (SPack x ys) (supd sg x (SNode vs)) []
| SE_Push : forall sg x y cs v,
    sg x = Some (SNode cs) -> sg y = Some v ->
    sexec sg (SPush x y) (supd sg x (SNode (cs ++ [v]))) []
| SE_Field : forall sg x y i cs v,
    sg y = Some (SNode cs) -> nth_error cs i = Some v ->
    sexec sg (SField x y i) (supd sg x v) []
| SE_Emit : forall sg y v,
    sg y = Some v ->
    sexec sg (SEmit y) sg [v]
| SE_Seq : forall sg1 sg2 sg3 s1 s2 tr1 tr2 tr,
    sexec sg1 s1 sg2 tr1 -> sexec sg2 s2 sg3 tr2 -> tr = tr1 ++ tr2 ->
    sexec sg1 (SSeq s1 s2) sg3 tr
| SE_IfT : forall sg sg' c s1 s2 v tr,
    sg c = Some v -> truthy v = true -> sexec sg s1 sg' tr ->
    sexec sg (SIf c s1 s2) sg' tr
| SE_IfF : forall sg sg' c s1 s2 v tr,
    sg c = Some v -> truthy v = false -> sexec sg s2 sg' tr ->
    sexec sg (SIf c s1 s2) sg' tr
| SE_WhileF : forall sg c h b v,
    sg c = Some v -> truthy v = false ->
    sexec sg (SWhile c h b) sg []
| SE_WhileT : forall sg sg1 sg2 c h b v tr1 tr2 tr,
    sg c = Some v -> truthy v = true ->
    sexec sg b sg1 tr1 -> sexec sg1 (SWhile c h b) sg2 tr2 ->
    tr = tr1 ++ tr2 ->
    sexec sg (SWhile c h b) sg2 tr
| SE_Call : forall sg x g ys vs ps body ret sgc tr v,
    funs g = Some (ps, body, ret) ->
    slookup_all sg ys = Some vs -> length ps = length vs ->
    sexec (sbind ps vs) body sgc tr -> sgc ret = Some v ->
    sexec sg (SCall x g ys) (supd sg x v) tr
| SE_CallIO : forall sg g z ys vz vs io ps body sgc tr v,
    procs g = Some (io, ps, body) ->
    sg z = Some vz -> slookup_all sg ys = Some vs -> length ps = length vs ->
    sexec (supd (sbind ps vs) io vz) body sgc tr -> sgc io = Some v ->
    sexec sg (SCallIO g z ys) (supd sg z v) tr
(* focus t on y.p { s }: t starts as the part at p, s runs, and the part
   is replaced by t's final value. *)
| SE_Focus : forall sg t y p s c cv sg' tr cv' c',
    sg y = Some c -> vget c p = Some cv ->
    sexec (supd sg t cv) s sg' tr -> sg' t = Some cv' -> vset c p cv' = Some c' ->
    sexec sg (SFocus t y p s) (supd sg' y c') tr
(* unpack y into xs: the parts of the record y become the variables xs. *)
| SE_Unpack : forall sg y xs cs,
    sg y = Some (SNode cs) -> length xs = length cs ->
    sexec sg (SUnpack y xs) (supd_list sg xs cs) []
(* region x := f(ys) { s }: in value semantics a region is transparent. *)
| SE_Region : forall sg x f ys s vs sg' tr,
    slookup_all sg ys = Some vs -> sexec (supd sg x (f vs)) s sg' tr ->
    sexec sg (SRegion x f ys s) sg' tr.

End SourceSemantics.

(* ------------------------------------------------------------------ *)
(* The ownership machine                                                *)
(* ------------------------------------------------------------------ *)

(* Source variables and compiler temporaries share one namespace. *)
Inductive TVar : Type :=
| Src (x : Var)
| Tmp (x : Var).

Definition tvar_eq_dec : forall a b : TVar, {a = b} + {a <> b}.
Proof. decide equality; apply Nat.eq_dec. Defined.

(* A runtime value: its observable content and the blocks it owns. *)
Definition TValue := (SVal * list Block)%type.
Definition TEnv := list (TVar * TValue).

Fixpoint tlookup (z : TVar) (rho : TEnv) : option TValue :=
  match rho with
  | [] => None
  | (k, v) :: r => if tvar_eq_dec k z then Some v else tlookup z r
  end.

Fixpoint tremove (x : TVar) (rho : TEnv) : TEnv :=
  match rho with
  | [] => []
  | (k, v) :: r => if tvar_eq_dec k x then tremove x r else (k, v) :: tremove x r
  end.

Fixpoint tremove_all (ys : list TVar) (rho : TEnv) : TEnv :=
  match ys with
  | [] => rho
  | y :: r => tremove_all r (tremove y rho)
  end.

Fixpoint tlookup_all (ys : list TVar) (rho : TEnv) : option (list TValue) :=
  match ys with
  | [] => Some []
  | y :: r =>
      match tlookup y rho, tlookup_all r rho with
      | Some v, Some vs => Some (v :: vs)
      | _, _ => None
      end
  end.

(* A read sees owned bindings first, then borrowed ones. *)
Definition tread (z : TVar) (rho beta : TEnv) : option TValue :=
  match tlookup z rho with
  | Some v => Some v
  | None => tlookup z beta
  end.

Fixpoint tread_all (ys : list TVar) (rho beta : TEnv) : option (list TValue) :=
  match ys with
  | [] => Some []
  | y :: r =>
      match tread y rho beta, tread_all r rho beta with
      | Some v, Some vs => Some (v :: vs)
      | _, _ => None
      end
  end.

Fixpoint tbind (ps : list Var) (vs : list TValue) : TEnv :=
  match ps, vs with
  | p :: ps', v :: vs' => (Src p, v) :: tbind ps' vs'
  | _, _ => []
  end.

Definition unbound (z : TVar) (rho beta : TEnv) : Prop :=
  tlookup z rho = None /\ tlookup z beta = None.

Definition dom (rho : TEnv) : list TVar := map fst rho.
Definition heap_of (rho : TEnv) : list Block := flat_map (fun p => snd (snd p)) rho.

Definition beqb (a b : Block) : bool := Nat.eqb (fst a) (fst b) && Nat.eqb (snd a) (snd b).
Definition bmem (b : Block) (bs : list Block) : bool := existsb (beqb b) bs.
Definition free (bs H : list Block) : list Block :=
  filter (fun b => negb (bmem b bs)) H.

(* The k fresh blocks of allocation step n, and freshness of step n. *)
Definition alloc (n k : nat) : list Block := map (fun j => (n, j)) (seq 0 k).

(* Split a footprint into consecutive pieces of the given sizes. *)
Fixpoint chunks (sizes : list nat) (bs : list Block) : list (list Block) :=
  match sizes with
  | [] => []
  | k :: r => firstn k bs :: chunks r (skipn k bs)
  end.
Definition fresh (n : nat) (H : list Block) : Prop := forall b, In b H -> fst b <> n.

(* A call targets the function table or the procedure table. *)
Inductive CallKind : Type := KFun | KProc.

Inductive TStmt : Type :=
| TSkip
| TDef (x : TVar) (f : list SVal -> SVal) (ys : list TVar)
| TCopy (x y : TVar)
| TMove (x y : TVar)
| TPack (x : TVar) (ys : list TVar)
| TPush (x y : TVar)
| TField (x y : TVar) (i : nat)
| TEmit (y : TVar)
| TDrop (x : TVar)
| TEnd (x : TVar)
| TSeq (t1 t2 : TStmt)
| TIf (c : TVar) (t1 t2 : TStmt)
| TWhile (c : TVar) (body : TStmt)
| TCall (k : CallKind) (x : TVar) (g : FName) (sys bys : list TVar)
| TFocus (t y : TVar) (p : list nat) (body : TStmt)
| TUnpack (y : TVar) (xs : list Var).

(* A compiled routine: its sink (owned) parameters, its borrowed
   parameters, its body, and its result variable. A procedure's inout
   parameter is its first sink parameter and also its result. *)
Definition TRoutine := (list Var * list Var * TStmt * Var)%type.
Definition TTable := FName -> option TRoutine.

Section TargetSemantics.
Variable tfuns : TTable.
Variable tprocs : TTable.

Definition troutine (k : CallKind) (g : FName) : option TRoutine :=
  match k with KFun => tfuns g | KProc => tprocs g end.

(* Every premise below is a runtime safety check. A step whose check fails
   has no rule: the machine is stuck. Only OWNED bindings can be moved,
   pushed into, packed, or dropped; a borrowed binding can only be read or
   ended. *)
Inductive texec : TEnv -> TEnv -> list Block -> nat -> TStmt ->
                  TEnv -> TEnv -> list Block -> nat -> list SVal -> Prop :=
| TE_Skip : forall rho beta H n, texec rho beta H n TSkip rho beta H n []
| TE_Def : forall rho beta H n x f ys vs,
    unbound x rho beta ->
    tread_all ys rho beta = Some vs ->
    Forall (fun v => incl (snd v) H) vs ->
    fresh n H ->
    texec rho beta H n (TDef x f ys)
          ((x, (f (map fst vs), alloc n (vsize (f (map fst vs))))) :: rho) beta
          (alloc n (vsize (f (map fst vs))) ++ H) (S n) []
| TE_Copy : forall rho beta H n x y c bs,
    unbound x rho beta -> tread y rho beta = Some (c, bs) ->
    incl bs H -> fresh n H ->
    texec rho beta H n (TCopy x y) ((x, (c, alloc n (vsize c))) :: rho) beta
          (alloc n (vsize c) ++ H) (S n) []
| TE_Move : forall rho beta H n x y v,
    unbound x rho beta -> tlookup y rho = Some v ->
    texec rho beta H n (TMove x y) ((x, v) :: tremove y rho) beta H n []
| TE_Pack : forall rho beta H n x ys vs,
    unbound x rho beta -> NoDup ys -> tlookup_all ys rho = Some vs ->
    fresh n H ->
    texec rho beta H n (TPack x ys)
          ((x, (SNode (map fst vs), (n, 0) :: flat_map snd vs)) :: tremove_all ys rho)
          beta ((n, 0) :: H) (S n) []
| TE_Push : forall rho beta H n x y cs bx v,
    x <> y -> tlookup x rho = Some (SNode cs, bx) -> tlookup y rho = Some v ->
    texec rho beta H n (TPush x y)
          ((x, (SNode (cs ++ [fst v]), bx ++ snd v)) :: tremove x (tremove y rho))
          beta H n []
| TE_Field : forall rho beta H n x y i cs bsy v,
    unbound x rho beta -> tread y rho beta = Some (SNode cs, bsy) ->
    incl bsy H -> nth_error cs i = Some v -> fresh n H ->
    texec rho beta H n (TField x y i) ((x, (v, alloc n (vsize v))) :: rho) beta
          (alloc n (vsize v) ++ H) (S n) []
| TE_Emit : forall rho beta H n y c bs,
    tread y rho beta = Some (c, bs) -> incl bs H ->
    texec rho beta H n (TEmit y) rho beta H n [c]
| TE_Drop : forall rho beta H n x c bs,
    tlookup x rho = Some (c, bs) -> incl bs H -> NoDup bs ->
    texec rho beta H n (TDrop x) (tremove x rho) beta (free bs H) n []
| TE_End : forall rho beta H n x v,
    tlookup x beta = Some v ->
    texec rho beta H n (TEnd x) rho (tremove x beta) H n []
| TE_Seq : forall rho1 beta1 H1 n1 rho2 beta2 H2 n2 rho3 beta3 H3 n3 t1 t2 tr1 tr2 tr,
    texec rho1 beta1 H1 n1 t1 rho2 beta2 H2 n2 tr1 ->
    texec rho2 beta2 H2 n2 t2 rho3 beta3 H3 n3 tr2 -> tr = tr1 ++ tr2 ->
    texec rho1 beta1 H1 n1 (TSeq t1 t2) rho3 beta3 H3 n3 tr
| TE_IfT : forall rho beta H n rho' beta' H' n' c t1 t2 cv bs tr,
    tread c rho beta = Some (cv, bs) -> incl bs H -> truthy cv = true ->
    texec rho beta H n t1 rho' beta' H' n' tr ->
    texec rho beta H n (TIf c t1 t2) rho' beta' H' n' tr
| TE_IfF : forall rho beta H n rho' beta' H' n' c t1 t2 cv bs tr,
    tread c rho beta = Some (cv, bs) -> incl bs H -> truthy cv = false ->
    texec rho beta H n t2 rho' beta' H' n' tr ->
    texec rho beta H n (TIf c t1 t2) rho' beta' H' n' tr
| TE_WhileF : forall rho beta H n c b cv bs,
    tread c rho beta = Some (cv, bs) -> incl bs H -> truthy cv = false ->
    texec rho beta H n (TWhile c b) rho beta H n []
| TE_WhileT : forall rho beta H n rho1 beta1 H1 n1 rho2 beta2 H2 n2 c b cv bs tr1 tr2 tr,
    tread c rho beta = Some (cv, bs) -> incl bs H -> truthy cv = true ->
    texec rho beta H n b rho1 beta1 H1 n1 tr1 ->
    texec rho1 beta1 H1 n1 (TWhile c b) rho2 beta2 H2 n2 tr2 -> tr = tr1 ++ tr2 ->
    texec rho beta H n (TWhile c b) rho2 beta2 H2 n2 tr
(* A call MOVES its sink arguments into the callee frame, where they are
   owned, LENDS its borrowed arguments, and binds the result. The result
   variable must be free once the sink arguments have moved out; an inout
   procedure call is therefore a call whose first sink argument is also the
   result. *)
| TE_Call : forall rho beta H n k x g sys bys svs bvs sps bps tb ret v H' n' tr,
    NoDup sys -> tlookup_all sys rho = Some svs ->
    unbound x (tremove_all sys rho) beta ->
    tread_all bys (tremove_all sys rho) beta = Some bvs ->
    Forall (fun v => incl (snd v) H) bvs ->
    troutine k g = Some (sps, bps, tb, ret) ->
    length sps = length svs -> length bps = length bvs ->
    texec (tbind sps svs) (tbind bps bvs) H n tb [(Src ret, v)] [] H' n' tr ->
    texec rho beta H n (TCall k x g sys bys) ((x, v) :: tremove_all sys rho) beta H' n' tr
(* A focus MOVES the part at path p out of the owned root y into t, with
   exactly that part's blocks; the rest of y is suspended and unreachable
   while the body runs; afterwards t's final value moves back into the
   same place. No block is copied. *)
| TE_Focus : forall rho beta H n t y p body c bs cv off rho1 beta1 H1 n1 tr cv' bf' c',
    tlookup y rho = Some (c, bs) -> vget c p = Some cv -> voff c p = Some off ->
    unbound t (tremove y rho) beta ->
    texec ((t, (cv, firstn (vsize cv) (skipn off bs))) :: tremove y rho) beta H n body
          rho1 beta1 H1 n1 tr ->
    tlookup t rho1 = Some (cv', bf') -> unbound y (tremove t rho1) beta1 ->
    vset c p cv' = Some c' ->
    texec rho beta H n (TFocus t y p body)
          ((y, (c', firstn off bs ++ bf' ++ skipn (off + vsize cv) bs)) :: tremove t rho1)
          beta1 H1 n1 tr
(* Unpack MOVES each part of an owned record y into its own variable with
   exactly its blocks and releases only y's own node block. *)
| TE_Unpack : forall rho beta H n y xs cs b0 rest,
    tlookup y rho = Some (SNode cs, b0 :: rest) -> length xs = length cs -> NoDup xs ->
    (forall x, In x xs -> unbound (Src x) (tremove y rho) beta) ->
    texec rho beta H n (TUnpack y xs)
          (tbind xs (combine cs (chunks (map vsize cs) rest)) ++ tremove y rho) beta
          (free [b0] H) n [].

End TargetSemantics.

(* ------------------------------------------------------------------ *)
(* The cleanup elaboration                                              *)
(* ------------------------------------------------------------------ *)

Definition vmem (x : Var) (L : list Var) : bool :=
  if in_dec Nat.eq_dec x L then true else false.
Definition vremove (x : Var) (L : list Var) : list Var :=
  filter (fun z => negb (Nat.eqb z x)) L.
Definition vdiff (A B : list Var) : list Var :=
  filter (fun z => negb (vmem z B)) A.

Fixpoint nodupb (ys : list Var) : bool :=
  match ys with
  | [] => true
  | y :: r => negb (vmem y r) && nodupb r
  end.

Definition inclb (A B : list Var) : bool := forallb (fun z => vmem z B) A.

(* A use must copy when the value is still needed or is only borrowed. *)
Definition keep (L B : list Var) (y : Var) : bool := vmem y L || vmem y B.

Fixpoint drops (vs B : list Var) : TStmt :=
  match vs with
  | [] => TSkip
  | v :: r =>
      TSeq (if in_dec Nat.eq_dec v B then TEnd (Src v) else TDrop (Src v)) (drops r B)
  end.

(* Release everything live in A that is no longer live in L. *)
Definition settle (A L B : list Var) : TStmt :=
  drops (nodup Nat.eq_dec (vdiff A L)) B.

Fixpoint copies (ws : list Var) : TStmt :=
  match ws with
  | [] => TSkip
  | w :: r => TSeq (TCopy (Tmp w) (Src w)) (copies r)
  end.

Definition pack_arg (L B : list Var) (y : Var) : TVar :=
  if keep L B y then Tmp y else Src y.

(* Parameter summaries. [Some ms] is a resolved summary: [true] marks a
   sink (consumed) parameter and [false] a verified borrowed one, one entry
   per parameter. [None] is an unresolved summary; it never stands for
   "borrowed", and a call through it is refused. [pmodes] covers a
   procedure's readonly parameters; its inout parameter is always sink. *)
Record Modes : Type :=
  { fmodes : FName -> option (list bool); pmodes : FName -> option (list bool) }.

(* No routine has a summary yet: every call is refused. Programs without
   calls elaborate under it, and mode inference starts from it. *)
Definition no_summaries : Modes := {| fmodes := fun _ => None; pmodes := fun _ => None |}.

Definition kmodes (M : Modes) (k : CallKind) (g : FName) : option (list bool) :=
  match k with KFun => fmodes M g | KProc => option_map (cons true) (pmodes M g) end.

Fixpoint sinks {A : Type} (ms : list bool) (l : list A) : list A :=
  match l with
  | [] => []
  | a :: r => if hd false ms then a :: sinks (tl ms) r else sinks (tl ms) r
  end.

Fixpoint borrows {A : Type} (ms : list bool) (l : list A) : list A :=
  match l with
  | [] => []
  | a :: r => if hd false ms then borrows (tl ms) r else a :: borrows (tl ms) r
  end.

(* A call [x := g(args)]. A sink argument that stays live across the call
   (read later, also lent to this call, or borrowed by the caller) is first
   copied into a temporary; every other sink argument moves. For an inout
   call, [x] is the first argument, and it always moves. *)
Definition elab_call (M : Modes) (k : CallKind) (x : Var) (g : FName)
    (args L B : list Var) : option (TStmt * list Var) :=
  match kmodes M k g with
  | None => None
  | Some ms =>
      let K := borrows ms args ++ vremove x L in
      if Nat.eqb (length ms) (length args) && nodupb (sinks ms args)
      then Some (TSeq (TSeq (copies (filter (keep K B) (sinks ms args)))
                            (TCall k (Src x) g (map (pack_arg K B) (sinks ms args))
                                   (map Src (borrows ms args))))
                      (settle (x :: filter (keep K B) (sinks ms args) ++ borrows ms args
                                 ++ vremove x L) L B),
                 args ++ vremove x L)
      else None
  end.

(* Whether a statement writes a variable (defines, mutates, or consumes it
   as a record). *)
Fixpoint writes (x : Var) (s : SStmt) : bool :=
  match s with
  | SSkip | SEmit _ => false
  | SDef d _ _ | SCopy d _ | SPack d _ | SPush d _ | SField d _ _ | SCall d _ _ => Nat.eqb d x
  | SCallIO _ z _ => Nat.eqb z x
  | SSeq a b | SIf _ a b => writes x a || writes x b
  | SWhile _ _ b => writes x b
  | SFocus t y _ b => Nat.eqb t x || Nat.eqb y x || writes x b
  | SUnpack y xs => Nat.eqb y x || vmem x xs
  | SRegion r _ _ b => Nat.eqb r x || writes x b
  end.

Fixpoint elab (M : Modes) (s : SStmt) (L B : list Var) : option (TStmt * list Var) :=
  match s with
  | SSkip => Some (TSkip, L)
  | SDef x f ys =>
      if in_dec Nat.eq_dec x ys then None
      else if in_dec Nat.eq_dec x B then None
      else Some (TSeq (TDef (Src x) f (map Src ys))
                      (settle (x :: ys ++ vremove x L) L B),
                 ys ++ vremove x L)
  | SCopy x y =>
      if Nat.eq_dec x y then None
      else if in_dec Nat.eq_dec x B then None
      else if keep L B y
           then Some (TSeq (TCopy (Src x) (Src y))
                           (settle (x :: y :: vremove x L) L B),
                      y :: vremove x L)
           else Some (TSeq (TMove (Src x) (Src y))
                           (settle (x :: vremove x L) L B),
                      y :: vremove x L)
  | SPack x ys =>
      if in_dec Nat.eq_dec x ys then None
      else if in_dec Nat.eq_dec x B then None
      else if nodupb ys
           then Some (TSeq (copies (filter (keep L B) ys))
                           (TSeq (TPack (Src x) (map (pack_arg L B) ys))
                                 (settle (x :: filter (keep L B) ys ++ vremove x L) L B)),
                      ys ++ vremove x L)
           else None
  | SPush x y =>
      if Nat.eq_dec x y then None
      else if in_dec Nat.eq_dec x B then None
      else if keep L B y
           then Some (TSeq (TSeq (TCopy (Tmp y) (Src y)) (TPush (Src x) (Tmp y)))
                           (settle (x :: y :: L) L B),
                      x :: y :: L)
           else Some (TSeq (TPush (Src x) (Src y)) (settle (x :: L) L B),
                      x :: y :: L)
  | SField x y i =>
      if Nat.eq_dec x y then None
      else if in_dec Nat.eq_dec x B then None
      else Some (TSeq (TField (Src x) (Src y) i)
                      (settle (x :: y :: vremove x L) L B),
                 y :: vremove x L)
  | SEmit y => Some (TSeq (TEmit (Src y)) (settle (y :: L) L B), y :: L)
  | SSeq s1 s2 =>
      match elab M s2 L B with
      | None => None
      | Some (t2, L2) =>
          match elab M s1 L2 B with
          | None => None
          | Some (t1, L1) => Some (TSeq t1 t2, L1)
          end
      end
  | SIf c s1 s2 =>
      match elab M s1 L B, elab M s2 L B with
      | Some (t1, L1), Some (t2, L2) =>
          let Lin := c :: L1 ++ L2 in
          Some (TIf (Src c) (TSeq (settle Lin L1 B) t1) (TSeq (settle Lin L2 B) t2), Lin)
      | _, _ => None
      end
  | SWhile c h b =>
      match elab M b h B with
      | None => None
      | Some (tb, Lb) =>
          if inclb Lb h && inclb L h && vmem c h
          then Some (TSeq (TWhile (Src c) (TSeq (settle h Lb B) tb)) (settle h L B), h)
          else None
      end
  | SCall x g ys =>
      if in_dec Nat.eq_dec x ys then None
      else if in_dec Nat.eq_dec x B then None
      else elab_call M KFun x g ys L B
  | SCallIO g z ys =>
      if in_dec Nat.eq_dec z ys then None
      else if in_dec Nat.eq_dec z B then None
      else elab_call M KProc z g (z :: ys) L B
  | SFocus t y p s =>
      (* The body runs with t live at its end and the root suspended; the
         root must not be live inside the body. *)
      if Nat.eq_dec t y then None
      else if in_dec Nat.eq_dec t B then None
      else if in_dec Nat.eq_dec y B then None
      else if in_dec Nat.eq_dec t L then None
      else match elab M s (t :: vremove y L) B with
           | None => None
           | Some (ts, Ls) =>
               if vmem y Ls then None
               else Some (TSeq (TFocus (Src t) (Src y) p (TSeq (settle (t :: vremove t Ls) Ls B) ts))
                               (settle (y :: vremove y L) L B),
                          y :: vremove t Ls)
           end
  | SUnpack y xs =>
      (* The record must be dead afterwards; unused parts are dropped. *)
      if in_dec Nat.eq_dec y L then None
      else if in_dec Nat.eq_dec y B then None
      else if in_dec Nat.eq_dec y xs then None
      else if nodupb xs && forallb (fun x => negb (vmem x B)) xs
           then Some (TSeq (TUnpack (Src y) xs) (settle (xs ++ vdiff L xs) L B), y :: vdiff L xs)
           else None
  | SRegion x f ys s =>
      (* The region value is kept live to the region's end, so every use that
         consumes it copies, and it is released at the end. *)
      if in_dec Nat.eq_dec x L then None
      else if in_dec Nat.eq_dec x B then None
      else if in_dec Nat.eq_dec x ys then None
      else if writes x s then None
      else match elab M s (x :: L) B with
           | None => None
           | Some (ts, Ls) =>
               Some (TSeq (TSeq (TDef (Src x) f (map Src ys)) (settle (x :: ys ++ vremove x Ls) Ls B))
                          (TSeq ts (settle (x :: L) L B)),
                     ys ++ vremove x Ls)
           end
  end.

(* A routine body is elaborated with its borrowed parameters in B and only
   its result live at exit. Parameters the body does not use are released
   on entry: sink ones are dropped and borrowed ones end. *)
Definition elab_routine (M : Modes) (ps : list Var) (ms : list bool) (body : SStmt)
    (ret : Var) : option TRoutine :=
  if Nat.eqb (length ms) (length ps) && nodupb ps then
    if vmem ret (borrows ms ps) then None
    else match elab M body [ret] (borrows ms ps) with
         | Some (tb, Lb) =>
             if inclb Lb ps
             then Some (sinks ms ps, borrows ms ps, TSeq (settle ps Lb (borrows ms ps)) tb, ret)
             else None
         | None => None
         end
  else None.

(* A routine without a resolved summary is not elaborated. *)
Definition elab_fun (M : Modes) (g : FName) (d : list Var * SStmt * Var) : option TRoutine :=
  match fmodes M g with
  | None => None
  | Some ms => match d with (ps, body, ret) => elab_routine M ps ms body ret end
  end.

(* A procedure owns its inout parameter for the duration of the call. *)
Definition elab_proc (M : Modes) (g : FName) (d : Var * list Var * SStmt) : option TRoutine :=
  match pmodes M g with
  | None => None
  | Some pm => match d with (io, ps, body) => elab_routine M (io :: ps) (true :: pm) body io end
  end.

Definition tfuns_of (M : Modes) (funs : SFunTable) : TTable :=
  fun g => match funs g with Some d => elab_fun M g d | None => None end.
Definition tprocs_of (M : Modes) (procs : SProcTable) : TTable :=
  fun g => match procs g with Some d => elab_proc M g d | None => None end.

(* Parameter-mode inference. A parameter is sink when the body stores it,
   moves it, mutates or redefines it, passes it on to a sink parameter, or
   returns it; every other parameter is borrowed. [elab] is sound for every
   mode table, so this inference is outside the trusted base. An unresolved
   callee contributes no sink use here; that is sound only because [elab]
   refuses the call itself until the callee is resolved. [owns] is monotone
   in [M], so iterating [infer_modes] from [no_summaries] only turns
   unresolved summaries into resolved ones and borrowed parameters into sink
   ones ([infer_ascends]). *)
Fixpoint owns (M : Modes) (p : Var) (s : SStmt) : bool :=
  match s with
  | SSkip | SEmit _ => false
  | SDef x _ _ | SField x _ _ => Nat.eqb x p
  | SCopy x y | SPush x y => Nat.eqb x p || Nat.eqb y p
  | SPack x ys => Nat.eqb x p || vmem p ys
  | SSeq s1 s2 | SIf _ s1 s2 => owns M p s1 || owns M p s2
  | SWhile _ _ b => owns M p b
  | SCall x g ys =>
      Nat.eqb x p || match fmodes M g with Some ms => vmem p (sinks ms ys) | None => false end
  | SCallIO g z ys =>
      Nat.eqb z p || match pmodes M g with Some ms => vmem p (sinks ms ys) | None => false end
  | SFocus t y _ b => Nat.eqb y p || Nat.eqb t p || owns M p b
  | SUnpack y xs => Nat.eqb y p || vmem p xs
  | SRegion r _ _ b => Nat.eqb r p || owns M p b
  end.

Definition infer_modes (funs : SFunTable) (procs : SProcTable) (M : Modes) : Modes :=
  {| fmodes := fun g => match funs g with
                        | Some (ps, body, ret) => Some (map (fun q => owns M q body || Nat.eqb q ret) ps)
                        | None => None
                        end;
     pmodes := fun g => match procs g with
                        | Some (io, ps, body) => Some (map (fun q => owns M q body) ps)
                        | None => None
                        end |}.

(* The all-borrowed table for the routines of a program: a resolved
   summary that lends every parameter. Unknown routines stay unresolved. *)
Definition borrow_modes (funs : SFunTable) (procs : SProcTable) : Modes :=
  {| fmodes := fun g => match funs g with
                        | Some (ps, _, _) => Some (map (fun _ => false) ps)
                        | None => None
                        end;
     pmodes := fun g => match procs g with
                        | Some (_, ps, _) => Some (map (fun _ => false) ps)
                        | None => None
                        end |}.

(* ------------------------------------------------------------------ *)
(* Invariants                                                           *)
(* ------------------------------------------------------------------ *)

(* Machine invariant for one frame. Owned and borrowed bindings are
   distinct; the live heap is a duplicate-free permutation of the owned
   footprints plus the frame heap R (blocks owned by suspended callers);
   borrowed values live inside R; the allocator frontier is above every
   live block; every value has one block per node. *)
Definition laid (rho : TEnv) : Prop :=
  forall z c bs, tlookup z rho = Some (c, bs) -> length bs = vsize c.

Definition INV (rho beta : TEnv) (H : list Block) (n : nat) (R : list Block) : Prop :=
  NoDup (dom rho ++ dom beta) /\ NoDup H /\ Permutation H (heap_of rho ++ R) /\
  (forall b, In b H -> fst b < n) /\
  (forall z c bs, tlookup z beta = Some (c, bs) -> incl bs R) /\
  laid rho /\ laid beta.

(* Boundary correspondence: owned live variables are exactly L minus B,
   borrowed live ones are exactly L inside B, no temporary survives, and
   contents agree with the source. *)
Definition CORR (sg : SEnv) (rho beta : TEnv) (L B : list Var) : Prop :=
  (forall x, tlookup (Src x) rho <> None <-> In x L /\ ~ In x B) /\
  (forall x, tlookup (Src x) beta <> None <-> In x L /\ In x B) /\
  (forall x, tlookup (Tmp x) rho = None) /\
  (forall x, tlookup (Tmp x) beta = None) /\
  (forall x c bs, tlookup (Src x) rho = Some (c, bs) -> sg x = Some c) /\
  (forall x c bs, tlookup (Src x) beta = Some (c, bs) -> sg x = Some c).

(* ------------------------------------------------------------------ *)
(* Generic list lemmas                                                  *)
(* ------------------------------------------------------------------ *)

Lemma NoDup_app_split : forall (A : Type) (l1 l2 : list A),
  NoDup (l1 ++ l2) -> NoDup l1 /\ NoDup l2 /\ (forall a, In a l1 -> ~ In a l2).
Proof.
  intros A l1. induction l1 as [|a l1 IH]; simpl; intros l2 HN.
  - split; [constructor|]. split; [exact HN|]. intros a [].
  - inversion HN as [|a' l' Ha HN']; subst.
    destruct (IH l2 HN') as [H1 [H2 H3]].
    split; [|split; [exact H2|]].
    + constructor; [|exact H1]. intros Hin. apply Ha. apply in_or_app. left. exact Hin.
    + intros b [Hb|Hb].
      * subst b. intros Hin. apply Ha. apply in_or_app. right. exact Hin.
      * apply H3. exact Hb.
Qed.

Lemma NoDup_app_join : forall (A : Type) (l1 l2 : list A),
  NoDup l1 -> NoDup l2 -> (forall a, In a l1 -> ~ In a l2) -> NoDup (l1 ++ l2).
Proof.
  intros A l1. induction l1 as [|a l1 IH]; simpl; intros l2 H1 H2 H3; [exact H2|].
  inversion H1 as [|a' l' Ha H1']; subst. constructor.
  - intros Hin. apply in_app_or in Hin. destruct Hin as [Hin|Hin]; [contradiction|].
    apply (H3 a); [left; reflexivity| exact Hin].
  - apply IH; [exact H1'| exact H2|]. intros b Hb. apply H3. right. exact Hb.
Qed.

Lemma NoDup_filter' : forall (A : Type) (f : A -> bool) l, NoDup l -> NoDup (filter f l).
Proof.
  intros A f l. induction l as [|a l IH]; simpl; intros HN; [constructor|].
  inversion HN as [|a' l' Ha HN']; subst.
  destruct (f a).
  - constructor; [|apply IH; exact HN']. intros Hin. apply filter_In in Hin.
    apply Ha. apply Hin.
  - apply IH. exact HN'.
Qed.

Lemma Permutation_filter_compat : forall (f : Block -> bool) l l',
  Permutation l l' -> Permutation (filter f l) (filter f l').
Proof.
  intros f l l' HP.
  induction HP as [|x l l' HP IH|x y l|l l' l'' H1 IH1 H2 IH2]; simpl.
  - constructor.
  - destruct (f x); [apply perm_skip|]; exact IH.
  - destruct (f x), (f y); first [apply perm_swap | apply Permutation_refl].
  - eapply Permutation_trans; eassumption.
Qed.

Lemma beqb_true : forall a b, beqb a b = true <-> a = b.
Proof.
  intros [a1 a2] [b1 b2]. unfold beqb. simpl. rewrite andb_true_iff, !Nat.eqb_eq.
  split; [intros [E1 E2]; subst; reflexivity| intros E; inversion E; auto].
Qed.

Lemma bmem_true : forall b bs, bmem b bs = true <-> In b bs.
Proof.
  intros b bs. unfold bmem. rewrite existsb_exists. split.
  - intros [b' [Hin Heq]]. apply beqb_true in Heq. subst. exact Hin.
  - intros Hin. exists b. split; [exact Hin| apply beqb_true; reflexivity].
Qed.

Lemma filter_none : forall (f : Block -> bool) l,
  (forall b, In b l -> f b = false) -> filter f l = [].
Proof.
  intros f l. induction l as [|a l IH]; simpl; intros Hf; [reflexivity|].
  rewrite Hf; [|left; reflexivity]. apply IH. intros b Hb. apply Hf. right. exact Hb.
Qed.

Lemma filter_all : forall (f : Block -> bool) l,
  (forall b, In b l -> f b = true) -> filter f l = l.
Proof.
  intros f l. induction l as [|a l IH]; simpl; intros Hf; [reflexivity|].
  rewrite Hf; [|left; reflexivity]. rewrite IH; [reflexivity|].
  intros b Hb. apply Hf. right. exact Hb.
Qed.

Lemma free_perm : forall bs R H,
  NoDup (bs ++ R) -> Permutation H (bs ++ R) -> Permutation (free bs H) R.
Proof.
  intros bs R H HN HP. unfold free.
  eapply Permutation_trans; [apply Permutation_filter_compat; exact HP|].
  rewrite filter_app.
  assert (E1 : filter (fun b => negb (bmem b bs)) bs = []).
  { apply filter_none. intros b Hb.
    assert (Hm : bmem b bs = true) by (apply bmem_true; exact Hb).
    rewrite Hm. reflexivity. }
  assert (E2 : filter (fun b => negb (bmem b bs)) R = R).
  { apply filter_all. intros b Hb.
    destruct (bmem b bs) eqn:Hm; [|reflexivity].
    apply bmem_true in Hm. exfalso.
    destruct (NoDup_app_split _ _ _ HN) as [_ [_ Hd]]. exact (Hd b Hm Hb). }
  rewrite E1, E2. apply Permutation_refl.
Qed.

(* ------------------------------------------------------------------ *)
(* Environment lemmas                                                   *)
(* ------------------------------------------------------------------ *)

Lemma tlookup_cons : forall k v rho z,
  tlookup z ((k, v) :: rho) = if tvar_eq_dec k z then Some v else tlookup z rho.
Proof. reflexivity. Qed.

Lemma tlookup_tremove : forall x rho z,
  tlookup z (tremove x rho) = if tvar_eq_dec x z then None else tlookup z rho.
Proof.
  intros x rho z. induction rho as [|[k v] r IH]; simpl.
  - destruct (tvar_eq_dec x z); reflexivity.
  - destruct (tvar_eq_dec k x) as [Hkx|Hkx].
    + subst k. rewrite IH. destruct (tvar_eq_dec x z); reflexivity.
    + simpl. destruct (tvar_eq_dec k z) as [Hkz|Hkz].
      * subst k. destruct (tvar_eq_dec x z); [congruence|reflexivity].
      * exact IH.
Qed.

Lemma tlookup_none_iff : forall z rho, tlookup z rho = None <-> ~ In z (dom rho).
Proof.
  intros z rho. induction rho as [|[k v] r IH]; simpl.
  - split; auto.
  - destruct (tvar_eq_dec k z) as [Hkz|Hkz].
    + split; [discriminate|]. intros Hn. exfalso. apply Hn. left. exact Hkz.
    + rewrite IH. split.
      * intros Hn [Heq|Hin]; [congruence|exact (Hn Hin)].
      * intros Hn Hin. apply Hn. right. exact Hin.
Qed.

Lemma tlookup_some_in : forall z rho v, tlookup z rho = Some v -> In z (dom rho).
Proof.
  intros z rho v Hl. destruct (in_dec tvar_eq_dec z (dom rho)) as [Hin|Hin];
    [exact Hin|]. apply tlookup_none_iff in Hin. congruence.
Qed.

Lemma in_dom_tremove : forall x rho z, In z (dom (tremove x rho)) -> In z (dom rho) /\ z <> x.
Proof.
  intros x rho z Hin.
  destruct (in_dec tvar_eq_dec z (dom rho)) as [Hz|Hz].
  - split; [exact Hz|]. intros Hzx. subst z.
    assert (Hn : tlookup x (tremove x rho) = None).
    { rewrite tlookup_tremove. destruct (tvar_eq_dec x x); [reflexivity|congruence]. }
    apply tlookup_none_iff in Hn. contradiction.
  - exfalso. apply tlookup_none_iff in Hz.
    assert (Hn : tlookup z (tremove x rho) = None).
    { rewrite tlookup_tremove. destruct (tvar_eq_dec x z); [reflexivity|exact Hz]. }
    apply tlookup_none_iff in Hn. contradiction.
Qed.

Lemma tremove_notin : forall x rho, ~ In x (dom rho) -> tremove x rho = rho.
Proof.
  intros x rho. induction rho as [|[k v] r IH]; simpl; intros Hn; [reflexivity|].
  destruct (tvar_eq_dec k x) as [Hkx|Hkx].
  - exfalso. apply Hn. left. exact Hkx.
  - rewrite IH; [reflexivity|]. intros Hin. apply Hn. right. exact Hin.
Qed.

Lemma nodup_dom_tremove : forall x rho, NoDup (dom rho) -> NoDup (dom (tremove x rho)).
Proof.
  intros x rho. induction rho as [|[k v] r IH]; simpl; intros HN; [constructor|].
  inversion HN as [|k' l' Hk HN']; subst.
  destruct (tvar_eq_dec k x) as [Hkx|Hkx].
  - apply IH. exact HN'.
  - simpl. constructor.
    + intros Hin. apply in_dom_tremove in Hin. apply Hk. apply Hin.
    + apply IH. exact HN'.
Qed.

Lemma heap_tremove : forall x rho c bs,
  NoDup (dom rho) -> tlookup x rho = Some (c, bs) ->
  Permutation (heap_of rho) (bs ++ heap_of (tremove x rho)).
Proof.
  intros x rho. induction rho as [|[k [c' bs']] r IH]; simpl; intros c bs HN Hl;
    [discriminate|].
  inversion HN as [|k' l' Hk HN']; subst.
  destruct (tvar_eq_dec k x) as [Hkx|Hkx].
  - subst k. inversion Hl; subst. rewrite tremove_notin; [|exact Hk].
    apply Permutation_refl.
  - simpl. specialize (IH c bs HN' Hl).
    eapply Permutation_trans.
    + apply Permutation_app_head. exact IH.
    + rewrite !app_assoc. apply Permutation_app_tail. apply Permutation_app_comm.
Qed.

Lemma lookup_heap_incl : forall x rho c bs,
  tlookup x rho = Some (c, bs) -> incl bs (heap_of rho).
Proof.
  intros x rho. induction rho as [|[k [c' bs']] r IH]; simpl; intros c bs Hl;
    [discriminate|].
  destruct (tvar_eq_dec k x).
  - inversion Hl; subst. apply incl_appl. apply incl_refl.
  - apply incl_appr. eapply IH. exact Hl.
Qed.

Lemma tlookup_all_tremove_notin : forall ys y rho,
  ~ In y ys -> tlookup_all ys (tremove y rho) = tlookup_all ys rho.
Proof.
  induction ys as [|a r IH]; simpl; intros y rho Hn; [reflexivity|].
  rewrite tlookup_tremove. destruct (tvar_eq_dec y a) as [Hya|Hya].
  - exfalso. apply Hn. left. symmetry. exact Hya.
  - rewrite IH; [reflexivity|]. intros Hin. apply Hn. right. exact Hin.
Qed.

Lemma tlookup_tremove_all : forall ys rho z,
  tlookup z (tremove_all ys rho) = if in_dec tvar_eq_dec z ys then None else tlookup z rho.
Proof.
  induction ys as [|y r IH]; simpl; intros rho z; [reflexivity|].
  rewrite IH, tlookup_tremove.
  destruct (in_dec tvar_eq_dec z r) as [Hr|Hr];
    destruct (tvar_eq_dec y z) as [Hyz|Hyz];
    destruct (tvar_eq_dec y z); try reflexivity; try congruence;
    destruct (in_dec tvar_eq_dec z (y :: r)) as [Hyr|Hyr]; try reflexivity;
    try (exfalso; apply Hyr; first [right; exact Hr | left; exact Hyz]);
    try (destruct Hyr as [Hyr|Hyr]; [congruence|contradiction]).
Qed.

Lemma nodup_dom_tremove_all : forall ys rho, NoDup (dom rho) -> NoDup (dom (tremove_all ys rho)).
Proof.
  induction ys as [|y r IH]; simpl; intros rho HN; [exact HN|].
  apply IH. apply nodup_dom_tremove. exact HN.
Qed.

Lemma in_dom_tremove_all : forall ys rho z, In z (dom (tremove_all ys rho)) -> In z (dom rho).
Proof.
  induction ys as [|y r IH]; simpl; intros rho z Hin; [exact Hin|].
  apply IH in Hin. apply in_dom_tremove in Hin. apply Hin.
Qed.

Lemma heap_tremove_all : forall ys rho vs,
  NoDup ys -> NoDup (dom rho) -> tlookup_all ys rho = Some vs ->
  Permutation (heap_of rho) (flat_map snd vs ++ heap_of (tremove_all ys rho)).
Proof.
  induction ys as [|y r IH]; simpl; intros rho vs HNy HN Hl.
  - inversion Hl; subst. apply Permutation_refl.
  - destruct (tlookup y rho) as [[c bs]|] eqn:Hy; [|discriminate].
    destruct (tlookup_all r rho) as [vs'|] eqn:Hr; [|discriminate].
    inversion Hl; subst. inversion HNy as [|y' r' Hyr HNr]; subst. simpl.
    eapply Permutation_trans; [eapply heap_tremove; eassumption|].
    rewrite <- app_assoc. apply Permutation_app_head.
    apply IH; [exact HNr| apply nodup_dom_tremove; exact HN|].
    rewrite tlookup_all_tremove_notin; [exact Hr|exact Hyr].
Qed.

Lemma tread_all_spec : forall ys rho beta vs,
  tread_all ys rho beta = Some vs -> Forall2 (fun y v => tread y rho beta = Some v) ys vs.
Proof.
  induction ys as [|y r IH]; simpl; intros rho beta vs Hl.
  - inversion Hl; subst. constructor.
  - destruct (tread y rho beta) eqn:Hy; [|discriminate].
    destruct (tread_all r rho beta) eqn:Hr; [|discriminate].
    inversion Hl; subst. constructor; [exact Hy| apply IH; exact Hr].
Qed.

Lemma tread_tremove_other : forall x z rho beta,
  x <> z -> tread z (tremove x rho) beta = tread z rho beta.
Proof.
  intros x z rho beta Hxz. unfold tread. rewrite tlookup_tremove.
  destruct (tvar_eq_dec x z); [contradiction|reflexivity].
Qed.

Lemma tread_all_tremove_notin : forall ys x rho beta,
  ~ In x ys -> tread_all ys (tremove x rho) beta = tread_all ys rho beta.
Proof.
  induction ys as [|y r IH]; simpl; intros x rho beta Hn; [reflexivity|].
  rewrite tread_tremove_other; [|intros E; apply Hn; left; symmetry; exact E].
  rewrite IH; [reflexivity|]. intros Hin. apply Hn. right. exact Hin.
Qed.

Lemma doms_cons : forall x v rho beta,
  unbound x rho beta -> NoDup (dom rho ++ dom beta) ->
  NoDup (dom ((x, v) :: rho) ++ dom beta).
Proof.
  intros x v rho beta [Hr Hb] HN. simpl. constructor; [|exact HN].
  intros Hin. apply in_app_or in Hin. destruct Hin as [Hin|Hin].
  - apply tlookup_none_iff in Hr. contradiction.
  - apply tlookup_none_iff in Hb. contradiction.
Qed.

Lemma doms_sub : forall rho rho' beta beta',
  NoDup (dom rho ++ dom beta) -> NoDup (dom rho') -> NoDup (dom beta') ->
  (forall z, In z (dom rho') -> In z (dom rho)) ->
  (forall z, In z (dom beta') -> In z (dom beta)) ->
  NoDup (dom rho' ++ dom beta').
Proof.
  intros rho rho' beta beta' HN H1 H2 S1 S2.
  destruct (NoDup_app_split _ _ _ HN) as [_ [_ Hd]].
  apply NoDup_app_join; [exact H1| exact H2|].
  intros z Hz Hz'. apply (Hd z); [apply S1; exact Hz| apply S2; exact Hz'].
Qed.

Lemma doms_l : forall rho beta, NoDup (dom rho ++ dom beta) -> NoDup (dom rho).
Proof. intros rho beta HN. apply (NoDup_app_split _ _ _ HN). Qed.

Lemma doms_r : forall rho beta, NoDup (dom rho ++ dom beta) -> NoDup (dom beta).
Proof. intros rho beta HN. apply (NoDup_app_split _ _ _ HN). Qed.

Lemma doms_disjoint : forall rho beta z,
  NoDup (dom rho ++ dom beta) -> In z (dom rho) -> ~ In z (dom beta).
Proof. intros rho beta z HN. apply (NoDup_app_split _ _ _ HN). Qed.

(* ------------------------------------------------------------------ *)
(* One-step invariant preservation                                      *)
(* ------------------------------------------------------------------ *)

Lemma read_frame : forall rho beta R z c bs,
  (forall w c' bs', tlookup w beta = Some (c', bs') -> incl bs' R) ->
  tread z rho beta = Some (c, bs) -> incl bs (heap_of rho ++ R).
Proof.
  intros rho beta R z c bs Hb Hr b Hin. apply in_or_app. unfold tread in Hr.
  destruct (tlookup z rho) as [v|] eqn:E.
  - inversion Hr; subst. left. eapply lookup_heap_incl; eassumption.
  - right. eapply Hb; eassumption.
Qed.

Lemma alloc_length : forall n k, length (alloc n k) = k.
Proof. intros. unfold alloc. rewrite length_map, length_seq. reflexivity. Qed.

Lemma alloc_fst : forall n k b, In b (alloc n k) -> fst b = n.
Proof.
  intros n k b Hin. unfold alloc in Hin. apply in_map_iff in Hin.
  destruct Hin as [j [E _]]. subst. reflexivity.
Qed.

Lemma map_pair_nodup : forall (n : nat) (l : list nat), NoDup l -> NoDup (map (fun j : nat => (n, j)) l).
Proof.
  intros n l HN. induction HN as [|j l Hj HN IH]; simpl; constructor; [|exact IH].
  intros Hin. apply in_map_iff in Hin. destruct Hin as [j' [E Hj']]. inversion E; subst. contradiction.
Qed.

Lemma alloc_nodup : forall n k, NoDup (alloc n k).
Proof. intros. apply map_pair_nodup. apply seq_NoDup. Qed.

Lemma vsize_push : forall cs c, vsize (SNode (cs ++ [c])) = vsize (SNode cs) + vsize c.
Proof. intros. simpl. rewrite map_app, list_sum_app. simpl. lia. Qed.

Lemma flat_map_laid : forall vs : list TValue,
  Forall (fun v => length (snd v) = vsize (fst v)) vs ->
  length (flat_map snd vs) = list_sum (map vsize (map fst vs)).
Proof.
  intros vs HF. induction HF as [|[c bs] vs Hv HF IH]; simpl; [reflexivity|].
  rewrite length_app, IH. simpl in Hv. lia.
Qed.

Lemma tlookup_all_in : forall ys rho vs, tlookup_all ys rho = Some vs ->
  forall v, In v vs -> exists y, tlookup y rho = Some v.
Proof.
  induction ys as [|y r IH]; simpl; intros rho vs Hl v Hin.
  - inversion Hl; subst. destruct Hin.
  - destruct (tlookup y rho) as [w|] eqn:Hy; [|discriminate].
    destruct (tlookup_all r rho) as [ws|] eqn:Hr; [|discriminate].
    inversion Hl; subst. destruct Hin as [E|Hin]; [subst; exists y; exact Hy|].
    eapply IH; [exact Hr| exact Hin].
Qed.

Lemma tread_all_in : forall ys rho beta vs, tread_all ys rho beta = Some vs ->
  forall v, In v vs -> exists y, tread y rho beta = Some v.
Proof.
  induction ys as [|y r IH]; simpl; intros rho beta vs Hl v Hin.
  - inversion Hl; subst. destruct Hin.
  - destruct (tread y rho beta) as [w|] eqn:Hy; [|discriminate].
    destruct (tread_all r rho beta) as [ws|] eqn:Hr; [|discriminate].
    inversion Hl; subst. destruct Hin as [E|Hin]; [subst; exists y; exact Hy|].
    eapply IH; [exact Hr| exact Hin].
Qed.

Lemma laid_tread : forall rho beta z c bs,
  laid rho -> laid beta -> tread z rho beta = Some (c, bs) -> length bs = vsize c.
Proof.
  intros rho beta z c bs Lr Lb Hr. unfold tread in Hr.
  destruct (tlookup z rho) as [v|] eqn:E.
  - inversion Hr; subst. eapply Lr. exact E.
  - eapply Lb. exact Hr.
Qed.

Lemma inv_read : forall rho beta H n R z c bs,
  INV rho beta H n R -> tread z rho beta = Some (c, bs) -> incl bs H.
Proof.
  intros rho beta H n R z c bs [_ [_ [HP [_ [Hb _]]]]] Hr b Hin.
  eapply Permutation_in; [apply Permutation_sym; exact HP|].
  eapply read_frame; eassumption.
Qed.

Lemma inv_fresh : forall rho beta H n R, INV rho beta H n R -> fresh n H.
Proof. intros rho beta H n R [_ [_ [_ [Hf _]]]] b Hin E. apply Hf in Hin. lia. Qed.

Lemma forall_read : forall rho beta H n R ys tvs,
  INV rho beta H n R -> tread_all ys rho beta = Some tvs ->
  Forall (fun v => incl (snd v) H) tvs.
Proof.
  intros rho beta H n R ys tvs Hinv Hl. apply tread_all_spec in Hl.
  induction Hl as [|y [c bs] ys' tvs' Hy Hrest IH]; constructor; [|exact IH].
  simpl. eapply inv_read; eassumption.
Qed.

Lemma forall_read_frame : forall rho beta R ys tvs,
  (forall w c' bs', tlookup w beta = Some (c', bs') -> incl bs' R) ->
  tread_all ys rho beta = Some tvs ->
  Forall (fun v => incl (snd v) (heap_of rho ++ R)) tvs.
Proof.
  intros rho beta R ys tvs Hb Hl. apply tread_all_spec in Hl.
  induction Hl as [|y [c bs] ys' tvs' Hy Hrest IH]; constructor; [|exact IH].
  simpl. eapply read_frame; eassumption.
Qed.

Lemma inv_alloc : forall rho beta H n R x v,
  INV rho beta H n R -> unbound x rho beta ->
  INV ((x, (v, alloc n (vsize v))) :: rho) beta (alloc n (vsize v) ++ H) (S n) R.
Proof.
  intros rho beta H n R x v [HN [HH [HP [Hf [Hb [Lr Lb]]]]]] Hx.
  split; [|split; [|split; [|split; [|split; [|split]]]]].
  - apply doms_cons; assumption.
  - apply NoDup_app_join; [apply alloc_nodup| exact HH|].
    intros b Ha Hh. apply alloc_fst in Ha. apply Hf in Hh. lia.
  - change (heap_of ((x, (v, alloc n (vsize v))) :: rho)) with (alloc n (vsize v) ++ heap_of rho).
    rewrite <- app_assoc. apply Permutation_app_head. exact HP.
  - intros b Hin. apply in_app_or in Hin. destruct Hin as [Hin|Hin].
    + apply alloc_fst in Hin. lia.
    + apply Hf in Hin. lia.
  - exact Hb.
  - intros z c bs Hz. rewrite tlookup_cons in Hz. destruct (tvar_eq_dec x z).
    + inversion Hz; subst. apply alloc_length.
    + eapply Lr. exact Hz.
  - exact Lb.
Qed.

Lemma inv_drop : forall rho beta H n R x c bs,
  INV rho beta H n R -> tlookup x rho = Some (c, bs) ->
  incl bs H /\ NoDup bs /\ INV (tremove x rho) beta (free bs H) n R.
Proof.
  intros rho beta H n R x c bs Hinv Hl.
  destruct Hinv as [HN [HH [HP [Hf [Hb [Lr Lb]]]]]].
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  assert (HP2 : Permutation H (bs ++ heap_of (tremove x rho) ++ R)).
  { eapply Permutation_trans; [exact HP|]. rewrite app_assoc.
    apply Permutation_app_tail. eapply heap_tremove; eassumption. }
  assert (HND : NoDup (bs ++ heap_of (tremove x rho) ++ R))
    by (eapply Permutation_NoDup; eassumption).
  split; [|split].
  - intros b Hbs. eapply Permutation_in; [apply Permutation_sym; exact HP2|].
    apply in_or_app. left. exact Hbs.
  - apply (NoDup_app_split _ _ _ HND).
  - split; [|split; [|split; [|split; [|split; [|split]]]]].
    + eapply doms_sub; [exact HN| apply nodup_dom_tremove; exact HNr| eapply doms_r; exact HN| |].
      * intros z Hz. apply in_dom_tremove in Hz. apply Hz.
      * intros z Hz. exact Hz.
    + apply NoDup_filter'. exact HH.
    + apply free_perm; assumption.
    + intros b Hbf. unfold free in Hbf. apply filter_In in Hbf. apply Hf. apply Hbf.
    + exact Hb.
    + intros z c' bs' Hz. rewrite tlookup_tremove in Hz.
      destruct (tvar_eq_dec x z); [discriminate| eapply Lr; exact Hz].
    + exact Lb.
Qed.

Lemma inv_end : forall rho beta H n R x v,
  INV rho beta H n R -> tlookup x beta = Some v -> INV rho (tremove x beta) H n R.
Proof.
  intros rho beta H n R x v [HN [HH [HP [Hf [Hb [Lr Lb]]]]]] Hl.
  split; [|split; [exact HH|split; [exact HP|split; [exact Hf|split; [|split; [exact Lr|]]]]]].
  - eapply doms_sub; [exact HN| eapply doms_l; exact HN| apply nodup_dom_tremove; eapply doms_r; exact HN| |].
    + intros z Hz. exact Hz.
    + intros z Hz. apply in_dom_tremove in Hz. apply Hz.
  - intros z c bs Hz. rewrite tlookup_tremove in Hz.
    destruct (tvar_eq_dec x z); [discriminate|]. eapply Hb. exact Hz.
  - intros z c bs Hz. rewrite tlookup_tremove in Hz.
    destruct (tvar_eq_dec x z); [discriminate|]. eapply Lb. exact Hz.
Qed.

Lemma inv_move : forall rho beta H n R x y v,
  INV rho beta H n R -> unbound x rho beta -> tlookup y rho = Some v ->
  INV ((x, v) :: tremove y rho) beta H n R.
Proof.
  intros rho beta H n R x y [c bs] [HN [HH [HP [Hf [Hb [Lr Lb]]]]]] Hx Hy.
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  split; [|split; [exact HH|split; [|split; [exact Hf|split; [exact Hb|split; [|exact Lb]]]]]].
  - apply doms_cons.
    + destruct Hx as [Hxr Hxb]. split; [|exact Hxb].
      rewrite tlookup_tremove. destruct (tvar_eq_dec y x); [reflexivity| exact Hxr].
    + eapply doms_sub; [exact HN| apply nodup_dom_tremove; exact HNr| eapply doms_r; exact HN| |].
      * intros z Hz. apply in_dom_tremove in Hz. apply Hz.
      * intros z Hz. exact Hz.
  - simpl. eapply Permutation_trans; [exact HP|].
    apply Permutation_app_tail. eapply heap_tremove; eassumption.
  - intros z c' bs' Hz. rewrite tlookup_cons in Hz. destruct (tvar_eq_dec x z).
    + inversion Hz; subst. eapply Lr. exact Hy.
    + rewrite tlookup_tremove in Hz. destruct (tvar_eq_dec y z); [discriminate| eapply Lr; exact Hz].
Qed.

Lemma inv_push : forall rho beta H n R x y cs bx v,
  INV rho beta H n R -> x <> y -> tlookup x rho = Some (SNode cs, bx) ->
  tlookup y rho = Some v ->
  INV ((x, (SNode (cs ++ [fst v]), bx ++ snd v)) :: tremove x (tremove y rho)) beta H n R.
Proof.
  intros rho beta H n R x y cs bx [cv bv] [HN [HH [HP [Hf [Hb [Lr Lb]]]]]] Hxy Hx Hy.
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  assert (HNy : NoDup (dom (tremove y rho))) by (apply nodup_dom_tremove; exact HNr).
  assert (Hx' : tlookup x (tremove y rho) = Some (SNode cs, bx)).
  { rewrite tlookup_tremove. destruct (tvar_eq_dec y x); [congruence|exact Hx]. }
  split; [|split; [exact HH|split; [|split; [exact Hf|split; [exact Hb|split; [|exact Lb]]]]]].
  - apply doms_cons.
    + split.
      * rewrite tlookup_tremove. destruct (tvar_eq_dec x x); [reflexivity|congruence].
      * destruct (tlookup x beta) eqn:E; [|reflexivity]. exfalso.
        apply (doms_disjoint rho beta x HN); [eapply tlookup_some_in; exact Hx| eapply tlookup_some_in; exact E].
    + eapply doms_sub; [exact HN| apply nodup_dom_tremove; exact HNy| eapply doms_r; exact HN| |].
      * intros z Hz. apply in_dom_tremove in Hz. destruct Hz as [Hz _].
        apply in_dom_tremove in Hz. apply Hz.
      * intros z Hz. exact Hz.
  - simpl. eapply Permutation_trans; [exact HP|].
    apply Permutation_app_tail.
    eapply Permutation_trans; [eapply heap_tremove; eassumption|].
    eapply Permutation_trans;
      [apply Permutation_app_head; eapply heap_tremove; eassumption|].
    rewrite !app_assoc. apply Permutation_app_tail. apply Permutation_app_comm.
  - intros z c' bs' Hz. rewrite tlookup_cons in Hz. destruct (tvar_eq_dec x z).
    + inversion Hz; subst. cbn [fst snd]. rewrite length_app, vsize_push.
      rewrite (Lr _ _ _ Hx), (Lr _ _ _ Hy). reflexivity.
    + rewrite tlookup_tremove in Hz. destruct (tvar_eq_dec x z); [contradiction|].
      rewrite tlookup_tremove in Hz. destruct (tvar_eq_dec y z); [discriminate| eapply Lr; exact Hz].
Qed.

Lemma inv_pack : forall rho beta H n R x ys vs,
  INV rho beta H n R -> unbound x rho beta -> NoDup ys -> tlookup_all ys rho = Some vs ->
  INV ((x, (SNode (map fst vs), (n, 0) :: flat_map snd vs)) :: tremove_all ys rho)
      beta ((n, 0) :: H) (S n) R.
Proof.
  intros rho beta H n R x ys vs [HN [HH [HP [Hf [Hb [Lr Lb]]]]]] Hx HNy Hl.
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  split; [|split; [|split; [|split; [|split; [|split]]]]].
  - apply doms_cons.
    + destruct Hx as [Hxr Hxb]. split; [|exact Hxb].
      apply tlookup_none_iff. intros Hin. apply in_dom_tremove_all in Hin.
      apply tlookup_none_iff in Hxr. contradiction.
    + eapply doms_sub; [exact HN| apply nodup_dom_tremove_all; exact HNr| eapply doms_r; exact HN| |].
      * intros z Hz. eapply in_dom_tremove_all. exact Hz.
      * intros z Hz. exact Hz.
  - constructor; [|exact HH]. intros Hin. apply Hf in Hin. simpl in Hin. lia.
  - simpl. apply perm_skip. eapply Permutation_trans; [exact HP|].
    apply Permutation_app_tail. eapply heap_tremove_all; eassumption.
  - intros b [Hb'|Hb']; [subst b; simpl; lia|]. apply Hf in Hb'. lia.
  - exact Hb.
  - intros z c bs Hz. rewrite tlookup_cons in Hz. destruct (tvar_eq_dec x z).
    + inversion Hz; subst. simpl. rewrite (flat_map_laid vs); [reflexivity|].
      apply Forall_forall. intros [c' bs'] Hin.
      destruct (tlookup_all_in ys rho vs Hl _ Hin) as [y Hy]. exact (Lr _ _ _ Hy).
    + rewrite tlookup_tremove_all in Hz.
      destruct (in_dec tvar_eq_dec z ys); [discriminate| eapply Lr; exact Hz].
  - exact Lb.
Qed.

(* ------------------------------------------------------------------ *)
(* Membership helpers for live sets                                     *)
(* ------------------------------------------------------------------ *)

Lemma vremove_In : forall x L z, In z (vremove x L) <-> In z L /\ z <> x.
Proof.
  intros x L z. unfold vremove. rewrite filter_In.
  destruct (Nat.eqb z x) eqn:E; simpl.
  - apply Nat.eqb_eq in E. split; [intros [_ F]; discriminate| intros [_ F]; contradiction].
  - apply Nat.eqb_neq in E. tauto.
Qed.

Lemma vmem_true : forall x L, vmem x L = true <-> In x L.
Proof.
  intros x L. unfold vmem.
  destruct (in_dec Nat.eq_dec x L) as [Hin|Hin]; split; intros Hb.
  - exact Hin.
  - reflexivity.
  - discriminate.
  - contradiction.
Qed.

Lemma vmem_false : forall x L, vmem x L = false <-> ~ In x L.
Proof.
  intros x L. unfold vmem.
  destruct (in_dec Nat.eq_dec x L) as [Hin|Hin]; split; intros Hb.
  - discriminate.
  - contradiction.
  - exact Hin.
  - reflexivity.
Qed.

Lemma keep_true : forall L B y, keep L B y = true <-> In y L \/ In y B.
Proof.
  intros L B y. unfold keep. rewrite orb_true_iff, !vmem_true. tauto.
Qed.

Lemma keep_false : forall L B y, keep L B y = false -> ~ In y L /\ ~ In y B.
Proof.
  intros L B y E. unfold keep in E. apply orb_false_iff in E. destruct E as [E1 E2].
  split; apply vmem_false; assumption.
Qed.

Lemma vdiff_In : forall A B z, In z (vdiff A B) <-> In z A /\ ~ In z B.
Proof.
  intros A B z. unfold vdiff. rewrite filter_In.
  destruct (vmem z B) eqn:E; simpl.
  - apply vmem_true in E. split; [intros [_ F]; discriminate| intros [_ F]; contradiction].
  - apply vmem_false in E. tauto.
Qed.

Lemma nodupb_spec : forall ys, nodupb ys = true -> NoDup ys.
Proof.
  induction ys as [|y r IH]; simpl; intros Hb; [constructor|].
  apply andb_true_iff in Hb. destruct Hb as [Hy Hr].
  constructor; [|apply IH; exact Hr].
  apply negb_true_iff in Hy. apply vmem_false. exact Hy.
Qed.

Lemma inclb_spec : forall A B, inclb A B = true -> incl A B.
Proof.
  intros A B Hb z Hz. unfold inclb in Hb. rewrite forallb_forall in Hb.
  apply vmem_true. apply Hb. exact Hz.
Qed.

Lemma vdiff_self : forall A, vdiff A A = [].
Proof.
  intros A. unfold vdiff. remember A as B eqn:EB. rewrite EB at 1.
  assert (Hsub : incl A B) by (subst; apply incl_refl). clear EB.
  induction A as [|a r IH]; simpl; [reflexivity|].
  assert (Ha : vmem a B = true) by (apply vmem_true; apply Hsub; left; reflexivity).
  rewrite Ha. simpl. apply IH. intros z Hz. apply Hsub. right. exact Hz.
Qed.

(* ------------------------------------------------------------------ *)
(* Lookup rewriting for one binding                                     *)
(* ------------------------------------------------------------------ *)

Lemma lk_src_src : forall x z v rho,
  tlookup (Src z) ((Src x, v) :: rho) = if Nat.eq_dec x z then Some v else tlookup (Src z) rho.
Proof.
  intros. simpl. destruct (tvar_eq_dec (Src x) (Src z)) as [E|E].
  - inversion E; subst. destruct (Nat.eq_dec z z) as [e|e]; [reflexivity|].
    exfalso. apply e. reflexivity.
  - destruct (Nat.eq_dec x z) as [e|e]; [|reflexivity].
    subst. exfalso. apply E. reflexivity.
Qed.

Lemma lk_tmp_src : forall x z v rho, tlookup (Tmp z) ((Src x, v) :: rho) = tlookup (Tmp z) rho.
Proof. intros. simpl. destruct (tvar_eq_dec (Src x) (Tmp z)); [discriminate|reflexivity]. Qed.

Lemma lk_src_tmp : forall x z v rho, tlookup (Src z) ((Tmp x, v) :: rho) = tlookup (Src z) rho.
Proof. intros. simpl. destruct (tvar_eq_dec (Tmp x) (Src z)); [discriminate|reflexivity]. Qed.

Lemma lk_tmp_tmp : forall x z v rho,
  tlookup (Tmp z) ((Tmp x, v) :: rho) = if Nat.eq_dec x z then Some v else tlookup (Tmp z) rho.
Proof.
  intros. simpl. destruct (tvar_eq_dec (Tmp x) (Tmp z)) as [E|E].
  - inversion E; subst. destruct (Nat.eq_dec z z) as [e|e]; [reflexivity|].
    exfalso. apply e. reflexivity.
  - destruct (Nat.eq_dec x z) as [e|e]; [|reflexivity].
    subst. exfalso. apply E. reflexivity.
Qed.

Lemma rm_src_src : forall x z rho,
  tlookup (Src z) (tremove (Src x) rho) = if Nat.eq_dec x z then None else tlookup (Src z) rho.
Proof.
  intros. rewrite tlookup_tremove. destruct (tvar_eq_dec (Src x) (Src z)) as [E|E].
  - inversion E; subst. destruct (Nat.eq_dec z z) as [e|e]; [reflexivity|].
    exfalso. apply e. reflexivity.
  - destruct (Nat.eq_dec x z) as [e|e]; [|reflexivity].
    subst. exfalso. apply E. reflexivity.
Qed.

Lemma rm_tmp_src : forall x z rho, tlookup (Tmp z) (tremove (Src x) rho) = tlookup (Tmp z) rho.
Proof. intros. rewrite tlookup_tremove. destruct (tvar_eq_dec (Src x) (Tmp z)); [discriminate|reflexivity]. Qed.

Lemma rm_src_tmp : forall x z rho, tlookup (Src z) (tremove (Tmp x) rho) = tlookup (Src z) rho.
Proof. intros. rewrite tlookup_tremove. destruct (tvar_eq_dec (Tmp x) (Src z)); [discriminate|reflexivity]. Qed.

Lemma rm_tmp_tmp : forall x z rho,
  tlookup (Tmp z) (tremove (Tmp x) rho) = if Nat.eq_dec x z then None else tlookup (Tmp z) rho.
Proof.
  intros. rewrite tlookup_tremove. destruct (tvar_eq_dec (Tmp x) (Tmp z)) as [E|E].
  - inversion E; subst. destruct (Nat.eq_dec z z) as [e|e]; [reflexivity|].
    exfalso. apply e. reflexivity.
  - destruct (Nat.eq_dec x z) as [e|e]; [|reflexivity].
    subst. exfalso. apply E. reflexivity.
Qed.

Lemma tread_cons_tmp : forall x z v rho beta,
  tread (Src z) ((Tmp x, v) :: rho) beta = tread (Src z) rho beta.
Proof. intros. unfold tread. rewrite lk_src_tmp. reflexivity. Qed.

Lemma supd_other : forall sg x z v, x <> z -> supd sg x v z = sg z.
Proof. intros. unfold supd. destruct (Nat.eq_dec x z); [contradiction|reflexivity]. Qed.

Lemma supd_same : forall sg x v, supd sg x v x = Some v.
Proof. intros. unfold supd. destruct (Nat.eq_dec x x); [reflexivity|contradiction]. Qed.

(* ------------------------------------------------------------------ *)
(* Releases: drops and settle                                           *)
(* ------------------------------------------------------------------ *)

Lemma drops_exec : forall tf tp vs B rho beta H n R,
  NoDup vs ->
  (forall v, In v vs -> ~ In v B -> tlookup (Src v) rho <> None) ->
  (forall v, In v vs -> In v B -> tlookup (Src v) beta <> None) ->
  INV rho beta H n R ->
  exists rho' beta' H',
    texec tf tp rho beta H n (drops vs B) rho' beta' H' n [] /\ INV rho' beta' H' n R /\
    (forall v, tlookup (Src v) rho' =
       if in_dec Nat.eq_dec v vs then (if in_dec Nat.eq_dec v B then tlookup (Src v) rho else None)
       else tlookup (Src v) rho) /\
    (forall v, tlookup (Src v) beta' =
       if in_dec Nat.eq_dec v vs then (if in_dec Nat.eq_dec v B then None else tlookup (Src v) beta)
       else tlookup (Src v) beta) /\
    (forall v, tlookup (Tmp v) rho' = tlookup (Tmp v) rho) /\
    (forall v, tlookup (Tmp v) beta' = tlookup (Tmp v) beta).
Proof.
  intros tf tp vs B. induction vs as [|v r IH]; intros rho beta H n R HN Ho Hb Hinv; cbn [drops].
  - exists rho, beta, H. split; [constructor|]. split; [exact Hinv|].
    split; [intros w; reflexivity|]. split; [intros w; reflexivity|].
    split; intros w; reflexivity.
  - inversion HN as [|v' r' Hvr HNr]; subst.
    destruct (in_dec Nat.eq_dec v B) as [HvB|HvB].
    + (* borrowed: end it *)
      destruct (tlookup (Src v) beta) as [[c bs]|] eqn:Hv;
        [|exfalso; apply (Hb v); [left; reflexivity| exact HvB| exact Hv]].
      pose proof (inv_end rho beta H n R (Src v) (c, bs) Hinv Hv) as Hinv1.
      destruct (IH rho (tremove (Src v) beta) H n R HNr) as [rho' [beta' [H' [Hex [Hinv' [Ho' [Hb' [Tr' Tb']]]]]]]].
      * intros w Hw HwB. apply Ho; [right; exact Hw| exact HwB].
      * intros w Hw HwB. rewrite rm_src_src. destruct (Nat.eq_dec v w) as [E|E];
          [subst; contradiction|]. apply Hb; [right; exact Hw| exact HwB].
      * exact Hinv1.
      * exists rho', beta', H'. split; [|split; [exact Hinv'|split; [|split; [|split]]]].
        -- eapply TE_Seq; [eapply TE_End; exact Hv| exact Hex| reflexivity].
        -- intros w. rewrite Ho'.
           destruct (in_dec Nat.eq_dec w r) as [Hw|Hw];
             destruct (in_dec Nat.eq_dec w (v :: r)) as [Hw2|Hw2];
             destruct (in_dec Nat.eq_dec w B) as [HwB|HwB]; try reflexivity;
             try (exfalso; apply Hw2; right; exact Hw).
           destruct Hw2 as [E|E]; [subst w; contradiction| contradiction].
        -- intros w. rewrite Hb', rm_src_src.
           destruct (in_dec Nat.eq_dec w r) as [Hw|Hw];
             destruct (in_dec Nat.eq_dec w (v :: r)) as [Hw2|Hw2];
             destruct (in_dec Nat.eq_dec w B) as [HwB|HwB];
             destruct (Nat.eq_dec v w) as [E|E]; try reflexivity;
             try (exfalso; apply Hw2; first [right; exact Hw| left; exact E]);
             try (subst w; contradiction);
             try (destruct Hw2 as [E2|E2]; [contradiction| contradiction]).
        -- intros w. rewrite Tr'. reflexivity.
        -- intros w. rewrite Tb', rm_tmp_src. reflexivity.
    + (* owned: drop it *)
      destruct (tlookup (Src v) rho) as [[c bs]|] eqn:Hv;
        [|exfalso; apply (Ho v); [left; reflexivity| exact HvB| exact Hv]].
      destruct (inv_drop rho beta H n R (Src v) c bs Hinv Hv) as [Hincl [HNbs Hinv1]].
      destruct (IH (tremove (Src v) rho) beta (free bs H) n R HNr) as [rho' [beta' [H' [Hex [Hinv' [Ho' [Hb' [Tr' Tb']]]]]]]].
      * intros w Hw HwB. rewrite rm_src_src. destruct (Nat.eq_dec v w) as [E|E];
          [subst; contradiction|]. apply Ho; [right; exact Hw| exact HwB].
      * intros w Hw HwB. apply Hb; [right; exact Hw| exact HwB].
      * exact Hinv1.
      * exists rho', beta', H'. split; [|split; [exact Hinv'|split; [|split; [|split]]]].
        -- eapply TE_Seq; [eapply TE_Drop; eassumption| exact Hex| reflexivity].
        -- intros w. rewrite Ho', rm_src_src.
           destruct (in_dec Nat.eq_dec w r) as [Hw|Hw];
             destruct (in_dec Nat.eq_dec w (v :: r)) as [Hw2|Hw2];
             destruct (in_dec Nat.eq_dec w B) as [HwB|HwB];
             destruct (Nat.eq_dec v w) as [E|E]; try reflexivity;
             try (exfalso; apply Hw2; first [right; exact Hw| left; exact E]);
             try (subst w; contradiction);
             try (destruct Hw2 as [E2|E2]; [contradiction| contradiction]).
        -- intros w. rewrite Hb'.
           destruct (in_dec Nat.eq_dec w r) as [Hw|Hw];
             destruct (in_dec Nat.eq_dec w (v :: r)) as [Hw2|Hw2];
             destruct (in_dec Nat.eq_dec w B) as [HwB|HwB]; try reflexivity;
             try (exfalso; apply Hw2; right; exact Hw).
           destruct Hw2 as [E|E]; [subst w; contradiction| contradiction].
        -- intros w. rewrite Tr', rm_tmp_src. reflexivity.
        -- intros w. rewrite Tb'. reflexivity.
Qed.

Lemma settle_exec : forall tf tp A L B sg rho beta H n R,
  incl L A -> INV rho beta H n R -> CORR sg rho beta A B ->
  exists rho' beta' H', texec tf tp rho beta H n (settle A L B) rho' beta' H' n [] /\
                        INV rho' beta' H' n R /\ CORR sg rho' beta' L B.
Proof.
  intros tf tp A L B sg rho beta H n R Hsub Hinv [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  set (D := nodup Nat.eq_dec (vdiff A L)).
  assert (HD : forall v, In v D <-> In v A /\ ~ In v L).
  { intros v. subst D. rewrite nodup_In. apply vdiff_In. }
  destruct (drops_exec tf tp D B rho beta H n R) as [rho' [beta' [H' [Hex [Hinv' [Ho' [Hb' [Tr' Tb']]]]]]]].
  - apply NoDup_nodup.
  - intros v Hv HvB. apply Ho. split; [apply HD; exact Hv| exact HvB].
  - intros v Hv HvB. apply Hb. split; [apply HD; exact Hv| exact HvB].
  - exact Hinv.
  - exists rho', beta', H'. split; [exact Hex|]. split; [exact Hinv'|].
    split; [|split; [|split; [|split; [|split]]]].
    + intros x. rewrite Ho'. destruct (in_dec Nat.eq_dec x D) as [Hx|Hx].
      * apply HD in Hx. destruct (in_dec Nat.eq_dec x B) as [HxB|HxB].
        -- rewrite Ho. split; [intros [_ F]; contradiction| intros [_ F]; contradiction].
        -- split; [intros F; exfalso; apply F; reflexivity| intros [F _]; exfalso; apply Hx; exact F].
      * rewrite Ho. split.
        -- intros [Ha HaB]. split; [|exact HaB].
           destruct (in_dec Nat.eq_dec x L) as [HL|HL]; [exact HL|].
           exfalso. apply Hx. apply HD. split; assumption.
        -- intros [HL HLB]. split; [apply Hsub; exact HL| exact HLB].
    + intros x. rewrite Hb'. destruct (in_dec Nat.eq_dec x D) as [Hx|Hx].
      * apply HD in Hx. destruct (in_dec Nat.eq_dec x B) as [HxB|HxB].
        -- split; [intros F; exfalso; apply F; reflexivity| intros [F _]; exfalso; apply Hx; exact F].
        -- rewrite Hb. split; [intros [_ F]; contradiction| intros [_ F]; contradiction].
      * rewrite Hb. split.
        -- intros [Ha HaB]. split; [|exact HaB].
           destruct (in_dec Nat.eq_dec x L) as [HL|HL]; [exact HL|].
           exfalso. apply Hx. apply HD. split; assumption.
        -- intros [HL HLB]. split; [apply Hsub; exact HL| exact HLB].
    + intros x. rewrite Tr'. apply Tr.
    + intros x. rewrite Tb'. apply Tb.
    + intros x c bs Hl. rewrite Ho' in Hl.
      destruct (in_dec Nat.eq_dec x D); [destruct (in_dec Nat.eq_dec x B); [|discriminate]|];
        eapply Vo; exact Hl.
    + intros x c bs Hl. rewrite Hb' in Hl.
      destruct (in_dec Nat.eq_dec x D); [destruct (in_dec Nat.eq_dec x B); [discriminate|]|];
        eapply Vb; exact Hl.
Qed.

(* ------------------------------------------------------------------ *)
(* Boundary-correspondence lemmas                                       *)
(* ------------------------------------------------------------------ *)

Lemma corr_unbound : forall sg rho beta L B x,
  CORR sg rho beta L B -> ~ In x L -> unbound (Src x) rho beta.
Proof.
  intros sg rho beta L B x [Ho [Hb _]] Hx. split.
  - destruct (tlookup (Src x) rho) eqn:E; [|reflexivity]. exfalso.
    assert (Hin : In x L /\ ~ In x B) by (apply Ho; congruence). apply Hx. apply Hin.
  - destruct (tlookup (Src x) beta) eqn:E; [|reflexivity]. exfalso.
    assert (Hin : In x L /\ In x B) by (apply Hb; congruence). apply Hx. apply Hin.
Qed.

Lemma corr_read : forall sg rho beta L B x, CORR sg rho beta L B -> In x L ->
  exists c bs, tread (Src x) rho beta = Some (c, bs) /\ sg x = Some c.
Proof.
  intros sg rho beta L B x HC Hx. destruct HC as [Ho [Hb [_ [_ [Vo Vb]]]]].
  unfold tread. destruct (tlookup (Src x) rho) as [[c bs]|] eqn:E.
  - exists c, bs. split; [reflexivity| eapply Vo; exact E].
  - destruct (in_dec Nat.eq_dec x B) as [HB|HB].
    + destruct (tlookup (Src x) beta) as [[c bs]|] eqn:E2.
      * exists c, bs. split; [reflexivity| eapply Vb; exact E2].
      * exfalso. apply (proj2 (Hb x)); [split; assumption| exact E2].
    + exfalso. apply (proj2 (Ho x)); [split; assumption| exact E].
Qed.

Lemma corr_owned : forall sg rho beta L B x, CORR sg rho beta L B -> In x L -> ~ In x B ->
  exists c bs, tlookup (Src x) rho = Some (c, bs) /\ sg x = Some c.
Proof.
  intros sg rho beta L B x [Ho [_ [_ [_ [Vo _]]]]] Hx HxB.
  destruct (tlookup (Src x) rho) as [[c bs]|] eqn:E.
  - exists c, bs. split; [reflexivity| eapply Vo; exact E].
  - exfalso. apply (proj2 (Ho x)); [split; assumption| exact E].
Qed.

Lemma read_all_src : forall sg rho beta L B ys vs,
  CORR sg rho beta L B -> incl ys L -> slookup_all sg ys = Some vs ->
  exists tvs, tread_all (map Src ys) rho beta = Some tvs /\ map fst tvs = vs.
Proof.
  intros sg rho beta L B ys. induction ys as [|y r IH]; simpl; intros vs HC Hsub Hs.
  - inversion Hs; subst. exists []. split; reflexivity.
  - destruct (sg y) as [v|] eqn:Hy; [|discriminate].
    destruct (slookup_all sg r) as [vs'|] eqn:Hr; [|discriminate].
    inversion Hs; subst.
    destruct (corr_read sg rho beta L B y HC) as [c [bs [Hl Hc]]];
      [apply Hsub; left; reflexivity|].
    destruct (IH vs' HC) as [tvs [Htl Hm]];
      [intros z Hz; apply Hsub; right; exact Hz| reflexivity|].
    rewrite Hl, Htl. exists ((c, bs) :: tvs). split; [reflexivity|].
    simpl. rewrite Hm. congruence.
Qed.

(* Binding a fresh owned variable grows the boundary set by it. *)
Lemma corr_bind : forall sg rho beta L B x v bs,
  CORR sg rho beta L B -> ~ In x L -> ~ In x B ->
  CORR (supd sg x v) ((Src x, (v, bs)) :: rho) beta (x :: L) B.
Proof.
  intros sg rho beta L B x v bs [Ho [Hb [Tr [Tb [Vo Vb]]]]] HxL HxB.
  split; [|split; [|split; [|split; [|split]]]].
  - intros z. rewrite lk_src_src. destruct (Nat.eq_dec x z) as [E|E].
    + subst z. split; [intros _; split; [left; reflexivity| exact HxB]| discriminate].
    + rewrite Ho. simpl. split.
      * intros [Hz HzB]. split; [right; exact Hz| exact HzB].
      * intros [[Hz|Hz] HzB]; [contradiction| split; assumption].
  - intros z. rewrite Hb. simpl. split.
    + intros [Hz HzB]. split; [right; exact Hz| exact HzB].
    + intros [[Hz|Hz] HzB]; [subst z; contradiction| split; assumption].
  - intros z. rewrite lk_tmp_src. apply Tr.
  - exact Tb.
  - intros z c bs' Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec x z) as [E|E].
    + subst z. inversion Hl; subst. apply supd_same.
    + rewrite supd_other; [|exact E]. eapply Vo. exact Hl.
  - intros z c bs' Hl. destruct (Nat.eq_dec x z) as [E|E].
    + subst z. exfalso. assert (Hin : In x L /\ In x B) by (apply Hb; congruence).
      apply HxL. apply Hin.
    + rewrite supd_other; [|exact E]. eapply Vb. exact Hl.
Qed.

(* Re-binding an owned live variable keeps the boundary set. *)
Lemma corr_rebind : forall sg rho beta L B z v bs,
  CORR sg rho beta L B -> In z L -> ~ In z B ->
  CORR (supd sg z v) ((Src z, (v, bs)) :: tremove (Src z) rho) beta L B.
Proof.
  intros sg rho beta L B z v bs [Ho [Hb [Tr [Tb [Vo Vb]]]]] HzL HzB.
  split; [|split; [|split; [|split; [|split]]]].
  - intros w. rewrite lk_src_src. destruct (Nat.eq_dec z w) as [E|E].
    + subst w. split; [intros _; split; assumption| discriminate].
    + rewrite rm_src_src. destruct (Nat.eq_dec z w); [contradiction|]. apply Ho.
  - exact Hb.
  - intros w. rewrite lk_tmp_src, rm_tmp_src. apply Tr.
  - exact Tb.
  - intros w c bs' Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec z w) as [E|E].
    + subst w. inversion Hl; subst. apply supd_same.
    + rewrite supd_other; [|exact E]. rewrite rm_src_src in Hl.
      destruct (Nat.eq_dec z w); [contradiction|]. eapply Vo. exact Hl.
  - intros w c bs' Hl. destruct (Nat.eq_dec z w) as [E|E].
    + subst w. exfalso. assert (Hin : In z L /\ In z B) by (apply Hb; congruence).
      apply HzB. apply Hin.
    + rewrite supd_other; [|exact E]. eapply Vb. exact Hl.
Qed.

Lemma src_notin_lin : forall x ys L, ~ In x ys -> ~ In x (ys ++ vremove x L).
Proof.
  intros x ys L Hx Hin. apply in_app_or in Hin. destruct Hin as [Hin|Hin];
    [contradiction|]. apply vremove_In in Hin. apply Hin. reflexivity.
Qed.

Lemma notin_y_rm : forall x y L, x <> y -> ~ In x (y :: vremove x L).
Proof.
  intros x y L Hxy [E|Hin]; [congruence|]. apply vremove_In in Hin. apply Hin. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Core steps: each primitive establishes CORR on its post-set          *)
(* ------------------------------------------------------------------ *)

Lemma core_def : forall tf tp sg rho beta H n R x f ys L B vs,
  INV rho beta H n R -> CORR sg rho beta (ys ++ vremove x L) B -> ~ In x ys -> ~ In x B ->
  slookup_all sg ys = Some vs ->
  exists rho' H' n',
    texec tf tp rho beta H n (TDef (Src x) f (map Src ys)) rho' beta H' n' [] /\
    INV rho' beta H' n' R /\ CORR (supd sg x (f vs)) rho' beta (x :: ys ++ vremove x L) B.
Proof.
  intros tf tp sg rho beta H n R x f ys L B vs Hinv HC Hx HxB Hs.
  destruct (read_all_src sg rho beta _ B ys vs HC) as [tvs [Htl Hm]];
    [intros z Hz; apply in_or_app; left; exact Hz| exact Hs|].
  assert (Hub : unbound (Src x) rho beta)
    by (eapply corr_unbound; [exact HC| apply src_notin_lin; exact Hx]).
  exists ((Src x, (f (map fst tvs), (alloc n (vsize (f (map fst tvs)))))) :: rho), (alloc n (vsize (f (map fst tvs))) ++ H), (S n).
  split; [|split].
  - eapply TE_Def; [exact Hub| exact Htl| eapply forall_read; eassumption| eapply inv_fresh; exact Hinv].
  - apply inv_alloc; assumption.
  - rewrite Hm. apply corr_bind; [exact HC| apply src_notin_lin; exact Hx| exact HxB].
Qed.

Lemma core_copy : forall tf tp sg rho beta H n R x y L B v,
  INV rho beta H n R -> CORR sg rho beta (y :: vremove x L) B -> x <> y -> ~ In x B ->
  sg y = Some v ->
  exists rho' H' n',
    texec tf tp rho beta H n (TCopy (Src x) (Src y)) rho' beta H' n' [] /\
    INV rho' beta H' n' R /\ CORR (supd sg x v) rho' beta (x :: y :: vremove x L) B.
Proof.
  intros tf tp sg rho beta H n R x y L B v Hinv HC Hxy HxB Hv.
  destruct (corr_read sg rho beta _ B y HC) as [c [bs [Hl Hc]]]; [left; reflexivity|].
  assert (c = v) by congruence. subst c.
  assert (Hub : unbound (Src x) rho beta)
    by (eapply corr_unbound; [exact HC| apply notin_y_rm; exact Hxy]).
  exists ((Src x, (v, (alloc n (vsize v)))) :: rho), (alloc n (vsize v) ++ H), (S n). split; [|split].
  - eapply TE_Copy; [exact Hub| exact Hl| eapply inv_read; eassumption| eapply inv_fresh; exact Hinv].
  - apply inv_alloc; assumption.
  - apply corr_bind; [exact HC| apply notin_y_rm; exact Hxy| exact HxB].
Qed.

Lemma core_move : forall tf tp sg rho beta H n R x y L B v,
  INV rho beta H n R -> CORR sg rho beta (y :: vremove x L) B -> x <> y ->
  ~ In y L -> ~ In y B -> ~ In x B -> sg y = Some v ->
  exists rho', texec tf tp rho beta H n (TMove (Src x) (Src y)) rho' beta H n [] /\
    INV rho' beta H n R /\ CORR (supd sg x v) rho' beta (x :: vremove x L) B.
Proof.
  intros tf tp sg rho beta H n R x y L B v Hinv HC Hxy HyL HyB HxB Hv.
  destruct (corr_owned sg rho beta _ B y HC) as [c [bs [Hl Hc]]]; [left; reflexivity| exact HyB|].
  assert (c = v) by congruence. subst c.
  assert (Hub : unbound (Src x) rho beta)
    by (eapply corr_unbound; [exact HC| apply notin_y_rm; exact Hxy]).
  destruct HC as [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  exists ((Src x, (v, bs)) :: tremove (Src y) rho). split; [|split].
  - eapply TE_Move; eassumption.
  - eapply inv_move; eassumption.
  - split; [|split; [|split; [|split; [|split]]]].
    + intros z. rewrite lk_src_src. destruct (Nat.eq_dec x z) as [E|E].
      * subst z. split; [intros _; split; [left; reflexivity| exact HxB]| discriminate].
      * rewrite rm_src_src. destruct (Nat.eq_dec y z) as [E2|E2].
        -- subst z. split; [intros F; exfalso; apply F; reflexivity|].
           intros [[F|F] _]; [contradiction|]. apply vremove_In in F.
           exfalso. apply HyL. apply F.
        -- rewrite Ho. simpl. split.
           ++ intros [[F|F] HzB]; [contradiction| split; [right; exact F| exact HzB]].
           ++ intros [[F|F] HzB]; [contradiction| split; [right; exact F| exact HzB]].
    + intros z. rewrite Hb. simpl. split.
      * intros [[F|F] HzB]; [subst z; contradiction| split; [right; exact F| exact HzB]].
      * intros [[F|F] HzB]; [subst z; contradiction| split; [right; exact F| exact HzB]].
    + intros z. rewrite lk_tmp_src, rm_tmp_src. apply Tr.
    + exact Tb.
    + intros z c bs' Hl'. rewrite lk_src_src in Hl'. destruct (Nat.eq_dec x z) as [E|E].
      * subst z. inversion Hl'; subst. apply supd_same.
      * rewrite supd_other; [|exact E]. rewrite rm_src_src in Hl'.
        destruct (Nat.eq_dec y z); [discriminate|]. eapply Vo. exact Hl'.
    + intros z c bs' Hl'. destruct (Nat.eq_dec x z) as [E|E].
      * subst z. exfalso.
        assert (Hin : In x (y :: vremove x L) /\ In x B) by (apply Hb; congruence).
        apply HxB. apply Hin.
      * rewrite supd_other; [|exact E]. eapply Vb. exact Hl'.
Qed.

Lemma core_field : forall tf tp sg rho beta H n R x y i L B cs v,
  INV rho beta H n R -> CORR sg rho beta (y :: vremove x L) B -> x <> y -> ~ In x B ->
  sg y = Some (SNode cs) -> nth_error cs i = Some v ->
  exists rho' H' n',
    texec tf tp rho beta H n (TField (Src x) (Src y) i) rho' beta H' n' [] /\
    INV rho' beta H' n' R /\ CORR (supd sg x v) rho' beta (x :: y :: vremove x L) B.
Proof.
  intros tf tp sg rho beta H n R x y i L B cs v Hinv HC Hxy HxB Hy Hi.
  destruct (corr_read sg rho beta _ B y HC) as [c [bs [Hl Hc]]]; [left; reflexivity|].
  assert (c = SNode cs) by congruence. subst c.
  assert (Hub : unbound (Src x) rho beta)
    by (eapply corr_unbound; [exact HC| apply notin_y_rm; exact Hxy]).
  exists ((Src x, (v, (alloc n (vsize v)))) :: rho), (alloc n (vsize v) ++ H), (S n). split; [|split].
  - eapply TE_Field; [exact Hub| exact Hl| eapply inv_read; eassumption| exact Hi| eapply inv_fresh; exact Hinv].
  - apply inv_alloc; assumption.
  - apply corr_bind; [exact HC| apply notin_y_rm; exact Hxy| exact HxB].
Qed.

Lemma core_emit : forall tf tp sg rho beta H n R y L B v,
  INV rho beta H n R -> CORR sg rho beta (y :: L) B -> sg y = Some v ->
  texec tf tp rho beta H n (TEmit (Src y)) rho beta H n [v].
Proof.
  intros tf tp sg rho beta H n R y L B v Hinv HC Hv.
  destruct (corr_read sg rho beta _ B y HC) as [c [bs [Hl Hc]]]; [left; reflexivity|].
  assert (c = v) by congruence. subst c.
  eapply TE_Emit; [exact Hl| eapply inv_read; eassumption].
Qed.

(* Push of a moved element: the source y is dead after the push and owned. *)
Lemma core_push_move : forall tf tp sg rho beta H n R x y L B cs v,
  INV rho beta H n R -> CORR sg rho beta (x :: y :: L) B -> x <> y ->
  ~ In y L -> ~ In y B -> ~ In x B ->
  sg x = Some (SNode cs) -> sg y = Some v ->
  exists rho',
    texec tf tp rho beta H n (TPush (Src x) (Src y)) rho' beta H n [] /\
    INV rho' beta H n R /\ CORR (supd sg x (SNode (cs ++ [v]))) rho' beta (x :: L) B.
Proof.
  intros tf tp sg rho beta H n R x y L B cs v Hinv HC Hxy HyL HyB HxB Hx Hy.
  destruct (corr_owned sg rho beta _ B x HC) as [cx [bx [Hlx Hcx]]]; [left; reflexivity| exact HxB|].
  destruct (corr_owned sg rho beta _ B y HC) as [cy [by' [Hly Hcy]]]; [right; left; reflexivity| exact HyB|].
  assert (cx = SNode cs) by congruence. assert (cy = v) by congruence. subst cx cy.
  destruct HC as [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  exists ((Src x, (SNode (cs ++ [v]), bx ++ by')) :: tremove (Src x) (tremove (Src y) rho)).
  split; [|split].
  - change (SNode (cs ++ [v]), bx ++ by') with (SNode (cs ++ [fst (v, by')]), bx ++ snd (v, by')).
    eapply TE_Push; [congruence| exact Hlx| exact Hly].
  - change (SNode (cs ++ [v]), bx ++ by') with (SNode (cs ++ [fst (v, by')]), bx ++ snd (v, by')).
    eapply inv_push; [exact Hinv| congruence| exact Hlx| exact Hly].
  - split; [|split; [|split; [|split; [|split]]]].
    + intros z. rewrite lk_src_src. destruct (Nat.eq_dec x z) as [E|E].
      * subst. split; [intros _; split; [left; reflexivity| exact HxB]| discriminate].
      * rewrite rm_src_src. destruct (Nat.eq_dec x z) as [E'|E']; [contradiction|].
        rewrite rm_src_src. destruct (Nat.eq_dec y z) as [E2|E2].
        -- subst. split; [intros F; exfalso; apply F; reflexivity|].
           intros [[F|F] _]; [contradiction| contradiction].
        -- rewrite Ho. simpl. split.
           ++ intros [[F|[F|F]] HzB]; [contradiction| contradiction| split; [right; exact F| exact HzB]].
           ++ intros [[F|F] HzB]; [contradiction| split; [right; right; exact F| exact HzB]].
    + intros z. rewrite Hb. simpl. split.
      * intros [[F|[F|F]] HzB]; [subst z; contradiction| subst z; contradiction| split; [right; exact F| exact HzB]].
      * intros [[F|F] HzB]; [subst z; contradiction| split; [right; right; exact F| exact HzB]].
    + intros z. rewrite lk_tmp_src, rm_tmp_src, rm_tmp_src. apply Tr.
    + exact Tb.
    + intros z c bs Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec x z) as [E|E].
      * subst. inversion Hl; subst. apply supd_same.
      * rewrite supd_other; [|exact E]. rewrite rm_src_src in Hl.
        destruct (Nat.eq_dec x z); [contradiction|]. rewrite rm_src_src in Hl.
        destruct (Nat.eq_dec y z); [discriminate|]. eapply Vo. exact Hl.
    + intros z c bs Hl. destruct (Nat.eq_dec x z) as [E|E].
      * subst z. exfalso. assert (Hin : In x (x :: y :: L) /\ In x B) by (apply Hb; congruence).
        apply HxB. apply Hin.
      * rewrite supd_other; [|exact E]. eapply Vb. exact Hl.
Qed.

(* Push of a kept element: the compiler copies it into a temporary and moves
   the temporary into x. *)
Lemma core_push_copy : forall tf tp sg rho beta H n R x y L B cs v,
  INV rho beta H n R -> CORR sg rho beta (x :: y :: L) B -> x <> y -> ~ In x B ->
  sg x = Some (SNode cs) -> sg y = Some v ->
  exists rho' H' n',
    texec tf tp rho beta H n (TSeq (TCopy (Tmp y) (Src y)) (TPush (Src x) (Tmp y))) rho' beta H' n' [] /\
    INV rho' beta H' n' R /\ CORR (supd sg x (SNode (cs ++ [v]))) rho' beta (x :: y :: L) B.
Proof.
  intros tf tp sg rho beta H n R x y L B cs v Hinv HC Hxy HxB Hx Hy.
  destruct (corr_owned sg rho beta _ B x HC) as [cx [bx [Hlx Hcx]]]; [left; reflexivity| exact HxB|].
  destruct (corr_read sg rho beta _ B y HC) as [cy [by' [Hly Hcy]]]; [right; left; reflexivity|].
  assert (cx = SNode cs) by congruence. assert (cy = v) by congruence. subst cx cy.
  destruct HC as [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  set (rho1 := (Tmp y, (v, (alloc n (vsize v)))) :: rho).
  assert (Hub : unbound (Tmp y) rho beta) by (split; [apply Tr| apply Tb]).
  assert (Hinv1 : INV rho1 beta (alloc n (vsize v) ++ H) (S n) R) by (apply inv_alloc; assumption).
  assert (Hlx1 : tlookup (Src x) rho1 = Some (SNode cs, bx)) by (subst rho1; rewrite lk_src_tmp; exact Hlx).
  assert (Hly1 : tlookup (Tmp y) rho1 = Some (v, (alloc n (vsize v))))
    by (subst rho1; rewrite lk_tmp_tmp; destruct (Nat.eq_dec y y); [reflexivity|contradiction]).
  exists ((Src x, (SNode (cs ++ [v]), bx ++ (alloc n (vsize v)))) :: tremove (Src x) (tremove (Tmp y) rho1)),
         (alloc n (vsize v) ++ H), (S n).
  split; [|split].
  - eapply TE_Seq.
    + eapply TE_Copy; [exact Hub| exact Hly| eapply inv_read; eassumption| eapply inv_fresh; exact Hinv].
    + change (SNode (cs ++ [v]), bx ++ (alloc n (vsize v))) with (SNode (cs ++ [fst (v, (alloc n (vsize v)))]), bx ++ snd (v, (alloc n (vsize v)))).
      eapply TE_Push; [discriminate| exact Hlx1| exact Hly1].
    + reflexivity.
  - change (SNode (cs ++ [v]), bx ++ (alloc n (vsize v))) with (SNode (cs ++ [fst (v, (alloc n (vsize v)))]), bx ++ snd (v, (alloc n (vsize v)))).
    eapply inv_push; [exact Hinv1| discriminate| exact Hlx1| exact Hly1].
  - subst rho1. split; [|split; [|split; [|split; [|split]]]].
    + intros z. rewrite lk_src_src. destruct (Nat.eq_dec x z) as [E|E].
      * subst. split; [intros _; split; [left; reflexivity| exact HxB]| discriminate].
      * rewrite rm_src_src. destruct (Nat.eq_dec x z) as [E'|E']; [contradiction|].
        rewrite rm_src_tmp, lk_src_tmp. apply Ho.
    + exact Hb.
    + intros z. rewrite lk_tmp_src, rm_tmp_src, rm_tmp_tmp, lk_tmp_tmp.
      destruct (Nat.eq_dec y z); [reflexivity| apply Tr].
    + exact Tb.
    + intros z c bs Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec x z) as [E|E].
      * subst. inversion Hl; subst. apply supd_same.
      * rewrite supd_other; [|exact E]. rewrite rm_src_src in Hl.
        destruct (Nat.eq_dec x z); [contradiction|].
        rewrite rm_src_tmp, lk_src_tmp in Hl. eapply Vo. exact Hl.
    + intros z c bs Hl. destruct (Nat.eq_dec x z) as [E|E].
      * subst z. exfalso. assert (Hin : In x (x :: y :: L) /\ In x B) by (apply Hb; congruence).
        apply HxB. apply Hin.
      * rewrite supd_other; [|exact E]. eapply Vb. exact Hl.
Qed.

(* Copies of the kept pack arguments into compiler temporaries. *)
Lemma copies_exec : forall tf tp ws rho beta H n R,
  NoDup ws -> INV rho beta H n R ->
  (forall w, In w ws -> tread (Src w) rho beta <> None) ->
  (forall w, In w ws -> unbound (Tmp w) rho beta) ->
  exists rho' H' n',
    texec tf tp rho beta H n (copies ws) rho' beta H' n' [] /\ INV rho' beta H' n' R /\
    (forall v, tlookup (Src v) rho' = tlookup (Src v) rho) /\
    (forall w, In w ws -> exists c bs bs',
        tread (Src w) rho beta = Some (c, bs) /\ tlookup (Tmp w) rho' = Some (c, bs')) /\
    (forall w, ~ In w ws -> tlookup (Tmp w) rho' = tlookup (Tmp w) rho).
Proof.
  intros tf tp ws. induction ws as [|w r IH]; simpl; intros rho beta H n R HN Hinv Hs Ht.
  - exists rho, H, n. split; [constructor|]. split; [exact Hinv|].
    split; [reflexivity|]. split; [intros w []|]. intros w _. reflexivity.
  - inversion HN as [|w' r' Hwr HNr]; subst.
    destruct (tread (Src w) rho beta) as [[c bs]|] eqn:Hw;
      [|exfalso; apply (Hs w); [left; reflexivity| exact Hw]].
    set (rho1 := (Tmp w, (c, (alloc n (vsize c)))) :: rho).
    assert (Hinv1 : INV rho1 beta (alloc n (vsize c) ++ H) (S n) R)
      by (apply inv_alloc; [exact Hinv| apply Ht; left; reflexivity]).
    destruct (IH rho1 beta (alloc n (vsize c) ++ H) (S n) R HNr Hinv1) as [rho' [H' [n' [Hex [Hinv' [HS [HT HO]]]]]]].
    + intros v Hv. subst rho1. rewrite tread_cons_tmp. apply Hs. right. exact Hv.
    + intros v Hv. subst rho1. destruct (Ht v (or_intror Hv)) as [Hv1 Hv2]. split; [|exact Hv2].
      rewrite lk_tmp_tmp. destruct (Nat.eq_dec w v); [subst; contradiction| exact Hv1].
    + exists rho', H', n'. split; [|split; [exact Hinv'|split; [|split]]].
      * eapply TE_Seq;
          [ eapply TE_Copy; [apply Ht; left; reflexivity| exact Hw| eapply inv_read; eassumption| eapply inv_fresh; exact Hinv]
          | exact Hex
          | reflexivity ].
      * intros v. rewrite HS. subst rho1. apply lk_src_tmp.
      * intros v [Hv|Hv].
        -- subst v. exists c, bs, (alloc n (vsize c)). split; [exact Hw|]. rewrite HO; [|exact Hwr].
           subst rho1. rewrite lk_tmp_tmp. destruct (Nat.eq_dec w w); [reflexivity|contradiction].
        -- destruct (HT v Hv) as [c' [bs' [bs'' [Hv1 Hv2]]]].
           exists c', bs', bs''. split; [|exact Hv2]. subst rho1. rewrite tread_cons_tmp in Hv1. exact Hv1.
      * intros v Hv. rewrite HO; [|intros F; apply Hv; right; exact F].
        subst rho1. rewrite lk_tmp_tmp. destruct (Nat.eq_dec w v) as [E|E];
          [exfalso; apply Hv; left; exact E| reflexivity].
Qed.

Lemma pack_arg_in : forall L B ys z,
  In z (map (pack_arg L B) ys) <->
  (exists y, In y ys /\ ((keep L B y = true /\ z = Tmp y) \/ (keep L B y = false /\ z = Src y))).
Proof.
  intros L B ys z. rewrite in_map_iff. split.
  - intros [y [Hz Hy]]. exists y. split; [exact Hy|]. unfold pack_arg in Hz.
    destruct (keep L B y); [left|right]; split; auto.
  - intros [y [Hy [[HL Hz]|[HL Hz]]]]; exists y; split; auto; unfold pack_arg;
      rewrite HL; congruence.
Qed.

Lemma nodup_pack_args : forall L B ys, NoDup ys -> NoDup (map (pack_arg L B) ys).
Proof.
  intros L B ys HN. induction HN as [|y r Hy HN IH]; simpl; constructor; [|exact IH].
  intros Hin. apply pack_arg_in in Hin. destruct Hin as [y' [Hy' Hk]].
  unfold pack_arg in Hk. destruct (keep L B y);
    destruct Hk as [[_ E]|[_ E]]; inversion E; subst; contradiction.
Qed.

Lemma core_pack : forall tf tp sg rho beta H n R x ys L B vs,
  INV rho beta H n R -> CORR sg rho beta (ys ++ vremove x L) B -> ~ In x ys -> ~ In x B ->
  NoDup ys -> slookup_all sg ys = Some vs ->
  exists rho' H' n',
    texec tf tp rho beta H n (TSeq (copies (filter (keep L B) ys))
                                    (TPack (Src x) (map (pack_arg L B) ys))) rho' beta H' n' [] /\
    INV rho' beta H' n' R /\
    CORR (supd sg x (SNode vs)) rho' beta (x :: filter (keep L B) ys ++ vremove x L) B.
Proof.
  intros tf tp sg rho beta H n R x ys L B vs Hinv HC Hx HxB HNy Hs.
  pose proof HC as [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  set (ws := filter (keep L B) ys).
  assert (Hws : forall w, In w ws <-> In w ys /\ keep L B w = true).
  { intros w. subst ws. apply filter_In. }
  destruct (copies_exec tf tp ws rho beta H n R) as [rho1 [H1 [n1 [Hex1 [Hinv1 [HS [HT HO]]]]]]].
  - subst ws. apply NoDup_filter'. exact HNy.
  - exact Hinv.
  - intros w Hw. destruct (corr_read sg rho beta _ B w HC) as [c [bs [Hl _]]];
      [apply in_or_app; left; apply Hws; exact Hw| congruence].
  - intros w _. split; [apply Tr| apply Tb].
  - assert (Hargs : forall y, In y ys -> exists c bs,
               tlookup (pack_arg L B y) rho1 = Some (c, bs) /\ sg y = Some c).
    { intros y Hy. unfold pack_arg. destruct (keep L B y) eqn:Hk.
      - destruct (HT y (proj2 (Hws y) (conj Hy Hk))) as [c [bs [bs' [H1y H2y]]]].
        exists c, bs'. split; [exact H2y|].
        destruct (corr_read sg rho beta _ B y HC) as [c' [bs'' [Hl Hc]]];
          [apply in_or_app; left; exact Hy|]. congruence.
      - destruct (keep_false L B y Hk) as [HyL HyB].
        destruct (corr_owned sg rho beta _ B y HC) as [c [bs [Hl Hc]]];
          [apply in_or_app; left; exact Hy| exact HyB|].
        exists c, bs. split; [rewrite HS; exact Hl| exact Hc]. }
    assert (Hall : forall ys' vs', incl ys' ys -> slookup_all sg ys' = Some vs' ->
               exists tvs, tlookup_all (map (pack_arg L B) ys') rho1 = Some tvs /\ map fst tvs = vs').
    { induction ys' as [|y r IH]; simpl; intros vs' Hsub Hs'.
      - inversion Hs'; subst. exists []. split; reflexivity.
      - destruct (sg y) as [v|] eqn:Hy; [|discriminate].
        destruct (slookup_all sg r) as [vr|] eqn:Hr; [|discriminate].
        inversion Hs'; subst.
        destruct (Hargs y (Hsub y (or_introl eq_refl))) as [c [bs [Hl Hc]]].
        destruct (IH vr (fun z Hz => Hsub z (or_intror Hz)) eq_refl) as [tvs [Htl Hm]].
        rewrite Hl, Htl. exists ((c, bs) :: tvs). split; [reflexivity|]. simpl. rewrite Hm. congruence. }
    destruct (Hall ys vs (incl_refl ys) Hs) as [tvs [Htl Hm]].
    assert (Hub1 : unbound (Src x) rho1 beta).
    { destruct (corr_unbound sg rho beta _ B x HC (src_notin_lin x ys L Hx)) as [U1 U2].
      split; [rewrite HS; exact U1| exact U2]. }
    exists ((Src x, (SNode (map fst tvs), (n1, 0) :: flat_map snd tvs)) :: tremove_all (map (pack_arg L B) ys) rho1),
           ((n1, 0) :: H1), (S n1).
    split; [|split].
    + eapply TE_Seq; [exact Hex1| | reflexivity].
      eapply TE_Pack; [exact Hub1| apply nodup_pack_args; exact HNy| exact Htl| eapply inv_fresh; exact Hinv1].
    + apply inv_pack; [exact Hinv1| exact Hub1| apply nodup_pack_args; exact HNy| exact Htl].
    + rewrite Hm. split; [|split; [|split; [|split; [|split]]]].
      * intros z. rewrite lk_src_src. destruct (Nat.eq_dec x z) as [E|E].
        -- subst. split; [intros _; split; [left; reflexivity| exact HxB]| discriminate].
        -- rewrite tlookup_tremove_all.
           destruct (in_dec tvar_eq_dec (Src z) (map (pack_arg L B) ys)) as [Hp|Hp].
           ++ apply pack_arg_in in Hp. destruct Hp as [y [Hy [[Hk F]|[Hk F]]]]; [discriminate|].
              inversion F; subst y. destruct (keep_false L B z Hk) as [HzL HzB].
              split; [intros F2; exfalso; apply F2; reflexivity|].
              intros [[F2|F2] _]; [contradiction|]. apply in_app_or in F2. destruct F2 as [F2|F2].
              ** apply filter_In in F2. destruct F2 as [_ F2]. rewrite Hk in F2. discriminate.
              ** apply vremove_In in F2. exfalso. apply HzL. apply F2.
           ++ rewrite HS, Ho. simpl. split.
              ** intros [Hin HzB]. split; [|exact HzB]. right. apply in_app_or in Hin.
                 destruct Hin as [Hin|Hin]; [|apply in_or_app; right; exact Hin].
                 apply in_or_app. left. apply filter_In. split; [exact Hin|].
                 destruct (keep L B z) eqn:Hk; [reflexivity|].
                 exfalso. apply Hp. apply pack_arg_in. exists z. split; [exact Hin|]. right. split; [exact Hk| reflexivity].
              ** intros [[F|Hin] HzB]; [contradiction|]. split; [|exact HzB].
                 apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                 --- apply filter_In in Hin. apply in_or_app. left. apply Hin.
                 --- apply in_or_app. right. exact Hin.
      * intros z. rewrite Hb. simpl. split.
        -- intros [Hin HzB]. split; [|exact HzB]. right. apply in_app_or in Hin.
           destruct Hin as [Hin|Hin]; [|apply in_or_app; right; exact Hin].
           apply in_or_app. left. apply filter_In. split; [exact Hin|].
           apply keep_true. right. exact HzB.
        -- intros [[F|Hin] HzB]; [subst z; contradiction|]. split; [|exact HzB].
           apply in_app_or in Hin. destruct Hin as [Hin|Hin].
           ++ apply filter_In in Hin. apply in_or_app. left. apply Hin.
           ++ apply in_or_app. right. exact Hin.
      * intros z. rewrite lk_tmp_src, tlookup_tremove_all.
        destruct (in_dec tvar_eq_dec (Tmp z) (map (pack_arg L B) ys)) as [Hp|Hp]; [reflexivity|].
        destruct (in_dec Nat.eq_dec z ws) as [Hz|Hz].
        -- exfalso. apply Hp. apply pack_arg_in. exists z. apply Hws in Hz.
           split; [apply Hz|]. left. split; [apply Hz|reflexivity].
        -- rewrite HO; [apply Tr|exact Hz].
      * exact Tb.
      * intros z c bs Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec x z) as [E|E].
        -- subst. inversion Hl; subst. apply supd_same.
        -- rewrite supd_other; [|exact E]. rewrite tlookup_tremove_all in Hl.
           destruct (in_dec tvar_eq_dec (Src z) (map (pack_arg L B) ys)); [discriminate|].
           rewrite HS in Hl. eapply Vo. exact Hl.
      * intros z c bs Hl. destruct (Nat.eq_dec x z) as [E|E].
        -- subst z. exfalso.
           assert (Hin : In x (ys ++ vremove x L) /\ In x B) by (apply Hb; congruence).
           apply HxB. apply Hin.
        -- rewrite supd_other; [|exact E]. eapply Vb. exact Hl.
Qed.

(* ------------------------------------------------------------------ *)
(* Call frames                                                          *)
(* ------------------------------------------------------------------ *)

Lemma tbind_cons : forall p ps tv tvs, tbind (p :: ps) (tv :: tvs) = (Src p, tv) :: tbind ps tvs.
Proof. reflexivity. Qed.

Lemma tbind_dom : forall ps tvs, length ps = length tvs -> dom (tbind ps tvs) = map Src ps.
Proof.
  induction ps as [|p ps IH]; intros tvs E; destruct tvs as [|tv tvs]; simpl in *;
    try discriminate; [reflexivity|].
  f_equal. apply IH. lia.
Qed.

Lemma NoDup_map_Src : forall ps, NoDup ps -> NoDup (map Src ps).
Proof.
  intros ps HN. induction HN as [|p ps Hp HN IH]; simpl; constructor; [|exact IH].
  intros Hin. apply in_map_iff in Hin. destruct Hin as [q [E Hq]]. inversion E; subst. contradiction.
Qed.

Lemma tbind_tmp : forall ps tvs x, tlookup (Tmp x) (tbind ps tvs) = None.
Proof.
  induction ps as [|p ps IH]; intros tvs x; destruct tvs as [|tv tvs]; try reflexivity.
  rewrite tbind_cons, lk_tmp_src. apply IH.
Qed.

Lemma tbind_src_bound : forall ps tvs x, length ps = length tvs ->
  (tlookup (Src x) (tbind ps tvs) <> None <-> In x ps).
Proof.
  induction ps as [|p ps IH]; intros tvs x E; destruct tvs as [|tv tvs]; simpl in E;
    try discriminate.
  - simpl. split; [intros F; exfalso; apply F; reflexivity| intros []].
  - rewrite tbind_cons, lk_src_src. destruct (Nat.eq_dec p x) as [Epx|Epx].
    + subst. split; [intros _; left; reflexivity| discriminate].
    + rewrite IH; [|lia]. simpl. split; [intros Hin; right; exact Hin|].
      intros [F|F]; [contradiction|exact F].
Qed.

Lemma tbind_src_value : forall ps tvs x c bs, length ps = length tvs ->
  tlookup (Src x) (tbind ps tvs) = Some (c, bs) -> sbind ps (map fst tvs) x = Some c.
Proof.
  induction ps as [|p ps IH]; intros tvs x c bs E Hl; destruct tvs as [|tv tvs]; simpl in E;
    try discriminate; try (simpl in Hl; discriminate).
  rewrite tbind_cons, lk_src_src in Hl.
  change (sbind (p :: ps) (map fst (tv :: tvs)) x) with (supd (sbind ps (map fst tvs)) p (fst tv) x).
  unfold supd. destruct (Nat.eq_dec p x) as [Epx|Epx].
  - inversion Hl; subst. reflexivity.
  - eapply IH; [lia| exact Hl].
Qed.

Lemma tbind_value_in : forall ps tvs z v, tlookup z (tbind ps tvs) = Some v -> In v tvs.
Proof.
  induction ps as [|p ps IH]; intros tvs z v Hl; destruct tvs as [|tv tvs];
    try (simpl in Hl; discriminate).
  rewrite tbind_cons, tlookup_cons in Hl. destruct (tvar_eq_dec (Src p) z).
  - inversion Hl; subst. left. reflexivity.
  - right. eapply IH. exact Hl.
Qed.

Lemma corr_exit_shape : forall sg rho beta ret B,
  CORR sg rho beta [ret] B -> ~ In ret B -> NoDup (dom rho) ->
  beta = [] /\ exists c bs, rho = [(Src ret, (c, bs))] /\ sg ret = Some c.
Proof.
  intros sg rho beta ret B [Ho [Hb [Tr [Tb [Vo Vb]]]]] HrB HN. split.
  - destruct beta as [|[k v] r]; [reflexivity|]. exfalso. destruct k as [y|y].
    + assert (Hin : In y [ret] /\ In y B).
      { apply Hb. rewrite lk_src_src. destruct (Nat.eq_dec y y) as [_|F]; [discriminate| exfalso; apply F; reflexivity]. }
      destruct Hin as [[E|[]] HyB]. subst y. contradiction.
    + specialize (Tb y). rewrite lk_tmp_tmp in Tb.
      destruct (Nat.eq_dec y y) as [_|F]; [discriminate| apply F; reflexivity].
  - destruct rho as [|[k v] r].
    + exfalso. apply (proj2 (Ho ret)); [split; [left; reflexivity| exact HrB]| reflexivity].
    + destruct k as [y|y].
      * assert (Hy : In y [ret] /\ ~ In y B).
        { apply Ho. rewrite lk_src_src. destruct (Nat.eq_dec y y) as [_|F]; [discriminate| exfalso; apply F; reflexivity]. }
        destruct Hy as [[E|[]] _]. subst y.
        destruct r as [|[k' v'] r'].
        -- destruct v as [c bs]. exists c, bs. split; [reflexivity|]. eapply Vo.
           rewrite lk_src_src. destruct (Nat.eq_dec ret ret) as [_|F]; [reflexivity| exfalso; apply F; reflexivity].
        -- exfalso. simpl in HN. inversion HN as [|a l Ha HN']; subst.
           destruct k' as [w|w].
           ++ assert (Hw : In w [ret] /\ ~ In w B).
              { apply Ho. rewrite lk_src_src. destruct (Nat.eq_dec ret w) as [E|E].
                - discriminate.
                - rewrite lk_src_src. destruct (Nat.eq_dec w w) as [_|F]; [discriminate| exfalso; apply F; reflexivity]. }
              destruct Hw as [[E|[]] _]. subst w. apply Ha. left. reflexivity.
           ++ specialize (Tr w). rewrite lk_tmp_src, lk_tmp_tmp in Tr.
              destruct (Nat.eq_dec w w) as [_|F]; [discriminate| apply F; reflexivity].
      * specialize (Tr y). rewrite lk_tmp_tmp in Tr.
        destruct (Nat.eq_dec y y) as [_|F]; [discriminate| exfalso; apply F; reflexivity].
Qed.

(* ------------------------------------------------------------------ *)
(* Parameter modes: splitting a list into its sink and borrowed parts  *)
(* ------------------------------------------------------------------ *)

Lemma sinks_borrows_perm : forall (A : Type) (ms : list bool) (l : list A),
  Permutation l (sinks ms l ++ borrows ms l).
Proof.
  intros A ms l. revert ms. induction l as [|a r IH]; intros ms; [simpl; constructor|].
  destruct ms as [|[|] ms']; simpl.
  - eapply Permutation_trans; [apply perm_skip; apply (IH [])|]. apply Permutation_middle.
  - apply perm_skip. apply IH.
  - eapply Permutation_trans; [apply perm_skip; apply IH|]. apply Permutation_middle.
Qed.

Lemma in_sinks : forall (A : Type) (ms : list bool) (l : list A) a, In a (sinks ms l) -> In a l.
Proof.
  intros A ms l a Hin.
  eapply Permutation_in; [apply Permutation_sym; apply (sinks_borrows_perm A ms l)|].
  apply in_or_app. left. exact Hin.
Qed.

Lemma in_borrows : forall (A : Type) (ms : list bool) (l : list A) a, In a (borrows ms l) -> In a l.
Proof.
  intros A ms l a Hin.
  eapply Permutation_in; [apply Permutation_sym; apply (sinks_borrows_perm A ms l)|].
  apply in_or_app. right. exact Hin.
Qed.

Lemma in_split_args : forall (A : Type) (ms : list bool) (l : list A) a,
  In a l -> In a (sinks ms l) \/ In a (borrows ms l).
Proof.
  intros A ms l a Hin. apply in_app_or.
  eapply Permutation_in; [apply sinks_borrows_perm| exact Hin].
Qed.

Lemma nodup_split : forall (A : Type) (ms : list bool) (l : list A),
  NoDup l -> NoDup (sinks ms l ++ borrows ms l).
Proof. intros A ms l HN. eapply Permutation_NoDup; [apply sinks_borrows_perm| exact HN]. Qed.

Lemma sinks_not_borrows : forall (A : Type) (ms : list bool) (l : list A) a,
  NoDup l -> In a (sinks ms l) -> ~ In a (borrows ms l).
Proof.
  intros A ms l a HN Hs.
  destruct (NoDup_app_split _ _ _ (nodup_split A ms l HN)) as [_ [_ Hd]].
  apply Hd. exact Hs.
Qed.

Lemma sinks_length : forall (A1 A2 : Type) (ms : list bool) (l1 : list A1) (l2 : list A2),
  length l1 = length l2 -> length (sinks ms l1) = length (sinks ms l2).
Proof.
  intros A1 A2 ms l1. revert ms. induction l1 as [|a r IH]; intros ms l2 E;
    destruct l2 as [|b r2]; simpl in E; try discriminate; [reflexivity|].
  destruct ms as [|[|] ms']; simpl; [| f_equal |]; apply IH; lia.
Qed.

Lemma borrows_length : forall (A1 A2 : Type) (ms : list bool) (l1 : list A1) (l2 : list A2),
  length l1 = length l2 -> length (borrows ms l1) = length (borrows ms l2).
Proof.
  intros A1 A2 ms l1. revert ms. induction l1 as [|a r IH]; intros ms l2 E;
    destruct l2 as [|b r2]; simpl in E; try discriminate; [reflexivity|].
  destruct ms as [|[|] ms']; simpl; [f_equal | | f_equal]; apply IH; lia.
Qed.

Lemma slookup_all_sinks : forall sg (ms : list bool) ys vs,
  slookup_all sg ys = Some vs -> slookup_all sg (sinks ms ys) = Some (sinks ms vs).
Proof.
  intros sg ms ys. revert ms. induction ys as [|y r IH]; intros ms vs Hs.
  - simpl in Hs. inversion Hs; subst. reflexivity.
  - simpl in Hs. destruct (sg y) as [v|] eqn:Hy; [|discriminate].
    destruct (slookup_all sg r) as [vr|] eqn:Hr; [|discriminate].
    inversion Hs; subst.
    destruct ms as [|[|] ms']; simpl;
      [apply IH; reflexivity| rewrite Hy, (IH ms' vr eq_refl); reflexivity| apply IH; reflexivity].
Qed.

Lemma slookup_all_borrows : forall sg (ms : list bool) ys vs,
  slookup_all sg ys = Some vs -> slookup_all sg (borrows ms ys) = Some (borrows ms vs).
Proof.
  intros sg ms ys. revert ms. induction ys as [|y r IH]; intros ms vs Hs.
  - simpl in Hs. inversion Hs; subst. reflexivity.
  - simpl in Hs. destruct (sg y) as [v|] eqn:Hy; [|discriminate].
    destruct (slookup_all sg r) as [vr|] eqn:Hr; [|discriminate].
    inversion Hs; subst.
    destruct ms as [|[|] ms']; simpl;
      [rewrite Hy, (IH [] vr eq_refl); reflexivity| apply IH; reflexivity| rewrite Hy, (IH ms' vr eq_refl); reflexivity].
Qed.

Lemma sbind_in : forall ps vs x c, sbind ps vs x = Some c -> In x ps.
Proof.
  induction ps as [|p ps IH]; intros vs x c Hl; destruct vs as [|v vs];
    simpl in Hl; try discriminate.
  try unfold supd in Hl. destruct (Nat.eq_dec p x) as [E|E]; [left; exact E|].
  right. eapply IH. exact Hl.
Qed.

Lemma sbind_sinks : forall ms ps vs x c, NoDup ps -> length ps = length vs ->
  sbind (sinks ms ps) (sinks ms vs) x = Some c -> sbind ps vs x = Some c.
Proof.
  intros ms ps. revert ms. induction ps as [|p ps IH]; intros ms vs x c HN E Hl;
    destruct vs as [|v vs]; simpl in E; try discriminate; try exact Hl.
  inversion HN as [|p' ps' Hp HN']; subst.
  change (sbind (p :: ps) (v :: vs) x) with (supd (sbind ps vs) p v x). unfold supd.
  destruct ms as [|[|] ms']; simpl in Hl; try unfold supd in Hl.
  - destruct (Nat.eq_dec p x) as [Epx|Epx].
    + subst x. exfalso. apply Hp. eapply in_sinks. eapply sbind_in. exact Hl.
    + eapply IH; [exact HN'| lia| exact Hl].
  - destruct (Nat.eq_dec p x) as [Epx|Epx]; [exact Hl|].
    eapply IH; [exact HN'| lia| exact Hl].
  - destruct (Nat.eq_dec p x) as [Epx|Epx].
    + subst x. exfalso. apply Hp. eapply in_sinks. eapply sbind_in. exact Hl.
    + eapply IH; [exact HN'| lia| exact Hl].
Qed.

Lemma sbind_borrows : forall ms ps vs x c, NoDup ps -> length ps = length vs ->
  sbind (borrows ms ps) (borrows ms vs) x = Some c -> sbind ps vs x = Some c.
Proof.
  intros ms ps. revert ms. induction ps as [|p ps IH]; intros ms vs x c HN E Hl;
    destruct vs as [|v vs]; simpl in E; try discriminate; try exact Hl.
  inversion HN as [|p' ps' Hp HN']; subst.
  change (sbind (p :: ps) (v :: vs) x) with (supd (sbind ps vs) p v x). unfold supd.
  destruct ms as [|[|] ms']; simpl in Hl; try unfold supd in Hl.
  - destruct (Nat.eq_dec p x) as [Epx|Epx]; [exact Hl|].
    eapply IH; [exact HN'| lia| exact Hl].
  - destruct (Nat.eq_dec p x) as [Epx|Epx].
    + subst x. exfalso. apply Hp. eapply in_borrows. eapply sbind_in. exact Hl.
    + eapply IH; [exact HN'| lia| exact Hl].
  - destruct (Nat.eq_dec p x) as [Epx|Epx]; [exact Hl|].
    eapply IH; [exact HN'| lia| exact Hl].
Qed.

Lemma tbind_heap : forall ps tvs, length ps = length tvs ->
  heap_of (tbind ps tvs) = flat_map snd tvs.
Proof.
  induction ps as [|p ps IH]; intros tvs E; destruct tvs as [|tv tvs]; simpl in E;
    try discriminate; [reflexivity|].
  rewrite tbind_cons. unfold heap_of in *. simpl. f_equal. apply IH. lia.
Qed.

(* ------------------------------------------------------------------ *)
(* Call frames                                                          *)
(* ------------------------------------------------------------------ *)

(* Entering a callee: the moved sink values become the callee's owned
   footprint, and everything the caller still owns joins the frame heap. *)
Lemma laid_tremove : forall x rho, laid rho -> laid (tremove x rho).
Proof.
  intros x rho Lr z c bs Hz. rewrite tlookup_tremove in Hz.
  destruct (tvar_eq_dec x z); [discriminate| eapply Lr; exact Hz].
Qed.

Lemma laid_tremove_all : forall ys rho, laid rho -> laid (tremove_all ys rho).
Proof.
  intros ys rho Lr z c bs Hz. rewrite tlookup_tremove_all in Hz.
  destruct (in_dec tvar_eq_dec z ys); [discriminate| eapply Lr; exact Hz].
Qed.

Lemma laid_cons : forall x c bs rho, length bs = vsize c -> laid rho -> laid ((x, (c, bs)) :: rho).
Proof.
  intros x c bs rho E Lr z c' bs' Hz. rewrite tlookup_cons in Hz. destruct (tvar_eq_dec x z).
  - inversion Hz; subst. exact E.
  - eapply Lr. exact Hz.
Qed.

Lemma laid_tbind : forall ps tvs, Forall (fun v => length (snd v) = vsize (fst v)) tvs ->
  laid (tbind ps tvs).
Proof.
  intros ps tvs HF z c bs Hz. apply tbind_value_in in Hz.
  rewrite Forall_forall in HF. exact (HF (c, bs) Hz).
Qed.

(* Entering a callee: the moved sink values become the callee's owned
   footprint, and everything the caller still owns joins the frame heap. *)
Lemma inv_call_frame : forall rho beta H n R sys bys svs bvs sps bps,
  INV rho beta H n R -> NoDup sys -> tlookup_all sys rho = Some svs ->
  tread_all bys (tremove_all sys rho) beta = Some bvs ->
  NoDup (sps ++ bps) -> length sps = length svs -> length bps = length bvs ->
  INV (tbind sps svs) (tbind bps bvs) H n (heap_of (tremove_all sys rho) ++ R).
Proof.
  intros rho beta H n R sys bys svs bvs sps bps Hinv HNs Hl Hr HNp E1 E2.
  pose proof Hinv as [HN [HH [HP [Hf [Hb [Lr Lb]]]]]].
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  split; [|split; [exact HH|split; [|split; [exact Hf|split; [|split]]]]].
  - rewrite (tbind_dom sps svs E1), (tbind_dom bps bvs E2), <- map_app.
    apply NoDup_map_Src. exact HNp.
  - rewrite (tbind_heap sps svs E1). eapply Permutation_trans; [exact HP|].
    rewrite app_assoc. apply Permutation_app_tail. eapply heap_tremove_all; eassumption.
  - intros z c bs Hz. apply tbind_value_in in Hz.
    pose proof (forall_read_frame (tremove_all sys rho) beta R bys bvs Hb Hr) as HF.
    rewrite Forall_forall in HF. apply (HF (c, bs) Hz).
  - apply laid_tbind. apply Forall_forall. intros [c bs] Hin.
    destruct (tlookup_all_in sys rho svs Hl _ Hin) as [y Hy]. exact (Lr _ _ _ Hy).
  - apply laid_tbind. apply Forall_forall. intros [c bs] Hin.
    destruct (tread_all_in bys _ beta bvs Hr _ Hin) as [y Hy].
    exact (laid_tread _ _ _ _ _ (laid_tremove_all sys rho Lr) Lb Hy).
Qed.

(* Returning from a callee: its result joins the caller's owned footprint. *)
Lemma inv_call_return : forall rho beta H n R sys x ret v H' n',
  INV rho beta H n R -> unbound x (tremove_all sys rho) beta ->
  INV [(Src ret, v)] [] H' n' (heap_of (tremove_all sys rho) ++ R) ->
  INV ((x, v) :: tremove_all sys rho) beta H' n' R.
Proof.
  intros rho beta H n R sys x ret [c bs] H' n' [HN [_ [_ [_ [Hb [Lr Lb]]]]]] Hub
    [_ [HH' [HP' [Hf' [_ [Lr' _]]]]]].
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  split; [|split; [exact HH'|split; [|split; [exact Hf'|split; [exact Hb|split; [|exact Lb]]]]]].
  - apply doms_cons; [exact Hub|].
    eapply doms_sub; [exact HN| apply nodup_dom_tremove_all; exact HNr| eapply doms_r; exact HN| |].
    + intros z Hz. eapply in_dom_tremove_all. exact Hz.
    + intros z Hz. exact Hz.
  - simpl. simpl in HP'. rewrite app_nil_r in HP'. rewrite <- app_assoc. exact HP'.
  - apply laid_cons; [| apply laid_tremove_all; exact Lr].
    apply (Lr' (Src ret)). rewrite lk_src_src. destruct (Nat.eq_dec ret ret) as [_|F]; [reflexivity| contradiction].
Qed.

(* The callee frame corresponds to the source frame: sink parameters are
   owned, borrowed parameters are lent, with the arguments' contents. *)
Lemma corr_entry_routine : forall ms qs vs svs bvs,
  NoDup qs -> length qs = length vs ->
  map fst svs = sinks ms vs -> map fst bvs = borrows ms vs ->
  CORR (sbind qs vs) (tbind (sinks ms qs) svs) (tbind (borrows ms qs) bvs) qs (borrows ms qs).
Proof.
  intros ms qs vs svs bvs HN E Hs Hb.
  assert (Es : length (sinks ms qs) = length svs)
    by (rewrite <- (length_map fst svs), Hs; apply sinks_length; exact E).
  assert (Eb : length (borrows ms qs) = length bvs)
    by (rewrite <- (length_map fst bvs), Hb; apply borrows_length; exact E).
  split; [|split; [|split; [|split; [|split]]]].
  - intros x. rewrite tbind_src_bound; [|exact Es]. split.
    + intros Hx. split; [eapply in_sinks; exact Hx| apply sinks_not_borrows; assumption].
    + intros [Hx HxB]. destruct (in_split_args _ ms qs x Hx) as [H1|H1]; [exact H1| contradiction].
  - intros x. rewrite tbind_src_bound; [|exact Eb]. split.
    + intros Hx. split; [eapply in_borrows; exact Hx| exact Hx].
    + intros [_ Hx]. exact Hx.
  - intros x. apply tbind_tmp.
  - intros x. apply tbind_tmp.
  - intros x c bs Hl. apply tbind_src_value in Hl; [|exact Es]. rewrite Hs in Hl.
    eapply sbind_sinks; [exact HN| exact E| exact Hl].
  - intros x c bs Hl. apply tbind_src_value in Hl; [|exact Eb]. rewrite Hb in Hl.
    eapply sbind_borrows; [exact HN| exact E| exact Hl].
Qed.

Lemma elab_routine_spec : forall M ps ms body ret d',
  elab_routine M ps ms body ret = Some d' ->
  NoDup ps /\ ~ In ret (borrows ms ps) /\
  exists tb Lb, elab M body [ret] (borrows ms ps) = Some (tb, Lb) /\ incl Lb ps /\
    d' = (sinks ms ps, borrows ms ps, TSeq (settle ps Lb (borrows ms ps)) tb, ret).
Proof.
  intros M ps ms body ret d' E. unfold elab_routine in E.
  destruct (Nat.eqb (length ms) (length ps)) eqn:Hml; [|discriminate].
  destruct (nodupb ps) eqn:Hn; [|discriminate]. cbn [andb] in E.
  destruct (vmem ret (borrows ms ps)) eqn:Hr; [discriminate|].
  destruct (elab M body [ret] (borrows ms ps)) as [[tb Lb]|] eqn:Eb; [|discriminate].
  destruct (inclb Lb ps) eqn:Hi; [|discriminate].
  inversion E; subst. split; [apply nodupb_spec; exact Hn|]. split; [apply vmem_false; exact Hr|].
  exists tb, Lb. split; [reflexivity|]. split; [apply inclb_spec; exact Hi| reflexivity].
Qed.

Lemma elab_call_spec : forall M k x g args L B t Lin,
  elab_call M k x g args L B = Some (t, Lin) ->
  exists ms, kmodes M k g = Some ms /\ length ms = length args /\
  NoDup (sinks ms args) /\
  t = TSeq (TSeq (copies (filter (keep (borrows ms args ++ vremove x L) B) (sinks ms args)))
                 (TCall k (Src x) g
                        (map (pack_arg (borrows ms args ++ vremove x L) B) (sinks ms args))
                        (map Src (borrows ms args))))
           (settle (x :: filter (keep (borrows ms args ++ vremove x L) B) (sinks ms args)
                       ++ borrows ms args ++ vremove x L) L B) /\
  Lin = args ++ vremove x L.
Proof.
  intros M k x g args L B t Lin E. unfold elab_call in E.
  destruct (kmodes M k g) as [ms|] eqn:Ek; [|discriminate]. cbv zeta in E.
  destruct (Nat.eqb (length ms) (length args)) eqn:Hml; [|discriminate].
  destruct (nodupb (sinks ms args)) eqn:Hn; [|discriminate].
  inversion E; subst. exists ms. split; [reflexivity|].
  split; [apply Nat.eqb_eq; exact Hml|].
  split; [apply nodupb_spec; exact Hn| split; reflexivity].
Qed.

(* Fail-closed call admission. An unresolved summary is never read as
   "borrowed", and a summary whose arity differs from the call is refused;
   no missing entry defaults to a borrow. *)
Theorem missing_summary_refuses_call : forall M k x g args L B,
  kmodes M k g = None -> elab_call M k x g args L B = None.
Proof. intros M k x g args L B E. unfold elab_call. rewrite E. reflexivity. Qed.

Theorem arity_mismatch_refuses_call : forall M k x g args L B ms,
  kmodes M k g = Some ms -> length ms <> length args -> elab_call M k x g args L B = None.
Proof.
  intros M k x g args L B ms E Hl. unfold elab_call. rewrite E. cbv zeta.
  destruct (Nat.eqb (length ms) (length args)) eqn:Hml; [|reflexivity].
  apply Nat.eqb_eq in Hml. contradiction.
Qed.

Theorem missing_summary_refuses_routine : forall M g d,
  fmodes M g = None -> elab_fun M g d = None.
Proof. intros M g d E. unfold elab_fun. rewrite E. reflexivity. Qed.

Theorem arity_mismatch_refuses_routine : forall M ps ms body ret,
  length ms <> length ps -> elab_routine M ps ms body ret = None.
Proof.
  intros M ps ms body ret Hl. unfold elab_routine.
  destruct (Nat.eqb (length ms) (length ps)) eqn:Hml; [|reflexivity].
  apply Nat.eqb_eq in Hml. contradiction.
Qed.

Lemma tread_all_ext : forall ys rho rho' beta,
  (forall y, In y ys -> tread y rho' beta = tread y rho beta) ->
  tread_all ys rho' beta = tread_all ys rho beta.
Proof.
  induction ys as [|y r IH]; simpl; intros rho rho' beta He; [reflexivity|].
  rewrite (He y (or_introl eq_refl)), (IH rho rho' beta (fun z Hz => He z (or_intror Hz))).
  reflexivity.
Qed.

(* After the copies, every moved argument (a source variable or its
   temporary copy) holds the source value. *)
Lemma pack_args_lookup : forall sg rho beta rho1 Lin K B ys vs,
  CORR sg rho beta Lin B -> incl ys Lin ->
  (forall v, tlookup (Src v) rho1 = tlookup (Src v) rho) ->
  (forall w, In w ys -> keep K B w = true -> exists c bs bs',
      tread (Src w) rho beta = Some (c, bs) /\ tlookup (Tmp w) rho1 = Some (c, bs')) ->
  slookup_all sg ys = Some vs ->
  exists tvs, tlookup_all (map (pack_arg K B) ys) rho1 = Some tvs /\ map fst tvs = vs.
Proof.
  intros sg rho beta rho1 Lin K B ys. induction ys as [|y r IH]; simpl; intros vs HC Hsub HS HT Hs.
  - inversion Hs; subst. exists []. split; reflexivity.
  - destruct (sg y) as [v|] eqn:Hy; [|discriminate].
    destruct (slookup_all sg r) as [vr|] eqn:Hr; [|discriminate]. inversion Hs; subst.
    assert (Hyl : exists c bs, tlookup (pack_arg K B y) rho1 = Some (c, bs) /\ sg y = Some c).
    { unfold pack_arg. destruct (keep K B y) eqn:Hk.
      - destruct (HT y (or_introl eq_refl) Hk) as [c [bs [bs' [H1 H2]]]].
        exists c, bs'. split; [exact H2|].
        destruct (corr_read sg rho beta Lin B y HC (Hsub y (or_introl eq_refl))) as [c' [bs'' [Hl Hc]]].
        congruence.
      - destruct (keep_false K B y Hk) as [_ HyB].
        destruct (corr_owned sg rho beta Lin B y HC (Hsub y (or_introl eq_refl)) HyB) as [c [bs [Hl Hc]]].
        exists c, bs. split; [rewrite HS; exact Hl| exact Hc]. }
    destruct Hyl as [c [bs [Hl Hc]]].
    destruct (IH vr HC (fun z Hz => Hsub z (or_intror Hz)) HS (fun w Hw => HT w (or_intror Hw)) eq_refl)
      as [tvs [Htl Hm]].
    rewrite Hl, Htl. exists ((c, bs) :: tvs). split; [reflexivity|]. simpl. rewrite Hm. congruence.
Qed.

(* One call step, for either kind of callee. The callee body is given as a
   hypothesis so that the main theorem can supply its induction
   hypothesis. *)
Lemma core_call : forall tf tp k sg rho beta H n R x g args L B vsrc ms qs tb ret sgc tr v,
  INV rho beta H n R ->
  CORR sg rho beta (args ++ vremove x L) B ->
  ~ In x B ->
  (In x args -> In x (sinks ms args) /\ keep (borrows ms args ++ vremove x L) B x = false) ->
  NoDup (sinks ms args) ->
  slookup_all sg args = Some vsrc ->
  troutine tf tp k g = Some (sinks ms qs, borrows ms qs, tb, ret) ->
  NoDup qs -> length qs = length vsrc -> ~ In ret (borrows ms qs) ->
  (forall rho0 beta0 H0 n0 R0, INV rho0 beta0 H0 n0 R0 ->
     CORR (sbind qs vsrc) rho0 beta0 qs (borrows ms qs) ->
     exists rho1 beta1 H1 n1, texec tf tp rho0 beta0 H0 n0 tb rho1 beta1 H1 n1 tr /\
       INV rho1 beta1 H1 n1 R0 /\ CORR sgc rho1 beta1 [ret] (borrows ms qs)) ->
  sgc ret = Some v ->
  exists rho' H' n',
    texec tf tp rho beta H n
      (TSeq (copies (filter (keep (borrows ms args ++ vremove x L) B) (sinks ms args)))
            (TCall k (Src x) g (map (pack_arg (borrows ms args ++ vremove x L) B) (sinks ms args))
                   (map Src (borrows ms args))))
      rho' beta H' n' tr /\
    INV rho' beta H' n' R /\
    CORR (supd sg x v) rho' beta
         (x :: filter (keep (borrows ms args ++ vremove x L) B) (sinks ms args)
            ++ borrows ms args ++ vremove x L) B.
Proof.
  intros tf tp k sg rho beta H n R x g args L B vsrc ms qs tb ret sgc tr v
    Hinv HC HxB Hx HNsa Hsrc Hrt HNq Hlen HrB Hbody Hret.
  pose proof HC as [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  assert (Hsa_args : forall y, In y (sinks ms args) -> In y args) by (intros y; apply in_sinks).
  assert (Hba_args : forall y, In y (borrows ms args) -> In y args) by (intros y; apply in_borrows).
  assert (Hsplit : forall y, In y args -> In y (sinks ms args) \/ In y (borrows ms args))
    by (intros y; apply in_split_args).
  assert (Hsrc_s : slookup_all sg (sinks ms args) = Some (sinks ms vsrc))
    by (apply slookup_all_sinks; exact Hsrc).
  assert (Hsrc_b : slookup_all sg (borrows ms args) = Some (borrows ms vsrc))
    by (apply slookup_all_borrows; exact Hsrc).
  remember (sinks ms args) as sa eqn:Esa.
  remember (borrows ms args) as ba eqn:Eba.
  remember (ba ++ vremove x L) as K eqn:EK.
  remember (filter (keep K B) sa) as ws eqn:Ews.
  assert (Hws : forall w, In w ws <-> In w sa /\ keep K B w = true)
    by (intros w; rewrite Ews; apply filter_In).
  assert (HbaK : forall y, In y ba -> keep K B y = true)
    by (intros y Hy; apply keep_true; left; rewrite EK; apply in_or_app; left; exact Hy).
  assert (HLK : forall y, In y (vremove x L) -> keep K B y = true)
    by (intros y Hy; apply keep_true; left; rewrite EK; apply in_or_app; right; exact Hy).
  assert (HLin : forall y, In y args -> In y (args ++ vremove x L))
    by (intros y Hy; apply in_or_app; left; exact Hy).
  (* 1. copy the sink arguments that stay live *)
  destruct (copies_exec tf tp ws rho beta H n R) as [rho1 [H1 [n1 [Hex1 [Hinv1 [HS [HT HO]]]]]]].
  - rewrite Ews. apply NoDup_filter'. exact HNsa.
  - exact Hinv.
  - intros w Hw. apply Hws in Hw. destruct Hw as [Hw _].
    destruct (corr_read sg rho beta _ B w HC (HLin w (Hsa_args w Hw))) as [c [bs [Hl _]]].
    congruence.
  - intros w _. split; [apply Tr| apply Tb].
  - remember (map (pack_arg K B) sa) as sys eqn:Esys.
    assert (Hsys : forall z, In z sys <->
              exists y, In y sa /\ ((keep K B y = true /\ z = Tmp y) \/ (keep K B y = false /\ z = Src y)))
      by (intros z; rewrite Esys; apply pack_arg_in).
    assert (HNsys : NoDup sys) by (rewrite Esys; apply nodup_pack_args; exact HNsa).
    (* 2. the moved arguments hold the source values *)
    destruct (pack_args_lookup sg rho beta rho1 _ K B sa (sinks ms vsrc) HC) as [svs [Hsl Hsm]].
    + intros y Hy. apply HLin, Hsa_args, Hy.
    + exact HS.
    + intros w Hw Hk. apply HT. apply Hws. split; assumption.
    + exact Hsrc_s.
    + rewrite <- Esys in Hsl.
      (* 3. the borrowed arguments are read through what remains *)
      destruct (read_all_src sg rho beta _ B ba (borrows ms vsrc) HC) as [bvs [Hbl Hbm]].
      * intros y Hy. apply HLin, Hba_args, Hy.
      * exact Hsrc_b.
      * assert (Hba_out : forall y, In y ba -> ~ In (Src y) sys).
        { intros y Hy Hin. apply Hsys in Hin.
          destruct Hin as [y' [_ [[_ E]|[Hk E]]]]; [discriminate|].
          injection E as E. subst y'. rewrite (HbaK y Hy) in Hk. discriminate. }
        assert (Hbl1 : tread_all (map Src ba) rho1 beta = Some bvs).
        { rewrite <- Hbl. apply tread_all_ext. intros w Hw. apply in_map_iff in Hw.
          destruct Hw as [y [E _]]. subst w. unfold tread. rewrite HS. reflexivity. }
        assert (Hbl' : tread_all (map Src ba) (tremove_all sys rho1) beta = Some bvs).
        { rewrite <- Hbl1. apply tread_all_ext. intros w Hw. apply in_map_iff in Hw.
          destruct Hw as [y [E Hy]]. subst w. unfold tread. rewrite tlookup_tremove_all.
          destruct (in_dec tvar_eq_dec (Src y) sys) as [Hin|_];
            [exfalso; exact (Hba_out y Hy Hin)| reflexivity]. }
        (* 4. the result variable is free once the sink arguments moved out *)
        assert (Hub : unbound (Src x) (tremove_all sys rho1) beta).
        { split.
          - rewrite tlookup_tremove_all.
            destruct (in_dec tvar_eq_dec (Src x) sys) as [_|Hn]; [reflexivity|].
            rewrite HS. destruct (in_dec Nat.eq_dec x args) as [Hxa|Hxa].
            + exfalso. destruct (Hx Hxa) as [Hxs Hk]. apply Hn. apply Hsys. exists x.
              split; [exact Hxs| right; split; [exact Hk| reflexivity]].
            + exact (proj1 (corr_unbound sg rho beta _ B x HC (src_notin_lin x args L Hxa))).
          - destruct (tlookup (Src x) beta) eqn:E; [|reflexivity]. exfalso.
            assert (F : In x (args ++ vremove x L) /\ In x B) by (apply Hb; congruence).
            apply HxB. apply F. }
        (* 5. run the callee in its own frame *)
        assert (Hls : length (sinks ms qs) = length svs).
        { transitivity (length (sinks ms vsrc)); [apply sinks_length; exact Hlen|].
          rewrite <- Hsm. apply length_map. }
        assert (Hlb : length (borrows ms qs) = length bvs).
        { transitivity (length (borrows ms vsrc)); [apply borrows_length; exact Hlen|].
          rewrite <- Hbm. apply length_map. }
        pose proof (inv_call_frame rho1 beta H1 n1 R sys (map Src ba) svs bvs (sinks ms qs) (borrows ms qs)
                      Hinv1 HNsys Hsl Hbl' (nodup_split _ ms qs HNq) Hls Hlb) as Hinv0.
        pose proof (corr_entry_routine ms qs vsrc svs bvs HNq Hlen Hsm Hbm) as HC0.
        destruct (Hbody _ _ _ _ _ Hinv0 HC0) as [rhob [betab [Hb' [nb [Hexb [Hinvb HCb]]]]]].
        destruct (corr_exit_shape sgc rhob betab ret _ HCb HrB (doms_l _ _ (proj1 Hinvb)))
          as [Eb0 [c [bs [Erb Hc]]]]. subst rhob betab.
        assert (c = v) by congruence. subst c.
        exists ((Src x, (v, bs)) :: tremove_all sys rho1), Hb', nb. split; [|split].
        -- eapply TE_Seq;
             [ exact Hex1
             | eapply TE_Call; [exact HNsys| exact Hsl| exact Hub| exact Hbl'|
                                exact (forall_read rho1 beta H1 n1 R _ bvs Hinv1 Hbl1)|
                                exact Hrt| exact Hls| exact Hlb| exact Hexb]
             | reflexivity ].
        -- exact (inv_call_return rho1 beta H1 n1 R sys (Src x) ret (v, bs) Hb' nb Hinv1 Hub Hinvb).
        -- rewrite EK. split; [|split; [|split; [|split; [|split]]]].
           ++ intros z. rewrite lk_src_src. destruct (Nat.eq_dec x z) as [E|E].
              ** subst z. split; [intros _; split; [left; reflexivity| exact HxB]| discriminate].
              ** rewrite tlookup_tremove_all. destruct (in_dec tvar_eq_dec (Src z) sys) as [Hp|Hp].
                 --- apply Hsys in Hp. destruct Hp as [y [Hy [[_ F]|[Hk F]]]]; [discriminate|].
                     injection F as F. subst y.
                     split; [intros F2; exfalso; apply F2; reflexivity|].
                     intros [[F2|F2] _]; [contradiction|].
                     apply in_app_or in F2. destruct F2 as [F2|F2].
                     +++ apply Hws in F2. destruct F2 as [_ F2]. congruence.
                     +++ apply in_app_or in F2. destruct F2 as [F2|F2].
                         *** rewrite (HbaK z F2) in Hk. discriminate.
                         *** rewrite (HLK z F2) in Hk. discriminate.
                 --- rewrite HS, Ho. simpl. split.
                     +++ intros [Hin HzB]. split; [|exact HzB]. right.
                         apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                         *** destruct (Hsplit z Hin) as [Hs|Hs].
                             ---- apply in_or_app. left. apply Hws. split; [exact Hs|].
                                  destruct (keep K B z) eqn:Hk; [reflexivity|].
                                  exfalso. apply Hp. apply Hsys. exists z.
                                  split; [exact Hs| right; split; [exact Hk| reflexivity]].
                             ---- apply in_or_app. right. apply in_or_app. left. exact Hs.
                         *** apply in_or_app. right. apply in_or_app. right. exact Hin.
                     +++ intros [[F|Hin] HzB]; [contradiction|]. split; [|exact HzB].
                         apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                         *** apply Hws in Hin. apply HLin. apply Hsa_args. apply Hin.
                         *** apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                             ---- apply HLin. apply Hba_args. exact Hin.
                             ---- apply in_or_app. right. exact Hin.
           ++ intros z. rewrite Hb. simpl. split.
              ** intros [Hin HzB]. split; [|exact HzB]. right.
                 apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                 --- destruct (Hsplit z Hin) as [Hs|Hs].
                     +++ apply in_or_app. left. apply Hws.
                         split; [exact Hs| apply keep_true; right; exact HzB].
                     +++ apply in_or_app. right. apply in_or_app. left. exact Hs.
                 --- apply in_or_app. right. apply in_or_app. right. exact Hin.
              ** intros [[F|Hin] HzB]; [subst z; contradiction|]. split; [|exact HzB].
                 apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                 --- apply Hws in Hin. apply HLin. apply Hsa_args. apply Hin.
                 --- apply in_app_or in Hin. destruct Hin as [Hin|Hin].
                     +++ apply HLin. apply Hba_args. exact Hin.
                     +++ apply in_or_app. right. exact Hin.
           ++ intros z. rewrite lk_tmp_src, tlookup_tremove_all.
              destruct (in_dec tvar_eq_dec (Tmp z) sys) as [Hp|Hp]; [reflexivity|].
              destruct (in_dec Nat.eq_dec z ws) as [Hz|Hz].
              ** exfalso. apply Hp. apply Hsys. exists z. apply Hws in Hz.
                 split; [apply Hz| left; split; [apply Hz| reflexivity]].
              ** rewrite HO; [apply Tr| exact Hz].
           ++ exact Tb.
           ++ intros z c bs' Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec x z) as [E|E].
              ** subst z. inversion Hl; subst. apply supd_same.
              ** rewrite supd_other; [|exact E]. rewrite tlookup_tremove_all in Hl.
                 destruct (in_dec tvar_eq_dec (Src z) sys); [discriminate|].
                 rewrite HS in Hl. eapply Vo. exact Hl.
           ++ intros z c bs' Hl. destruct (Nat.eq_dec x z) as [E|E].
              ** subst z. exfalso.
                 assert (Hin : In x (args ++ vremove x L) /\ In x B) by (apply Hb; congruence).
                 apply HxB. apply Hin.
              ** rewrite supd_other; [|exact E]. eapply Vb. exact Hl.
Qed.

(* ------------------------------------------------------------------ *)
(* Main theorem                                                         *)
(* ------------------------------------------------------------------ *)

Lemma incl_settle_def : forall x ys L, incl L (x :: ys ++ vremove x L).
Proof.
  intros x ys L z Hz. destruct (Nat.eq_dec z x) as [E|E]; [left; congruence|].
  right. apply in_or_app. right. apply vremove_In. split; assumption.
Qed.

Lemma incl_settle_rm : forall x L, incl L (x :: vremove x L).
Proof.
  intros x L z Hz. destruct (Nat.eq_dec z x) as [E|E]; [left; congruence|].
  right. apply vremove_In. split; assumption.
Qed.

Lemma incl_settle_rm2 : forall x y L, incl L (x :: y :: vremove x L).
Proof.
  intros x y L z Hz. destruct (Nat.eq_dec z x) as [E|E]; [left; congruence|].
  right. right. apply vremove_In. split; assumption.
Qed.

Lemma incl_settle_call : forall x A1 A2 L, incl L (x :: A1 ++ A2 ++ vremove x L).
Proof.
  intros x A1 A2 L z Hz. destruct (Nat.eq_dec z x) as [E|E]; [left; congruence|].
  right. apply in_or_app. right. apply in_or_app. right. apply vremove_In. split; assumption.
Qed.

Lemma seq_settle : forall tf tp rho beta H n t rho1 beta1 H1 n1 tr A L B rho' beta' H',
  texec tf tp rho beta H n t rho1 beta1 H1 n1 tr ->
  texec tf tp rho1 beta1 H1 n1 (settle A L B) rho' beta' H' n1 [] ->
  texec tf tp rho beta H n (TSeq t (settle A L B)) rho' beta' H' n1 tr.
Proof.
  intros. eapply TE_Seq; [eassumption| eassumption| rewrite app_nil_r; reflexivity].
Qed.

(* ------------------------------------------------------------------ *)
(* Places: paths, footprint segments, and the focus frame              *)
(* ------------------------------------------------------------------ *)

Lemma nth_sizes_le : forall cs i c, nth_error cs i = Some c ->
  list_sum (map vsize (firstn i cs)) + vsize c <= list_sum (map vsize cs).
Proof.
  induction cs as [|a r IH]; intros i c E; destruct i as [|j]; simpl in E; try discriminate.
  - inversion E; subst. simpl. lia.
  - simpl. specialize (IH j c E). lia.
Qed.

Lemma vget_voff : forall p c cv, vget c p = Some cv ->
  exists off, voff c p = Some off /\ off + vsize cv <= vsize c.
Proof.
  induction p as [|i q IH]; intros c cv E; simpl in E.
  - inversion E; subst. exists 0. split; [reflexivity| lia].
  - destruct c as [k|cs]; [discriminate|].
    destruct (nth_error cs i) as [ci|] eqn:Ei; [|discriminate].
    destruct (IH ci cv E) as [o [Eo Ho]].
    exists (1 + list_sum (map vsize (firstn i cs)) + o). split.
    + simpl. rewrite Ei, Eo. reflexivity.
    + pose proof (nth_sizes_le cs i ci Ei). simpl. lia.
Qed.

Lemma replace_sizes : forall cs i c x, nth_error cs i = Some c ->
  list_sum (map vsize (replace_nth cs i x)) + vsize c = list_sum (map vsize cs) + vsize x.
Proof.
  induction cs as [|a r IH]; intros i c x E; destruct i as [|j]; simpl in E; try discriminate.
  - inversion E; subst. simpl. lia.
  - simpl. specialize (IH j c x E). lia.
Qed.

Lemma vget_vset_size : forall p c cv w c', vget c p = Some cv -> vset c p w = Some c' ->
  vsize c' + vsize cv = vsize c + vsize w.
Proof.
  induction p as [|i q IH]; intros c cv w c' Eg Es; simpl in Eg, Es.
  - inversion Eg; inversion Es; subst. lia.
  - destruct c as [k|cs]; [discriminate|].
    destruct (nth_error cs i) as [ci|] eqn:Ei; [|discriminate].
    destruct (vset ci q w) as [ci'|] eqn:Ec; [|discriminate]. inversion Es; subst.
    pose proof (IH ci cv w ci' Eg Ec). pose proof (replace_sizes cs i ci ci' Ei). simpl. lia.
Qed.

Lemma split_eq : forall (bs : list Block) off sz,
  bs = firstn off bs ++ firstn sz (skipn off bs) ++ skipn (off + sz) bs.
Proof.
  intros bs off sz.
  assert (E : skipn (off + sz) bs = skipn sz (skipn off bs)) by (rewrite skipn_skipn; f_equal; lia).
  rewrite E, firstn_skipn, firstn_skipn. reflexivity.
Qed.

Lemma perm_mid : forall (A B C : list Block), Permutation (A ++ B ++ C) (B ++ A ++ C).
Proof. intros. rewrite !app_assoc. apply Permutation_app_tail. apply Permutation_app_comm. Qed.

Lemma perm_focus_entry : forall (S A C X R : list Block),
  Permutation (((A ++ S ++ C) ++ X) ++ R) ((S ++ X) ++ (A ++ C) ++ R).
Proof.
  intros S A C X R.
  apply Permutation_trans with (((S ++ A ++ C) ++ X) ++ R).
  - apply Permutation_app_tail. apply Permutation_app_tail. apply perm_mid.
  - rewrite <- !app_assoc. apply Permutation_app_head.
    rewrite (app_assoc A C (X ++ R)), (app_assoc A C R). apply perm_mid.
Qed.

Lemma perm_focus_exit : forall (F X A C R : list Block),
  Permutation ((F ++ X) ++ (A ++ C) ++ R) (((A ++ F ++ C) ++ X) ++ R).
Proof.
  intros F X A C R. rewrite <- (app_assoc (A ++ F ++ C) X R).
  apply Permutation_trans with (F ++ (A ++ C) ++ X ++ R).
  - rewrite <- app_assoc. apply Permutation_app_head. apply perm_mid.
  - rewrite <- !app_assoc. apply perm_mid.
Qed.

Lemma seg_length : forall (bs : list Block) off sz, off + sz <= length bs ->
  length (firstn sz (skipn off bs)) = sz.
Proof. intros. rewrite length_firstn, length_skipn. lia. Qed.

Lemma reassembled_length : forall (bs bf : list Block) off sz, off + sz <= length bs ->
  length (firstn off bs ++ bf ++ skipn (off + sz) bs) = length bs - sz + length bf.
Proof. intros. rewrite !length_app, length_firstn, length_skipn. lia. Qed.

(* A frame can only end its borrowed bindings, never add one. *)
Lemma beta_shrinks : forall tf tp rho beta H n t rho' beta' H' n' tr,
  texec tf tp rho beta H n t rho' beta' H' n' tr ->
  forall z v, tlookup z beta' = Some v -> tlookup z beta = Some v.
Proof.
  intros tf tp rho beta H n t rho' beta' H' n' tr E.
  induction E; intros z w Hz;
    first [ exact Hz
          | (rewrite tlookup_tremove in Hz; destruct (tvar_eq_dec x z); [discriminate| exact Hz])
          | (apply IHE1; apply IHE2; exact Hz)
          | (apply IHE; exact Hz) ].
Qed.

(* Entering a focus: the part's segment becomes the temporary's footprint,
   and the rest of the root is suspended in the frame heap. *)
Lemma focus_inv_entry : forall rho beta H n R t y c bs cv off,
  INV rho beta H n R -> tlookup y rho = Some (c, bs) -> unbound t (tremove y rho) beta ->
  off + vsize cv <= vsize c ->
  INV ((t, (cv, firstn (vsize cv) (skipn off bs))) :: tremove y rho) beta H n
      ((firstn off bs ++ skipn (off + vsize cv) bs) ++ R).
Proof.
  intros rho beta H n R t y c bs cv off Hinv Hy Ht Hle.
  pose proof Hinv as [HN [HH [HP [Hf [Hb [Lr Lb]]]]]].
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  assert (Hlen : length bs = vsize c) by (eapply Lr; exact Hy).
  split; [|split; [exact HH|split; [|split; [exact Hf|split; [|split; [|exact Lb]]]]]].
  - apply doms_cons; [exact Ht|].
    eapply doms_sub; [exact HN| apply nodup_dom_tremove; exact HNr| eapply doms_r; exact HN| |].
    + intros z Hz. apply in_dom_tremove in Hz. apply Hz.
    + intros z Hz. exact Hz.
  - change (heap_of ((t, (cv, firstn (vsize cv) (skipn off bs))) :: tremove y rho))
      with (firstn (vsize cv) (skipn off bs) ++ heap_of (tremove y rho)).
    eapply Permutation_trans; [exact HP|].
    eapply Permutation_trans; [apply Permutation_app_tail; eapply heap_tremove; eassumption|].
    rewrite (split_eq bs off (vsize cv)) at 1. apply perm_focus_entry.
  - intros z c' bs' Hz. apply incl_appr. eapply Hb. exact Hz.
  - apply laid_cons; [apply seg_length; lia| apply laid_tremove; exact Lr].
Qed.

(* Leaving a focus: the temporary's final footprint goes back into the
   root at the same offset. *)
Lemma focus_inv_exit : forall rho beta H n R y c bs cv off rho1 beta1 H1 n1 t cv' bf' c',
  INV rho beta H n R -> tlookup y rho = Some (c, bs) -> off + vsize cv <= vsize c ->
  INV rho1 beta1 H1 n1 ((firstn off bs ++ skipn (off + vsize cv) bs) ++ R) ->
  (forall z v, tlookup z beta1 = Some v -> tlookup z beta = Some v) ->
  tlookup t rho1 = Some (cv', bf') -> unbound y (tremove t rho1) beta1 ->
  vsize c' + vsize cv = vsize c + vsize cv' ->
  INV ((y, (c', firstn off bs ++ bf' ++ skipn (off + vsize cv) bs)) :: tremove t rho1)
      beta1 H1 n1 R.
Proof.
  intros rho beta H n R y c bs cv off rho1 beta1 H1 n1 t cv' bf' c'
    [_ [_ [_ [_ [Hb [Lr _]]]]]] Hy Hle [HN1 [HH1 [HP1 [Hf1 [_ [Lr1 Lb1]]]]]] Hsh Ht Huy Hsz.
  assert (HNr1 : NoDup (dom rho1)) by (eapply doms_l; exact HN1).
  assert (Hlen : length bs = vsize c) by (eapply Lr; exact Hy).
  assert (Hlf : length bf' = vsize cv') by (eapply Lr1; exact Ht).
  split; [|split; [exact HH1|split; [|split; [exact Hf1|split; [|split; [|exact Lb1]]]]]].
  - apply doms_cons; [exact Huy|].
    eapply doms_sub; [exact HN1| apply nodup_dom_tremove; exact HNr1| eapply doms_r; exact HN1| |].
    + intros z Hz. apply in_dom_tremove in Hz. apply Hz.
    + intros z Hz. exact Hz.
  - change (heap_of ((y, (c', firstn off bs ++ bf' ++ skipn (off + vsize cv) bs)) :: tremove t rho1))
      with ((firstn off bs ++ bf' ++ skipn (off + vsize cv) bs) ++ heap_of (tremove t rho1)).
    eapply Permutation_trans; [exact HP1|].
    eapply Permutation_trans; [apply Permutation_app_tail; eapply heap_tremove; [exact HNr1| exact Ht]|].
    apply perm_focus_exit.
  - intros z c0 bs0 Hz. apply (Hb z c0 bs0). apply Hsh. exact Hz.
  - apply laid_cons; [|apply laid_tremove; exact Lr1].
    rewrite reassembled_length by lia. lia.
Qed.

(* Swapping which of two owned variables is bound: used on entry (the root
   is suspended, the temporary appears) and on exit (the reverse). *)
Lemma corr_swap : forall sg rho beta X B a b v bs,
  CORR sg rho beta (a :: vremove b X) B -> a <> b -> ~ In a B -> ~ In b B -> ~ In a X ->
  CORR (supd sg b v) ((Src b, (v, bs)) :: tremove (Src a) rho) beta (b :: vremove b X) B.
Proof.
  intros sg rho beta X B a b v bs [Ho [Hb [Tr [Tb [Vo Vb]]]]] Hab HaB HbB HaX.
  split; [|split; [|split; [|split; [|split]]]].
  - intros z. rewrite lk_src_src. destruct (Nat.eq_dec b z) as [E|E].
    + subst z. split; [intros _; split; [left; reflexivity| exact HbB]| discriminate].
    + rewrite rm_src_src. destruct (Nat.eq_dec a z) as [E2|E2].
      * subst z. split; [intros F; exfalso; apply F; reflexivity|].
        intros [[F|F] _]; [contradiction|]. apply vremove_In in F. exfalso. apply HaX. apply F.
      * rewrite Ho. simpl. split.
        -- intros [[F|F] HzB]; [contradiction|]. split; [right; exact F| exact HzB].
        -- intros [[F|F] HzB]; [contradiction|]. split; [right; exact F| exact HzB].
  - intros z. rewrite Hb. simpl. split.
    + intros [[F|F] HzB]; [subst z; contradiction|]. split; [right; exact F| exact HzB].
    + intros [[F|F] HzB]; [subst z; contradiction|]. split; [right; exact F| exact HzB].
  - intros z. rewrite lk_tmp_src, rm_tmp_src. apply Tr.
  - exact Tb.
  - intros z c bs' Hl. rewrite lk_src_src in Hl. destruct (Nat.eq_dec b z) as [E|E].
    + subst z. inversion Hl; subst. apply supd_same.
    + rewrite supd_other; [|exact E]. rewrite rm_src_src in Hl.
      destruct (Nat.eq_dec a z); [discriminate| eapply Vo; exact Hl].
  - intros z c bs' Hl. rewrite supd_other.
    + eapply Vb. exact Hl.
    + intros E. subst z. assert (F : In b (a :: vremove b X) /\ In b B) by (apply Hb; congruence).
      apply HbB. apply F.
Qed.

(* ------------------------------------------------------------------ *)
(* Unpack: a record's parts become separate owned variables             *)
(* ------------------------------------------------------------------ *)

Lemma tlookup_app : forall z (l1 l2 : TEnv),
  tlookup z (l1 ++ l2) = match tlookup z l1 with Some v => Some v | None => tlookup z l2 end.
Proof.
  intros z l1 l2. induction l1 as [|[k v] r IH]; simpl; [reflexivity|].
  destruct (tvar_eq_dec k z); [reflexivity| exact IH].
Qed.

Lemma dom_app : forall (l1 l2 : TEnv), dom (l1 ++ l2) = dom l1 ++ dom l2.
Proof. intros. unfold dom. apply map_app. Qed.

Lemma heap_app : forall (l1 l2 : TEnv), heap_of (l1 ++ l2) = heap_of l1 ++ heap_of l2.
Proof. intros. unfold heap_of. apply flat_map_app. Qed.

Lemma laid_app : forall l1 l2, laid l1 -> laid l2 -> laid (l1 ++ l2).
Proof.
  intros l1 l2 L1 L2 z c bs Hz. rewrite tlookup_app in Hz.
  destruct (tlookup z l1) eqn:E.
  - inversion Hz; subst. eapply L1. exact E.
  - eapply L2. exact Hz.
Qed.

Lemma chunks_length : forall sizes bs, length (chunks sizes bs) = length sizes.
Proof. induction sizes as [|k r IH]; intros bs; simpl; [reflexivity| rewrite IH; reflexivity]. Qed.

Lemma chunks_concat : forall sizes bs, list_sum sizes = length bs -> concat (chunks sizes bs) = bs.
Proof.
  induction sizes as [|k r IH]; intros bs E; simpl in *.
  - destruct bs; [reflexivity| discriminate].
  - rewrite IH; [apply firstn_skipn| rewrite length_skipn; lia].
Qed.

Lemma flat_map_combine : forall (cs : list SVal) (chs : list (list Block)),
  length cs = length chs -> flat_map snd (combine cs chs) = concat chs.
Proof.
  induction cs as [|c r IH]; intros chs E; destruct chs as [|h t]; simpl in *; try discriminate;
    [reflexivity|].
  rewrite IH; [reflexivity| lia].
Qed.

Lemma map_fst_combine : forall (cs : list SVal) (chs : list (list Block)),
  length cs = length chs -> map fst (combine cs chs) = cs.
Proof.
  induction cs as [|c r IH]; intros chs E; destruct chs; simpl in *; try discriminate;
    [reflexivity| rewrite IH; [reflexivity| lia]].
Qed.

Lemma chunks_laid : forall cs rest, list_sum (map vsize cs) = length rest ->
  Forall (fun v => length (snd v) = vsize (fst v)) (combine cs (chunks (map vsize cs) rest)).
Proof.
  induction cs as [|c r IH]; intros rest E; simpl in *; constructor.
  - simpl. rewrite length_firstn. lia.
  - apply IH. rewrite length_skipn. lia.
Qed.

Lemma supd_list_out : forall sg xs cs z, ~ In z xs -> supd_list sg xs cs z = sg z.
Proof.
  intros sg xs. induction xs as [|x r IH]; intros cs z Hz; destruct cs as [|c cr]; simpl; try reflexivity.
  unfold supd. destruct (Nat.eq_dec x z) as [E|E]; [exfalso; apply Hz; left; exact E|].
  apply IH. intros F. apply Hz. right. exact F.
Qed.

Lemma supd_list_in : forall sg xs cs z, In z xs -> length xs = length cs ->
  supd_list sg xs cs z = sbind xs cs z.
Proof.
  intros sg xs. induction xs as [|x r IH]; intros cs z Hz E; destruct cs as [|c cr];
    simpl in *; try discriminate; [destruct Hz|].
  unfold supd. destruct (Nat.eq_dec x z) as [Ex|Ex]; [reflexivity|].
  destruct Hz as [Hz|Hz]; [contradiction|]. apply IH; [exact Hz| lia].
Qed.

(* Unpack moves each part with exactly its own blocks; only the record's
   own node block is released. *)
Lemma core_unpack : forall tf tp sg rho beta H n R y xs L B cs,
  INV rho beta H n R -> CORR sg rho beta (y :: vdiff L xs) B ->
  ~ In y L -> ~ In y B -> ~ In y xs -> NoDup xs -> (forall x, In x xs -> ~ In x B) ->
  sg y = Some (SNode cs) -> length xs = length cs ->
  exists rho' H',
    texec tf tp rho beta H n (TUnpack (Src y) xs) rho' beta H' n [] /\
    INV rho' beta H' n R /\
    CORR (supd_list sg xs cs) rho' beta (xs ++ vdiff L xs) B.
Proof.
  intros tf tp sg rho beta H n R y xs L B cs Hinv HC HyL HyB Hyx HNx HxB Hy Hlen.
  pose proof HC as [Ho [Hb [Tr [Tb [Vo Vb]]]]].
  destruct (corr_owned sg rho beta _ B y HC) as [c0 [bs [Hly Hc0]]]; [left; reflexivity| exact HyB|].
  assert (c0 = SNode cs) by congruence. subst c0.
  pose proof Hinv as [HN [HH [HP [Hf [Hbr [Lr Lb]]]]]].
  assert (HNr : NoDup (dom rho)) by (eapply doms_l; exact HN).
  assert (Hlb : length bs = vsize (SNode cs)) by (eapply Lr; exact Hly).
  destruct bs as [|b0 rest]; [simpl in Hlb; discriminate|].
  simpl in Hlb. injection Hlb as Hrest.
  remember (chunks (map vsize cs) rest) as chs eqn:Echs.
  assert (Hchl : length cs = length chs) by (rewrite Echs, chunks_length, length_map; reflexivity).
  assert (Hvl : length xs = length (combine cs chs)) by (rewrite length_combine; lia).
  assert (Hcat : concat chs = rest) by (rewrite Echs; apply chunks_concat; lia).
  assert (Hux : forall x, In x xs -> unbound (Src x) (tremove (Src y) rho) beta).
  { intros x Hx. destruct (corr_unbound sg rho beta _ B x HC) as [U1 U2].
    - intros [F|F]; [subst; contradiction|]. apply vdiff_In in F. apply F. exact Hx.
    - split; [rewrite rm_src_src; destruct (Nat.eq_dec y x); [reflexivity| exact U1]| exact U2]. }
  exists (tbind xs (combine cs chs) ++ tremove (Src y) rho), (free [b0] H). split; [|split].
  - rewrite Echs. eapply TE_Unpack; [exact Hly| exact Hlen| exact HNx| exact Hux].
  - assert (HP2 : Permutation H ([b0] ++ rest ++ heap_of (tremove (Src y) rho) ++ R)).
    { eapply Permutation_trans; [exact HP|].
      assert (E : [b0] ++ rest ++ heap_of (tremove (Src y) rho) ++ R
                  = ((b0 :: rest) ++ heap_of (tremove (Src y) rho)) ++ R)
        by (simpl; rewrite app_assoc; reflexivity).
      rewrite E. apply Permutation_app_tail. eapply heap_tremove; [exact HNr| exact Hly]. }
    split; [|split; [|split; [|split; [|split; [|split]]]]].
    + rewrite dom_app, (tbind_dom xs _ Hvl), <- app_assoc. apply NoDup_app_join.
      * apply NoDup_map_Src. exact HNx.
      * eapply doms_sub; [exact HN| apply nodup_dom_tremove; exact HNr| eapply doms_r; exact HN| |].
        -- intros z Hz. apply in_dom_tremove in Hz. apply Hz.
        -- intros z Hz. exact Hz.
      * intros z Hz Hin. apply in_map_iff in Hz. destruct Hz as [x [E Hx]]. subst z.
        destruct (Hux x Hx) as [U1 U2]. apply in_app_or in Hin. destruct Hin as [Hin|Hin].
        -- apply tlookup_none_iff in U1. contradiction.
        -- apply tlookup_none_iff in U2. contradiction.
    + apply NoDup_filter'. exact HH.
    + rewrite heap_app, (tbind_heap xs _ Hvl), (flat_map_combine cs chs Hchl), Hcat, <- app_assoc.
      apply free_perm; [eapply Permutation_NoDup; [exact HP2| exact HH]| exact HP2].
    + intros b Hbf. unfold free in Hbf. apply filter_In in Hbf. apply Hf. apply Hbf.
    + exact Hbr.
    + apply laid_app; [| apply laid_tremove; exact Lr].
      apply laid_tbind. rewrite Echs. apply chunks_laid. lia.
    + exact Lb.
  - split; [|split; [|split; [|split; [|split]]]].
    + intros z. rewrite tlookup_app. destruct (in_dec Nat.eq_dec z xs) as [Hz|Hz].
      * assert (Hb' : tlookup (Src z) (tbind xs (combine cs chs)) <> None)
          by (apply tbind_src_bound; [exact Hvl| exact Hz]).
        destruct (tlookup (Src z) (tbind xs (combine cs chs))) as [v|]; [|contradiction].
        split; [intros _; split; [apply in_or_app; left; exact Hz| apply HxB; exact Hz]| discriminate].
      * assert (Hn : tlookup (Src z) (tbind xs (combine cs chs)) = None).
        { destruct (tlookup (Src z) (tbind xs (combine cs chs))) eqn:E; [|reflexivity].
          exfalso. apply Hz. apply (tbind_src_bound xs _ z Hvl). congruence. }
        rewrite Hn, rm_src_src. destruct (Nat.eq_dec y z) as [E|E].
        -- subst z. split; [intros F; exfalso; apply F; reflexivity|].
           intros [F _]. apply in_app_or in F. destruct F as [F|F]; [contradiction|].
           apply vdiff_In in F. exfalso. apply HyL. apply F.
        -- rewrite Ho. split.
           ++ intros [[F|F] HzB]; [contradiction|]. split; [apply in_or_app; right; exact F| exact HzB].
           ++ intros [F HzB]. apply in_app_or in F. destruct F as [F|F]; [contradiction|].
              split; [right; exact F| exact HzB].
    + intros z. rewrite Hb. split.
      * intros [[F|F] HzB]; [subst z; contradiction|]. split; [apply in_or_app; right; exact F| exact HzB].
      * intros [F HzB]. apply in_app_or in F. destruct F as [F|F].
        -- exfalso. apply (HxB z F). exact HzB.
        -- split; [right; exact F| exact HzB].
    + intros z. rewrite tlookup_app, tbind_tmp, rm_tmp_src. exact (Tr z).
    + exact Tb.
    + intros z c bs' Hl. rewrite tlookup_app in Hl.
      destruct (tlookup (Src z) (tbind xs (combine cs chs))) as [v|] eqn:E.
      * inversion Hl; subst. assert (Hz : In z xs) by (apply (tbind_src_bound xs _ z Hvl); congruence).
        apply tbind_src_value in E; [|exact Hvl]. rewrite map_fst_combine in E; [|exact Hchl].
        rewrite supd_list_in; [exact E| exact Hz| exact Hlen].
      * assert (Hz : ~ In z xs).
        { intros Hz. apply (tbind_src_bound xs _ z Hvl) in Hz. contradiction. }
        rewrite supd_list_out; [|exact Hz]. rewrite rm_src_src in Hl.
        destruct (Nat.eq_dec y z); [discriminate| eapply Vo; exact Hl].
    + intros z c bs' Hl. rewrite supd_list_out.
      * eapply Vb. exact Hl.
      * intros Hz. apply (HxB z Hz).
        assert (F : In z (y :: vdiff L xs) /\ In z B) by (apply Hb; congruence). apply F.
Qed.

Section Soundness.
Variable M : Modes.
Variable funs : SFunTable.
Variable procs : SProcTable.
Hypothesis funs_ok : forall g d, funs g = Some d -> elab_fun M g d <> None.
Hypothesis procs_ok : forall g d, procs g = Some d -> elab_proc M g d <> None.

Theorem elab_sound : forall sg s sg' tr,
  sexec funs procs sg s sg' tr ->
  forall L B t Lin rho beta H n R,
    elab M s L B = Some (t, Lin) -> INV rho beta H n R -> CORR sg rho beta Lin B ->
    exists rho' beta' H' n',
      texec (tfuns_of M funs) (tprocs_of M procs) rho beta H n t rho' beta' H' n' tr /\
      INV rho' beta' H' n' R /\ CORR sg' rho' beta' L B.
Proof.
  intros sg s sg' tr Hs.
  induction Hs as
    [ sg
    | sg x f ys vs Hl
    | sg x y v Hy
    | sg x ys vs Hl
    | sg x y cs v Hx Hy
    | sg x y i cs v Hy Hi
    | sg y v Hy
    | sg1 sg2 sg3 s1 s2 tr1 tr2 tr Hs1 IH1 Hs2 IH2 Htr
    | sg sg' c s1 s2 v tr Hc Ht Hs1 IH1
    | sg sg' c s1 s2 v tr Hc Ht Hs2 IH2
    | sg c h b v Hc Ht
    | sg sg1 sg2 c h b v tr1 tr2 tr Hc Ht Hb IHb Hw IHw Htr
    | sg x g ys vs ps body ret sgc tr v Hg Hl Hlen Hbody IHbody Hret
    | sg g z ys vz vs io ps body sgc tr v Hg Hz Hl Hlen Hbody IHbody Hret
    | sg ft fy fp fs c cv sg1 tr cv' c' Hy Hg Hsb IHs Ht Hset
    | sg uy uxs ucs Huy Hulen
    | sg rx rf rys rs rvs rsg tr Hrl Hrs IHrs ];
    intros L B t Lin rho beta H n R Hel Hinv HC; simpl in Hel.
  - (* skip *)
    inversion Hel; subst. exists rho, beta, H, n. split; [constructor| split; assumption].
  - (* def *)
    destruct (in_dec Nat.eq_dec x ys) as [Hxy|Hxy]; [discriminate|].
    destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
    inversion Hel; subst t Lin.
    destruct (core_def (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x f ys L B vs Hinv HC Hxy HxB Hl)
      as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H1 n1 R (incl_settle_def x ys L) Hinv1 HC1)
      as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', n1. split; [eapply seq_settle; eassumption| split; assumption].
  - (* copy *)
    destruct (Nat.eq_dec x y) as [Exy|Exy]; [discriminate|].
    destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
    destruct (keep L B y) eqn:Hk; inversion Hel; subst t Lin.
    + destruct (core_copy (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x y L B v Hinv HC Exy HxB Hy)
        as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
      destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H1 n1 R (incl_settle_rm2 x y L) Hinv1 HC1)
        as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
      exists rho', beta', H', n1. split; [eapply seq_settle; eassumption| split; assumption].
    + destruct (keep_false L B y Hk) as [HyL HyB].
      destruct (core_move (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x y L B v Hinv HC Exy HyL HyB HxB Hy)
        as [rho1 [Hex1 [Hinv1 HC1]]].
      destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H n R (incl_settle_rm x L) Hinv1 HC1)
        as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
      exists rho', beta', H', n. split; [eapply seq_settle; eassumption| split; assumption].
  - (* pack *)
    destruct (in_dec Nat.eq_dec x ys) as [Hxy|Hxy]; [discriminate|].
    destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
    destruct (nodupb ys) eqn:Hnd; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (core_pack (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x ys L B vs Hinv HC Hxy HxB (nodupb_spec ys Hnd) Hl)
      as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
    assert (Hsub : incl L (x :: filter (keep L B) ys ++ vremove x L)).
    { intros z Hz. destruct (Nat.eq_dec z x) as [E|E]; [left; congruence|].
      right. apply in_or_app. right. apply vremove_In. split; assumption. }
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H1 n1 R Hsub Hinv1 HC1)
      as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', n1. split; [|split; assumption].
    inversion Hex1; subst.
    eapply TE_Seq; [eassumption| eapply TE_Seq; [eassumption| exact Hex2| reflexivity]|].
    destruct tr1, tr2; simpl in *; try discriminate; reflexivity.
  - (* push *)
    destruct (Nat.eq_dec x y) as [Exy|Exy]; [discriminate|].
    destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
    destruct (keep L B y) eqn:Hk; inversion Hel; subst t Lin.
    + destruct (core_push_copy (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x y L B cs v Hinv HC Exy HxB Hx Hy)
        as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
      assert (Hsub : incl L (x :: y :: L)) by (intros z Hz; right; right; exact Hz).
      destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H1 n1 R Hsub Hinv1 HC1)
        as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
      exists rho', beta', H', n1. split; [eapply seq_settle; eassumption| split; assumption].
    + destruct (keep_false L B y Hk) as [HyL HyB].
      destruct (core_push_move (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x y L B cs v Hinv HC Exy HyL HyB HxB Hx Hy)
        as [rho1 [Hex1 [Hinv1 HC1]]].
      assert (Hsub : incl L (x :: L)) by (intros z Hz; right; exact Hz).
      destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H n R Hsub Hinv1 HC1)
        as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
      exists rho', beta', H', n. split; [eapply seq_settle; eassumption| split; assumption].
  - (* field *)
    destruct (Nat.eq_dec x y) as [Exy|Exy]; [discriminate|].
    destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
    inversion Hel; subst t Lin.
    destruct (core_field (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R x y i L B cs v Hinv HC Exy HxB Hy Hi)
      as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H1 n1 R (incl_settle_rm2 x y L) Hinv1 HC1)
      as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', n1. split; [eapply seq_settle; eassumption| split; assumption].
  - (* emit *)
    inversion Hel; subst t Lin.
    pose proof (core_emit (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R y L B v Hinv HC Hy) as Hex1.
    assert (Hsub : incl L (y :: L)) by (intros z Hz; right; exact Hz).
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho beta H n R Hsub Hinv HC)
      as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', n. split; [eapply seq_settle; eassumption| split; assumption].
  - (* seq *)
    destruct (elab M s2 L B) as [[t2 L2]|] eqn:E2; [|discriminate].
    destruct (elab M s1 L2 B) as [[t1 L1]|] eqn:E1; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (IH1 L2 B t1 L1 rho beta H n R E1 Hinv HC) as [rho1 [beta1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]]].
    destruct (IH2 L B t2 L2 rho1 beta1 H1 n1 R E2 Hinv1 HC1) as [rho' [beta' [H' [n' [Hex2 [Hinv' HC']]]]]].
    exists rho', beta', H', n'. split; [eapply TE_Seq; eassumption| split; assumption].
  - (* if true *)
    destruct (elab M s1 L B) as [[t1 L1]|] eqn:E1; [|discriminate].
    destruct (elab M s2 L B) as [[t2 L2]|] eqn:E2; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [left; reflexivity|].
    assert (cv = v) by congruence. subst cv.
    assert (Hsub : incl L1 (c :: L1 ++ L2)) by (intros z Hz; right; apply in_or_app; left; exact Hz).
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L1 B _ rho beta H n R Hsub Hinv HC)
      as [rho1 [beta1 [H1 [Hex1 [Hinv1 HC1]]]]].
    destruct (IH1 L B t1 L1 rho1 beta1 H1 n R E1 Hinv1 HC1) as [rho' [beta' [H' [n' [Hex2 [Hinv' HC']]]]]].
    exists rho', beta', H', n'. split; [|split; assumption].
    eapply TE_IfT; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    eapply TE_Seq; [exact Hex1| exact Hex2| reflexivity].
  - (* if false *)
    destruct (elab M s1 L B) as [[t1 L1]|] eqn:E1; [|discriminate].
    destruct (elab M s2 L B) as [[t2 L2]|] eqn:E2; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [left; reflexivity|].
    assert (cv = v) by congruence. subst cv.
    assert (Hsub : incl L2 (c :: L1 ++ L2)) by (intros z Hz; right; apply in_or_app; right; exact Hz).
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L2 B _ rho beta H n R Hsub Hinv HC)
      as [rho1 [beta1 [H1 [Hex1 [Hinv1 HC1]]]]].
    destruct (IH2 L B t2 L2 rho1 beta1 H1 n R E2 Hinv1 HC1) as [rho' [beta' [H' [n' [Hex2 [Hinv' HC']]]]]].
    exists rho', beta', H', n'. split; [|split; assumption].
    eapply TE_IfF; [exact Hl| eapply inv_read; eassumption| exact Ht|].
    eapply TE_Seq; [exact Hex1| exact Hex2| reflexivity].
  - (* while exits *)
    destruct (elab M b h B) as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L h && vmem c h) eqn:Hchk; [|discriminate].
    apply andb_true_iff in Hchk. destruct Hchk as [Hchk Hch].
    apply andb_true_iff in Hchk. destruct Hchk as [HLb HL].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [apply vmem_true; exact Hch|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) h L B sg rho beta H n R (inclb_spec L h HL) Hinv HC)
      as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', n. split; [|split; assumption].
    eapply TE_Seq; [eapply TE_WhileF; [exact Hl| eapply inv_read; eassumption| exact Ht]| exact Hex2| reflexivity].
  - (* while iterates *)
    destruct (elab M b h B) as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb h && inclb L h && vmem c h) eqn:Hchk; [|discriminate].
    apply andb_true_iff in Hchk. destruct Hchk as [Hchk Hch].
    apply andb_true_iff in Hchk. destruct Hchk as [HLb HL].
    inversion Hel; subst t Lin.
    destruct (corr_read sg rho beta _ B c HC) as [cv [bs [Hl Hcv]]]; [apply vmem_true; exact Hch|].
    assert (cv = v) by congruence. subst cv.
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) h Lb B sg rho beta H n R (inclb_spec Lb h HLb) Hinv HC)
      as [rho1 [beta1 [H1 [Hex1 [Hinv1 HC1]]]]].
    destruct (IHb h B tb Lb rho1 beta1 H1 n R Eb Hinv1 HC1) as [rho2 [beta2 [H2 [n2 [Hex2 [Hinv2 HC2]]]]]].
    assert (Eh : elab M (SWhile c h b) h B =
                 Some (TSeq (TWhile (Src c) (TSeq (settle h Lb B) tb)) (settle h h B), h)).
    { simpl. rewrite Eb. assert (Hhh : inclb h h = true).
      { unfold inclb. apply forallb_forall. intros z Hz. apply vmem_true. exact Hz. }
      rewrite HLb, Hhh, Hch. reflexivity. }
    destruct (IHw h B _ h rho2 beta2 H2 n2 R Eh Hinv2 HC2) as [rho3 [beta3 [H3 [n3 [Hex3 [Hinv3 HC3]]]]]].
    inversion Hex3 as [| | | | | | | | | | ? ? ? ? rho4 beta4 H4 n4 ? ? ? ? ? ? tr3 tr4 ? Hloop Hset Htr34 | | | | | | |]; subst.
    unfold settle in Hset. rewrite vdiff_self in Hset. simpl in Hset.
    inversion Hset; subst.
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) h L B sg2 rho3 beta3 H3 n3 R (inclb_spec L h HL) Hinv3 HC3)
      as [rho' [beta' [H' [Hex4 [Hinv' HC']]]]].
    exists rho', beta', H', n3. split; [|split; assumption].
    assert (Hbody : texec (tfuns_of M funs) (tprocs_of M procs) rho beta H n (TSeq (settle h Lb B) tb) rho2 beta2 H2 n2 tr1)
      by (eapply TE_Seq; [exact Hex1| exact Hex2| reflexivity]).
    assert (Hwhile : texec (tfuns_of M funs) (tprocs_of M procs) rho beta H n
                           (TWhile (Src c) (TSeq (settle h Lb B) tb)) rho3 beta3 H3 n3 (tr1 ++ tr3))
      by (eapply TE_WhileT; [exact Hl| eapply inv_read; eassumption| exact Ht| exact Hbody| exact Hloop| reflexivity]).
    eapply TE_Seq; [exact Hwhile| exact Hex4|].
    repeat rewrite app_nil_r. reflexivity.
  - (* call: sink arguments move in, borrowed ones are lent *)
    destruct (in_dec Nat.eq_dec x ys) as [Hxy|Hxy]; [discriminate|].
    destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
    change (elab_call M KFun x g ys L B = Some (t, Lin)) in Hel.
    destruct (elab_call_spec M KFun x g ys L B t Lin Hel) as [ms [Ek [Hml [Hnd [Et ELin]]]]].
    subst t Lin. simpl in Ek.
    destruct (elab_fun M g (ps, body, ret)) as [d'|] eqn:Ef;
      [|exfalso; exact (funs_ok g (ps, body, ret) Hg Ef)].
    assert (Er : elab_routine M ps ms body ret = Some d')
      by (unfold elab_fun in Ef; rewrite Ek in Ef; exact Ef).
    destruct (elab_routine_spec M ps ms body ret d' Er)
      as [HNp [Hrp [tb [Lb [Eb [HLb Ed]]]]]]. subst d'.
    assert (Hrt : troutine (tfuns_of M funs) (tprocs_of M procs) KFun g =
                  Some (sinks ms ps, borrows ms ps, TSeq (settle ps Lb (borrows ms ps)) tb, ret))
      by (unfold troutine, tfuns_of; rewrite Hg; exact Ef).
    destruct (core_call (tfuns_of M funs) (tprocs_of M procs) KFun sg rho beta H n R x g ys L B vs
                ms ps _ ret sgc tr v Hinv HC HxB
                (fun F => False_ind _ (Hxy F)) Hnd Hl Hrt HNp Hlen Hrp)
      as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
    + intros rho0 beta0 H0 n0 R0 Hi0 Hc0.
      destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) ps Lb _ _ _ _ H0 n0 R0 HLb Hi0 Hc0)
        as [rhoa [betaa [Ha [Hexa [Hinva HCa]]]]].
      destruct (IHbody [ret] _ tb Lb rhoa betaa Ha n0 R0 Eb Hinva HCa)
        as [rhob [betab [Hb' [nb [Hexb [Hinvb HCb]]]]]].
      exists rhob, betab, Hb', nb.
      split; [eapply TE_Seq; [exact Hexa| exact Hexb| reflexivity]| split; assumption].
    + exact Hret.
    + destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ _ beta H1 n1 R
                  (incl_settle_call x _ _ L) Hinv1 HC1) as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
      exists rho', beta', H', n1. split; [eapply seq_settle; eassumption| split; assumption].
  - (* inout call: the inout argument is the first sink argument and the result *)
    destruct (in_dec Nat.eq_dec z ys) as [Hzy|Hzy]; [discriminate|].
    destruct (in_dec Nat.eq_dec z B) as [HzB|HzB]; [discriminate|].
    change (elab_call M KProc z g (z :: ys) L B = Some (t, Lin)) in Hel.
    destruct (elab_call_spec M KProc z g (z :: ys) L B t Lin Hel) as [ms [Ek [Hml [Hnd [Et ELin]]]]].
    subst t Lin.
    assert (Hpm : exists pm, pmodes M g = Some pm /\ ms = true :: pm).
    { revert Ek. cbn [kmodes option_map]. destruct (pmodes M g) as [pm|]; intros Ek; [|discriminate].
      injection Ek as Ek. exists pm. split; [reflexivity| symmetry; exact Ek]. }
    destruct Hpm as [pm [Epm Ems]]. subst ms.
    destruct (elab_proc M g (io, ps, body)) as [d'|] eqn:Ep;
      [|exfalso; exact (procs_ok g (io, ps, body) Hg Ep)].
    assert (Er : elab_routine M (io :: ps) (true :: pm) body io = Some d')
      by (unfold elab_proc in Ep; rewrite Epm in Ep; exact Ep).
    destruct (elab_routine_spec M (io :: ps) (true :: pm) body io d' Er)
      as [HNp [Hrp [tb [Lb [Eb [HLb Ed]]]]]]. subst d'.
    assert (Hrt : troutine (tfuns_of M funs) (tprocs_of M procs) KProc g =
                  Some (sinks (true :: pm) (io :: ps), borrows (true :: pm) (io :: ps),
                        TSeq (settle (io :: ps) Lb (borrows (true :: pm) (io :: ps))) tb, io))
      by (unfold troutine, tprocs_of; rewrite Hg; exact Ep).
    assert (Hzk : In z (z :: ys) -> In z (sinks (true :: pm) (z :: ys)) /\
                  keep (borrows (true :: pm) (z :: ys) ++ vremove z L) B z = false).
    { intros _. split; [simpl; left; reflexivity|].
      unfold keep. apply orb_false_iff. split; apply vmem_false; [|exact HzB].
      intros Hin. apply in_app_or in Hin. destruct Hin as [Hin|Hin].
      - simpl in Hin. apply Hzy. eapply in_borrows. exact Hin.
      - apply vremove_In in Hin. apply Hin. reflexivity. }
    assert (Hsrc : slookup_all sg (z :: ys) = Some (vz :: vs)) by (simpl; rewrite Hz, Hl; reflexivity).
    assert (Hlen' : length (io :: ps) = length (vz :: vs)) by (simpl; rewrite Hlen; reflexivity).
    destruct (core_call (tfuns_of M funs) (tprocs_of M procs) KProc sg rho beta H n R z g (z :: ys) L B
                (vz :: vs) (true :: pm) (io :: ps) _ io sgc tr v Hinv HC HzB Hzk
                Hnd Hsrc Hrt HNp Hlen' Hrp)
      as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
    + intros rho0 beta0 H0 n0 R0 Hi0 Hc0.
      destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) (io :: ps) Lb _ _ _ _ H0 n0 R0 HLb Hi0 Hc0)
        as [rhoa [betaa [Ha [Hexa [Hinva HCa]]]]].
      destruct (IHbody [io] _ tb Lb rhoa betaa Ha n0 R0 Eb Hinva HCa)
        as [rhob [betab [Hb' [nb [Hexb [Hinvb HCb]]]]]].
      exists rhob, betab, Hb', nb.
      split; [eapply TE_Seq; [exact Hexa| exact Hexb| reflexivity]| split; assumption].
    + exact Hret.
    + destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ _ beta H1 n1 R
                  (incl_settle_call z _ _ L) Hinv1 HC1) as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
      exists rho', beta', H', n1. split; [eapply seq_settle; eassumption| split; assumption].
  - (* focus: a part moves out of the root, is worked on, and moves back *)
    destruct (Nat.eq_dec ft fy) as [Hty|Hty]; [discriminate|].
    destruct (in_dec Nat.eq_dec ft B) as [HtB|HtB]; [discriminate|].
    destruct (in_dec Nat.eq_dec fy B) as [HyB|HyB]; [discriminate|].
    destruct (in_dec Nat.eq_dec ft L) as [HtL|HtL]; [discriminate|].
    destruct (elab M fs (ft :: vremove fy L) B) as [[ts Ls]|] eqn:Es; [|discriminate].
    destruct (vmem fy Ls) eqn:HyLs; [discriminate|].
    inversion Hel; subst t Lin.
    apply vmem_false in HyLs.
    destruct (corr_owned sg rho beta _ B fy HC) as [c0 [bs [Hly Hc0]]]; [left; reflexivity| exact HyB|].
    assert (c0 = c) by congruence. subst c0.
    destruct (vget_voff fp c cv Hg) as [off [Hoff Hle]].
    assert (Hut : unbound (Src ft) (tremove (Src fy) rho) beta).
    { destruct (corr_unbound sg rho beta _ B ft HC) as [U1 U2].
      - intros [F|F]; [congruence|]. apply vremove_In in F. apply F. reflexivity.
      - split; [rewrite rm_src_src; destruct (Nat.eq_dec fy ft); [reflexivity| exact U1]| exact U2]. }
    pose proof (focus_inv_entry rho beta H n R (Src ft) (Src fy) c bs cv off Hinv Hly Hut Hle) as Hinv0.
    pose proof (corr_swap sg rho beta Ls B fy ft cv (firstn (vsize cv) (skipn off bs))
                  HC (not_eq_sym Hty) HyB HtB HyLs) as HC0.
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ Ls B _ _ _ H n _
                (incl_settle_rm ft Ls) Hinv0 HC0) as [rhoa [betaa [Ha [Hexa [Hinva HCa]]]]].
    destruct (IHs (ft :: vremove fy L) B ts Ls rhoa betaa Ha n _ Es Hinva HCa)
      as [rhob [betab [Hb' [nb [Hexb [Hinvb HCb]]]]]].
    destruct (corr_owned sg1 rhob betab _ B ft HCb) as [cv0 [bf' [Hlt Hcv0]]]; [left; reflexivity| exact HtB|].
    assert (cv0 = cv') by congruence. subst cv0.
    assert (Huy : unbound (Src fy) (tremove (Src ft) rhob) betab).
    { destruct (corr_unbound sg1 rhob betab _ B fy HCb) as [U1 U2].
      - intros [F|F]; [congruence|]. apply vremove_In in F. apply F. reflexivity.
      - split; [rewrite rm_src_src; destruct (Nat.eq_dec ft fy); [reflexivity| exact U1]| exact U2]. }
    assert (Hinner : texec (tfuns_of M funs) (tprocs_of M procs)
                      ((Src ft, (cv, firstn (vsize cv) (skipn off bs))) :: tremove (Src fy) rho) beta H n
                      (TSeq (settle (ft :: vremove ft Ls) Ls B) ts) rhob betab Hb' nb tr)
      by (eapply TE_Seq; [exact Hexa| exact Hexb| reflexivity]).
    pose proof (beta_shrinks _ _ _ _ _ _ _ _ _ _ _ _ Hinner) as Hsh.
    pose proof (vget_vset_size fp c cv cv' c' Hg Hset) as Hsz.
    pose proof (focus_inv_exit rho beta H n R (Src fy) c bs cv off rhob betab Hb' nb (Src ft) cv' bf' c'
                  Hinv Hly Hle Hinvb Hsh Hlt Huy Hsz) as Hinv1.
    pose proof (corr_swap sg1 rhob betab L B ft fy c' (firstn off bs ++ bf' ++ skipn (off + vsize cv) bs)
                  HCb Hty HtB HyB HtL) as HC1.
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ _ _ Hb' nb R
                (incl_settle_rm fy L) Hinv1 HC1) as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', nb. split; [|split; assumption].
    eapply seq_settle; [|exact Hex2].
    eapply TE_Focus; [exact Hly| exact Hg| exact Hoff| exact Hut| exact Hinner| exact Hlt| exact Huy| exact Hset].
  - (* unpack: the parts of a dead record become separate owners *)
    destruct (in_dec Nat.eq_dec uy L) as [HyL|HyL]; [discriminate|].
    destruct (in_dec Nat.eq_dec uy B) as [HyB|HyB]; [discriminate|].
    destruct (in_dec Nat.eq_dec uy uxs) as [Hyx|Hyx]; [discriminate|].
    destruct (nodupb uxs && forallb (fun x => negb (vmem x B)) uxs) eqn:Hck; [|discriminate].
    apply andb_true_iff in Hck. destruct Hck as [Hnd Hfb].
    inversion Hel; subst t Lin.
    assert (HxB : forall x, In x uxs -> ~ In x B).
    { intros x Hx. rewrite forallb_forall in Hfb. specialize (Hfb x Hx).
      apply negb_true_iff in Hfb. apply vmem_false. exact Hfb. }
    destruct (core_unpack (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R uy uxs L B ucs
                Hinv HC HyL HyB Hyx (nodupb_spec _ Hnd) HxB Huy Hulen)
      as [rho1 [H1 [Hex1 [Hinv1 HC1]]]].
    assert (Hsub : incl L (uxs ++ vdiff L uxs)).
    { intros z Hz. apply in_or_app. destruct (in_dec Nat.eq_dec z uxs) as [Hzx|Hzx];
        [left; exact Hzx| right; apply vdiff_In; split; assumption]. }
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ L B _ rho1 beta H1 n R Hsub Hinv1 HC1)
      as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', n. split; [eapply seq_settle; eassumption| split; assumption].
  - (* region: the value lives to the end of the region and is released there *)
    destruct (in_dec Nat.eq_dec rx L) as [HxL|HxL]; [discriminate|].
    destruct (in_dec Nat.eq_dec rx B) as [HxB|HxB]; [discriminate|].
    destruct (in_dec Nat.eq_dec rx rys) as [Hxy|Hxy]; [discriminate|].
    destruct (writes rx rs) eqn:Hw; [discriminate|].
    destruct (elab M rs (rx :: L) B) as [[ts Ls]|] eqn:Es; [|discriminate].
    inversion Hel; subst t Lin.
    destruct (core_def (tfuns_of M funs) (tprocs_of M procs) sg rho beta H n R rx rf rys Ls B rvs
                Hinv HC Hxy HxB Hrl) as [rho1 [H1 [n1 [Hex1 [Hinv1 HC1]]]]].
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) _ Ls B _ _ beta H1 n1 R
                (incl_settle_def rx rys Ls) Hinv1 HC1) as [rhoa [betaa [Ha [Hexa [Hinva HCa]]]]].
    destruct (IHrs (rx :: L) B ts Ls rhoa betaa Ha n1 R Es Hinva HCa)
      as [rhob [betab [Hb' [nb [Hexb [Hinvb HCb]]]]]].
    assert (Hsub : incl L (rx :: L)) by (intros z Hz; right; exact Hz).
    destruct (settle_exec (tfuns_of M funs) (tprocs_of M procs) (rx :: L) L B _ rhob betab Hb' nb R
                Hsub Hinvb HCb) as [rho' [beta' [H' [Hex2 [Hinv' HC']]]]].
    exists rho', beta', H', nb. split; [|split; assumption].
    eapply TE_Seq; [eapply seq_settle; eassumption| eapply seq_settle; eassumption| reflexivity].
Qed.

Lemma corr_empty_envs : forall sg rho beta, CORR sg rho beta [] [] -> rho = [] /\ beta = [].
Proof.
  intros sg rho beta [Ho [Hb [Tr [Tb _]]]]. split.
  - destruct rho as [|[k v] r]; [reflexivity|]. exfalso. destruct k as [x|x].
    + assert (F : In x [] /\ ~ In x []) by (apply Ho; rewrite lk_src_src;
        destruct (Nat.eq_dec x x) as [_|F]; [discriminate| exfalso; apply F; reflexivity]).
      destruct F as [[] _].
    + specialize (Tr x). rewrite lk_tmp_tmp in Tr.
      destruct (Nat.eq_dec x x) as [_|F]; [discriminate| apply F; reflexivity].
  - destruct beta as [|[k v] r]; [reflexivity|]. exfalso. destruct k as [x|x].
    + assert (F : In x [] /\ In x []) by (apply Hb; rewrite lk_src_src;
        destruct (Nat.eq_dec x x) as [_|F]; [discriminate| exfalso; apply F; reflexivity]).
      destruct F as [[] _].
    + specialize (Tb x). rewrite lk_tmp_tmp in Tb.
      destruct (Nat.eq_dec x x) as [_|F]; [discriminate| apply F; reflexivity].
Qed.

Theorem closed_program_frees_everything : forall s t sg sg' tr,
  elab M s [] [] = Some (t, []) -> sexec funs procs sg s sg' tr ->
  exists n', texec (tfuns_of M funs) (tprocs_of M procs) [] [] [] 0 t [] [] [] n' tr.
Proof.
  intros s t sg sg' tr Hel Hs.
  assert (Hinv0 : INV [] [] [] 0 []).
  { split; [constructor|split; [constructor|split; [constructor|split; [|split; [|split]]]]].
    - intros b [].
    - intros z c bs F. discriminate.
    - intros z c bs F. discriminate.
    - intros z c bs F. discriminate. }
  assert (HC0 : CORR sg [] [] [] []).
  { split; [|split; [|split; [|split; [|split]]]].
    - intros x. simpl. split; [intros F; exfalso; apply F; reflexivity| intros [[] _]].
    - intros x. simpl. split; [intros F; exfalso; apply F; reflexivity| intros [[] _]].
    - intros x. reflexivity.
    - intros x. reflexivity.
    - intros x c bs F. discriminate.
    - intros x c bs F. discriminate. }
  destruct (elab_sound sg s sg' tr Hs [] [] t [] [] [] [] 0 [] Hel Hinv0 HC0)
    as [rho' [beta' [H' [n' [Hex [Hinv' HC']]]]]].
  destruct (corr_empty_envs sg' rho' beta' HC') as [E1 E2]. subst rho' beta'.
  destruct Hinv' as [_ [_ [HP _]]]. simpl in HP. apply Permutation_sym in HP.
  apply Permutation_nil in HP. subst H'.
  exists n'. exact Hex.
Qed.

End Soundness.

(* ------------------------------------------------------------------ *)
(* Consequences                                                         *)
(* ------------------------------------------------------------------ *)

(* At every boundary reached by the elaborated program, the live heap is
   exactly the disjoint union of the owned footprints and the frame heap. *)
Theorem heap_is_live_footprint : forall rho beta H n R,
  INV rho beta H n R -> Permutation H (heap_of rho ++ R) /\ NoDup (heap_of rho ++ R).
Proof.
  intros rho beta H n R [_ [HH [HP _]]]. split; [exact HP|].
  eapply Permutation_NoDup; eassumption.
Qed.

(* Implementation refinement for release placement. The part of a post-set
   that is still live afterwards releases nothing, so the releases after a
   statement are determined by the operands it mentions. An implementation
   can compute them with one membership test per operand instead of walking
   the live set; only branch arms, loop boundaries and routine entry compare
   whole live sets. *)
Lemma settle_tail : forall A T L B, incl T L -> settle (A ++ T) L B = settle A L B.
Proof.
  intros A T L B Hs. unfold settle, vdiff. rewrite filter_app.
  assert (E : filter (fun z => negb (vmem z L)) T = []).
  { clear A. induction T as [|z T IH]; simpl; [reflexivity|].
    assert (Hz : vmem z L = true) by (apply vmem_true; apply Hs; left; reflexivity).
    rewrite Hz. simpl. apply IH. intros w Hw. apply Hs. right. exact Hw. }
  rewrite E, app_nil_r. reflexivity.
Qed.

Theorem elab_def_releases_operands : forall M x f ys L B,
  ~ In x ys -> ~ In x B ->
  elab M (SDef x f ys) L B =
    Some (TSeq (TDef (Src x) f (map Src ys)) (settle (x :: ys) L B), ys ++ vremove x L).
Proof.
  intros M x f ys L B Hxy HxB.
  rewrite <- (settle_tail (x :: ys) (vremove x L) L B)
    by (intros z Hz; apply vremove_In in Hz; apply Hz).
  simpl. destruct (in_dec Nat.eq_dec x ys); [contradiction|].
  destruct (in_dec Nat.eq_dec x B); [contradiction|]. reflexivity.
Qed.

Theorem elab_call_releases_operands : forall M k x g args L B t Lin,
  elab_call M k x g args L B = Some (t, Lin) ->
  exists ms tc, kmodes M k g = Some ms /\
    t = TSeq tc (settle (x :: filter (keep (borrows ms args ++ vremove x L) B) (sinks ms args)
                           ++ borrows ms args) L B).
Proof.
  intros M k x g args L B t Lin E.
  destruct (elab_call_spec M k x g args L B t Lin E) as [ms [Ek [_ [_ [Et _]]]]]. subst t.
  exists ms. eexists. split; [exact Ek|]. f_equal.
  rewrite <- (settle_tail (x :: filter (keep (borrows ms args ++ vremove x L) B) (sinks ms args)
                             ++ borrows ms args) (vremove x L) L B)
    by (intros z Hz; apply vremove_In in Hz; apply Hz).
  rewrite <- app_comm_cons, <- app_assoc. reflexivity.
Qed.

(* The move/copy decision table. *)
Theorem elab_copy_moves_dead_source : forall M x y L B,
  x <> y -> ~ In x B -> ~ In y L -> ~ In y B ->
  elab M (SCopy x y) L B =
    Some (TSeq (TMove (Src x) (Src y)) (settle (x :: vremove x L) L B), y :: vremove x L).
Proof.
  intros M x y L B Hxy HxB HyL HyB. simpl. destruct (Nat.eq_dec x y); [contradiction|].
  destruct (in_dec Nat.eq_dec x B); [contradiction|].
  assert (Hk : keep L B y = false).
  { unfold keep. apply orb_false_iff. split; apply vmem_false; assumption. }
  rewrite Hk. reflexivity.
Qed.

Theorem elab_copy_copies_live_source : forall M x y L B,
  x <> y -> ~ In x B -> In y L ->
  elab M (SCopy x y) L B =
    Some (TSeq (TCopy (Src x) (Src y)) (settle (x :: y :: vremove x L) L B), y :: vremove x L).
Proof.
  intros M x y L B Hxy HxB HyL. simpl. destruct (Nat.eq_dec x y); [contradiction|].
  destruct (in_dec Nat.eq_dec x B); [contradiction|].
  assert (Hk : keep L B y = true) by (apply keep_true; left; exact HyL).
  rewrite Hk. reflexivity.
Qed.

(* A borrowed parameter is never moved: its storage belongs to the caller. *)
Theorem elab_copy_copies_borrowed_source : forall M x y L B,
  x <> y -> ~ In x B -> In y B ->
  elab M (SCopy x y) L B =
    Some (TSeq (TCopy (Src x) (Src y)) (settle (x :: y :: vremove x L) L B), y :: vremove x L).
Proof.
  intros M x y L B Hxy HxB HyB. simpl. destruct (Nat.eq_dec x y); [contradiction|].
  destruct (in_dec Nat.eq_dec x B); [contradiction|].
  assert (Hk : keep L B y = true) by (apply keep_true; right; exact HyB).
  rewrite Hk. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Refutations                                                          *)
(* ------------------------------------------------------------------ *)

(* A shallow alias copy (the descriptor is copied, the blocks are shared)
   gives two owners one block; the two automatic drops then free it twice,
   and the second free is refused. *)
Definition alias_env : TEnv := [(Src 1, (SLeaf 7, [(0, 0)])); (Src 0, (SLeaf 7, [(0, 0)]))].

Theorem alias_copy_breaks_invariant : forall n R, ~ INV alias_env [] [(0, 0)] n R.
Proof.
  intros n R [_ [_ [HP _]]]. simpl in HP.
  apply Permutation_length in HP. simpl in HP. lia.
Qed.

Theorem alias_copy_double_free : forall tf tp rho' beta' H' n' tr,
  ~ texec tf tp alias_env [] [(0, 0)] 1 (TSeq (TDrop (Src 1)) (TDrop (Src 0))) rho' beta' H' n' tr.
Proof.
  intros tf tp rho' beta' H' n' tr Hex. inversion Hex; subst.
  match goal with Hd : texec _ _ alias_env [] _ 1 (TDrop (Src 1)) _ _ _ _ _ |- _ => inversion Hd; subst end.
  match goal with Hd : texec _ _ (tremove (Src 1) alias_env) _ _ _ (TDrop (Src 0)) _ _ _ _ _ |- _ => inversion Hd; subst end.
  match goal with
  | Hl : tlookup (Src 0) (tremove (Src 1) alias_env) = Some (?c, ?bs),
    Hi : incl ?bs (free ?bs1 _),
    Hl1 : tlookup (Src 1) alias_env = Some (?c1, ?bs1) |- _ =>
      simpl in Hl, Hl1; inversion Hl; inversion Hl1; subst;
      specialize (Hi (0, 0) (or_introl eq_refl)); simpl in Hi; exact Hi
  end.
Qed.

(* Dropping before the last use leaves that use with no rule. *)
Theorem early_drop_use_after_free : forall tf tp rho' beta' H' n' tr,
  ~ texec tf tp [(Src 0, (SLeaf 1, [(0, 0)]))] [] [(0, 0)] 1 (TSeq (TDrop (Src 0)) (TEmit (Src 0)))
          rho' beta' H' n' tr.
Proof.
  intros tf tp rho' beta' H' n' tr Hex. inversion Hex; subst.
  match goal with Hd : texec _ _ _ _ _ _ (TDrop (Src 0)) _ _ _ _ _ |- _ => inversion Hd; subst end.
  match goal with He : texec _ _ _ _ _ _ (TEmit (Src 0)) _ _ _ _ _ |- _ => inversion He; subst end.
  match goal with Hl : tread (Src 0) (tremove (Src 0) _) _ = Some _ |- _ => simpl in Hl; discriminate end.
Qed.

(* A callee cannot release a borrowed parameter: the drop has no rule. *)
Theorem borrowed_drop_refused : forall tf tp rho' beta' H' n' tr,
  ~ texec tf tp [] [(Src 0, (SLeaf 1, [(0, 0)]))] [(0, 0)] 1 (TDrop (Src 0)) rho' beta' H' n' tr.
Proof.
  intros tf tp rho' beta' H' n' tr Hex. inversion Hex; subst.
  match goal with Hl : tlookup (Src 0) [] = Some _ |- _ => simpl in Hl; discriminate end.
Qed.

(* Without the settle drops, the value of a dead definition stays live
   with no owner left to release it. *)
Theorem missing_settle_leaks : forall tf tp M,
  texec tf tp [] [] [] 0 (TDef (Src 0) (fun _ => SLeaf 3) [])
        [(Src 0, (SLeaf 3, [(0, 0)]))] [] [(0, 0)] 1 [] /\
  elab M (SDef 0 (fun _ => SLeaf 3) []) [] [] =
    Some (TSeq (TDef (Src 0) (fun _ => SLeaf 3) []) (settle [0] [] []), []).
Proof.
  intros tf tp M. split.
  - refine (TE_Def tf tp [] [] [] 0 (Src 0) (fun _ => SLeaf 3) [] [] _ _ _ _);
      [split; reflexivity| reflexivity| constructor| intros b []].
  - reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Witnesses: an Array<GuiDraw> with a Label(String) element            *)
(* ------------------------------------------------------------------ *)

Fixpoint count_copies (t : TStmt) : nat :=
  match t with
  | TCopy _ _ => 1
  | TSeq a b => count_copies a + count_copies b
  | TIf _ a b => count_copies a + count_copies b
  | TWhile _ b => count_copies b
  | TFocus _ _ _ b => count_copies b
  | _ => 0
  end.

Definition no_funs : SFunTable := fun _ => None.
Definition no_procs : SProcTable := fun _ => None.

(* Inline: build a String, wrap it in Label(...), push the label into a
   draw list, and observe the list. No source line mentions ownership, and
   the elaborated program copies nothing. *)
Definition gui_program : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 42) [])
  (SSeq (SPack 1 [0])
  (SSeq (SDef 2 (fun _ => SNode []) [])
  (SSeq (SPush 2 1)
        (SEmit 2)))).

Example gui_program_moves_only :
  exists t, elab no_summaries gui_program [] [] = Some (t, []) /\ count_copies t = 0.
Proof. eexists. split; reflexivity. Qed.

(* Reading the label again after the push forces exactly one copy. *)
Definition gui_program_reuse : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 42) [])
  (SSeq (SPack 1 [0])
  (SSeq (SDef 2 (fun _ => SNode []) [])
  (SSeq (SPush 2 1)
  (SSeq (SEmit 1)
        (SEmit 2))))).

Example gui_program_reuse_copies_once :
  exists t, elab no_summaries gui_program_reuse [] [] = Some (t, []) /\ count_copies t = 1.
Proof. eexists. split; reflexivity. Qed.

Lemma no_funs_ok : forall M g d, no_funs g = Some d -> elab_fun M g d <> None.
Proof. intros M g d E. discriminate. Qed.

Lemma no_procs_ok : forall M g d, no_procs g = Some d -> elab_proc M g d <> None.
Proof. intros M g d E. discriminate. Qed.

Lemma gui_program_source :
  exists sg', sexec no_funs no_procs sempty gui_program sg' [SNode [SNode [SLeaf 42]]].
Proof.
  eexists. unfold gui_program.
  eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |].
  - eapply SE_Seq; [apply SE_Pack with (vs := [SLeaf 42]); reflexivity| |].
    + eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |].
      * eapply SE_Seq; [eapply SE_Push; reflexivity| eapply SE_Emit; reflexivity|].
        reflexivity.
      * reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Example gui_program_runs_clean :
  exists t n', elab no_summaries gui_program [] [] = Some (t, []) /\
    texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs) [] [] [] 0 t [] [] [] n'
          [SNode [SNode [SLeaf 42]]].
Proof.
  destruct gui_program_source as [sg' Hs].
  destruct gui_program_moves_only as [t [Ht _]].
  destruct (closed_program_frees_everything no_summaries no_funs no_procs
              (no_funs_ok no_summaries) (no_procs_ok no_summaries) gui_program t _ _ _ Ht Hs)
    as [n' Hex].
  exists t, n'. split; assumption.
Qed.

(* Through calls: MakeLabel(text) builds the label and AddDraw(inout draws,
   label) appends it. *)
Definition gui_funs : SFunTable :=
  fun g => if Nat.eqb g 0 then Some ([0], SPack 1 [0], 1) else None.
Definition gui_procs : SProcTable :=
  fun g => if Nat.eqb g 0 then Some (5, [6], SPush 5 6) else None.

Definition gui_calls : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 42) [])
  (SSeq (SCall 1 0 [0])
  (SSeq (SDef 2 (fun _ => SNode []) [])
  (SSeq (SCallIO 0 2 [1])
        (SEmit 2)))).

(* Both routines elaborate under every mode table that resolves them with
   one entry per parameter, whatever that entry is. *)
Lemma gui_funs_ok : forall M, (exists m, fmodes M 0 = Some [m]) ->
  forall g d, gui_funs g = Some d -> elab_fun M g d <> None.
Proof.
  intros M [m Hm] g d E. unfold gui_funs in E. destruct (Nat.eqb g 0) eqn:Eg; [|discriminate].
  apply Nat.eqb_eq in Eg. subst g. inversion E; subst.
  unfold elab_fun. rewrite Hm. unfold elab_routine. destruct m; simpl; discriminate.
Qed.

Lemma gui_procs_ok : forall M, (exists m, pmodes M 0 = Some [m]) ->
  forall g d, gui_procs g = Some d -> elab_proc M g d <> None.
Proof.
  intros M [m Hm] g d E. unfold gui_procs in E. destruct (Nat.eqb g 0) eqn:Eg; [|discriminate].
  apply Nat.eqb_eq in Eg. subst g. inversion E; subst.
  unfold elab_proc. rewrite Hm. unfold elab_routine. destruct m; simpl; discriminate.
Qed.

(* The all-borrowed table for the GUI routines. *)
Definition gui_borrow : Modes := borrow_modes gui_funs gui_procs.

(* Borrow-only parameters: the constructor and the append each copy their
   borrowed argument, so the call version costs two copies. *)
Example gui_calls_copy_twice :
  exists t ft pt,
    elab gui_borrow gui_calls [] [] = Some (t, []) /\
    tfuns_of gui_borrow gui_funs 0 = Some ([], [0], ft, 1) /\
    tprocs_of gui_borrow gui_procs 0 = Some ([5], [6], pt, 5) /\
    count_copies t + count_copies ft + count_copies pt = 2.
Proof. do 3 eexists. split; [reflexivity| split; [reflexivity| split; reflexivity]]. Qed.

Lemma gui_calls_source :
  exists sg', sexec gui_funs gui_procs sempty gui_calls sg' [SNode [SNode [SLeaf 42]]].
Proof.
  eexists. unfold gui_calls.
  eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |].
  - eapply SE_Seq.
    + eapply SE_Call with (vs := [SLeaf 42]);
        [reflexivity| reflexivity| reflexivity| apply SE_Pack with (vs := [SLeaf 42]); reflexivity| reflexivity].
    + eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |].
      * eapply SE_Seq.
        -- eapply SE_CallIO with (vs := [SNode [SLeaf 42]]);
             [reflexivity| reflexivity| reflexivity| reflexivity| eapply SE_Push; reflexivity| reflexivity].
        -- eapply SE_Emit. reflexivity.
        -- reflexivity.
      * reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Example gui_calls_runs_clean :
  exists t n', elab gui_borrow gui_calls [] [] = Some (t, []) /\
    texec (tfuns_of gui_borrow gui_funs) (tprocs_of gui_borrow gui_procs) [] [] [] 0 t [] [] [] n'
          [SNode [SNode [SLeaf 42]]].
Proof.
  destruct gui_calls_source as [sg' Hs].
  destruct gui_calls_copy_twice as [t [ft [pt [Ht _]]]].
  destruct (closed_program_frees_everything gui_borrow gui_funs gui_procs
              (gui_funs_ok gui_borrow (ex_intro _ _ eq_refl))
              (gui_procs_ok gui_borrow (ex_intro _ _ eq_refl)) gui_calls t _ _ _ Ht Hs)
    as [n' Hex].
  exists t, n'. split; assumption.
Qed.

(* Mode inference on the same program. MakeLabel stores its text and
   AddDraw stores its label, so both parameters are inferred sink. A second
   inference round changes nothing. *)
Definition gui_modes : Modes := infer_modes gui_funs gui_procs no_summaries.

Example gui_modes_inferred :
  fmodes gui_modes 0 = Some [true] /\ pmodes gui_modes 0 = Some [true].
Proof. split; reflexivity. Qed.

Example gui_modes_fixpoint : forall g,
  fmodes (infer_modes gui_funs gui_procs gui_modes) g = fmodes gui_modes g /\
  pmodes (infer_modes gui_funs gui_procs gui_modes) g = pmodes gui_modes g.
Proof.
  intros g. unfold gui_modes, infer_modes, gui_funs, gui_procs. simpl.
  destruct (Nat.eqb g 0); split; reflexivity.
Qed.

(* With sink parameters the call version copies nothing, like the inline
   version: the text moves into MakeLabel, the label moves into AddDraw,
   and the draw list moves through the inout call and back. *)
Example gui_calls_sink_moves_only :
  exists t ft pt,
    elab gui_modes gui_calls [] [] = Some (t, []) /\
    tfuns_of gui_modes gui_funs 0 = Some ([0], [], ft, 1) /\
    tprocs_of gui_modes gui_procs 0 = Some ([5; 6], [], pt, 5) /\
    count_copies t + count_copies ft + count_copies pt = 0.
Proof. do 3 eexists. split; [reflexivity| split; [reflexivity| split; reflexivity]]. Qed.

Example gui_calls_sink_runs_clean :
  exists t n', elab gui_modes gui_calls [] [] = Some (t, []) /\
    texec (tfuns_of gui_modes gui_funs) (tprocs_of gui_modes gui_procs) [] [] [] 0 t [] [] [] n'
          [SNode [SNode [SLeaf 42]]].
Proof.
  destruct gui_calls_source as [sg' Hs].
  destruct gui_calls_sink_moves_only as [t [ft [pt [Ht _]]]].
  destruct (closed_program_frees_everything gui_modes gui_funs gui_procs
              (gui_funs_ok gui_modes (ex_intro _ _ eq_refl))
              (gui_procs_ok gui_modes (ex_intro _ _ eq_refl)) gui_calls t _ _ _ Ht Hs)
    as [n' Hex].
  exists t, n'. split; assumption.
Qed.

(* A caller that still needs the text after building the label. With sink
   parameters it pays one copy, at the call site; with borrow-only
   parameters it pays two, inside the callees. *)
Definition gui_calls_reuse : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 42) [])
  (SSeq (SCall 1 0 [0])
  (SSeq (SEmit 0)
  (SSeq (SDef 2 (fun _ => SNode []) [])
  (SSeq (SCallIO 0 2 [1])
        (SEmit 2))))).

Example gui_calls_sink_reuse_copies_once :
  exists t ft pt,
    elab gui_modes gui_calls_reuse [] [] = Some (t, []) /\
    tfuns_of gui_modes gui_funs 0 = Some ([0], [], ft, 1) /\
    tprocs_of gui_modes gui_procs 0 = Some ([5; 6], [], pt, 5) /\
    count_copies t = 1 /\ count_copies ft + count_copies pt = 0.
Proof.
  do 3 eexists. split; [reflexivity| split; [reflexivity| split; [reflexivity| split; reflexivity]]].
Qed.

Example gui_calls_reuse_borrowed_copies_twice :
  exists t ft pt,
    elab gui_borrow gui_calls_reuse [] [] = Some (t, []) /\
    tfuns_of gui_borrow gui_funs 0 = Some ([], [0], ft, 1) /\
    tprocs_of gui_borrow gui_procs 0 = Some ([5], [6], pt, 5) /\
    count_copies t + count_copies ft + count_copies pt = 2.
Proof. do 3 eexists. split; [reflexivity| split; [reflexivity| split; reflexivity]]. Qed.

(* An owned functional update: Append(xs, y) pushes y and returns xs. With
   borrow-only parameters it is refused, because a borrowed parameter can
   be neither mutated nor returned. Inference makes both parameters sink,
   and the body then moves y into xs with no copy and no annotation. *)
Definition upd_funs : SFunTable :=
  fun g => if Nat.eqb g 0 then Some ([0; 1], SPush 0 1, 0) else None.

Example owned_update_needs_sink :
  elab_fun (borrow_modes upd_funs no_procs) 0 ([0; 1], SPush 0 1, 0) = None /\
  fmodes (infer_modes upd_funs no_procs no_summaries) 0 = Some [true; true] /\
  exists t, elab_fun (infer_modes upd_funs no_procs no_summaries) 0 ([0; 1], SPush 0 1, 0) =
              Some ([0; 1], [], t, 0) /\ count_copies t = 0.
Proof. split; [reflexivity| split; [reflexivity| eexists; split; reflexivity]]. Qed.

(* ------------------------------------------------------------------ *)
(* Places: member-path inout, update from self, read-only projection   *)
(* ------------------------------------------------------------------ *)

(* Field reads that allocate a fresh copy of a part. *)
Fixpoint count_field_reads (t : TStmt) : nat :=
  match t with
  | TField _ _ _ => 1
  | TSeq a b | TIf _ a b => count_field_reads a + count_field_reads b
  | TWhile _ b | TFocus _ _ _ b => count_field_reads b
  | _ => 0
  end.

(* state := GuiState(draws = []), with a label built by MakeLabel. *)
Definition gui_state_prefix : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 42) [])
  (SSeq (SCall 1 0 [0])
  (SSeq (SPack 2 [])
        (SPack 3 [2]))).

(* AddDraw(inout state.draws, label): the field moves out and back. *)
Definition gui_state_inout : SStmt :=
  SSeq gui_state_prefix (SSeq (SFocus 4 3 [0] (SCallIO 0 4 [1])) (SEmit 3)).

(* The same update written with the detach/restore ceremony that a missing
   place rule forces today: copy the field out, update it, rebuild. *)
Definition gui_state_detach : SStmt :=
  SSeq gui_state_prefix
  (SSeq (SField 4 3 0)
  (SSeq (SCallIO 0 4 [1])
  (SSeq (SPack 5 [4])
        (SEmit 5)))).

Example gui_state_focus_copies_nothing :
  exists t, elab gui_modes gui_state_inout [] [] = Some (t, []) /\
    count_copies t = 0 /\ count_field_reads t = 0.
Proof. eexists. split; [reflexivity| split; reflexivity]. Qed.

Example gui_state_detach_copies_the_field :
  exists t, elab gui_modes gui_state_detach [] [] = Some (t, []) /\
    count_copies t = 0 /\ count_field_reads t = 1.
Proof. eexists. split; [reflexivity| split; reflexivity]. Qed.

Lemma gui_state_source :
  exists sg', sexec gui_funs gui_procs sempty gui_state_inout sg' [SNode [SNode [SNode [SLeaf 42]]]].
Proof.
  eexists. unfold gui_state_inout, gui_state_prefix.
  eapply SE_Seq.
  - eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |].
    + eapply SE_Seq.
      * eapply SE_Call with (vs := [SLeaf 42]);
          [reflexivity| reflexivity| reflexivity| apply SE_Pack with (vs := [SLeaf 42]); reflexivity| reflexivity].
      * eapply SE_Seq; [apply SE_Pack with (vs := []); reflexivity|
                        apply SE_Pack with (vs := [SNode []]); reflexivity| reflexivity].
      * reflexivity.
    + reflexivity.
  - eapply SE_Seq.
    + eapply SE_Focus;
        [ reflexivity
        | reflexivity
        | eapply SE_CallIO with (vs := [SNode [SLeaf 42]]);
            [reflexivity| reflexivity| reflexivity| reflexivity| eapply SE_Push; reflexivity| reflexivity]
        | reflexivity
        | reflexivity ].
    + eapply SE_Emit. reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Example gui_state_runs_clean :
  exists t n', elab gui_modes gui_state_inout [] [] = Some (t, []) /\
    texec (tfuns_of gui_modes gui_funs) (tprocs_of gui_modes gui_procs) [] [] [] 0 t [] [] [] n'
          [SNode [SNode [SNode [SLeaf 42]]]].
Proof.
  destruct gui_state_source as [sg' Hs].
  destruct gui_state_focus_copies_nothing as [t [Ht _]].
  destruct (closed_program_frees_everything gui_modes gui_funs gui_procs
              (gui_funs_ok gui_modes (ex_intro _ _ eq_refl))
              (gui_procs_ok gui_modes (ex_intro _ _ eq_refl)) gui_state_inout t _ _ _ Ht Hs)
    as [n' Hex].
  exists t, n'. split; assumption.
Qed.

(* state.draws = Append(state.draws, label): an owned update of a part
   through a sink function, with no copy. *)
Definition app_funs : SFunTable :=
  fun g => if Nat.eqb g 7 then Some ([0; 1], SPush 0 1, 0) else gui_funs g.
Definition app_modes : Modes := infer_modes app_funs gui_procs no_summaries.

Definition gui_state_update : SStmt :=
  SSeq gui_state_prefix
  (SSeq (SFocus 4 3 [0] (SSeq (SCall 5 7 [4; 1]) (SCopy 4 5))) (SEmit 3)).

Example gui_state_update_copies_nothing :
  exists t ft, elab app_modes gui_state_update [] [] = Some (t, []) /\
    tfuns_of app_modes app_funs 7 = Some ([0; 1], [], ft, 0) /\
    count_copies t + count_copies ft = 0 /\ count_field_reads t = 0.
Proof. do 2 eexists. split; [reflexivity| split; [reflexivity| split; reflexivity]]. Qed.

(* let draws = state.draws, read only: no copy through a focus, one field
   copy without it. *)
Definition gui_state_view : SStmt :=
  SSeq gui_state_prefix (SSeq (SFocus 4 3 [0] (SEmit 4)) (SEmit 3)).
Definition gui_state_view_copy : SStmt :=
  SSeq gui_state_prefix (SSeq (SField 4 3 0) (SSeq (SEmit 4) (SEmit 3))).

Example projection_view_copies_nothing :
  (exists t, elab gui_modes gui_state_view [] [] = Some (t, []) /\
     count_copies t = 0 /\ count_field_reads t = 0) /\
  (exists t, elab gui_modes gui_state_view_copy [] [] = Some (t, []) /\ count_field_reads t = 1).
Proof. split; eexists; repeat split; reflexivity. Qed.

(* Refused focus shapes: the body reads the suspended root, the temporary
   escapes the focus, the temporary is the root, or the root is borrowed. *)
Example focus_refusals :
  elab gui_modes (SFocus 4 3 [0] (SEmit 3)) [] [] = None /\
  elab gui_modes (SFocus 4 3 [0] (SEmit 4)) [4] [] = None /\
  elab gui_modes (SFocus 3 3 [0] (SEmit 3)) [] [] = None /\
  elab gui_modes (SFocus 4 3 [0] (SEmit 4)) [] [3] = None.
Proof. repeat split; reflexivity. Qed.

(* Faithfulness of the layout: after a pack, the focus segment of child i
   is exactly the footprint that child had before the pack. *)
Lemma firstn_prefix : forall (A : Type) (l1 l2 : list A), firstn (length l1) (l1 ++ l2) = l1.
Proof. intros A l1. induction l1 as [|a r IH]; intros l2; simpl; [reflexivity| rewrite IH; reflexivity]. Qed.

Lemma skipn_prefix : forall (A : Type) (l1 l2 : list A) k, skipn (length l1 + k) (l1 ++ l2) = skipn k l2.
Proof. intros A l1. induction l1 as [|a r IH]; intros l2 k; simpl; [reflexivity| apply IH]. Qed.

Lemma segment_of_children : forall (vs : list TValue) i v,
  nth_error vs i = Some v ->
  firstn (length (snd v))
         (skipn (list_sum (map (fun w => length (snd w)) (firstn i vs))) (flat_map snd vs)) = snd v.
Proof.
  induction vs as [|w r IH]; intros i v E; destruct i as [|j]; simpl in E; try discriminate.
  - inversion E; subst. simpl. apply firstn_prefix.
  - simpl. rewrite skipn_prefix. apply IH. exact E.
Qed.

Lemma sizes_as_lengths : forall (vs : list TValue),
  Forall (fun w => length (snd w) = vsize (fst w)) vs ->
  map vsize (map fst vs) = map (fun w => length (snd w)) vs.
Proof. intros vs HF. induction HF as [|w r Hw HF IH]; simpl; [reflexivity| rewrite Hw, IH; reflexivity]. Qed.

Theorem pack_child_segment : forall (vs : list TValue) (i : nat) (v : TValue) (n off : nat),
  Forall (fun w => length (snd w) = vsize (fst w)) vs -> nth_error vs i = Some v ->
  voff (SNode (map fst vs)) [i] = Some off ->
  firstn (vsize (fst v)) (skipn off ((n, 0) :: flat_map snd vs)) = snd v.
Proof.
  intros vs i v n off HF Ei Eo.
  assert (Em : nth_error (map fst vs) i = Some (fst v)) by (apply map_nth_error; exact Ei).
  simpl in Eo. rewrite Em in Eo. simpl in Eo. inversion Eo; subst off. clear Eo.
  assert (Hv : length (snd v) = vsize (fst v)).
  { rewrite Forall_forall in HF. apply HF. eapply nth_error_In. exact Ei. }
  rewrite Nat.add_0_r, <- Hv. simpl.
  rewrite <- firstn_map, (sizes_as_lengths vs HF), firstn_map.
  apply segment_of_children. exact Ei.
Qed.

(* ------------------------------------------------------------------ *)
(* Overlapping projections: unpack, use the parts together, repack     *)
(* ------------------------------------------------------------------ *)

(* h := Rec(items = [], name = "..."). *)
Definition rec_prefix : SStmt :=
  SSeq (SDef 0 (fun _ => SLeaf 42) [])
  (SSeq (SPack 1 [])
        (SPack 2 [1; 0])).

(* let items = h.items while h.name is also read, then h is used whole. *)
Definition rec_unpack_use : SStmt :=
  SSeq rec_prefix
  (SSeq (SUnpack 2 [3; 4])
  (SSeq (SEmit 4)
  (SSeq (SEmit 3)
  (SSeq (SPack 2 [3; 4])
        (SEmit 2))))).

(* The same reads through field copies. *)
Definition rec_field_use : SStmt :=
  SSeq rec_prefix
  (SSeq (SField 3 2 0)
  (SSeq (SField 4 2 1)
  (SSeq (SEmit 4)
  (SSeq (SEmit 3)
        (SEmit 2))))).

Example overlapping_projection_copies_nothing :
  (exists t, elab no_summaries rec_unpack_use [] [] = Some (t, []) /\
     count_copies t = 0 /\ count_field_reads t = 0) /\
  (exists t, elab no_summaries rec_field_use [] [] = Some (t, []) /\ count_field_reads t = 2).
Proof. split; eexists; repeat split; reflexivity. Qed.

Lemma rec_unpack_source :
  exists sg', sexec no_funs no_procs sempty rec_unpack_use sg'
    [SLeaf 42; SNode []; SNode [SNode []; SLeaf 42]].
Proof.
  eexists. unfold rec_unpack_use, rec_prefix.
  eapply SE_Seq.
  - eapply SE_Seq; [apply SE_Def with (vs := []); reflexivity| |].
    + eapply SE_Seq; [apply SE_Pack with (vs := []); reflexivity|
                      apply SE_Pack with (vs := [SNode []; SLeaf 42]); reflexivity| reflexivity].
    + reflexivity.
  - eapply SE_Seq; [apply SE_Unpack with (cs := [SNode []; SLeaf 42]); reflexivity| |].
    + eapply SE_Seq; [eapply SE_Emit; reflexivity| |].
      * eapply SE_Seq; [eapply SE_Emit; reflexivity| |].
        -- eapply SE_Seq; [apply SE_Pack with (vs := [SNode []; SLeaf 42]); reflexivity|
                           eapply SE_Emit; reflexivity| reflexivity].
        -- reflexivity.
      * reflexivity.
    + reflexivity.
  - reflexivity.
Qed.

Example rec_unpack_runs_clean :
  exists t n', elab no_summaries rec_unpack_use [] [] = Some (t, []) /\
    texec (tfuns_of no_summaries no_funs) (tprocs_of no_summaries no_procs) [] [] [] 0 t [] [] [] n'
          [SLeaf 42; SNode []; SNode [SNode []; SLeaf 42]].
Proof.
  destruct rec_unpack_source as [sg' Hs].
  destruct overlapping_projection_copies_nothing as [[t [Ht _]] _].
  destruct (closed_program_frees_everything no_summaries no_funs no_procs
              (no_funs_ok no_summaries) (no_procs_ok no_summaries) rec_unpack_use t _ _ _ Ht Hs)
    as [n' Hex].
  exists t, n'. split; assumption.
Qed.

(* Taking a part out of a record that is dead afterwards moves it; the other
   part is dropped. A field read would copy it. *)
Definition rec_take : SStmt := SSeq rec_prefix (SSeq (SUnpack 2 [3; 4]) (SEmit 3)).
Definition rec_take_field : SStmt := SSeq rec_prefix (SSeq (SField 3 2 0) (SEmit 3)).

Example dead_aggregate_part_moves :
  (exists t, elab no_summaries rec_take [] [] = Some (t, []) /\
     count_copies t = 0 /\ count_field_reads t = 0) /\
  (exists t, elab no_summaries rec_take_field [] [] = Some (t, []) /\ count_field_reads t = 1).
Proof. split; eexists; repeat split; reflexivity. Qed.

(* Refused unpack shapes: the record is still needed afterwards, it is
   borrowed, or a part reuses its name. *)
Example unpack_refusals :
  elab no_summaries (SUnpack 2 [3; 4]) [2] [] = None /\
  elab no_summaries (SUnpack 2 [3; 4]) [] [2] = None /\
  elab no_summaries (SUnpack 2 [2; 4]) [] [] = None /\
  elab no_summaries (SUnpack 2 [3; 3]) [] [] = None.
Proof. repeat split; reflexivity. Qed.

(* Faithfulness: unpacking a packed value gives back each part's original
   footprint. *)
Theorem unpack_after_pack : forall vs : list TValue,
  Forall (fun w => length (snd w) = vsize (fst w)) vs ->
  combine (map fst vs) (chunks (map vsize (map fst vs)) (flat_map snd vs)) = vs.
Proof.
  intros vs HF. induction HF as [|[c bs] r Hv HF IH]; [reflexivity|].
  simpl in *. rewrite <- Hv, firstn_prefix.
  rewrite <- (Nat.add_0_r (length bs)), skipn_prefix. simpl. rewrite IH. reflexivity.
Qed.

(* ------------------------------------------------------------------ *)
(* Regions: values live to the end of their region and go together     *)
(* ------------------------------------------------------------------ *)

Fixpoint drops_var (z : TVar) (t : TStmt) : bool :=
  match t with
  | TDrop z' => if tvar_eq_dec z z' then true else false
  | TSeq a b | TIf _ a b => drops_var z a || drops_var z b
  | TWhile _ b | TFocus _ _ _ b => drops_var z b
  | _ => false
  end.

Lemma drops_no_var : forall vs B x, (forall v, In v vs -> v <> x) ->
  drops_var (Src x) (drops vs B) = false.
Proof.
  induction vs as [|v r IH]; intros B x Hv; [reflexivity|].
  cbn [drops drops_var]. rewrite (IH B x (fun w Hw => Hv w (or_intror Hw))).
  destruct (in_dec Nat.eq_dec v B); cbn [drops_var]; [reflexivity|].
  destruct (tvar_eq_dec (Src x) (Src v)) as [E|E]; [|reflexivity].
  exfalso. inversion E. apply (Hv v); [left; reflexivity| congruence].
Qed.

Lemma drops_has : forall vs B x, In x vs -> ~ In x B -> drops_var (Src x) (drops vs B) = true.
Proof.
  induction vs as [|v r IH]; intros B x Hin HxB; [destruct Hin|].
  cbn [drops drops_var]. destruct Hin as [E|Hin].
  - subst v. destruct (in_dec Nat.eq_dec x B) as [F|_]; [contradiction|].
    cbn [drops_var]. destruct (tvar_eq_dec (Src x) (Src x)) as [_|F]; [reflexivity| contradiction].
  - rewrite (IH B x Hin HxB). apply orb_true_r.
Qed.

Lemma settle_no_drop : forall A L B x, In x L -> drops_var (Src x) (settle A L B) = false.
Proof.
  intros A L B x Hx. unfold settle. apply drops_no_var.
  intros v Hv E. subst v. rewrite nodup_In, vdiff_In in Hv. apply Hv. exact Hx.
Qed.

Lemma copies_no_drop : forall ws x, drops_var (Src x) (copies ws) = false.
Proof. induction ws as [|w r IH]; intros x; cbn [copies drops_var orb]; [reflexivity| exact (IH x)]. Qed.

Ltac nodrop :=
  cbn [drops_var orb];
  repeat first [ rewrite copies_no_drop | rewrite settle_no_drop by assumption ];
  reflexivity.

(* A variable that is live after a statement and not written by it is
   never dropped inside it, and it is live before it. *)
Lemma no_drop_live : forall M s L B tt Lin v,
  elab M s L B = Some (tt, Lin) -> In v L -> writes v s = false ->
  drops_var (Src v) tt = false /\ In v Lin.
Proof.
  intros M s.
  induction s as [| x f ys | x y | x ys | x y | x y i | y | s1 IHs1 s2 IHs2 | c s1 IHs1 s2 IHs2
                  | c head body IHbody | x g ys | g z ys | t y p s IHs | y xs | x f ys s IHs ];
    intros L B tt Lin v Hel Hx Hw; simpl in Hel, Hw.
  - inversion Hel; subst tt Lin. split; [reflexivity| exact Hx].
  - destruct (in_dec Nat.eq_dec x ys); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    inversion Hel; subst tt Lin. apply Nat.eqb_neq in Hw. split; [nodrop|].
    apply in_or_app. right. apply vremove_In. split; [exact Hx| congruence].
  - destruct (Nat.eq_dec x y); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    apply Nat.eqb_neq in Hw.
    destruct (keep L B y); inversion Hel; subst tt Lin;
      (split; [nodrop| right; apply vremove_In; split; [exact Hx| congruence]]).
  - destruct (in_dec Nat.eq_dec x ys); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    destruct (nodupb ys); [|discriminate]. inversion Hel; subst tt Lin.
    apply Nat.eqb_neq in Hw. split; [nodrop|].
    apply in_or_app. right. apply vremove_In. split; [exact Hx| congruence].
  - destruct (Nat.eq_dec x y); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    apply Nat.eqb_neq in Hw.
    destruct (keep L B y); inversion Hel; subst tt Lin;
      (split; [nodrop| right; right; exact Hx]).
  - destruct (Nat.eq_dec x y); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    inversion Hel; subst tt Lin. apply Nat.eqb_neq in Hw. split; [nodrop|].
    right. apply vremove_In. split; [exact Hx| congruence].
  - inversion Hel; subst tt Lin. split; [nodrop| right; exact Hx].
  - apply orb_false_iff in Hw. destruct Hw as [Hw1 Hw2].
    destruct (elab M s2 L B) as [[t2 L2]|] eqn:E2; [|discriminate].
    destruct (elab M s1 L2 B) as [[t1 L1]|] eqn:E1; [|discriminate].
    inversion Hel; subst tt Lin.
    destruct (IHs2 L B t2 L2 v E2 Hx Hw2) as [D2 I2].
    destruct (IHs1 L2 B t1 L1 v E1 I2 Hw1) as [D1 I1].
    split; [cbn [drops_var]; rewrite D1, D2; reflexivity| exact I1].
  - apply orb_false_iff in Hw. destruct Hw as [Hw1 Hw2].
    destruct (elab M s1 L B) as [[t1 L1]|] eqn:E1; [|discriminate].
    destruct (elab M s2 L B) as [[t2 L2]|] eqn:E2; [|discriminate].
    inversion Hel; subst tt Lin.
    destruct (IHs1 L B t1 L1 v E1 Hx Hw1) as [D1 I1].
    destruct (IHs2 L B t2 L2 v E2 Hx Hw2) as [D2 I2].
    split; [cbn [drops_var orb]; rewrite D1, D2, settle_no_drop, settle_no_drop by assumption; reflexivity|].
    right. apply in_or_app. left. exact I1.
  - destruct (elab M body head B) as [[tb Lb]|] eqn:Eb; [|discriminate].
    destruct (inclb Lb head && inclb L head && vmem c head) eqn:Hck; [|discriminate].
    apply andb_true_iff in Hck. destruct Hck as [Hck _]. apply andb_true_iff in Hck. destruct Hck as [_ HL].
    inversion Hel; subst tt Lin.
    assert (Hh : In v head) by (apply (inclb_spec L head HL); exact Hx).
    destruct (IHbody head B tb Lb v Eb Hh Hw) as [Db Ib].
    split; [cbn [drops_var orb]; rewrite Db, settle_no_drop, settle_no_drop by assumption; reflexivity| exact Hh].
  - destruct (in_dec Nat.eq_dec x ys); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    apply Nat.eqb_neq in Hw.
    change (elab_call M KFun x g ys L B = Some (tt, Lin)) in Hel.
    destruct (elab_call_spec M KFun x g ys L B tt Lin Hel) as [ms [_ [_ [_ [Et ELin]]]]]. subst.
    split; [nodrop|]. apply in_or_app. right. apply vremove_In. split; [exact Hx| congruence].
  - destruct (in_dec Nat.eq_dec z ys); [discriminate|]. destruct (in_dec Nat.eq_dec z B); [discriminate|].
    apply Nat.eqb_neq in Hw.
    change (elab_call M KProc z g (z :: ys) L B = Some (tt, Lin)) in Hel.
    destruct (elab_call_spec M KProc z g (z :: ys) L B tt Lin Hel) as [ms [_ [_ [_ [Et ELin]]]]]. subst.
    split; [nodrop|]. simpl. right. apply in_or_app. right. apply vremove_In. split; [exact Hx| congruence].
  - apply orb_false_iff in Hw. destruct Hw as [Hw Hws]. apply orb_false_iff in Hw. destruct Hw as [Hwt Hwy].
    apply Nat.eqb_neq in Hwt. apply Nat.eqb_neq in Hwy.
    destruct (Nat.eq_dec t y); [discriminate|]. destruct (in_dec Nat.eq_dec t B); [discriminate|].
    destruct (in_dec Nat.eq_dec y B); [discriminate|]. destruct (in_dec Nat.eq_dec t L); [discriminate|].
    destruct (elab M s (t :: vremove y L) B) as [[ts Ls]|] eqn:Es; [|discriminate].
    destruct (vmem y Ls); [discriminate|]. inversion Hel; subst tt Lin.
    assert (Hin : In v (t :: vremove y L)) by (right; apply vremove_In; split; [exact Hx| congruence]).
    destruct (IHs (t :: vremove y L) B ts Ls v Es Hin Hws) as [Ds Is].
    split; [cbn [drops_var orb]; rewrite Ds, settle_no_drop, settle_no_drop by assumption; reflexivity|].
    right. apply vremove_In. split; [exact Is| congruence].
  - apply orb_false_iff in Hw. destruct Hw as [Hwy Hwx]. apply Nat.eqb_neq in Hwy.
    destruct (in_dec Nat.eq_dec y L); [discriminate|]. destruct (in_dec Nat.eq_dec y B); [discriminate|].
    destruct (in_dec Nat.eq_dec y xs); [discriminate|].
    destruct (nodupb xs && forallb (fun x => negb (vmem x B)) xs); [|discriminate].
    inversion Hel; subst tt Lin. split; [nodrop|].
    right. apply vdiff_In. split; [exact Hx| apply vmem_false; exact Hwx].
  - apply orb_false_iff in Hw. destruct Hw as [Hwx Hws]. apply Nat.eqb_neq in Hwx.
    destruct (in_dec Nat.eq_dec x L); [discriminate|]. destruct (in_dec Nat.eq_dec x B); [discriminate|].
    destruct (in_dec Nat.eq_dec x ys); [discriminate|]. destruct (writes x s); [discriminate|].
    destruct (elab M s (x :: L) B) as [[ts Ls]|] eqn:Es; [|discriminate].
    inversion Hel; subst tt Lin.
    destruct (IHs (x :: L) B ts Ls v Es (or_intror Hx) Hws) as [Ds Is].
    split; [cbn [drops_var orb]; rewrite Ds, settle_no_drop, settle_no_drop by assumption; reflexivity|].
    apply in_or_app. right. apply vremove_In. split; [exact Is| congruence].
Qed.

(* A region value is created at the region's start, is never dropped inside
   the region, and is released by the region's exit. Every region value of
   nested regions is therefore released in one release set at their common
   exit, which the runtime may implement as one arena reset. *)
Theorem region_released_at_exit : forall M x f ys s L B t Lin,
  elab M (SRegion x f ys s) L B = Some (t, Lin) ->
  exists ts Ls,
    t = TSeq (TSeq (TDef (Src x) f (map Src ys)) (settle (x :: ys ++ vremove x Ls) Ls B))
             (TSeq ts (settle (x :: L) L B)) /\
    drops_var (Src x) (settle (x :: ys ++ vremove x Ls) Ls B) = false /\
    drops_var (Src x) ts = false /\
    drops_var (Src x) (settle (x :: L) L B) = true.
Proof.
  intros M x f ys s L B t Lin E. simpl in E.
  destruct (in_dec Nat.eq_dec x L) as [HxL|HxL]; [discriminate|].
  destruct (in_dec Nat.eq_dec x B) as [HxB|HxB]; [discriminate|].
  destruct (in_dec Nat.eq_dec x ys); [discriminate|].
  destruct (writes x s) eqn:Hw; [discriminate|].
  destruct (elab M s (x :: L) B) as [[ts Ls]|] eqn:Es; [|discriminate].
  inversion E; subst.
  destruct (no_drop_live M s (x :: L) B ts Ls x Es (or_introl eq_refl) Hw) as [Hts HLs].
  exists ts, Ls. split; [reflexivity|]. split; [apply settle_no_drop; exact HLs|].
  split; [exact Hts|]. unfold settle. apply drops_has; [|exact HxB].
  rewrite nodup_In, vdiff_In. split; [left; reflexivity| exact HxL].
Qed.

(* A temporary text observed inside its region is created once, copied
   never, and released at the region's end. *)
Definition region_obs : SStmt := SRegion 0 (fun _ => SLeaf 5) [] (SSeq (SEmit 0) (SEmit 0)).

Example region_value_released_once :
  exists t, elab no_summaries region_obs [] [] = Some (t, []) /\ count_copies t = 0.
Proof. eexists. split; reflexivity. Qed.

(* A value that escapes its region is copied out at the escape. *)
Definition region_escape : SStmt :=
  SSeq (SRegion 0 (fun _ => SLeaf 5) [] (SPack 1 [0])) (SEmit 1).

Example region_escape_copies :
  exists t, elab no_summaries region_escape [] [] = Some (t, []) /\ count_copies t = 1.
Proof. eexists. split; reflexivity. Qed.

(* Refused regions: the region value is still needed after the region, it
   is redefined inside the region, or it is borrowed. *)
Example region_refusals :
  elab no_summaries (SRegion 0 (fun _ => SLeaf 5) [] (SEmit 0)) [0] [] = None /\
  elab no_summaries (SRegion 0 (fun _ => SLeaf 5) [] (SDef 0 (fun _ => SLeaf 6) [])) [] [] = None /\
  elab no_summaries (SRegion 0 (fun _ => SLeaf 5) [] (SEmit 0)) [] [0] = None.
Proof. repeat split; reflexivity. Qed.

(* ------------------------------------------------------------------ *)
(* Fail-closed summaries and the mode fixpoint                          *)
(* ------------------------------------------------------------------ *)

(* A call through an unresolved summary is refused, even though the same
   program elaborates once the callee is resolved as all-borrowed. A table
   for other routines resolves nothing here either. *)
Example missing_summary_is_refused :
  elab no_summaries gui_calls [] [] = None /\
  elab (borrow_modes no_funs no_procs) gui_calls [] [] = None /\
  exists t, elab gui_borrow gui_calls [] [] = Some (t, []).
Proof. split; [reflexivity| split; [reflexivity| eexists; reflexivity]]. Qed.

(* A summary with fewer entries than the call has arguments is refused at
   the call and at the routine; a missing entry is never read as a borrow. *)
Definition short_modes : Modes := {| fmodes := fun _ => Some []; pmodes := fun _ => Some [] |}.

Example arity_mismatch_is_refused :
  elab short_modes gui_calls [] [] = None /\
  elab_fun short_modes 0 ([0], SPack 1 [0], 1) = None /\
  elab_proc short_modes 0 (5, [6], SPush 5 6) = None.
Proof. split; [reflexivity| split; reflexivity]. Qed.

(* Summary order: an unresolved summary is below every summary, and a
   resolved one is below another of the same arity that is sink in at least
   the same positions. *)
Definition flag_le (a b : bool) : Prop := a = true -> b = true.

Definition summary_le (o1 o2 : option (list bool)) : Prop :=
  match o1, o2 with
  | None, _ => True
  | Some a, Some b => Forall2 flag_le a b
  | Some _, None => False
  end.

Definition modes_le (M1 M2 : Modes) : Prop :=
  forall g, summary_le (fmodes M1 g) (fmodes M2 g) /\ summary_le (pmodes M1 g) (pmodes M2 g).

Lemma no_summaries_le : forall M, modes_le no_summaries M.
Proof. intros M g. split; exact I. Qed.

Lemma sinks_le_in : forall ms1 ms2 (ys : list Var) p,
  Forall2 flag_le ms1 ms2 -> In p (sinks ms1 ys) -> In p (sinks ms2 ys).
Proof.
  intros ms1 ms2 ys p H. revert ys.
  induction H as [|a b r1 r2 Hab Hr IH]; intros ys Hv; [exact Hv|].
  destruct ys as [|y yr]; [exact Hv|]. cbn [sinks hd tl] in Hv |- *.
  destruct a.
  - rewrite (Hab eq_refl). destruct Hv as [Hv|Hv]; [left; exact Hv| right; exact (IH yr Hv)].
  - destruct b; [right|]; exact (IH yr Hv).
Qed.

Lemma summary_sinks_le : forall o1 o2 (ys : list Var) p,
  summary_le o1 o2 ->
  match o1 with Some ms => vmem p (sinks ms ys) | None => false end = true ->
  match o2 with Some ms => vmem p (sinks ms ys) | None => false end = true.
Proof.
  intros [ms1|] [ms2|] ys p Hle Hv; simpl in Hle; try discriminate; try contradiction.
  apply vmem_true. apply vmem_true in Hv. exact (sinks_le_in ms1 ms2 ys p Hle Hv).
Qed.

Theorem owns_monotone : forall M1 M2 p s,
  modes_le M1 M2 -> owns M1 p s = true -> owns M2 p s = true.
Proof.
  intros M1 M2 p s Hle.
  induction s as [| | | | | | | s1 IH1 s2 IH2 | c s1 IH1 s2 IH2 | c h b IHb
                  | x g ys | g z ys | t y q b IHb | y xs | r f ys b IHb ];
    intros Ho; simpl in Ho |- *; try exact Ho.
  - apply orb_true_iff in Ho. apply orb_true_iff.
    destruct Ho as [Ho|Ho]; [left; exact (IH1 Ho)| right; exact (IH2 Ho)].
  - apply orb_true_iff in Ho. apply orb_true_iff.
    destruct Ho as [Ho|Ho]; [left; exact (IH1 Ho)| right; exact (IH2 Ho)].
  - exact (IHb Ho).
  - apply orb_true_iff in Ho. apply orb_true_iff.
    destruct Ho as [Ho|Ho]; [left; exact Ho| right].
    exact (summary_sinks_le _ _ ys p (proj1 (Hle g)) Ho).
  - apply orb_true_iff in Ho. apply orb_true_iff.
    destruct Ho as [Ho|Ho]; [left; exact Ho| right].
    exact (summary_sinks_le _ _ ys p (proj2 (Hle g)) Ho).
  - apply orb_true_iff in Ho. apply orb_true_iff.
    destruct Ho as [Ho|Ho]; [left; exact Ho| right; exact (IHb Ho)].
  - apply orb_true_iff in Ho. apply orb_true_iff.
    destruct Ho as [Ho|Ho]; [left; exact Ho| right; exact (IHb Ho)].
Qed.

Theorem infer_monotone : forall funs procs M1 M2,
  modes_le M1 M2 -> modes_le (infer_modes funs procs M1) (infer_modes funs procs M2).
Proof.
  unfold modes_le. intros funs procs M1 M2 Hle g. simpl. split.
  - destruct (funs g) as [[[ps body] ret]|]; simpl; [|exact I].
    induction ps as [|q r IH]; simpl; [constructor| constructor; [|exact IH]].
    intros Hq. apply orb_true_iff in Hq. apply orb_true_iff.
    destruct Hq as [Hq|Hq]; [left; exact (owns_monotone M1 M2 q body Hle Hq)| right; exact Hq].
  - destruct (procs g) as [[[io ps] body]|]; simpl; [|exact I].
    induction ps as [|q r IH]; simpl; [constructor| constructor; [|exact IH]].
    intros Hq. exact (owns_monotone M1 M2 q body Hle Hq).
Qed.

(* Rounds of inference from no summaries. *)
Fixpoint iter_modes (funs : SFunTable) (procs : SProcTable) (n : nat) : Modes :=
  match n with
  | 0 => no_summaries
  | S k => infer_modes funs procs (iter_modes funs procs k)
  end.

(* The rounds ascend: no round withdraws a summary or turns a sink
   parameter back into a borrowed one. *)
Theorem infer_ascends : forall funs procs n,
  modes_le (iter_modes funs procs n) (iter_modes funs procs (S n)).
Proof.
  intros funs procs n. induction n as [|k IH]; [apply no_summaries_le|].
  exact (infer_monotone funs procs _ _ IH).
Qed.

(* From the first round on, every defined routine is resolved with exactly
   one entry per parameter, and an undefined routine stays unresolved. So
   later rounds only flip borrowed entries to sink, at most once each. *)
Theorem infer_resolves_defined : forall funs procs M g,
  (forall ps body ret, funs g = Some (ps, body, ret) ->
     exists ms, fmodes (infer_modes funs procs M) g = Some ms /\ length ms = length ps) /\
  (forall io ps body, procs g = Some (io, ps, body) ->
     exists ms, pmodes (infer_modes funs procs M) g = Some ms /\ length ms = length ps) /\
  (funs g = None -> fmodes (infer_modes funs procs M) g = None) /\
  (procs g = None -> pmodes (infer_modes funs procs M) g = None).
Proof.
  intros funs procs M g. simpl.
  split; [intros ps body ret E; rewrite E; eexists; split; [reflexivity| apply length_map]|].
  split; [intros io ps body E; rewrite E; eexists; split; [reflexivity| apply length_map]|].
  split; intros E; rewrite E; reflexivity.
Qed.

(* A chain f0(a) = f1(a), f1(b) = Label(b). The first round cannot see
   that f0's parameter reaches a sink: f0 lends it and pays one copy. The
   second round sees through f1, f0's parameter becomes sink, and the copy
   disappears; the third round changes nothing. A table that has not
   converged costs copies, never safety. *)
Definition chain_funs : SFunTable :=
  fun g => if Nat.eqb g 0 then Some ([0], SCall 1 1 [0], 1)
           else if Nat.eqb g 1 then Some ([0], SPack 1 [0], 1) else None.

Example chain_needs_two_rounds :
  fmodes (iter_modes chain_funs no_procs 1) 0 = Some [false] /\
  fmodes (iter_modes chain_funs no_procs 1) 1 = Some [true] /\
  fmodes (iter_modes chain_funs no_procs 2) 0 = Some [true] /\
  fmodes (iter_modes chain_funs no_procs 3) 0 = fmodes (iter_modes chain_funs no_procs 2) 0 /\
  fmodes (iter_modes chain_funs no_procs 3) 1 = fmodes (iter_modes chain_funs no_procs 2) 1 /\
  (exists t1, elab_fun (iter_modes chain_funs no_procs 1) 0 ([0], SCall 1 1 [0], 1) =
                Some ([], [0], t1, 1) /\ count_copies t1 = 1) /\
  (exists t2, elab_fun (iter_modes chain_funs no_procs 2) 0 ([0], SCall 1 1 [0], 1) =
                Some ([0], [], t2, 1) /\ count_copies t2 = 0).
Proof.
  split; [reflexivity|]. split; [reflexivity|]. split; [reflexivity|].
  split; [reflexivity|]. split; [reflexivity|].
  split; eexists; split; reflexivity.
Qed.
