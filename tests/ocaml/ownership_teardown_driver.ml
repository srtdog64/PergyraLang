(* Finite inputs, independent value oracles, and bounded OCaml cost evidence.
   Policies are freshly extracted. This is not a native allocator/GC benchmark. *)
open Ownership_teardown

let fail msg = prerr_endline ("[teardown-observer] " ^ msg); exit 1
let check msg condition = if not condition then fail msg

let node ?(owner = ORoot 0) fields = { owner0 = owner; fields }
let slot x nd = { s_gen = x mod 3; s_node = Some nd }
let empty_slot = { s_gen = 0; s_node = None }
let heap_of nodes x = if x < Array.length nodes then slot x nodes.(x) else empty_slot
let link x = (x, x mod 3)
let rows_of nodes =
  let rows = Array.make (Array.length nodes) [] in
  Array.iteri (fun src nd -> for k = 0 to 1 do
    match nd.fields k with None -> () | Some (t, _) -> rows.(t) <- rows.(t) @ [(src,k)]
  done) nodes; rows
let index_of rows t = if t < Array.length rows then rows.(t) else []
let update_oracle rows src k value =
  let out = Array.map (List.filter (fun e -> e <> (src,k))) rows in
  (match value with None -> () | Some (t,_) -> out.(t) <- out.(t) @ [(src,k)]); out
let assert_rows name n expected result =
  for t = 0 to n do
    let row = if t < n then expected.(t) else [] in
    check (name ^ ": value mismatch") (result t = row)
  done

let assert_teardown nodes h ix unit =
  let n = Array.length nodes in
  let kd = function ORoot 0 -> List.init n Fun.id | _ -> [] in
  let state = { heap0 = h; rix = ix; kids0 = kd; roots = [0]; bound = n;
                root_gen = (fun r -> r + 7) } in
  let actual = teardown state unit in
  let retired x = List.mem x unit in
  let expected = Array.init n (fun x -> if retired x then None else
    Some (node ~owner:nodes.(x).owner0 (fun k ->
      match nodes.(x).fields k with
      | Some (t,_) when retired t -> None | old -> old))) in
  let rows = Array.make n [] in
  Array.iteri (fun x nd ->
    (match nd with None -> () | Some nd -> for k = 0 to 1 do
      match nd.fields k with None -> () | Some (t,_) -> rows.(t) <- rows.(t) @ [(x,k)]
    done);
    let actual_slot = actual.heap0 x in
    check "teardown generation mismatch" (actual_slot.s_gen = (h x).s_gen + (if retired x then 1 else 0));
    match nd, actual_slot.s_node with
    | None, None -> ()
    | Some expected_node, Some actual_node ->
        check "teardown owner mismatch" (expected_node.owner0 = actual_node.owner0);
        for k = 0 to 2 do check "teardown field mismatch" (expected_node.fields k = actual_node.fields k) done
    | _ -> fail "teardown liveness mismatch"
  ) expected;
  assert_rows "teardown reverse index" n rows actual.rix;
  check "teardown children mismatch"
    (actual.kids0 (ORoot 0) = List.filter (fun x -> not (retired x)) (kd (ORoot 0)));
  check "teardown foreign owner row changed" (actual.kids0 (ORoot 1) = [] && actual.kids0 (OParent 0) = []);
  check "pure teardown changed roots/bound" (actual.roots = state.roots && actual.bound = state.bound);
  for r = 0 to 2 do check "node retirement changed root epoch" (actual.root_gen r = state.root_gen r) done;
  check "teardown changed free off-frame slot" (actual.heap0 n = h n)

(* Independent identity oracle: fresh extracted checks, not a reimplementation
   of admission. RootDrop is the pure transform on a known exact singleton;
   declaration/allocation below are finite fixtures, not a runtime executor. *)
let assert_identities state =
  let checks = ref 0 in
  for x = 0 to 2 do
    let actual_slot = state.heap0 x in
    for g = 0 to actual_slot.s_gen + 1 do
      let expected = if g = actual_slot.s_gen then actual_slot.s_node else None in
      let actual = resolve_node state.heap0 (x,g) in
      (match expected, actual with
      | None, None -> ()
      | Some e, Some a ->
          check "checked node owner mismatch" (e.owner0 = a.owner0);
          for k = 0 to 2 do check "checked node field mismatch" (e.fields k = a.fields k) done
      | _ -> fail "stale/dead node read accepted or current read refused");
      incr checks
    done
  done;
  for r = 0 to 2 do for g = 0 to state.root_gen r + 1 do
    let expected = List.mem r state.roots && state.root_gen r = g in
    check "stale/dead root accepted or current root refused" (check_root state (r,g) = expected);
    incr checks
  done done;
  !checks

