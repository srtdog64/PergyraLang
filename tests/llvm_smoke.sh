#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
source "$ROOT_DIR/tests/pgy_binary_path_helpers.sh"
pgy_prepend_windows_runtime_paths
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"
if [[ "$PGY" != *.exe && -x "${PGY}.exe" ]]; then
    PGY="${PGY}.exe"
fi

if [[ ! -x "$PGY" ]]; then
    echo "missing compiler binary: $PGY" >&2
    exit 1
fi

TMPDIR="$(mktemp -d)"
trap 'rm -rf "$TMPDIR"' EXIT

run_case() {
    local name="$1"
    local file="$2"
    shift 2
    local file_arg
    local output

    file_arg="$(pgy_path_for_compiler "$PGY" "$file")"
    if ! output="$(
        cd "$(dirname "$file")"
        "$PGY" "$file_arg" --run --backend=llvm 2>&1
    )"; then
        echo "[llvm-smoke] $name failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    for expected in "$@"; do
        if ! grep -Fq -- "$expected" <<<"$output"; then
            echo "[llvm-smoke] $name failed" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[llvm-smoke] $name ok"
}

run_ir_contains_case() {
    local name="$1"
    local file="$2"
    local needle="$3"
    local ll="$TMPDIR/${name}.ll"
    local file_arg
    local ll_arg
    local output

    file_arg="$(pgy_path_for_compiler "$PGY" "$file")"
    ll_arg="$(pgy_path_for_compiler "$PGY" "$ll")"
    if ! output="$(
        cd "$(dirname "$file")"
        "$PGY" "$file_arg" --emit-llvm -o "$ll_arg" 2>&1
    )"; then
        echo "[llvm-smoke] $name failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    if [[ ! -f "$ll" ]] || ! grep -Fq -- "$needle" "$ll"; then
        echo "[llvm-smoke] $name failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--- llvm ir ---" >&2
        [[ -f "$ll" ]] && sed -n '1,220p' "$ll" >&2
        echo "--------------" >&2
        exit 1
    fi
    echo "[llvm-smoke] $name ok"
}

run_ir_not_contains_case() {
    local name="$1"
    local file="$2"
    local needle="$3"
    local ll="$TMPDIR/${name}.ll"
    local file_arg
    local ll_arg
    local output

    file_arg="$(pgy_path_for_compiler "$PGY" "$file")"
    ll_arg="$(pgy_path_for_compiler "$PGY" "$ll")"
    if ! output="$(
        cd "$(dirname "$file")"
        "$PGY" "$file_arg" --emit-llvm -o "$ll_arg" 2>&1
    )"; then
        echo "[llvm-smoke] $name failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--------------" >&2
        exit 1
    fi
    if [[ ! -f "$ll" ]] || grep -Fq -- "$needle" "$ll"; then
        echo "[llvm-smoke] $name failed" >&2
        echo "--- output ---" >&2
        echo "$output" >&2
        echo "--- llvm ir ---" >&2
        [[ -f "$ll" ]] && sed -n '1,220p' "$ll" >&2
        echo "--------------" >&2
        exit 1
    fi
    echo "[llvm-smoke] $name ok"
}

run_compile_fails_case() {
    local name="$1"
    local file="$2"
    shift 2
    local out="$TMPDIR/${name}.out"
    local file_arg
    local out_arg
    local output

    file_arg="$(pgy_path_for_compiler "$PGY" "$file")"
    out_arg="$(pgy_path_for_compiler "$PGY" "$out")"
    if output="$(
        cd "$(dirname "$file")"
        "$PGY" "$file_arg" --backend=llvm -o "$out_arg" 2>&1
    )"; then
        echo "[llvm-smoke] $name unexpectedly succeeded" >&2
        exit 1
    fi
    for expected in "$@"; do
        if ! grep -Fq -- "$expected" <<<"$output"; then
            echo "[llvm-smoke] $name failed" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[llvm-smoke] $name ok"
}

cat > "$TMPDIR/phi_lowering.pgy" <<'EOF'
func Choose(flag: Bool) -> Int {
    let x: Int = 0;
    if flag {
        x = 10;
    } else {
        x = 20;
    }
    return x;
}

func Main() -> Void {
    Log(Choose(true));
}
EOF
run_ir_contains_case "true_phi_lowering" "$TMPDIR/phi_lowering.pgy" " phi i32 "

cat > "$TMPDIR/ssa_def_reassign_type_fact.pgy" <<'EOF'
class Cell {
    let value: Int;

    func Score(self) -> Int {
        return value * 10;
    }
}

func Main() -> Void {
    let x: Int = 0;
    if true {
        x = 1;
    }
    Log(x);

    let label: String = "cold";
    if true {
        label = "hot";
    }
    Log(label);

    let cell: Cell = Cell(2);
    if true {
        cell = Cell(7);
    }
    Log(cell.Score());
}
EOF
run_case "ssa_def_reassign_type_fact" "$TMPDIR/ssa_def_reassign_type_fact.pgy" "1" "hot" "70"

cat > "$TMPDIR/loop_break_continue.pgy" <<'EOF'
func Main() -> Void {
    let sum: Int = 0;
    for i in 0..6 {
        if i == 2 { continue; }
        if i == 5 { break; }
        sum = sum + i;
    }
    Log(sum);
}
EOF
run_case "break_continue" "$TMPDIR/loop_break_continue.pgy" "8"

cat > "$TMPDIR/array_enum.pgy" <<'EOF'
enum Color { Red, Green, Blue }
func Main() -> Void {
    let values: Array<Int> = [1, 2, 3];
    let c: Color = Red;
    Log(values[1]);
    if c == Color.Red { Log(9); }
}
EOF
run_case "array_enum" "$TMPDIR/array_enum.pgy" "2" "9"

cat > "$TMPDIR/field_channel.pgy" <<'EOF'
class SelectBox {
    let ch: Channel<Int>;

    func Pull(self) -> Int {
        ch <- 9;
        select {
            case v = <-ch:
                return v;
            default:
                return 0;
        }
        return 0;
    }
}
EOF
run_ir_contains_case "field_channel_send" "$TMPDIR/field_channel.pgy" "pgy_lane_channel_send_Int"
run_ir_contains_case "field_channel_select" "$TMPDIR/field_channel.pgy" "pgy_lane_channel_ready_Int"

