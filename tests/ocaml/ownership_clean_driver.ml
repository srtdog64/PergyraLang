(* Observers and fixed inputs only. All ownership decisions come from the
   freshly extracted OwnershipCleanCore.elab, not a second heap machine. *)
open Ownership_clean

let fail message = prerr_endline message; exit 1

let rec seq = function
  | [] -> SSkip
  | [s] -> s
  | s :: rest -> SSeq (s, seq rest)

let leaf x = SDef (x, (fun _ -> SLeaf 0), [])
let vars n = List.init n Fun.id

let rec source_nodes = function
  | SSeq (a, b) | SIf (_, a, b) -> 1 + source_nodes a + source_nodes b
  | SWhile (_, _, b) -> 1 + source_nodes b
  | _ -> 1

let rec target_counts = function
  | TSeq (a, b) | TIf (_, a, b) ->
      let na, da, ma = target_counts a in
      let nb, db, mb = target_counts b in (na + nb, da + db, ma + mb)
  | TWhile (_, b) -> target_counts b
  | TDrop _ -> (1, 1, 0)
  | TMove _ -> (1, 0, 1)
  | TSkip -> (0, 0, 0)
  | _ -> (1, 0, 0)

(* No caller-selected default summary: a call needs an explicit table. *)
let admitted ?(modes = no_summaries) name s live =
  match elab modes s live [] with
  | Some result -> result
  | None -> fail ("unexpected elaboration refusal: " ^ name)

let check ?(modes = no_summaries) name s live expected_live expected_copies =
  let t, lin = admitted ~modes name s live in
  if lin <> expected_live || count_copies t <> expected_copies then
    fail ("wrong live-in or copy decision: " ^ name)

(* Resolved all-borrowed summaries of a fixed arity, for every routine. *)
let all_borrow n =
  let ms = Some (List.init n (fun _ -> false)) in { fmodes = (fun _ -> ms); pmodes = (fun _ -> ms) }
let one_borrow = all_borrow 1
let fun_borrow ((ps, _), _) = all_borrow (List.length ps)
let proc_borrow ((_, ps), _) = all_borrow (List.length ps)