let identity_selftest () =
  let a = node (fun k -> if k = 0 then Some (1,6) else None) in
  let b = node ~owner:(ORoot 1) (fun k -> if k = 0 then Some (0,4) else None) in
  let initial = { heap0 = (function 0 -> {s_gen=4;s_node=Some a}
                                  | 1 -> {s_gen=6;s_node=Some b} | _ -> empty_slot);
    rix = (function 0 -> [1,0] | 1 -> [0,0] | _ -> []);
    kids0 = (function ORoot 0 -> [0] | ORoot 1 -> [1] | _ -> []);
    roots = [0;1]; bound = 2; root_gen = (function 0 -> 3 | 1 -> 7 | _ -> 11) } in
  let state = ref initial and old_roots = ref [] and old_nodes = ref [] and checks = ref 0 in
  for _ = 1 to 64 do
    checks := !checks + assert_identities !state;
    let before = !state in
    let old_root = (0,before.root_gen 0) and old_node = (0,(before.heap0 0).s_gen) in
    check "current root refused before drop" (check_root before old_root);
    let retired = root_drop before 0 [0] in
    (* Count the reached immutable source read, not heap/model size. *)
    for x = 0 to 2 do
      let reads = ref 0 in
      let observed = { before with heap0=(fun y -> incr reads; before.heap0 y) } in
      ignore ((root_drop observed 0 [0]).heap0 x);
      check "one retirement reread source slot" (!reads = 1);
      reads := 0;
      ignore (resolve_node observed.heap0 (x,(before.heap0 x).s_gen));
      check "checked node read reread source slot" (!reads = 1)
    done;
    check "root retirement failed to advance epoch" (retired.root_gen 0 = before.root_gen 0 + 1);
    check "root retirement changed other epoch" (retired.root_gen 1 = before.root_gen 1 && retired.root_gen 2 = before.root_gen 2);
    check "root retirement kept dead root" (retired.roots = [1]);
    check "dead current root admitted" (not (check_root retired (0,retired.root_gen 0)));
    check "root retirement failed to clear surviving incoming link" (field_at retired.heap0 1 0 = None);
    check "root retirement changed surviving node generation" ((retired.heap0 1).s_gen = (before.heap0 1).s_gen);
    old_roots := old_root :: !old_roots; old_nodes := old_node :: !old_nodes;
    checks := !checks + assert_identities retired;
    let reopened = { retired with roots = 0 :: retired.roots } in
    check "redeclared current root refused" (check_root reopened (0,reopened.root_gen 0));
    let fresh_slot = {s_gen=(reopened.heap0 0).s_gen;s_node=Some (node (fun _ -> None))} in
    state := { reopened with heap0=(fun x -> if x = 0 then fresh_slot else reopened.heap0 x);
      kids0=(fun q -> if q = ORoot 0 then [0] else reopened.kids0 q) };
    List.iter (fun old -> check "old root handle revived on reuse" (not (check_root !state old))) !old_roots;
    List.iter (fun old -> check "saved local handle revived on reuse" (resolve_node (!state).heap0 old = None)) !old_nodes;
    checks := !checks + assert_identities !state
  done;
  Printf.printf "[teardown-observer] 64 root/node reuse rounds, %d checked identity cases PASS\n%!" !checks