cat > "$TMPDIR/field_channel_builtins.pgy" <<'EOF'
class BuiltinBox {
    let ch: Channel<Int>;

    func Probe(self) -> Int {
        let sent: Bool = TrySend(ch, 1);
        if sent {
            Log(ChannelLength(ch));
        }
        ChannelClose(ch);
        if ChannelClosed(ch) {
            return ChannelCapacity(ch);
        }
        return ChannelSpace(ch);
    }
}
EOF
run_ir_contains_case "field_channel_try_send" "$TMPDIR/field_channel_builtins.pgy" "pgy_lane_channel_try_send_Int"
run_ir_contains_case "field_channel_builtin_query" "$TMPDIR/field_channel_builtins.pgy" "pgy_channel_length_Int"
run_ir_contains_case "field_channel_close" "$TMPDIR/field_channel_builtins.pgy" "pgy_channel_close_Int"

cat > "$TMPDIR/field_channel_recv_infer.pgy" <<'EOF'
class StringBox {
    let ch: Channel<String>;

    func Pull(self) -> Void {
        let value = <-ch;
        Log(value);
    }
}
EOF
run_ir_contains_case "field_channel_recv_infer" "$TMPDIR/field_channel_recv_infer.pgy" "pgy_lane_channel_recv_val_String"
run_ir_not_contains_case "field_channel_recv_not_i32" "$TMPDIR/field_channel_recv_infer.pgy" "alloca i32"

cat > "$TMPDIR/tagged_union.pgy" <<'EOF'
enum Shape {
    Circle(Int),
    Rect(Int, Int),
    None
}

func Main() -> Void {
    let c: Shape = Circle(10);
    let r: Shape = Rect(4, 5);
    Log(c.Circle._0);
    Log(r.Rect._0);
    Log(r.Rect._1);
    let x: Shape = Circle(7);
    Log(x.Circle._0);
}
EOF
run_case "tagged_union" "$TMPDIR/tagged_union.pgy" "10" "4" "5" "7"

cat > "$TMPDIR/dynamic_array_ops.pgy" <<'EOF'
func Main() -> Void {
    let values: Array<Int> = [];
    ArrayPush(values, 10);
    ArrayPush(values, 20);
    ArraySet(values, 1, 77);
    Log(ArrayLength(values));
    Log(values[1]);
    ArrayPop(values);
    Log(ArrayLength(values));
}
EOF
run_case "dynamic_array_ops" "$TMPDIR/dynamic_array_ops.pgy" "2" "77" "1"

cat > "$TMPDIR/set_ops.pgy" <<'EOF'
func Main() -> Void {
    let seen: Set<Int> = SetNew();
    SetAdd(seen, 7);
    SetAdd(seen, 9);
    Log(SetHas(seen, 7));
    SetRemove(seen, 7);
    Log(SetHas(seen, 7));
    Log(SetSize(seen));
}
EOF
run_case "set_ops" "$TMPDIR/set_ops.pgy" "true" "false" "1"

cat > "$TMPDIR/string_io.pgy" <<EOF
func Main() -> Void {
    let s: String = Concat(Upper(Trim("  hi")), Lower(" THERE"));
    Log(StringLength(s));
    Log(Contains(s, "HI"));
    WriteFile("io.txt", Replace(s, "HI", "BYE"));
    Log(FileExists("io.txt"));
    let out: String = ReadFile("io.txt");
    Log(out);
}
EOF
run_case "string_io" "$TMPDIR/string_io.pgy" "8" "true" "true" "BYE there"

cat > "$TMPDIR/math.pgy" <<'EOF'
namespace Math {
    func HiddenAdd(a: Int, b: Int) -> Int { return a + b; }
    export func Add(a: Int, b: Int) -> Int { return HiddenAdd(a, b); }
}
EOF
cat > "$TMPDIR/module_main.pgy" <<'EOF'
import "math.pgy";
func Main() -> Void {
    Log(Math.Add(2, 5));
}
EOF
run_case "namespace_export_import" "$TMPDIR/module_main.pgy" "7"

cat > "$TMPDIR/operator_overload.pgy" <<'EOF'
struct Vec2 {
    x: Int;
    y: Int;
}

func operator_add_Vec2(a: Vec2, b: Vec2) -> Vec2 {
    return a;
}

func Main() -> Void {
    let a: Vec2 = Vec2(1, 2);
    let b: Vec2 = Vec2(3, 4);
    let c: Vec2 = a + b;
    Log(c.x);
}
EOF
run_case "operator_overload" "$TMPDIR/operator_overload.pgy" "1"

cat > "$TMPDIR/role_operator_overload.pgy" <<'EOF'
ability Arithmetic {
    func Add(other: Int) -> Int;
}

role IntMath for Int {
    impl ability Arithmetic {
        func Add(other: Int) -> Int {
            return 123;
        }
    }
}

func Main() -> Void {
    let a: Int = 1;
    let b: Int = 2;
    Log(a + b);
}
EOF
run_case "role_operator_overload" "$TMPDIR/role_operator_overload.pgy" "123"

cat > "$TMPDIR/qubit_slot.pgy" <<'EOF'
func Main() -> Void {
    let q: QubitSlot = ClaimQubit();
    Log(QubitState(q));
    let first: Int = Measure(q);
    let second: Int = Measure(q);
    Log(first);
    Log(second);

    let a: QubitSlot = ClaimQubit();
    let b: QubitSlot = ClaimQubit();
    Entangle(a, b);
    let ra: Int = Measure(a);
    let rb: Int = Measure(b);
    Log(ra);
    Log(rb);

    ReleaseQubit(q);
    ReleaseQubit(a);
    ReleaseQubit(b);
}
EOF
run_case "qubit_slot" "$TMPDIR/qubit_slot.pgy" "2"

cat > "$TMPDIR/device_slot_remote.pgy" <<'EOF'
async func Main() -> Void {
    let dev: DeviceSlot<Int> = ClaimDeviceSlot();
    DeviceWrite(dev, 11);
    let pending: RemoteFuture<Int> = SubmitDeviceRead(dev);
    let value: Int = (await pending)?;
    ReleaseDeviceSlot(dev);
    Log(value);
}
EOF
run_case "device_slot_remote" "$TMPDIR/device_slot_remote.pgy" "11"