let selftest () =
  check "gui-move-only" gui_program [] [] 0;
  check "gui-live-label" gui_program_reuse [] [] 1;
  check "dead-source-move" (SCopy (1, 0)) [] [0] 0;
  check "live-source-copy" (SCopy (1, 0)) [0] [0; 0] 1;
  check "dead-pack-element" (SPack (1, [0])) [] [0] 0;
  check "live-pack-element" (SPack (1, [0])) [0] [0; 0] 1;
  check "branch-arm-settle" (SIf (0, SEmit 1, SEmit 2)) [] [0; 1; 2] 0;
  check "loop-certificate" (SWhile (0, [0], leaf 0)) [] [0] 0;
  List.iter (fun (name, s) ->
    match elab no_summaries s [] [0] with
    | Some (t, [0]) when count_copies t = 1 ->
        let _, drops, moves = target_counts t in
        if drops <> 1 || moves <> 0 then fail ("borrowed source moved/dropped: " ^ name)
    | _ -> fail ("borrowed input must be copied: " ^ name))
    ["borrowed-copy", SCopy (1, 0); "borrowed-pack", SPack (1, [0])];
  check ~modes:one_borrow "call-ending-argument-loan" (SCall (1, 0, [0])) [] [0] 0;
  (* The result's old binding moves in. Live-in is args ++ (L minus result),
     not the old borrow-only observer's duplicated result tail. *)
  check ~modes:one_borrow "inout-call-moves-result-back" (SCallIO (0, 1, [0])) [1] [1; 0] 0;
  check ~modes:gui_borrow "gui-calls-caller" gui_calls [] [] 0;
  let constructor = match gui_funs 0 with
    | Some d -> (match elab_fun gui_borrow 0 d with
        | Some ((([], [0]), t), 1) when count_copies t = 1 -> t
        | _ -> fail "GUI constructor must copy its borrowed input once")
    | None -> fail "missing GUI function fixture" in
  let append = match gui_procs 0 with
    | Some d -> (match elab_proc gui_borrow 0 d with
        | Some ((([5], [6]), t), 5) when count_copies t = 1 -> t
        | _ -> fail "GUI inout procedure must copy its borrowed element once")
    | None -> fail "missing GUI procedure fixture" in
  let caller, _ = admitted ~modes:gui_borrow "gui-call-composition" gui_calls [] in
  (* Borrow-only is a deliberate baseline, not a fallback mode table. *)
  if count_copies caller + count_copies constructor + count_copies append <> 2 then
    fail "GUI call-composition copy count drift";
  let negatives = [
    "self-copy", SCopy (0, 0), [], [];
    "self-definition", SDef (0, (fun _ -> SLeaf 0), [0]), [], [];
    "self-pack", SPack (0, [0]), [], [];
    "duplicate-pack", SPack (0, [1; 1]), [], [];
    "self-push", SPush (0, 0), [], [];
    "self-field", SField (0, 0, 0), [], [];
    "missing-loop-condition", SWhile (0, [], SSkip), [], [];
    "missing-body-live-in", SWhile (0, [0], SEmit 9), [], [];
    "missing-loop-exit", SWhile (0, [0], SSkip), [9], [];
    "overwrite-borrowed-def", leaf 0, [], [0];
    "overwrite-borrowed-copy", SCopy (0, 1), [], [0];
    "overwrite-borrowed-field", SField (0, 1, 0), [], [0];
    "mutate-borrowed-push", SPush (0, 1), [], [0];
    "call-result-overlaps-argument", SCall (0, 0, [0]), [], [];
    "overwrite-borrowed-call-result", SCall (0, 0, [1]), [], [0];
    "inout-overlaps-read-argument", SCallIO (0, 0, [0]), [], [];
    "inout-of-borrowed-input", SCallIO (0, 0, [1]), [], [0]
  ] in
  List.iter (fun (name, s, l, b) ->
    match elab one_borrow s l b with
    | None -> ()
    | Some _ -> fail ("accepted invalid core input: " ^ name)) negatives;
  let invalid_funs = [
    "duplicate-parameter", (([0; 0], SPack (1, [0])), 1);
    "return-borrowed-parameter", (([0], SSkip), 0);
    "missing-function-input", (([0], SPack (1, [9])), 1);
    "mutate-function-loan", (([0], leaf 0), 1)
  ] in
  List.iter (fun (name, definition) ->
    match elab_fun (fun_borrow definition) 0 definition with
    | None -> ()
    | Some _ -> fail ("accepted invalid function: " ^ name)) invalid_funs;
  let invalid_procs = [
    "duplicate-read-parameter", ((0, [1; 1]), SSkip);
    "inout-read-parameter-overlap", ((0, [0]), SSkip);
    "missing-procedure-input", ((0, [1]), SPush (0, 9))
  ] in
  List.iter (fun (name, definition) ->
    match elab_proc (proc_borrow definition) 0 definition with
    | None -> ()
    | Some _ -> fail ("accepted invalid inout procedure: " ^ name)) invalid_procs;
  (* Fail-closed summaries: each refusal is paired with the same input under
     a resolved table of the right arity, which must be admitted. *)
  let short = all_borrow 0 and long = all_borrow 2 in
  let label_fun = (([0], SPack (1, [0])), 1) and append_proc = ((5, [6]), SPush (5, 6)) in
  let summary_controls = [
    "missing summary at a call",
      elab no_summaries (SCall (1, 0, [0])) [] [0] = None,
      elab one_borrow (SCall (1, 0, [0])) [] [0] <> None;
    "missing summary at an inout call",
      elab no_summaries (SCallIO (0, 1, [0])) [1] [] = None,
      elab one_borrow (SCallIO (0, 1, [0])) [1] [] <> None;
    "summary of another routine",
      elab gui_borrow (SCall (1, 5, [0])) [] [0] = None,
      elab gui_borrow (SCall (1, 0, [0])) [] [0] <> None;
    "call summary shorter than the call",
      elab short (SCall (1, 0, [0])) [] [0] = None,
      elab one_borrow (SCall (1, 0, [0])) [] [0] <> None;
    "call summary longer than the call",
      elab long (SCall (1, 0, [0])) [] [0] = None,
      elab long (SCall (1, 0, [0; 2])) [] [0; 2] <> None;
    "inout summary shorter than the call",
      elab short (SCallIO (0, 1, [0])) [1] [] = None,
      elab one_borrow (SCallIO (0, 1, [0])) [1] [] <> None;
    "missing function summary",
      elab_fun no_summaries 0 label_fun = None, elab_fun one_borrow 0 label_fun <> None;
    "function summary shorter than its parameters",
      elab_fun short 0 label_fun = None, elab_fun one_borrow 0 label_fun <> None;
    "missing procedure summary",
      elab_proc no_summaries 0 append_proc = None, elab_proc one_borrow 0 append_proc <> None;
    "procedure summary shorter than its parameters",
      elab_proc short 0 append_proc = None, elab_proc one_borrow 0 append_proc <> None
  ] in
  List.iter (fun (name, refused, accepted) ->
    if not refused then fail ("accepted an unresolved or mismatched summary: " ^ name);
    if not accepted then fail ("summary control is vacuous: " ^ name)) summary_controls;
  let t, _ = admitted "gui" gui_program [] in
  let _, drops, _ = target_counts t in
  if drops <> 1 then fail "GUI must have one terminal deep-footprint drop";
  Printf.printf "[ownership-clean] 15 decision controls, %d refusal controls and %d summary controls PASS\n%!"
    (List.length negatives + List.length invalid_funs + List.length invalid_procs)
    (List.length summary_controls);
  let checks = ref 0 in
  let verify name predicate =
    incr checks; if not predicate then fail ("sink/composition control: " ^ name) in
  verify "inferred GUI sink modes"
    (gui_modes.fmodes 0 = Some [true] && gui_modes.pmodes 0 = Some [true]);
  let next_gui = infer_modes gui_funs gui_procs gui_modes in
  verify "GUI mode stability"
    (next_gui.fmodes 0 = gui_modes.fmodes 0 && next_gui.pmodes 0 = gui_modes.pmodes 0);
  let sink_constructor = match gui_funs 0 with
    | Some d -> (match elab_fun gui_modes 0 d with
        | Some ((([0], []), t), 1) -> t
        | _ -> fail "GUI constructor sink signature drift")
    | None -> fail "missing GUI constructor" in
  let sink_append = match gui_procs 0 with
    | Some d -> (match elab_proc gui_modes 0 d with
        | Some ((([5; 6], []), t), 5) -> t
        | _ -> fail "GUI append sink signature drift")
    | None -> fail "missing GUI append" in
  let sink_caller, _ = admitted ~modes:gui_modes "sink-caller" gui_calls [] in
  verify "sink call chain copies zero"
    (count_copies sink_caller + count_copies sink_constructor + count_copies sink_append = 0);
  let reuse_caller, _ = admitted ~modes:gui_modes "sink-reuse" gui_calls_reuse [] in
  verify "live source still needs one copy" (count_copies reuse_caller = 1);
  let update g = if g = 0 then Some (([0; 1], SPush (0, 1)), 0) else None in
  let none _ = None in
  let update_modes = infer_modes update none no_summaries in
  verify "owned update cannot borrow all" (match update 0 with
    | Some d -> elab_fun (borrow_modes update none) 0 d = None
    | None -> false);
  verify "owned update sink admission" (match update 0 with
    | Some d -> (match elab_fun update_modes 0 d with
        | Some ((([0; 1], []), t), 0) -> count_copies t = 0
        | _ -> false)
    | None -> false);
  let two_sinks = {fmodes = (fun _ -> Some [true; true]); pmodes = (fun _ -> None)} in
  verify "duplicate sink actual refused"
    (elab two_sinks (SCall (2, 0, [0; 0])) [2] [] = None &&
     elab_normalized two_sinks (SCall (2, 0, [0; 0])) [2] [] = None);
  let mixed = {fmodes = (fun _ -> Some [true; false]); pmodes = (fun _ -> None)} in
  let mixed_call, _ = admitted ~modes:mixed "sink-and-borrow-alias" (SCall (2, 0, [0; 0])) [2] in
  verify "sink also borrowed needs copy" (count_copies mixed_call = 1);
  let chain = function
    | 0 -> Some (([0], SCall (1, 1, [0])), 1)
    | 1 -> Some (([0], SCall (1, 2, [0])), 1)
    | 2 -> Some (([0], SPack (1, [0])), 1)
    | _ -> None in
  let m1 = infer_modes chain none no_summaries in
  let m2 = infer_modes chain none m1 in
  let m3 = infer_modes chain none m2 in
  let m4 = infer_modes chain none m3 in
  verify "one inference round is not a call-graph fixpoint"
    (m1.fmodes 0 = Some [false] && m1.fmodes 2 = Some [true] &&
     m2.fmodes 0 = Some [false] && m2.fmodes 1 = Some [true] &&
     m3.fmodes 0 = Some [true]);
  verify "three-function example reaches stable modes"
    (List.for_all (fun g -> m4.fmodes g = m3.fmodes g) [0; 1; 2; 3]);
  verify "extracted rounds match the hand-iterated rounds"
    (List.for_all (fun g -> (iter_modes chain none 3).fmodes g = m3.fmodes g) [0; 1; 2; 3]);
  verify "an undefined routine stays unresolved after inference" (m4.fmodes 3 = None);
  verify "a non-converged table costs a copy, not a refusal"
    (match chain 0 with
     | Some d -> (match elab_fun m2 0 d, elab_fun m3 0 d with
         | Some ((_, t2), _), Some ((_, t3), _) -> count_copies t2 = 1 && count_copies t3 = 0
         | _ -> false)
     | None -> false);
  List.iter (fun (name, modes, input) ->
    let raw, lin = admitted ~modes name input [] in
    let normalized = match elab_normalized modes input [] [] with
      | Some (t, lin') when lin = lin' -> t
      | _ -> fail ("normalization admission/live-in drift: " ^ name) in
    verify (name ^ " preserves primitive sites")
      (target_counts raw = target_counts normalized && count_copies raw = count_copies normalized);
    verify (name ^ " shrinks syntax") (cleanup_nodes normalized < cleanup_nodes raw);
    Printf.printf "[ownership-clean-composition] %s syntax %d -> %d; copies %d -> %d\n%!"
      name (cleanup_nodes raw) (cleanup_nodes normalized) (count_copies raw) (count_copies normalized)
  ) ["gui-inline", no_summaries, gui_program;
     "gui-borrow-caller", gui_borrow, gui_calls;
     "gui-sink-caller", gui_modes, gui_calls;
     "gui-sink-reuse", gui_modes, gui_calls_reuse;
     "branch", no_summaries, SIf (0, SEmit 1, SEmit 2)];
  List.iter (fun (name, s, l, b) ->
    verify ("normalization preserves refusal: " ^ name)
      (elab_normalized one_borrow s l b = None)) negatives;
  verify "empty branch keeps its guard" (match normalize_cleanup (TIf (Src 0, TSkip, TSkip)) with
    | TIf (Src 0, TSkip, TSkip) -> true | _ -> false);
  verify "no idempotent drop rewrite" (match normalize_cleanup (TSeq (TDrop (Src 0), TDrop (Src 0))) with
    | TSeq (TDrop (Src 0), TDrop (Src 0)) -> true | _ -> false);
  verify "no idempotent emit rewrite" (match normalize_cleanup (TSeq (TEmit (Src 0), TEmit (Src 0))) with
    | TSeq (TEmit (Src 0), TEmit (Src 0)) -> true | _ -> false);
  Printf.printf "[ownership-clean] %d sink/composition controls PASS\n%!" !checks

let readonly_artifacts flag =
  let original, _ = admitted "readonly-original" (ro_demo_program flag) [] in
  let optimized = match elab_readonly_program no_summaries (ro_demo_prefix flag) 1 0 ro_demo_body with
    | Some (t, []) -> t
    | _ -> fail "readonly branch witness refused" in
  original, optimized

let readonly_selftest () =
  let checks = ref 0 in
  let verify name predicate =
    incr checks; if not predicate then fail ("readonly elision: " ^ name) in
  List.iter (fun flag ->
    let original, optimized = readonly_artifacts flag in
    verify "one fewer copy" (count_copies original = 1 && count_copies optimized = 0);
    verify "one fewer certified allocation"
      (fixed_allocations original = Some 3 && fixed_allocations optimized = Some 2)
  ) [0; 1];
  (* Only writes to the alias or the root are refused (value semantics). *)
  List.iter (fun (name, body) ->
    verify name (match ro_admit 1 0 body [] [] with
      | RORefused ROUnsafeRegion -> true | _ -> false)
  ) [
    "write root", leaf 0;
    "write alias", leaf 1;
    "write root in one arm", SIf (2, SEmit 1, leaf 0);
    "write alias inside loop", SWhile (2, [0; 1; 2], leaf 1);
    "field result overwrites root", SField (0, 1, 0);
    "field result overwrites alias", SField (1, 0, 0);
    "mutate root", SPush (0, 3);
    "mutate alias", SPush (1, 3);
    "copy over root", SCopy (0, 3);
    "call result overwrites alias", SCall (1, 0, [3]);
    "inout root", SCallIO (0, 0, [3]);
    "write root inside a branch store", SIf (2, SEmit 1, SPack (0, [3]))
  ];
  List.iter (fun (name, body, renamed) ->
    verify name (match ro_admit 1 0 body [] [] with
      | ROAccepted r -> r = renamed | _ -> false)
  ) [
    "store alias in aggregate", SPack (3, [1]), SPack (3, [0]);
    "store root in aggregate", SPack (3, [0]), SPack (3, [0]);
    "consume alias", SPush (3, 1), SPush (3, 0);
    "retain under another name", SCopy (3, 1), SCopy (3, 0);
    "call reads the alias", SCall (3, 99, [1]), SCall (3, 99, [0]);
    "inout on a third variable", SCallIO (0, 3, [1]), SCallIO (0, 3, [0]);
    "call hidden in branch", SIf (2, SEmit 1, SCall (3, 0, [0])), SIf (2, SEmit 0, SCall (3, 0, [0]))
  ];
  verify "self alias" (match ro_admit 0 0 (SEmit 0) [] [] with
    | RORefused RODestinationIsRoot -> true | _ -> false);
  verify "live-out alias" (match ro_admit 1 0 (SEmit 1) [1] [] with
    | RORefused RODestinationLiveOut -> true | _ -> false);
  verify "borrowed destination" (match ro_admit 1 0 (SEmit 1) [] [1] with
    | RORefused RODestinationBorrowed -> true | _ -> false);
  verify "borrowed root is not overwritten" (match ro_admit 1 0 (SEmit 1) [] [0] with
    | ROAccepted (SEmit 0) -> true | _ -> false);
  verify "missing unused root cannot disappear"
    (elab_readonly_program no_summaries SSkip 1 0 SSkip = None);
  verify "original admission refusal cannot disappear"
    (elab_readonly_program no_summaries (SCopy (0, 0)) 1 0 ro_demo_body = None);
  verify "root survives its last direct use until the later alias read"
    (match elab_readonly_program no_summaries (leaf 0) 1 0 (SSeq (SEmit 0, SEmit 1)) with
     | Some (TSeq (TDef (Src 0, _, []),
         TSeq (TEmit (Src 0), TSeq (TEmit (Src 0), TDrop (Src 0)))), []) -> true
     | _ -> false);
  verify "loop certificate names follow root substitution"
    (match ro_admit 1 0 (SWhile (1, [1; 0], SEmit 1)) [] [] with
     | ROAccepted (SWhile (0, [0; 0], SEmit 0)) -> true | _ -> false);
  let loop_body = SSeq (SWhile (2, [0; 1; 2], SSeq (SEmit 1, leaf 2)), SEmit 0) in
  let original, _ = admitted "readonly-loop-original"
    (SSeq (ro_demo_prefix 1, SSeq (SCopy (1, 0), loop_body))) [] in
  let optimized = match elab_readonly_program no_summaries (ro_demo_prefix 1) 1 0 loop_body with
    | Some (t, []) -> t | _ -> fail "readonly loop candidate refused" in
  verify "loop elaboration removes the alias copy"
    (count_copies original = 1 && count_copies optimized = 0);
  verify "loop runtime allocation count remains unmeasured" (fixed_allocations optimized = None);
  let field_prefix = SDef (0, (fun _ -> SNode [SLeaf 7]), []) in
  let field_body = SSeq (SField (3, 1, 0), SSeq (SEmit 3, SEmit 0)) in
  let original, _ = admitted "readonly-field-original"
    (SSeq (field_prefix, SSeq (SCopy (1, 0), field_body))) [] in
  let optimized = match elab_readonly_program no_summaries field_prefix 1 0 field_body with
    | Some (t, []) -> t | _ -> fail "readonly copied-field candidate refused" in
  verify "field remains a copied value, only whole alias copy disappears"
    (count_copies original = 1 && count_copies optimized = 0 &&
     fixed_allocations original = Some 3 && fixed_allocations optimized = Some 2);
  verify "unknown call cost is not zero" (fixed_allocations (TCall (KFun, Src 0, 9, [], [])) = None);
  verify "unequal branch cost is not a constant"
    (fixed_allocations (TIf (Src 0, TSkip, TCopy (Src 1, Src 2))) = None);
  verify "equal branches allocate on one path, not both"
    (fixed_allocations (TIf (Src 0, TCopy (Src 1, Src 2), TCopy (Src 3, Src 4))) = Some 1);
  Printf.printf "[ownership-readonly] %d admission/alias/branch/loop/cost controls PASS\n%!" !checks

let places_selftest () =
  let checks = ref 0 in
  let verify name predicate =
    incr checks; if not predicate then fail ("places: " ^ name) in
  let prog modes name s = match elab modes s [] [] with
    | Some (t, []) -> t
    | _ -> fail ("places program refused: " ^ name) in
  let inout = prog gui_modes "inout-member" gui_state_inout in
  verify "inout on a member path copies nothing"
    (count_copies inout = 0 && count_field_reads inout = 0);
  let detach = prog gui_modes "detach-restore" gui_state_detach in
  verify "detach/restore pays one field copy" (count_field_reads detach = 1);
  let update = prog app_modes "update-from-self" gui_state_update in
  verify "update from self copies nothing"
    (count_copies update = 0 && count_field_reads update = 0 &&
     (match app_funs 7 with
      | Some d -> (match elab_fun app_modes 7 d with
          | Some ((([0; 1], []), t), 0) -> count_copies t = 0
          | _ -> false)
      | None -> false));
  let view = prog gui_modes "read-only-view" gui_state_view in
  verify "read-only view copies nothing" (count_copies view = 0 && count_field_reads view = 0);
  let view_copy = prog gui_modes "field-copy-view" gui_state_view_copy in
  verify "binding the field to a local pays one field copy" (count_field_reads view_copy = 1);
  List.iter (fun (name, s, l, b) ->
    verify name (elab gui_modes s l b = None)) [
    "body reads the suspended root", SFocus (4, 3, [0], SEmit 3), [], [];
    "temporary escapes the focus", SFocus (4, 3, [0], SEmit 4), [4], [];
    "temporary is the root", SFocus (3, 3, [0], SEmit 3), [], [];
    "borrowed root", SFocus (4, 3, [0], SEmit 4), [], [3]
  ];
  Printf.printf "[ownership-places] %d member-path/update/view/refusal controls PASS\n%!" !checks

let exits_selftest () =
  let checks = ref 0 in
  let verify name predicate =
    incr checks; if not predicate then fail ("exits: " ^ name) in
  let closed_drops name s live x = match xelab no_summaries s live x [] with
    | Some (t, []) -> xdrops t
    | _ -> fail ("exit program refused: " ^ name) in
  let partial = closed_drops "partial-build" partial_build [] no_exits in
  verify "error path releases both built parts" (List.mem (Src 0) partial && List.mem (Src 1) partial);
  let ret = {xbrk = []; xcont = []; xret = [9]; xerr = []} in
  let early = closed_drops "early-return" early_return_body [9] ret in
  verify "early return releases the other locals" (List.mem (Src 0) early && List.mem (Src 1) early);
  verify "early return never drops the result" (not (List.mem (Src 9) early));
  let brk = closed_drops "break" break_body [] no_exits in
  verify "break releases the loop-local value" (List.mem (Src 1) brk);
  verify "a return before the result exists is not closed"
    (xelab no_summaries XReturn [] ret [] = Some (XTReturn, [9]));
  verify "loop head without its condition"
    (xelab no_summaries (XLoop (0, [], XS SSkip)) [] no_exits [] = None);
  verify "loop continuation outside the head"
    (xelab no_summaries (XLoop (0, [0], XBreak)) [5] no_exits [] = None);
  verify "a call without a summary stays refused under exits"
    (xelab no_summaries (XS (SCall (1, 0, [0]))) [] no_exits [0] = None &&
     xelab one_borrow (XS (SCall (1, 0, [0]))) [] no_exits [0] <> None);
  Printf.printf "[ownership-exits] %d error/return/break/refusal controls PASS\n%!" !checks

let readonly_cost () =
  List.iter (fun flag ->
    let original, optimized = readonly_artifacts flag in
    let before, after = match fixed_allocations original, fixed_allocations optimized with
      | Some a, Some b -> a, b | _ -> fail "readonly cost has no fixed-path certificate" in
    Printf.printf
      "{\"flag\":%d,\"copies_before\":%d,\"copies_after\":%d,\"allocations_before\":%d,\"allocations_after\":%d,\"proof\":\"ro_demo_one_fewer_allocation\",\"scope\":\"abstract allocation frontier; not physical runtime bytes\"}\n%!"
      flag (count_copies original) (count_copies optimized) before after
  ) [0; 1]

type workload = {
  name : string; statements : int; width : int; depth : int;
  input : sStmt; live : var list; borrowed : var list; expected_live : var list; batch : int
}

let statement_workload n width = {
  name = if width = 0 then "statements" else "live-width";
  statements = n; width; depth = 0;
  input = seq (List.init n (fun _ -> leaf width));
  live = vars width; borrowed = []; expected_live = vars width; batch = if width = 0 then 32 else 2
}

let loop_workload depth width =
  let head = vars width in
  let body = seq (List.init 32 (fun _ -> leaf width)) in
  let rec nest d b = if d = 0 then b else nest (d - 1) (SWhile (0, head, b)) in {
    name = "loops"; statements = 32; width; depth;
    input = nest depth body; live = head; borrowed = []; expected_live = head; batch = 2
  }

let wide_live n =
  let definitions = List.init n leaf in
  let observations = List.init n (fun i -> SEmit i) in {
    name = "wide-live-program"; statements = 2 * n; width = n; depth = 0;
    input = seq (definitions @ observations); live = []; borrowed = []; expected_live = []; batch = 1
  }

let workloads =
  List.map (fun n -> statement_workload n 0) [128; 256; 512; 1024; 2048] @
  List.map (fun w -> statement_workload 64 w) [8; 16; 32; 64; 128] @
  List.map (fun w -> let c = statement_workload 64 w in
    {c with name = "borrowed-width"; borrowed = vars w}) [8; 16; 32; 64; 128] @
  List.map (fun d -> loop_workload d 32) [1; 2; 4; 8; 16; 32] @
  List.map (fun w -> loop_workload 8 w) [8; 16; 64; 128] @
  List.map wide_live [16; 32; 64; 128; 256]

let measure w =
  let t, lin = match elab no_summaries w.input w.live w.borrowed with
    | Some r -> r | None -> fail ("cost input refused: " ^ w.name) in
  if lin <> w.expected_live then fail ("cost input live-in drift: " ^ w.name);
  let instructions, drops, moves = target_counts t in
  let copies = count_copies t in
  let normalized = normalize_cleanup t in
  if target_counts normalized <> (instructions, drops, moves) ||
     count_copies normalized <> copies || cleanup_nodes normalized > cleanup_nodes t then
    fail ("normalization changed resource sites or grew syntax: " ^ w.name);
  let cpu = Array.make 5 0. and wall = Array.make 5 0. and words = Array.make 5 0. in
  for repeat = 0 to 4 do
    Gc.full_major ();
    let g0 = Gc.quick_stat () in
    let c0 = Sys.time () and t0 = Unix.gettimeofday () in
    for _ = 1 to w.batch do
      match Sys.opaque_identity (elab no_summaries w.input w.live w.borrowed) with
      | Some _ -> ()
      | None -> fail "measured elaboration unexpectedly refused"
    done;
    let t1 = Unix.gettimeofday () and c1 = Sys.time () in
    let g1 = Gc.quick_stat () in
    cpu.(repeat) <- (c1 -. c0) /. float w.batch;
    wall.(repeat) <- (t1 -. t0) /. float w.batch;
    words.(repeat) <- ((g1.minor_words +. g1.major_words -. g1.promoted_words) -.
                      (g0.minor_words +. g0.major_words -. g0.promoted_words)) /. float w.batch
  done;
  let median samples = Array.sort Float.compare samples; samples.(2) in
  Printf.printf
    "{\"name\":\"%s\",\"statements\":%d,\"live_width\":%d,\"borrowed_width\":%d,\"loop_depth\":%d,\"source_nodes\":%d,\"target_instructions\":%d,\"copies\":%d,\"drop_sites\":%d,\"moves\":%d,\"raw_syntax_nodes\":%d,\"normalized_syntax_nodes\":%d,\"repeats\":5,\"batch\":%d,\"cpu_seconds_median\":%.9g,\"wall_seconds_median\":%.9g,\"allocated_words_median\":%.9g}\n%!"
    w.name w.statements w.width (List.length w.borrowed) w.depth (source_nodes w.input)
    instructions copies drops moves (cleanup_nodes t) (cleanup_nodes normalized)
    w.batch (median cpu) (median wall) (median words)

let () =
  match Array.to_list Sys.argv with
  | [_; "--selftest"] -> selftest (); readonly_selftest (); places_selftest (); exits_selftest ()
  | [_; "--bench"] -> List.iter measure workloads
  | [_; "--readonly-cost"] -> readonly_cost ()
  | _ -> fail "usage: ownership_clean_driver --selftest|--bench|--readonly-cost"
