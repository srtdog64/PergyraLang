(* The generated module owns the decision. This bridge only translates the
   finite binary protocol; malformed input is an explicit process failure. *)
let bits_of_line line =
  List.init (String.length line) (fun i ->
    match line.[i] with
    | '0' -> false
    | '1' -> true
    | _ -> invalid_arg "checker input must contain only binary digits")

let () =
  try
    while true do
      let line = read_line () in
      let decision = Proof_certificate_checker.check_bits (bits_of_line line) in
      print_endline (if decision then "1" else "0")
    done
  with
  | End_of_file -> ()
  | Invalid_argument message -> prerr_endline message; exit 2