cat > "$TMPDIR/async_func_decl.pgy" <<'EOF'
async func Inc(x: Int) -> Int {
    return x + 1;
}

async func Main() -> Void {
    let f: Future<Int> = spawn Inc(4);
    let v: Int = await f;
    Log(v);
}
EOF
run_case "async_func_decl" "$TMPDIR/async_func_decl.pgy" "5"

cat > "$TMPDIR/secure_slot_view.pgy" <<'EOF'
func Main() -> Void {
    let rs: SecureSlot<Int> = ClaimSecureSlot();
    let ws: SecureSlot<Int> = ClaimSecureSlot();
    Write(rs, 5, rs_token);
    pin rs as rv: ReadView<Int> {
        Log(Read(rv));
    }
    pin ws as wv: WriteView<Int> {
        Write(wv, 5);
        Write(wv, 9);
    }
    Log(Read(ws, ws_token));
}
EOF
run_case "secure_slot_view" "$TMPDIR/secure_slot_view.pgy" "5" "9"

cat > "$TMPDIR/subject_projection.pgy" <<'EOF'
subject Player {
    let hp: Int;
    let name: String;
}

tobject PlayerDto {
    hp: Int;
    name: String;
}

object PlayerView {
    hp: Int;
}

func Main() -> Void {
    let player: Player = Player(42, "neo");
    let snapshot: PlayerDto = ToTObject(PlayerDto, player);
    let view: PlayerView = ToObject(PlayerView, player);
    Log(snapshot.hp);
    Log(view.hp);
    Log(snapshot.name);
}
EOF
run_case "subject_projection" "$TMPDIR/subject_projection.pgy" "42" "neo"

cat > "$TMPDIR/subject_method_recursion_defer.pgy" <<'EOF'
subject Counter {
    let ticks: Int;

    func Sum(self, n: Int) -> Int {
        if n <= 0 { return 0; }
        defer {
            ticks = ticks + 1;
        };
        return n + self.Sum(n - 1);
    }
}

func Main() -> Void {
    let c: Counter = Counter(0);
    Log(c.Sum(4));
    Log(c.ticks);
}
EOF
run_case "subject_method_recursion_defer" "$TMPDIR/subject_method_recursion_defer.pgy" "10" "4"

cat > "$TMPDIR/intent_failure_result.pgy" <<'EOF'
subject Driver {
    let hp: Int;
    let started: Bool;

    action Ignite(self) -> Void {
        started = true;
        hp = hp + 1;
    }

    action RollbackIgnite(self) -> Void {
        started = false;
        hp = hp - 1;
    }
}

zone CockpitZone {
    subject slot driver: Driver
}

intent DriveCar(cockpit: CockpitZone, driver: Driver) {
    step Ignite {
        where: CockpitZone;
        using: cockpit;
        who: driver;
        on: driver.Ignite();
        compensate: driver.RollbackIgnite();
        guard: false;
        post: driver.started;
    }

    success: true;
    failure: true;
}

func Main() -> Void {
    let d = Driver(0, false);
    let cockpit = CockpitZone(Driver(99, true));
    Log(DriveCar(cockpit, d));
    Log(IntentLastFailed());
}
EOF
run_case "intent_failure_result" "$TMPDIR/intent_failure_result.pgy" "false" "true"

cat > "$TMPDIR/relation_effect_projection_sync.pgy" <<'EOF'
subject Player {
    let hp: Int;
    let name: String;
}

object PlayerView {
    hp: Int;
}

tobject PlayerDto {
    hp: Int;
    name: String;
}

relation TrustedLink for source: Player, target: Player {
    object slot snapshot: PlayerView
    tobject slot packet: PlayerDto
    refresh snapshot from source
    publish packet from target

    func Show(self) -> Void {
        Log(HasProjection(snapshot));
        Log(HasProjection(packet));
        Log(self.snapshot.hp);
        Log(self.packet.name);
    }
}

effect Poisoned for bearer: Player {
    object slot view: PlayerView
    tobject slot packet: PlayerDto
    refresh view from bearer
    publish packet from bearer

    func Show(self) -> Void {
        Log(HasProjection(view));
        Log(HasProjection(packet));
        Log(self.view.hp);
        Log(self.packet.name);
    }
}

func Main() -> Void {
    let trust = TrustedLink(Player(7, "src"), Player(9, "dst"));
    let poison = Poisoned(Player(5, "bear"));
    trust.Show();
    poison.Show();
}
EOF
run_case "relation_effect_projection_sync" "$TMPDIR/relation_effect_projection_sync.pgy" "true" "true" "7" "dst" "true" "true" "5" "bear"

cat > "$TMPDIR/zone_has_layer.pgy" <<'EOF'
subject Player {
    let hp: Int;
}

object PlayerView {
    hp: Int;
}

tobject PlayerDto {
    hp: Int;
}

effect Poisoned for bearer: Player { }
relation TrustedLink for source: Player, target: Player { }

zone BattleZone {
    subject slot player: Player
    subject slot enemy: Player
    object slot playerView: PlayerView
    tobject slot snapshot: PlayerDto
    effect slot poison: Poisoned
    relation slot trust: TrustedLink
    state poisoned: effect poison on player
    state allied: relation trust between player, enemy
    refresh playerView from player
    publish snapshot from player
    maintain poisoned
    maintain allied

    func Show() -> Void {
        Log(HasProjection(playerView));
        Log(HasProjection(snapshot));
        Log(HasLayer(poison));
        Log(HasLayer("trust"));
        Log(HasState(poisoned));
        Log(HasState(allied));
    }
}

func Main() -> Void {
    let battle = BattleZone(Player(7), Player(9));
    battle.Show();
}
EOF
run_case "zone_has_layer" "$TMPDIR/zone_has_layer.pgy" "true" "true" "true" "true" "true" "true"

cat > "$TMPDIR/zone_action_effect_runtime.pgy" <<'EOF'
subject Player {
    let hp: Int;

    action Attack(self) -> Void
        within BattleZone
        causes Poisoned
        authorized by self {
        hp = hp - 1;
    }
}

object PlayerView {
    hp: Int;
}

effect Poisoned for bearer: Player {
    object slot view: PlayerView
    refresh view from bearer
}

