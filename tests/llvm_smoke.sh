#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PGY="${PGY_BIN:-$ROOT_DIR/bin/pgy}"

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
    local output

    output="$("$PGY" "$file" --run --backend=llvm 2>&1)"
    for expected in "$@"; do
        if ! grep -Fq "$expected" <<<"$output"; then
            echo "[llvm-smoke] $name failed" >&2
            echo "--- output ---" >&2
            echo "$output" >&2
            echo "--------------" >&2
            exit 1
        fi
    done
    echo "[llvm-smoke] $name ok"
}

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

DATA_FILE="$TMPDIR/io.txt"
cat > "$TMPDIR/string_io.pgy" <<EOF
func Main() -> Void {
    let s: String = Concat(Upper(Trim("  hi")), Lower(" THERE"));
    Log(StringLength(s));
    Log(Contains(s, "HI"));
    WriteFile("$DATA_FILE", Replace(s, "HI", "BYE"));
    let out: String = ReadFile("$DATA_FILE");
    Log(out);
}
EOF
run_case "string_io" "$TMPDIR/string_io.pgy" "8" "true" "BYE there"

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

cat > "$TMPDIR/secure_slot_view.pgy" <<'EOF'
func Main() -> Void {
    let ss: SecureSlot<Int> = ClaimSecureSlot();
    let rv: ReadView<Int> = ViewRead(ss);
    let wv: WriteView<Int> = ViewWrite(ss);
    Write(wv, 5);
    Log(Read(rv));
    Write(wv, 9);
    Log(Read(rv));
}
EOF
run_case "secure_slot_view" "$TMPDIR/secure_slot_view.pgy" "5" "9"

cat > "$TMPDIR/subject_projection.pgy" <<'EOF'
subject Player {
    let hp: Int;
    let name: String;
}

dto PlayerDto {
    hp: Int;
    name: String;
}

object PlayerView {
    hp: Int;
}

func Main() -> Void {
    let player: Player = Player(42, "neo");
    let snapshot: PlayerDto = ToDto(PlayerDto, player);
    let view: PlayerView = ToObject(PlayerView, player);
    Log(snapshot.hp);
    Log(view.hp);
    Log(snapshot.name);
}
EOF
run_case "subject_projection" "$TMPDIR/subject_projection.pgy" "42" "neo"

cat > "$TMPDIR/relation_effect_projection_sync.pgy" <<'EOF'
subject Player {
    let hp: Int;
    let name: String;
}

object PlayerView {
    hp: Int;
}

dto PlayerDto {
    hp: Int;
    name: String;
}

relation TrustedLink for source: Player, target: Player {
    object slot snapshot: PlayerView
    dto slot packet: PlayerDto
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
    dto slot packet: PlayerDto
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

dto PlayerDto {
    hp: Int;
}

effect Poisoned for bearer: Player { }
relation TrustedLink for source: Player, target: Player { }

zone BattleZone {
    subject slot player: Player
    subject slot enemy: Player
    object slot playerView: PlayerView
    dto slot snapshot: PlayerDto
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

cat > "$TMPDIR/zone_layer_projection_runtime.pgy" <<'EOF'
subject Player {
    let hp: Int;
    let name: String;
}

object PlayerView {
    hp: Int;
}

dto PlayerDto {
    name: String;
}

effect Poisoned for bearer: Player {
    object slot view: PlayerView
    refresh view from bearer
}

relation TrustedLink for source: Player, target: Player {
    dto slot packet: PlayerDto
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
    let gameWorld = GameWorld(battle);
    gameWorld.Show();
}
EOF
run_case "world_zone_cross_queries" "$TMPDIR/world_zone_cross_queries.pgy" "true" "true" "true"

cat > "$TMPDIR/subject_class_dispatch.pgy" <<'EOF'
subject ActiveCounter {
    let count: Int;

    func Tick(self, delta: Int) -> Int {
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

cat > "$TMPDIR/secure_slot_actor_cell.pgy" <<'EOF'
actor Bot {
    let hp: Int;
}

func Main() -> Void {
    let s: SecureSlot<Bot> = Bot(7);
    Write(s, Bot(9), s_token);
    Release(s, s_token);
    Log(1);
}
EOF
run_case "secure_slot_actor_cell" "$TMPDIR/secure_slot_actor_cell.pgy" "1"

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
    let ch: Channel<Int> = Channel(4);
    async {
        ch <- 11;
    }
    select {
        case v = <-ch:
            Log(v);
        default:
            Log(0);
    }
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