let authority_selftest () =
  let module A = Ownership_teardown_authority in
  let cases = ref 0 in
  let accept name = function
    | A.Accepted s -> incr cases; s
    | A.Refused _ -> fail ("authority success refused: " ^ name) in
  let refuse name expected before = function
    | A.Refused (why, unchanged) ->
        check ("authority failure mismatch: " ^ name) (why = expected);
        check ("authority rejection changed state: " ^ name) (unchanged == before);
        incr cases
    | A.Accepted _ -> fail ("authority refusal accepted: " ^ name) in
  let base = accept "root issuance" (A.create_root (A.empty_authority 10) 0) in
  let parent = accept "owned allocation" (A.allocate_owned base 0 (A.ORoot 0) 0) in
  let owned = accept "child allocation" (A.allocate_owned parent 1 (A.OParent 0) 0) in
  let root = A.RetireRoot (0,0) and subtree = A.RetireNode (0,0) in
  let foreign = A.in_context owned 20 in
  refuse "foreign pin issuance" A.MissingCleanupRight foreign (A.begin_lease foreign (1,0) A.PinLease 20);
  List.iter (fun target ->
    refuse "foreign identifying handle" A.MissingCleanupRight foreign (A.retire foreign target [0;1]);
    List.iter (fun unit -> refuse "invalid complete unit" A.InvalidUnit owned (A.retire owned target unit))
      [[];[0];[1];[0;1;1];[0;1;2]];
    let retired = accept "own retirement" (A.retire owned target [0;1]) in
    check "authority retirement left parent/child" ((retired.forest.heap0 0).s_node = None &&
      (retired.forest.heap0 1).s_node = None)
  ) [root;subtree];
  List.iter (fun kind ->
    let leased = accept "child lease issuance" (A.begin_lease owned (1,0) kind 10) in
    List.iter (fun target -> refuse "descendant protected" A.ActiveLease leased
      (A.retire leased target [0;1])) [root;subtree];
    let alien = A.in_context leased 20 in
    refuse "other context ending lease" A.WrongLeaseHolder alien (A.end_lease alien 0);
    let ended = accept "own lease end" (A.end_lease leased 0) in
    refuse "duplicate lease end" A.MissingLease ended (A.end_lease ended 0);
    ignore (accept "retirement after lease" (A.retire ended root [0;1]));
    let reloaned = accept "new lease" (A.begin_lease ended (1,0) kind 10) in
    refuse "old lease replay" A.MissingLease reloaned (A.end_lease reloaned 0);
    refuse "new lease remains active" A.ActiveLease reloaned (A.retire reloaned root [0;1])
  ) [A.BorrowLease;A.PinLease];
  let moved = accept "transfer" (A.transfer_cleanup owned (0,0) 20) in
  refuse "old owner cannot mint pin" A.MissingCleanupRight moved (A.begin_lease moved (1,0) A.PinLease 10);
  refuse "old owner after transfer" A.MissingCleanupRight moved (A.retire moved root [0;1]);
  refuse "old owner cannot transfer again" A.MissingCleanupRight moved (A.transfer_cleanup moved (0,0) 30);
  ignore (accept "new owner retirement" (A.retire (A.in_context moved 20) root [0;1]));
  let consumed = accept "root consume" (A.retire owned root [0;1]) in
  check "consumed root retained cleanup right" (consumed.cleanup_rights 0 = None);
  refuse "root consumed twice" A.StaleIdentity consumed (A.retire consumed root [0;1]);
  let reopened = accept "root reuse" (A.create_root consumed 0) in
  let reused = accept "slot reuse" (A.allocate_owned reopened 0 (A.ORoot 0) 1) in
  refuse "stale root after reuse" A.StaleIdentity reused (A.retire reused root [0]);
  refuse "stale node after reuse" A.StaleIdentity reused (A.retire reused subtree [0]);
  ignore (accept "new epoch retirement" (A.retire reused (A.RetireRoot (0,1)) [0]));
  (* Another root's lease must not prevent this root's complete retirement. *)
  let other_root = accept "other root" (A.create_root foreign 1) in
  let other_node = accept "other owned node" (A.allocate_owned other_root 2 (A.ORoot 1) 0) in
  let other_loan = accept "external loan" (A.begin_lease other_node (2,0) A.BorrowLease 20) in
  let own_context = A.in_context other_loan 10 in
  let own_drop = accept "external lease unaffected" (A.retire own_context root [0;1]) in
  check "foreign node/right/lease damaged" ((own_drop.forest.heap0 2).s_node <> None &&
    own_drop.cleanup_rights 1 = Some (0,20) && List.length own_drop.active_leases = 1);
  let lent = accept "owner-approved foreign loan" (A.begin_lease owned (1,0) A.BorrowLease 20) in
  refuse "lender cannot cancel reader lease" A.WrongLeaseHolder lent (A.end_lease lent 0);
  let reader = A.in_context lent 20 in
  refuse "reader cannot release owner" A.MissingCleanupRight reader (A.retire reader root [0;1]);
  ignore (accept "reader returns loan" (A.end_lease reader 0));
  (* Independent same-input st_ab fixture; its AuthorityInvariant is also
     kernel-proved in Redteam. No native state-import or bypass API is claimed. *)
  let ab_heap x = if x = 0 then { A.s_gen=0; s_node=Some { A.owner0=A.ORoot 0; fields=(fun _ -> None) } }
    else if x = 1 then { A.s_gen=0; s_node=Some { A.owner0=A.ORoot 1;
      fields=(fun k -> if k = 0 then Some (0,0) else None) } }
    else { A.s_gen=0; s_node=None } in
  let ab_forest = { A.heap0=ab_heap; rix=(fun t -> if t = 0 then [1,0] else []);
    kids0=(function A.ORoot 0 -> [0] | A.ORoot 1 -> [1] | _ -> []);
    roots=[0;1]; bound=2; root_gen=(fun _ -> 0) } in
  let ab_reader = { A.forest=ab_forest; acting_context=20;
    cleanup_rights=(fun r -> if r = 0 then Some (0,10) else if r = 1 then Some (0,20) else None);
    active_leases=[]; next_lease=0 } in
  List.iter (fun target ->
    refuse "same-input copied link" A.MissingCleanupRight ab_reader (A.retire ab_reader target [0]);
    let ab_owner = A.in_context ab_reader 10 in
    let cleared = accept "same-input owner teardown" (A.retire ab_owner target [0]) in
    check "authority incoming link not cleared" ((cleared.forest.heap0 0).s_node = None &&
      match (cleared.forest.heap0 1).s_node with Some nd -> nd.fields 0 = None | None -> false)
  ) [A.RetireRoot (0,0); A.RetireNode (0,0)];
  Printf.printf "[teardown-observer] %d extracted authority success/refusal cases PASS\n%!" !cases