zone BattleZone {
    subject slot player: Player
    effect slot poison: Poisoned
    authority player

    func Tick(self) -> Void {
        self.player.Attack();
        Log(HasLayer(poison));
        Log(self.poison.view.hp);
    }
}

func Main() -> Void {
    let battle = BattleZone(Player(7));
    battle.Tick();
}
EOF
run_case "zone_action_effect_runtime" "$TMPDIR/zone_action_effect_runtime.pgy" "true" "6"

cat > "$TMPDIR/zone_layer_projection_runtime.pgy" <<'EOF'
subject Player {
    let hp: Int;
    let name: String;
}

object PlayerView {
    hp: Int;
}

tobject PlayerDto {
    name: String;
}

effect Poisoned for bearer: Player {
    object slot view: PlayerView
    refresh view from bearer
}

relation TrustedLink for source: Player, target: Player {
    tobject slot packet: PlayerDto
    publish packet from target
}

zone BattleZone {
    subject slot player: Player
    subject slot enemy: Player
    effect slot poison: Poisoned
    relation slot trust: TrustedLink
    apply poison to player
    link trust between player, enemy

    func Show(self) -> Void {
        Log(self.poison.view.hp);
        Log(self.trust.packet.name);
    }
}

func Main() -> Void {
    let battle = BattleZone(Player(7, "src"), Player(9, "dst"));
    battle.Show();
}
EOF
run_case "zone_layer_projection_runtime" "$TMPDIR/zone_layer_projection_runtime.pgy" "7" "dst"

cat > "$TMPDIR/world_zone_cross_queries.pgy" <<'EOF'
subject Player {
    let hp: Int;
}

object PlayerView {
    hp: Int;
}

effect Poisoned for bearer: Player { }

zone BattleZone {
    subject slot player: Player
    object slot playerView: PlayerView
    effect slot poison: Poisoned
    state poisoned: effect poison on player
    refresh playerView from player
    maintain poisoned
}

world GameWorld {
    zone battle: BattleZone

    func Show(self) -> Void {
        Log(HasZoneProjection(battle, playerView));
        Log(HasZoneLayer(battle, poison));
        Log(HasZoneState(battle, poisoned));
    }
}

func Main() -> Void {
    let battle = BattleZone(Player(7));
    let gameWorld = GameWorld(Clone(battle));
    gameWorld.Show();
}
EOF
run_case "world_zone_cross_queries" "$TMPDIR/world_zone_cross_queries.pgy" "true" "true" "true"

cat > "$TMPDIR/world_derived_states.pgy" <<'EOF'
subject Player { let hp: Int; }
object PlayerView { hp: Int; }
effect Poisoned for bearer: Player { }

zone BattleZone {
    subject slot player: Player
    object slot playerView: PlayerView
    effect slot poison: Poisoned
    state poisoned: effect poison on player
    refresh playerView from player
    maintain poisoned
}

world GameWorld {
    zone battle: BattleZone
    state battleProjected: zone battle projection playerView
    state battleLayered: zone battle layer poison
    state battlePoisoned: zone battle state poisoned
    activate battle

    func Show(self) -> Void {
        Log(HasZone(battleProjected));
        Log(HasZone(battleLayered));
        Log(HasZone(battlePoisoned));
    }
}

func Main() -> Void {
    let battle = BattleZone(Player(7));
    let gameWorld = GameWorld(Clone(battle));
    gameWorld.Show();
}
EOF
run_case "world_derived_states" "$TMPDIR/world_derived_states.pgy" "true" "true" "true"

cat > "$TMPDIR/world_composed_states.pgy" <<'EOF'
zone BattleZone { }

world GameWorld {
    zone battle: BattleZone
    zone camp: BattleZone
    state battleLive: zone battle
    state campLive: zone camp
    state allLive: all battleLive, campLive
    state anyLive: any allLive, campLive
    activate battle
    maintain camp

    func Show(self) -> Void {
        Log(HasZone(allLive));
        Log(HasZone(anyLive));
    }
}

func Main() -> Void {
    let gameWorld = GameWorld(BattleZone(), BattleZone());
    gameWorld.Show();
}
EOF
run_case "world_composed_states" "$TMPDIR/world_composed_states.pgy" "true" "true"

cat > "$TMPDIR/world_zone_mutation_dirty.pgy" <<'EOF'
subject Player { let hp: Int; }
object PlayerView { hp: Int; }

zone BattleZone {
    subject slot player: Player
    object slot playerView: PlayerView
    refresh playerView from player
}

world GameWorld {
    zone battle: BattleZone
    state battleProjected: zone battle projection playerView
    activate battle

    func Mutate(self, hp: Int) -> Void {
        self.battle = BattleZone(Player(hp));
    }

    func Show(self) -> Void {
        Log(HasZone(battleProjected));
        Log(self.battle.playerView.hp);
    }
}

func Main() -> Void {
    let gameWorld = GameWorld(BattleZone(Player(7)));
    gameWorld.Show();
    gameWorld.Mutate(9);
    gameWorld.Show();
}
EOF
run_case "world_zone_mutation_dirty" "$TMPDIR/world_zone_mutation_dirty.pgy" "true" "7" "true" "9"

cat > "$TMPDIR/world_nested_member_assign.pgy" <<'EOF'
subject Player { let hp: Int; }
object PlayerView { hp: Int; }

zone BattleZone {
    subject slot player: Player
    object slot playerView: PlayerView
    refresh playerView from player
}

world GameWorld {
    zone battle: BattleZone
    state battleProjected: zone battle projection playerView
    activate battle

    func Mutate(self, hp: Int) -> Void {
        self.battle.player.hp = hp;
    }

    func Show(self) -> Void {
        Log(HasZone(battleProjected));
        Log(self.battle.playerView.hp);
    }
}

func Main() -> Void {
    let gameWorld = GameWorld(BattleZone(Player(7)));
    gameWorld.Show();
    gameWorld.Mutate(9);
    gameWorld.Show();
}
EOF
run_case "world_nested_member_assign" "$TMPDIR/world_nested_member_assign.pgy" "true" "7" "true" "9"

cat > "$TMPDIR/object_layer_binding.pgy" <<'EOF'
object Door { hp: Int; }
object DoorView { hp: Int; }
object Key { id: Int; }
object KeyView { id: Int; }

