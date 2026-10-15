(* Independently assembled model inputs for the freshly extracted checker.
   No parser for compiler artifacts exists yet; --selftest is the sole CLI. *)
open Ownership_refusal
open RefusalExplanation

let rec n x = if x < 0 then invalid_arg "negative natural" else if x = 0 then O else S (n (x - 1))
let block x = (n x, O)
let seed0 = { seed_kind = UnconsumedAlias; seed_at = n 0; seed_guard = Some (n 10) }
let e1 = { edge_kind = Assignment; edge_from = n 0; edge_to = n 1; edge_guard = None }
let e2 = { edge_kind = ArgumentToResult; edge_from = n 1; edge_to = n 2; edge_guard = Some (n 11) }
let e3 = { edge_kind = FieldAccess; edge_from = n 2; edge_to = n 3; edge_guard = None }
let loop = { edge_kind = Assignment; edge_from = n 3; edge_to = n 3; edge_guard = None }
let tree = BecauseTransfer (e3, BecauseTransfer (e2, BecauseTransfer (e1, BecauseSeed seed0)))
let trace = [Allocate (n 3, block 0); CopyDescriptor (n 4, n 3);
             Mutate (n 7, n 4, block 1); ReadDescriptor (n 3)]
let at = { boundary_site = n 40; boundary_target = n 3;
           boundary_effect = Some (n 12); boundary_trace = trace }
let p = { seeds = [seed0]; edges = [e1; e2; e3; loop];
          boundaries = [at; { boundary_site = n 41; boundary_target = O;
                             boundary_effect = None; boundary_trace = [] }];
          relocates = (fun op -> op = n 7) }
let summary k = List.mem k [n 10; n 11; n 12]
let hazard = { witness_boundary = O; witness_site = n 40;
               witness_tree = tree; witness_cause = Hazard }
let missing = { hazard with witness_cause = ExclusivityAt (n 3) }
let with_trace ops = { p with boundaries = [{ at with boundary_trace = ops }] }
let no_move = { p with relocates = (fun _ -> false) }
let checks = ref 0
let check label condition =
  if not condition then (Printf.eprintf "FAIL: %s\n%!" label; exit 1);
  incr checks;
  Printf.printf "ok: %s\n%!" label

let selftest () =
  check "actual copied-descriptor stale read"
    (run p.relocates empty_heap trace = StaleRead (n 3, block 0));
  check "hazard certificate" (check_witness p summary O hazard);
  check "named missing exclusivity" (check_witness no_move summary O missing);
  check "no relocation cannot certify hazard" (not (check_witness no_move summary O hazard));
  check "hazard cannot be relabeled missing" (not (check_witness p summary O missing));
  check "read before growth" (not (check_witness
    (with_trace [Allocate (n 3, block 0); CopyDescriptor (n 4, n 3);
      ReadDescriptor (n 3); Mutate (n 7, n 4, block 1)]) summary O hazard));
  check "other register's stale read" (not (check_witness
    (with_trace [Allocate (O, block 0); CopyDescriptor (n 4, O);
      Mutate (n 7, n 4, block 1); ReadDescriptor O]) summary O hazard));
  check "reissued allocation is invalid, not a hazard certificate" (not (check_witness
    (with_trace [Allocate (n 3, block 0); CopyDescriptor (n 4, n 3);
      Mutate (n 7, n 4, block 0); ReadDescriptor (n 3)]) summary O hazard));
  check "unallocated read" (not (check_witness (with_trace [ReadDescriptor (n 3)]) summary O hazard));
  check "unrelated existing boundary" (not (check_witness p summary (n 1) hazard));
  check "unknown boundary" (not (check_witness p summary (n 9) hazard));
  check "forged claimed boundary" (not (check_witness p summary O { hazard with witness_boundary = n 1 }));
  check "forged location" (not (check_witness p summary O { hazard with witness_site = n 41 }));
  check "wrong parent" (not (check_witness p summary O
    { hazard with witness_tree = BecauseTransfer (e3, BecauseSeed seed0) }));
  check "wrong target" (not (check_witness p summary O
    { hazard with witness_tree = BecauseSeed seed0 }));
  check "unrelated summary, even when true" (not (check_witness p (fun _ -> true) O
    { hazard with witness_tree = BecauseTransfer (e3,
      BecauseTransfer ({ e2 with edge_guard = Some (n 99) }, BecauseTransfer (e1, BecauseSeed seed0))) }));
  check "forged seed kind" (not (check_tree p summary
    (BecauseSeed { seed0 with seed_kind = ConstructorOverlap })));
  check "wrong missing fact" (not (check_witness no_move summary O
    { missing with witness_cause = ExclusivityAt (n 99) }));
  check "exact cited summary support" (witness_support p O hazard = [n 12; n 11; n 10]);
  check "unrelated summary change" (check_witness p (fun k -> k = n 99 || summary k) O hazard);
  List.iter (fun key ->
    let reduced k = k <> n key && summary k in
    check (Printf.sprintf "removed required summary %d" key)
      (not (check_witness p reduced O hazard) && not (refusal p reduced O))) [10;11;12];
  let cycle = { p with seeds = []; edges =
    [{ e1 with edge_from = O; edge_to = n 1 };
     { e1 with edge_from = n 1; edge_to = O }] } in
  check "seedless cycle stays exclusive" (rounds cycle summary (n 2) = [] && not (refusal cycle summary O));
  check "first synchronous round is shortest derivation height"
    (not (List.mem (n 3) (rounds p summary (n 3))) &&
     List.mem (n 3) (rounds p summary (n 4)) &&
     check_tree p summary (BecauseTransfer (loop, tree)));
  check "graph changes are outside the summary-locality contract"
    (not (refusal cycle summary O) && refusal p summary O);
  List.iter (fun kind ->
    let r = { seed_kind = kind; seed_at = O; seed_guard = None } in
    let q = { p with seeds = [r]; edges = [] } in
    check "seed category certificate" (check_tree q (fun _ -> false) (BecauseSeed r)))
    [ConstructorOverlap; UnconsumedAlias; RefOrigin; IndexOrigin; SharedResultField; UnavailableAnalysisFact];
  let masks = List.init 8 Fun.id in
  let from_mask mask key =
    List.exists (fun (i,k) -> key = n k && mask land (1 lsl i) <> 0) [(0,10);(1,11);(2,12)] in
  List.iter (fun mask ->
    let s = from_mask mask in
    let w1 = { witness_boundary = n 1; witness_site = n 41;
               witness_tree = BecauseSeed seed0; witness_cause = ExclusivityAt O } in
    check (Printf.sprintf "all boundary refusals have certificates: summary mask %d" mask)
      (refusal p s O = check_witness p s O hazard &&
       refusal p s (n 1) = check_witness p s (n 1) w1)) masks;
  let comparable = ref 0 in
  let monotone = List.for_all (fun precise -> List.for_all (fun coarse ->
    if precise land coarse <> precise then true else begin
      incr comparable;
      List.for_all (fun requested -> not (refusal p (from_mask precise) requested) ||
        refusal p (from_mask coarse) requested) [O; n 1]
    end) masks) masks in
  check "all comparable adverse-summary pairs" (monotone && !comparable = 27);
  Printf.printf "PASS: %d controls; 8 summary assignments x 2 boundaries; 27 ordered summary pairs x 2 boundaries.\n%!" !checks

let () = match Array.to_list Sys.argv with
  | [_; "--selftest"] -> selftest ()
  | _ -> prerr_endline "usage: ownership_refusal_driver --selftest"; exit 2