let selftest () =
  let n = 3 and checks = ref 0 and retirements = ref 0 in
  (* All 4096 two-field states on three live nodes; links carry their actual
     generations. Index fixtures are independently built from stored fields. *)
  for mask = 0 to 4095 do
    let digit p = (mask / (1 lsl (2*p))) mod 4 in
    let nodes = Array.init n (fun src -> node (fun k ->
      if k > 1 then None else let d = digit (src*2+k) in
      if d = 0 then None else Some (link (d-1)))) in
    let h = heap_of nodes and rows = rows_of nodes in
    let ix = index_of rows in
    for src = 0 to n-1 do for k = 0 to 1 do for d = 0 to n do
      let value = if d = 0 then None else Some (link (d-1)) in
      let expected = update_oracle rows src k value in
      let actual = index_set h ix src k value in
      let scan = index_set_scan_spec ix src k value in
      assert_rows "local field" n expected actual;
      assert_rows "scan field" n expected scan;
      for t = 0 to n-1 do
        let old_target = match nodes.(src).fields k with None -> -1 | Some (t,_) -> t in
        let new_target = match value with None -> -1 | Some (t,_) -> t in
        if t <> old_target && t <> new_target then
          check "unrelated field row copied" (actual t == ix t)
      done;
      incr checks
    done done done;
    (* Every singleton is an exact node unit in this all-root forest;
       [0;1;2] is the root unit. The pure transform is not a Step checker. *)
    List.iter (fun unit -> assert_teardown nodes h ix unit; incr retirements)
      [[0];[1];[2];[0;1;2]]
  done;
  for old_id = 0 to 3 do for new_id = 0 to 3 do
    let old = if old_id < 2 then ORoot old_id else OParent (old_id-2) in
    let fresh = if new_id < 2 then ORoot new_id else OParent (new_id-2) in
    let owners = [ORoot 0; ORoot 1; OParent 0; OParent 1] in
    let fixtures = List.mapi (fun i q -> q, (if q = old then [4;8;9] else [10+i])) owners in
    let kd q = match List.assoc_opt q fixtures with Some xs -> xs | None -> [] in
    let actual = kids_move kd 8 old fresh and scan = kids_move_scan_spec kd 8 fresh in
    List.iter (fun q ->
      let removed = if q = old then List.filter ((<>) 8) (kd q) else kd q in
      let expected = if q = fresh then 8 :: removed else removed in
      check "children value mismatch" (actual q = expected && scan q = expected);
      if q <> old && q <> fresh then check "unrelated children row copied" (actual q == kd q)
    ) owners
  done done;
  check "unique empty refused" (unit_unique []);
  check "unique list refused" (unit_unique [0;2;7]);
  check "duplicate unit admitted" (not (unit_unique [0;0]));
  check "nonadjacent duplicate admitted" (not (unit_unique [0;2;0]));
  Printf.printf "[teardown-observer] %d field updates, %d teardown cases, 16 parent cases, 4 schedule controls PASS\n%!"
    !checks !retirements;
  identity_selftest ();
  authority_selftest ()

let median xs = let ys = List.sort Float.compare xs in List.nth ys (List.length ys / 2)
let sink = ref 0
let measure repeats observe build =
  let sample () =
    Gc.full_major ();
    let allocated = Gc.allocated_bytes () and started = Sys.time () in
    for _ = 1 to repeats do sink := (!sink + observe (build ())) mod 1000000007 done;
    (Sys.time () -. started, Gc.allocated_bytes () -. allocated)
  in
  let samples = List.init 5 (fun _ -> sample ()) in
  (median (List.map fst samples), median (List.map snd samples), samples)