effect Highlighted for object target: Door {
    object slot view: DoorView
    refresh view from target
}

relation KeyBinding for object door: Door, object key: Key {
    object slot snapshot: KeyView
    refresh snapshot from key
}

zone LockZone {
    binding slot door: Door
    binding slot key: Key
    effect slot glow: Highlighted
    relation slot binding: KeyBinding
    apply glow to door
    link binding between door, key

    func Show(self) -> Void {
        Log(HasLayer(glow));
        Log(HasLayer(binding));
        Log(self.glow.view.hp);
        Log(self.binding.snapshot.id);
    }
}

func Main() -> Void {
    let lock: LockZone = LockZone(Door(5), Key(9));
    lock.Show();
}
EOF
run_case "object_layer_binding" "$TMPDIR/object_layer_binding.pgy" "true" "true" "5" "9"

cat > "$TMPDIR/subject_class_dispatch.pgy" <<'EOF'
subject ActiveCounter {
    let count: Int;

    action Tick(self, delta: Int) -> Int {
        count = count + delta;
        return count;
    }
}

class PassiveCounter {
    let count: Int;

    func Tick(self, delta: Int) -> Int {
        count = count + delta;
        return count;
    }
}

func Main() -> Void {
    let active: ActiveCounter = ActiveCounter(1);
    let passive: PassiveCounter = PassiveCounter(1);
    Log(active.Tick(2));
    Log(active.count);
    Log(passive.Tick(2));
    Log(passive.count);
}
EOF
run_case "subject_class_dispatch" "$TMPDIR/subject_class_dispatch.pgy" "3" "1"

cat > "$TMPDIR/slot_subject_cell.pgy" <<'EOF'
subject Vec2 {
    let x: Int;
    let y: Int;
}

func Main() -> Void {
    let s: Slot<Vec2> = Vec2(3, 7);
    Write(s, Vec2(1, 2));
    Release(s);
    Log(1);
}
EOF
run_case "slot_subject_cell" "$TMPDIR/slot_subject_cell.pgy" "1"

cat > "$TMPDIR/secure_slot_subject_cell.pgy" <<'EOF'
subject Vec2 {
    let x: Int;
    let y: Int;
}

func Main() -> Void {
    let s: SecureSlot<Vec2> = Vec2(3, 7);
    Write(s, Vec2(1, 2), s_token);
    Release(s, s_token);
    Log(1);
}
EOF
run_case "secure_slot_subject_cell" "$TMPDIR/secure_slot_subject_cell.pgy" "1"

cat > "$TMPDIR/secure_slot_subject_bot.pgy" <<'EOF'
subject Bot {
    let hp: Int;
}

func Main() -> Void {
    let s: SecureSlot<Bot> = Bot(7);
    Write(s, Bot(9), s_token);
    Release(s, s_token);
    Log(1);
}
EOF
run_case "secure_slot_subject_bot" "$TMPDIR/secure_slot_subject_bot.pgy" "1"

cat > "$TMPDIR/slot_subject_boundary_ref.pgy" <<'EOF'
subject Vec2 {
    let x: Int;
    let y: Int;
}

func Touch(ref s: Slot<Vec2>) -> Void {
    Write(s, Vec2(1, 2));
}

func Main() -> Void {
    let s: Slot<Vec2> = Vec2(3, 7);
    Touch(s);
    Release(s);
    Log(1);
}
EOF
run_case "slot_subject_boundary_ref" "$TMPDIR/slot_subject_boundary_ref.pgy" "1"

cat > "$TMPDIR/secure_slot_subject_boundary_own.pgy" <<'EOF'
subject Vec2 {
    let x: Int;
    let y: Int;
}

func Consume(own s: SecureSlot<Vec2>) -> Void {
    Write(s, Vec2(1, 2), s_token);
    Release(s, s_token);
}

func Main() -> Void {
    let s: SecureSlot<Vec2> = Vec2(3, 7);
    Consume(s);
    Log(1);
}
EOF
run_case "secure_slot_subject_boundary_own" "$TMPDIR/secure_slot_subject_boundary_own.pgy" "1"

cat > "$TMPDIR/secure_slot_subject_boundary_forward_own.pgy" <<'EOF'
subject Vec2 {
    let x: Int;
    let y: Int;
}

func ConsumeInner(own s: SecureSlot<Vec2>) -> Void {
    Write(s, Vec2(1, 2), s_token);
    Release(s, s_token);
}

func ConsumeOuter(own s: SecureSlot<Vec2>) -> Void {
    ConsumeInner(s);
}

func Main() -> Void {
    let s: SecureSlot<Vec2> = Vec2(3, 7);
    ConsumeOuter(s);
    Log(1);
}
EOF
run_case "secure_slot_subject_boundary_forward_own" "$TMPDIR/secure_slot_subject_boundary_forward_own.pgy" "1"

cat > "$TMPDIR/select_ready.pgy" <<'EOF'
func Main() -> Void {
    let ch: Channel<Int> = Channel(4);
    parallel {
        ch <- 7;
    }
    select {
        case v = <-ch:
            Log(v);
        default:
            Log(0);
    }
}
EOF
run_case "select_ready" "$TMPDIR/select_ready.pgy" "7"

cat > "$TMPDIR/select_fairness.pgy" <<'EOF'
func Main() -> Void {
    let a: Channel<Int> = Channel(4);
    let b: Channel<Int> = Channel(4);
    a <- 1;
    a <- 3;
    b <- 2;
    b <- 4;

    for i in 0..4 {
        select {
            case v = <-a:
                Log(v);
            case v = <-b:
                Log(v);
            default:
                Log(0);
        }
    }
}
EOF
run_case "select_fairness" "$TMPDIR/select_fairness.pgy" "1" "2" "3" "4"

cat > "$TMPDIR/cancel_future.pgy" <<'EOF'
func Worker() -> Int {
    return 1;
}

func Main() -> Void {
    let pending: Future<Int> = spawn Worker();
    let cancelled: Bool = Cancel(pending);
    Log(cancelled);
}
EOF
run_case "cancel_future" "$TMPDIR/cancel_future.pgy" "true"

cat > "$TMPDIR/cancel_propagation.pgy" <<'EOF'
func Child() -> Int {
    if (IsCancelled()) {
        return 9;
    }
    return 0;
}

async func Parent() -> Int {
    let child: Future<Int> = spawn Child();
    let value: Int = await child;
    return value;
}

async func Main() -> Void {
    let parent: Future<Int> = spawn Parent();
    let cancelled: Bool = Cancel(parent);
    Log(cancelled);
    let value: Int = await parent;
    Log(value);
}
EOF
run_case "cancel_propagation" "$TMPDIR/cancel_propagation.pgy" "true" "9"

cat > "$TMPDIR/channel_pressure.pgy" <<'EOF'
func Main() -> Void {
    let ch: Channel<Int> = Channel(2);
    Log(ChannelCapacity(ch));
    Log(ChannelLength(ch));
    Log(ChannelSpace(ch));
    Log(ChannelFull(ch));
    Log(ChannelClosed(ch));
    ch <- 7;
    Log(ChannelLength(ch));
    Log(ChannelSpace(ch));
    Log(ChannelFull(ch));
    ch <- 8;
    Log(ChannelLength(ch));
    Log(ChannelSpace(ch));
    Log(ChannelFull(ch));
    ChannelClose(ch);
    Log(ChannelClosed(ch));
}
EOF
run_case "channel_pressure" "$TMPDIR/channel_pressure.pgy" "2" "0" "2" "false" "false" "1" "1" "false" "2" "0" "true" "true"

cat > "$TMPDIR/async_block_runtime.pgy" <<'EOF'
func Main() -> Void {
    async {
    }
    Log(11);
}
EOF
run_case "async_block_runtime" "$TMPDIR/async_block_runtime.pgy" "11"

cat > "$TMPDIR/defer_scope_exit.pgy" <<'EOF'
func Main() -> Void {
    defer { Log(1); };
    Log(2);
}
EOF
run_case "defer_scope_exit" "$TMPDIR/defer_scope_exit.pgy" "2" "1"

cat > "$TMPDIR/branch_defer_scope.pgy" <<'EOF'
func Main() -> Void {
    if true {
        defer { Log(1); };
    }
    Log(2);
}
EOF
run_case "branch_defer_scope" "$TMPDIR/branch_defer_scope.pgy" "2" "1"

cat > "$TMPDIR/branch_defer_skipped.pgy" <<'EOF'
func Main() -> Void {
    if false {
        defer { Log(1); };
    }
    Log(2);
}
EOF
run_case "branch_defer_skipped" "$TMPDIR/branch_defer_skipped.pgy" "2"

cat > "$TMPDIR/loop_defer_break_current.pgy" <<'EOF'
func Main() -> Void {
    while true {
        defer { Log(1); };
        break;
    }
    Log(2);
}
EOF
run_case "loop_defer_break_current" "$TMPDIR/loop_defer_break_current.pgy" "2" "1"

cat > "$TMPDIR/generic_call.pgy" <<'EOF'
func Identity<T>(x: T) -> T {
    return x;
}

func Main() -> Void {
    let a: Int = Identity(42);
    Log(a);
}
EOF
run_case "generic_call" "$TMPDIR/generic_call.pgy" "42"

cat > "$TMPDIR/generic_spawn.pgy" <<'EOF'
func Identity<T>(x: T) -> T {
    return x;
}

async func Main() -> Void {
    let task = spawn Identity(42);
    let value: Int = await task;
    Log(value);
}
EOF
run_case "generic_spawn" "$TMPDIR/generic_spawn.pgy" "42"

cat > "$TMPDIR/generic_spawn_multi.pgy" <<'EOF'
func PickSecond<T>(left: T, right: T) -> T {
    return right;
}

async func Main() -> Void {
    let task = spawn PickSecond(10, 77);
    let value: Int = await task;
    Log(value);
}
EOF
run_case "generic_spawn_multi" "$TMPDIR/generic_spawn_multi.pgy" "77"

cat > "$TMPDIR/future_annotation.pgy" <<'EOF'
func PairInt(x: Int, y: Int) -> Int { return y; }
async func Main() -> Void {
    let task: Future<Int> = spawn PairInt(10, 77);
    let value: Int = await task;
    Log(value);
}
EOF
run_case "future_annotation" "$TMPDIR/future_annotation.pgy" "77"

cat > "$TMPDIR/string_spawn.pgy" <<'EOF'
func Echo<T>(x: T) -> T { return x; }
async func Main() -> Void {
    let task = spawn Echo("hi");
    let value: String = await task;
    Log(value);
}
EOF
run_case "string_spawn" "$TMPDIR/string_spawn.pgy" "hi"

cat > "$TMPDIR/zone_effect_pool_runtime.pgy" <<'EOF'
subject Player {
    let hp: Int;
}

effect DamageEffect for bearer: Player { }

zone BattleZone {
    subject slot player: Player
    effect pool damage: DamageEffect capacity 4
    apply damage to player

    func Show() -> Void {
        Log(HasLayer(damage));
    }
}

func Main() -> Void {
    let battle = BattleZone(Player(7));
    battle.Show();
}
EOF
run_case "zone_effect_pool_runtime" "$TMPDIR/zone_effect_pool_runtime.pgy" "true"

# ---------------------------------------------------------------------------
# Arithmetic, comparison, logical operators
# ---------------------------------------------------------------------------
cat > "$TMPDIR/operators.pgy" <<'EOF'
func Main() -> Void {
    let a: Int = 10;
    let b: Int = 3;
    Log(a + b);
    Log(a - b);
    Log(a * b);
    Log(a / b);
    Log(a % b);
    Log(a > b);
    Log(a < b);
    Log(a == b);
    Log(a != b);
    Log(a >= 10);
    Log(b <= 3);
    let t: Bool = true;
    let f: Bool = false;
    Log(t && f);
    Log(t || f);
    Log(!f);
    Log(-5);
}
EOF
run_case "operators" "$TMPDIR/operators.pgy" "13" "7" "30" "3" "1" "true" "false" "false" "true" "true" "true" "false" "true" "true" "-5"

# ---------------------------------------------------------------------------
# While loop
# ---------------------------------------------------------------------------
cat > "$TMPDIR/while_loop.pgy" <<'EOF'
func Main() -> Void {
    let i: Int = 0;
    let sum: Int = 0;
    while i < 5 {
        sum = sum + i;
        i = i + 1;
    }
    Log(sum);
}
EOF
run_case "while_loop" "$TMPDIR/while_loop.pgy" "10"