let samples_json samples = "[" ^ String.concat "," (List.map (fun (cpu, allocated) ->
  Printf.sprintf "{\"cpu_s\":%.9f,\"allocated_bytes\":%.0f}" cpu allocated) samples) ^ "]"
let print_case kind n scans local append_visits before after =
  let tb, ab, sb = before and ta, aa, sa = after in
  Printf.printf "{\"kind\":\"%s\",\"nodes\":%d,\"scan_filter_visits\":%d,\"local_filter_visits\":%d,\"append_visits\":%d,\"scan_cpu_s\":%.9f,\"local_cpu_s\":%.9f,\"scan_allocated_bytes\":%.0f,\"local_allocated_bytes\":%.0f,\"scan_samples\":%s,\"local_samples\":%s}\n%!"
    kind n scans local append_visits tb ta ab aa (samples_json sb) (samples_json sa)

let benchmark () =
  let repeats = 64 in
  List.iter (fun n ->
    List.iter (fun mode ->
      let rows = if mode = 1 then Array.init n (fun t -> if t = 0 then List.init n (fun x -> x,0) else [])
                 else if mode = 2 then Array.init n (fun t ->
                   if t = 0 then [0,0] else if t = n-1 then List.init (n-1) (fun x -> x+1,0) else [])
                 else Array.init n (fun t -> [t,0]) in
      let nodes = Array.init n (fun x -> node (fun k ->
        if k = 0 then Some (link (if mode = 1 then 0 else if mode = 2 && x <> 0 then n-1 else x)) else None)) in
      let h = heap_of nodes and ix = index_of rows in
      let value = Some (link (n-1)) in
      let expected = update_oracle rows 0 0 value in
      assert_rows "benchmark local field" n expected (index_set h ix 0 0 value);
      assert_rows "benchmark scan field" n expected (index_set_scan_spec ix 0 0 value);
      let observe f = let result = ref 0 in for t = 0 to n-1 do
        result := !result + List.length (f t) done; !result in
      let before = measure repeats observe (fun () -> index_set_scan_spec ix 0 0 value) in
      let after = measure repeats observe (fun () -> index_set h ix 0 0 value) in
      check "benchmark field parity" (observe (index_set_scan_spec ix 0 0 value) = observe (index_set h ix 0 0 value));
      print_case (if mode = 1 then "crowded_field" else if mode = 2 then "crowded_new_field" else "distributed_field") n n
        (index_set_filter_visits h ix 0 0) (List.length (ix (n-1))) before after
    ) [0;1;2];
    let families = Array.init n (fun p -> [n+p]) in
    let root_rows = Array.init n (fun r -> [r]) in
    let kd = function OParent p when p < n -> families.(p)
                  | ORoot r when r < n -> root_rows.(r) | _ -> [] in
    let observe f = let result = ref 0 in for q = 0 to n-1 do
      result := !result + List.length (f (OParent q)) + List.length (f (ORoot q)) done; !result in
    let actual = kids_move kd n (OParent 0) (OParent (n-1)) in
    let scan = kids_move_scan_spec kd n (OParent (n-1)) in
    for q = 0 to n-1 do
      let expected = if q = 0 then [] else if q = n-1 then [n;2*n-1] else kd (OParent q) in
      check "benchmark children parity" (actual (OParent q) = expected && scan (OParent q) = expected);
      check "benchmark root row changed" (actual (ORoot q) = kd (ORoot q) && scan (ORoot q) = kd (ORoot q))
    done;
    let before = measure repeats observe (fun () -> kids_move_scan_spec kd n (OParent (n-1))) in
    let after = measure repeats observe (fun () -> kids_move kd n (OParent 0) (OParent (n-1))) in
    print_case "distributed_parent" n (2*n) (kids_move_filter_visits kd (OParent 0)) 0 before after
  ) [512;4096;16384];
  List.iter (fun n ->
    let unit = List.init n Fun.id in
    let started = Sys.time () in check "unique benchmark refused" (unit_unique unit);
    Printf.printf "{\"kind\":\"unique_certificate\",\"nodes\":%d,\"membership_comparisons\":%d,\"cpu_s\":%.9f}\n%!"
      n (n*(n-1)/2) (Sys.time () -. started)
  ) [256;1024;4096];
  Printf.eprintf "[teardown-observer] bounded OCaml CPU/allocation; repeats=64; median=5; checksum=%d\n%!" !sink

let () = match Array.to_list Sys.argv with
  | [_;"--selftest"] -> selftest ()
  | [_;"--benchmark"] -> benchmark ()
  | _ -> fail "usage: ownership_teardown_driver --selftest|--benchmark"