# ---------------------------------------------------------------------------
# If / else if / else
# ---------------------------------------------------------------------------
cat > "$TMPDIR/if_else_chain.pgy" <<'EOF'
func Classify(x: Int) -> String {
    if x < 0 {
        return "negative";
    } else if x == 0 {
        return "zero";
    } else {
        return "positive";
    }
}

func Main() -> Void {
    Log(Classify(-3));
    Log(Classify(0));
    Log(Classify(7));
}
EOF
run_case "if_else_chain" "$TMPDIR/if_else_chain.pgy" "negative" "zero" "positive"

# ---------------------------------------------------------------------------
# Match statement
# ---------------------------------------------------------------------------
cat > "$TMPDIR/match_stmt.pgy" <<'EOF'
func Describe(x: Int) -> String {
    match x {
        case 1: return "one";
        case 2: return "two";
        case 3: return "three";
        default: return "other";
    }
}

func Main() -> Void {
    Log(Describe(1));
    Log(Describe(2));
    Log(Describe(3));
    Log(Describe(99));
}
EOF
run_case "match_stmt" "$TMPDIR/match_stmt.pgy" "one" "two" "three" "other"

# ---------------------------------------------------------------------------
# Recursive function
# ---------------------------------------------------------------------------
cat > "$TMPDIR/recursion.pgy" <<'EOF'
func Factorial(n: Int) -> Int {
    if n <= 1 { return 1; }
    return n * Factorial(n - 1);
}

func Main() -> Void {
    Log(Factorial(5));
    Log(Factorial(1));
    Log(Factorial(0));
}
EOF
run_case "recursion" "$TMPDIR/recursion.pgy" "120" "1" "1"

# ---------------------------------------------------------------------------
# Nested function calls
# ---------------------------------------------------------------------------
cat > "$TMPDIR/nested_calls.pgy" <<'EOF'
func Add(a: Int, b: Int) -> Int { return a + b; }
func Mul(a: Int, b: Int) -> Int { return a * b; }

func Main() -> Void {
    Log(Add(Mul(2, 3), Mul(4, 5)));
}
EOF
run_case "nested_calls" "$TMPDIR/nested_calls.pgy" "26"

# ---------------------------------------------------------------------------
# Function routine exact lookup before prefix specialization fallback
# ---------------------------------------------------------------------------
cat > "$TMPDIR/function_prefix_exact_lookup.pgy" <<'EOF'
func Pick_Value() -> Int { return 99; }
func Pick() -> Int { return 7; }

func Main() -> Void {
    Log(Pick());
    Log(Pick_Value());
}
EOF
run_case "function_routine_prefix_collision" "$TMPDIR/function_prefix_exact_lookup.pgy" "7" "99"

# ---------------------------------------------------------------------------
# Hosted method routine exact lookup before prefix specialization fallback
# ---------------------------------------------------------------------------
cat > "$TMPDIR/method_prefix_exact_lookup.pgy" <<'EOF'
class Box {
    let value: Int;

    func Get_Value(self) -> Int {
        return 99;
    }

    func Get(self) -> Int {
        return value;
    }
}

func Main() -> Void {
    let box = Box(7);
    Log(box.Get());
    Log(box.Get_Value());
}
EOF
run_case "method_routine_prefix_collision" "$TMPDIR/method_prefix_exact_lookup.pgy" "7" "99"

# ---------------------------------------------------------------------------
# String concatenation
# ---------------------------------------------------------------------------
cat > "$TMPDIR/string_concat.pgy" <<'EOF'
func Main() -> Void {
    let a: String = "hello";
    let b: String = " world";
    let c: String = Concat(a, b);
    Log(c);
    Log(StringLength(c));
}
EOF
run_case "string_concat" "$TMPDIR/string_concat.pgy" "hello world" "11"

# ---------------------------------------------------------------------------
# List generic collection
# ---------------------------------------------------------------------------
cat > "$TMPDIR/list_ops.pgy" <<'EOF'
func Main() -> Void {
    let items: List<Int> = ListNew();
    ListPush(items, 10);
    ListPush(items, 20);
    ListPush(items, 30);
    Log(ListSize(items));
    Log(ListGet(items, 0));
    Log(ListGet(items, 2));
    ListSet(items, 1, 99);
    Log(ListGet(items, 1));
    ListRemove(items, 0);
    Log(ListSize(items));
    Log(ListGet(items, 0));
}
EOF
run_case "list_ops" "$TMPDIR/list_ops.pgy" "3" "10" "30" "99" "2" "99"

# ---------------------------------------------------------------------------
# Queue generic collection
# ---------------------------------------------------------------------------
cat > "$TMPDIR/queue_ops.pgy" <<'EOF'
func Main() -> Void {
    let q: Queue<Int> = QueueNew();
    Log(QueueEmpty(q));
    QueuePush(q, 1);
    QueuePush(q, 2);
    QueuePush(q, 3);
    Log(QueueSize(q));
    Log(QueueEmpty(q));
    let first: Int = QueuePop(q);
    Log(first);
    Log(QueueSize(q));
}
EOF
run_case "queue_ops" "$TMPDIR/queue_ops.pgy" "true" "3" "false" "1" "2"

# ---------------------------------------------------------------------------
# Map generic collection
# ---------------------------------------------------------------------------
cat > "$TMPDIR/map_ops.pgy" <<'EOF'
func Main() -> Void {
    let m: HashMap<String, Int> = MapNew();
    MapSet(m, "a", 1);
    MapSet(m, "b", 2);
    Log(MapSize(m));
    Log(MapHas(m, "a"));
    Log(MapHas(m, "c"));
    Log(MapGet(m, "b"));
    MapRemove(m, "a");
    Log(MapSize(m));
    Log(MapHas(m, "a"));
}
EOF
run_case "map_ops" "$TMPDIR/map_ops.pgy" "2" "true" "false" "2" "1" "false"

cat > "$TMPDIR/map_ops_long_bool.pgy" <<'EOF'
func Main() -> Void {
    let longMap: HashMap<Long, Long> = MapNew();
    MapSet(longMap, 7L, 70L);
    MapSet(longMap, 9L, 90L);
    Log(MapGet(longMap, 7L));
    Log(MapHas(longMap, 9L));
    Log(ArrayLength(MapKeys(longMap)));

    let boolMap: HashMap<Bool, Int> = MapNew();
    MapSet(boolMap, true, 11);
    MapSet(boolMap, false, 22);
    Log(MapGet(boolMap, true));
    Log(MapHas(boolMap, false));
    Log(ArrayLength(MapKeys(boolMap)));
}
EOF
run_case "map_ops_long_bool" "$TMPDIR/map_ops_long_bool.pgy" "70" "true" "2" "11" "true" "2"

# ---------------------------------------------------------------------------
# Lambda expression
# ---------------------------------------------------------------------------
cat > "$TMPDIR/lambda_expr.pgy" <<'EOF'
func Apply(f: func(Int) -> Int, x: Int) -> Int {
    return f(x);
}

func Main() -> Void {
    let double: func(Int) -> Int = (x: Int) => x * 2;
    Log(Apply(double, 5));
    Log(Apply((x: Int) => x + 10, 3));
}
EOF
run_case "lambda_expr" "$TMPDIR/lambda_expr.pgy" "10" "13"

# ---------------------------------------------------------------------------
# Multiple return types (Int, Bool, String, Float)
# ---------------------------------------------------------------------------
cat > "$TMPDIR/multi_types.pgy" <<'EOF'
func GetInt() -> Int { return 42; }
func GetBool() -> Bool { return true; }
func GetString() -> String { return "ok"; }

func Main() -> Void {
    Log(GetInt());
    Log(GetBool());
    Log(GetString());
}
EOF
run_case "multi_types" "$TMPDIR/multi_types.pgy" "42" "true" "ok"

# ---------------------------------------------------------------------------
# Event system (subscribe, invoke)
# ---------------------------------------------------------------------------
cat > "$TMPDIR/event_system.pgy" <<'EOF'
event OnDamage(amount: Int);

func HandleDamage(amount: Int) -> Void {
    Log(amount);
}

func Main() -> Void {
    OnDamage += HandleDamage;
    OnDamage(25);
    OnDamage -= HandleDamage;
    OnDamage(99);
}
EOF
run_case "event_system" "$TMPDIR/event_system.pgy" "25"

# ---------------------------------------------------------------------------
# Party with role and bind
# ---------------------------------------------------------------------------
cat > "$TMPDIR/party_role_bind.pgy" <<'EOF'
ability Speakable {
    func Greet() -> String;
}

subject Host {
    let name: String;
}

role Greeter for Host {
    impl ability Speakable {
        func Greet() -> String {
            return "hello";
        }
    }
}

party Speaker {
    role slot speaker: Speakable
    shared round: Int = 1
}

func Main() -> Void {
    let s: Speaker = Speaker();
    Log(s.round);
}
EOF
run_case "party_role_bind" "$TMPDIR/party_role_bind.pgy" "1"

# ---------------------------------------------------------------------------
# For loop with range and nested loops
# ---------------------------------------------------------------------------
cat > "$TMPDIR/nested_loops.pgy" <<'EOF'
func Main() -> Void {
    let total: Int = 0;
    for i in 0..3 {
        for j in 0..3 {
            total = total + 1;
        }
    }
    Log(total);
}
EOF
run_case "nested_loops" "$TMPDIR/nested_loops.pgy" "9"

# ---------------------------------------------------------------------------
# Slot basic ops (claim, write, read, release)
# ---------------------------------------------------------------------------
cat > "$TMPDIR/slot_basic.pgy" <<'EOF'
func Main() -> Void {
    let s: Slot<Int> = ClaimSlot();
    Write(s, 42);
    let v: Int = Read(s);
    Log(v);
    Write(s, 99);
    Log(Read(s));
    Release(s);
}
EOF
run_case "slot_basic" "$TMPDIR/slot_basic.pgy" "42" "99"

# ---------------------------------------------------------------------------
# Channel send/recv inline
# ---------------------------------------------------------------------------
cat > "$TMPDIR/channel_basic.pgy" <<'EOF'
func Main() -> Void {
    let ch: Channel<String> = Channel(2);
    ch <- "ping";
    ch <- "pong";
    let a: String = <-ch;
    let b: String = <-ch;
    Log(a);
    Log(b);
}
EOF
run_case "channel_basic" "$TMPDIR/channel_basic.pgy" "ping" "pong"

cat > "$TMPDIR/channel_field_constructor_reject.pgy" <<'EOF'
class ChannelBox {
    let ch: Channel<Int>;
}

func Main() -> Void {
    let ch: Channel<Int> = Channel(2);
    let box: ChannelBox = ChannelBox(ch);
}
EOF
run_compile_fails_case "channel_field_constructor_reject" \
    "$TMPDIR/channel_field_constructor_reject.pgy" \
    "cannot aggregate-construct or default-initialize Channel<T> field 'ch'"

cat > "$TMPDIR/channel_field_default_constructor_reject.pgy" <<'EOF'
class ChannelBox {
    let ch: Channel<Int>;
}

func Main() -> Void {
    let box: ChannelBox = ChannelBox();
}
EOF
run_compile_fails_case "channel_field_default_constructor_reject" \
    "$TMPDIR/channel_field_default_constructor_reject.pgy" \
    "default-initialize Channel<T> field 'ch'"

# ---------------------------------------------------------------------------
# Extern function
# ---------------------------------------------------------------------------
cat > "$TMPDIR/extern_fn.pgy" <<'EOF'
extern "c" {
    func pgy_now_ms() -> Int;
}

func Main() -> Void {
    let ignored = pgy_now_ms();
    Log(1);
}
EOF
run_case "extern_fn" "$TMPDIR/extern_fn.pgy" "1"

cat > "$TMPDIR/extern_spawn.pgy" <<'EOF'
extern "c" {
    func pgy_checked_div_i32_export(lhs: Int, rhs: Int) -> Int;
}

async func Main() -> Void {
    let task: Future<Int> = spawn pgy_checked_div_i32_export(42, 7);
    let value: Int = await task;
    Log(value);
}
EOF
run_case "extern_spawn" "$TMPDIR/extern_spawn.pgy" "6"

echo ""
echo "[llvm-smoke] all tests passed"
