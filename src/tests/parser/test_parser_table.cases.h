    const TestCase tests[] = {
        {
            "Basic Let Declaration",
            "let x = 42;\n"
            "let name = \"Pergyra\";\n"
            "let flag = true;",
            1
        },
        {
            "Function Declaration",
            "func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}",
            1
        },
        {
            "Generic Function",
            "func Identity<T>(value: T) -> T {\n"
            "    return value;\n"
            "}",
            1
        },
        {
            "Nested Generic Type Arguments",
            "func Main() -> Void {\n"
            "    let buckets: HashMap<String, List<String>> = MapNew();\n"
            "}",
            1
        },
        {
            "Reserved Generic Type Elision Is Rejected",
            "func Main() -> Void {\n"
            "    let values: List<_> = ListNew();\n"
            "}",
            0
        },
        {
            "Reserved Generic Parameter Placeholder Is Rejected",
            "func Identity<_>(value: Int) -> Int {\n"
            "    return value;\n"
            "}",
            0
        },
        {
            "Reserved Optional Chaining Is Rejected",
            "func Main() -> Void {\n"
            "    Log(user?.name);\n"
            "}",
            0
        },
        {
            "Option Coalescing Parses",
            "func Main() -> Void {\n"
            "    Log(value ?? 0);\n"
            "}",
            1
        },
        {
            "Reserved Attribute Marker Is Rejected",
            "@test\n"
            "func Main() -> Void {\n"
            "    Log(1);\n"
            "}",
            0
        },
        {
            "Reserved Spread Rest Is Rejected",
            "func Main() -> Void {\n"
            "    Log(...items);\n"
            "}",
            0
        },
        {
            "Reserved Default Value Argument Is Rejected",
            "func Add(x: Int = 1) -> Int {\n"
            "    return x;\n"
            "}",
            0
        },
        {
            "Reserved Async Default Value Argument Is Rejected",
            "async func AddAsync(x: Int = 1) -> Int {\n"
            "    return x;\n"
            "}",
            0
        },
        {
            "Reserved Lambda Default Value Argument Is Rejected",
            "func Main() -> Void {\n"
            "    let f = (x: Int = 1) => x;\n"
            "}",
            0
        },
        {
            "Reserved Named Field Destructuring Is Rejected",
            "func Main() -> Void {\n"
            "    let {x, y} = point;\n"
            "}",
            0
        },
        {
            "Function Typed Locals And Returns",
            "func AddOne(x: Int) -> Int {\n"
            "    return x + 1;\n"
            "}\n"
            "func MakeAdder() -> func(Int) -> Int {\n"
            "    return AddOne;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let f: func(Int) -> Int = AddOne;\n"
            "    let g = MakeAdder();\n"
            "    Log(f(4));\n"
            "    Log(g(9));\n"
            "}",
            1
        },
        {
            "Pin Remains Ordinary Identifier Call",
            "func pin(x: Int) -> Void {\n"
            "    Log(x);\n"
            "}\n"
            "func Main() -> Void {\n"
            "    pin(7);\n"
            "}",
            1
        },
        {
            "Pin Lease Block Syntax Parses As Scoped View Block",
            "func Main() -> Void {\n"
            "    let scores: Slot<Int> = ClaimSlot<Int>();\n"
            "    pin scores as view: ReadView<Int> {\n"
            "        Log(view);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Pin Lease Block Rejects Non-Named Source",
            "func Main() -> Void {\n"
            "    pin ClaimSlot<Int>() as view: ReadView<Int> {\n"
            "        Log(view);\n"
            "    }\n"
            "}",
            0
        },
        {
            "Escaped String Literal",
            "func Main() -> Void {\n"
            "    Log(\"{\\\"ok\\\":true}\\n\");\n"
            "}",
            1
        },
        {
            "Async Function With Ref Slot Param",
            "subject WorkerLedger {\n"
            "    let load: Int;\n"
            "}\n"
            "async func Worker(jobs: Channel<Int>, ref ledger: Slot<WorkerLedger>) -> Int {\n"
            "    return 1;\n"
            "}\n",
            1
        },
        {
            "Function with Where Clause",
            "func Sort<T>(items: Array<T>) -> Array<T>\n"
            "    where T: Comparable {\n"
            "    // Implementation\n"
            "    return items;\n"
            "}",
            1
        },
        {
            "Slot Operations",
            "let slot = ClaimSlot<Int>();\n"
            "Write(slot, 42);\n"
            "let value = Read(slot);\n"
            "Release(slot);",
            1
        },
        {
            "With Statement",
            "with slot<String> as s {\n"
            "    s.Write(\"Hello\");\n"
            "    Log(s.Read());\n"
            "}",
            1
        },
        {
            "Secure Slot",
            "with SecureSlot<Int>(SECURITY_LEVEL_HARDWARE) as hp {\n"
            "    hp.Write(100);\n"
            "}",
            1
        },
        {
            "Parallel Block",
            "let result = parallel {\n"
            "    ProcessA();\n"
            "    ProcessB();\n"
            "    ProcessC();\n"
            "};",
            1
        },
        {
            "For Loop",
            "for i in 1..10 {\n"
            "    Log(i);\n"
            "}",
            1
        },
        {
            "If Statement",
            "if x > 10 {\n"
            "    Log(\"Greater\");\n"
            "} else {\n"
            "    Log(\"Less or equal\");\n"
            "}",
            1
        },
        {
            "Class Declaration",
            "class Player<T> where T: Serializable {\n"
            "    private let _name: String;\n"
            "    public let Health: Int;\n"
            "    public func TakeDamage(amount: Int) {\n"
            "        Health = Health - amount;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Top-level Public Subject Declaration",
            "public subject Vault {\n"
            "    let code: Int;\n"
            "}",
            1
        },
        {
            "Top-level Private Zone Declaration",
            "private zone VaultZone {\n"
            "    subject slot owner: Keeper;\n"
            "}",
            1
        },
        {
            "Top-level Public Intent Declaration",
            "public intent Patrol {\n"
            "    who guard: Guard;\n"
            "    step Check {\n"
            "        where: GuardZone;\n"
            "        who: guard;\n"
            "        on: 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Intent Default Who Alias Clause",
            "intent Patrol(guard: Guard) {\n"
            "    who: guard;\n"
            "    step Check {\n"
            "        on: 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Intent Default Where Clause",
            "intent Patrol(guard: Guard) {\n"
            "    where: GuardZone;\n"
            "    step Check {\n"
            "        who: guard;\n"
            "        on: 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Top-level Private Event Declaration",
            "private event HiddenPing(value: Int);",
            1
        },
        {
            "Struct Declaration",
            "struct Vec3 {\n"
            "    x: Float;\n"
            "    y: Float;\n"
            "    z: Float;\n"
            "    func Length() -> Float {\n"
            "        return x;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Extern C Block",
            "extern \"C\" {\n"
            "    func SDL_Init(flags: Int) -> Int;\n"
            "    func SDL_Quit();\n"
            "}\n"
            "func Main() -> Int {\n"
            "    return SDL_Init(0);\n"
            "}",
            1
        },
        {
            "Complex Expression",
            "let result = (a + b * c) / (d - e) && flag || !other;",
            1
        },
        {
            "Method Chaining",
            "let result = entity.Method1().Method2(42).Property;",
            1
        },
        {
            "Using Alias Statement",
            "func Main() -> Void {\n"
            "    using entity.Route() as route;\n"
            "    Log(route);\n"
            "}",
            1
        },
        {
            "Array Access",
            "let value = array[index + 1];\n"
            "matrix[i][j] = value * 2;",
            1
        },
        {
            "Array Literal",
            "let values: Array<Int> = [1, 2, 3];\n"
            "Log(values[1]);",
            1
        },
        {
            "While Loop",
            "func Countdown(n: Int) -> Void {\n"
            "    let count: Int = n;\n"
            "    while count > 0 {\n"
            "        Log(count);\n"
            "        count = count - 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Match Statement",
            "func Classify(n: Int) -> Void {\n"
            "    match n {\n"
            "        case 0:\n"
            "            Log(\"zero\");\n"
            "        case 1:\n"
            "            Log(\"one\");\n"
            "        case 2 if n > 0:\n"
            "            Log(\"two positive\");\n"
            "        default:\n"
            "            Log(\"other\");\n"
            "    }\n"
            "}",
            1
        },
        {
            "Leading Dot Variant Shorthand",
            "enum OptionInt { Some(Int), None }\n"
            "func Wrap(n: Int) -> OptionInt {\n"
            "    if n > 0 {\n"
            "        return .Some(n);\n"
            "    }\n"
            "    return .None;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let value: OptionInt = .Some(7);\n"
            "    match value {\n"
            "        case .Some(v):\n"
            "            Log(v);\n"
            "        case .None:\n"
            "            Log(0);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Full Example",
            "// Fibonacci function\n"
            "func Fibonacci(n: Int) -> Int {\n"
            "    if n <= 1 {\n"
            "        return n;\n"
            "    }\n"
            "    with slot<Int> as prev {\n"
            "        prev.Write(0);\n"
            "        with slot<Int> as curr {\n"
            "            curr.Write(1);\n"
            "            for i in 2..n {\n"
            "                let next = prev.Read() + curr.Read();\n"
            "                prev.Write(curr.Read());\n"
            "                curr.Write(next);\n"
            "            }\n"
            "            return curr.Read();\n"
            "        }\n"
            "    }\n"
            "}",
            1
        },
        {
            "Ability Declaration",
            "ability Damageable {\n"
            "    fields health: Int\n"
            "    func TakeDamage(amount: Int) -> Void {\n"
            "        Log(amount);\n"
            "    }\n"
            "    func GetHealth() -> Int;\n"
            "}",
            1
        },
        {
            "Role Declaration",
            "role PlayerDamageable for Player {\n"
            "    include role BuffableRole\n"
            "    impl ability Damageable {\n"
            "        func TakeDamage(amount: Int) -> Void {\n"
            "            Log(amount);\n"
            "        }\n"
            "    }\n"
            "    override func GetHealth() -> Int {\n"
            "        return 100;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Party Declaration",
            "party DungeonTeam {\n"
            "    role slot tank: Damageable & Guardable\n"
            "    role slot healer: Healing\n"
            "    shared formation: String = \"standard\"\n"
            "    func Execute() -> Void {\n"
            "        Log(formation);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Reserved Role Slot Ability Union Is Rejected",
            "party FlexibleTeam {\n"
            "    role slot support: Healing | Buffing\n"
            "}",
            0
        },
        {
            "Role Slot Boolean And Alias Is Rejected",
            "party DungeonTeam {\n"
            "    role slot tank: Damageable && Guardable\n"
            "}",
            0
        },
        {
            "Reserved Container Role Slot Intersection Is Rejected",
            "party CityDistrict {\n"
            "    role slot citizens: Array<Living & Economic>\n"
            "}",
            0
        },
        {
            "Roster Declaration",
            "roster CombatSystem {\n"
            "    party slot team1: DungeonTeam\n"
            "    party slot team2: DungeonTeam\n"
            "    shared round: Int = 0\n"
            "    func StartRound() -> Void {\n"
            "        Log(round);\n"
            "    }\n"
            "}",
            1
        },
        {
            "World Declaration",
            "world GameWorld {\n"
            "    roster combat: CombatSystem\n"
            "    zone battle: BattleZone\n"
            "    state liveBattle: zone battle\n"
            "    activate liveBattle\n"
            "    maintain battle\n"
            "    deactivate liveBattle\n"
            "    shared tick: Int = 0\n"
            "    func Update() -> Void {\n"
            "        Log(tick);\n"
            "    }\n"
            "}",
            1
        },
        {
            "World As Local Variable",
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let world = GameWorld();\n"
            "    Log(world);\n"
            "}",
            1
        },
        {
            "World Derived States",
            "world GameWorld {\n"
            "    zone battle: BattleZone\n"
            "    state battleReady: zone battle projection playerView\n"
            "    state battleLinked: zone battle layer poison\n"
            "    state battlePoisoned: zone battle state poisoned\n"
            "    state battleVisible: all battleReady, battleLinked\n"
            "    state battleInteresting: any battleVisible, battlePoisoned\n"
            "}",
            1
        },
        {
            "Intent Declaration",
            "subject Player {\n"
            "    let hp: Int;\n"
            "    action pay(self) -> Void { return; }\n"
            "}\n"
            "subject Merchant {\n"
            "    let trust: Int;\n"
            "}\n"
            "ability Payable { func Pay() -> Void; }\n"
            "effect PaymentEffect for bearer: Player { }\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Player\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Player, seller: Merchant) {\n"
            "    exclusive;\n"
            "    rollback: current;\n"
            "    priority: 10;\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        on: buyer.pay();\n"
            "        on: buyer.pay();\n"
            "        compensate: buyer.pay();\n"
            "        pre: buyer.hp >= 0;\n"
            "        guard: buyer.hp >= 0;\n"
            "        requires: Payable;\n"
            "        authorized by: buyer;\n"
            "        causes: PaymentEffect;\n"
            "        post: buyer.hp >= 0;\n"
            "        invariant: buyer.hp >= 0;\n"
            "        expect: buyer.hp >= 0;\n"
            "    }\n"
            "    success: true;\n"
            "    failure: false;\n"
            "}",
            1
        },
        {
            "Intent Step Authorized Requires By",
            "subject Buyer {\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        authorized: buyer;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n",
            0
        },
        {
            "Intent Step Duplicate Requires Clause",
            "subject Buyer {\n"
            "    action Pay(self) -> Void { return; }\n"
            "}\n"
            "zone PaymentZone {\n"
            "    subject slot buyer: Buyer\n"
            "}\n"
            "intent Purchase(payment: PaymentZone, buyer: Buyer) {\n"
            "    step pay {\n"
            "        where: PaymentZone;\n"
            "        using: payment;\n"
            "        who: buyer;\n"
            "        requires: Payable;\n"
            "        requires: Movable;\n"
            "        on: buyer.Pay();\n"
            "    }\n"
            "}\n",
            0
        },
        {
            "Intent Step Duplicate Transfer Clause",
            "zone LoadingZone {\n"
            "}\n"
            "zone DeliveryZone {\n"
            "}\n"
            "intent Deliver(src: LoadingZone, dst: DeliveryZone) {\n"
            "    step handoff {\n"
            "        who: courier;\n"
            "        transfer: src -> dst;\n"
            "        move dst to src;\n"
            "        on: 1;\n"
            "    }\n"
            "}\n",
            0
        },
        {
            "Intent Declaration Duplicate Mode Clause",
            "intent Patrol {\n"
            "    exclusive;\n"
            "    concurrent;\n"
            "}\n",
            0
        },
        {
            "Intent Duplicate Binding Alias Clause",
            "intent Patrol(guard: Guard) {\n"
            "    involves guard: Guard;\n"
            "    step Check {\n"
            "        who: guard;\n"
            "        on: 1;\n"
            "    }\n"
            "}\n",
            0
        },
        {
            "Intent Duplicate Step Name",
            "intent Patrol(guard: Guard) {\n"
            "    step Check {\n"
            "        who: guard;\n"
            "        on: 1;\n"
            "    }\n"
            "    step Check {\n"
            "        who: guard;\n"
            "        on: 2;\n"
            "    }\n"
            "}\n",
            0
        },
        {
            "Intent Step Subintent Clause",
            "subject Buyer {\n"
            "    let hp: Int;\n"
            "}\n"
            "intent Charge(buyer: Buyer) {\n"
            "    step verify {\n"
            "        expect: true;\n"
            "    }\n"
            "}\n"
            "intent Checkout(buyer: Buyer) {\n"
            "    step pay {\n"
            "        intent: Charge(buyer);\n"
            "        expect: true;\n"
            "    }\n"
            "}\n",
            1
        },
        {
            "Limited Domain Keywords As Local Variables",
            "func Main() -> Void {\n"
            "    let zone = 1;\n"
            "    let world = zone;\n"
            "    Log(world);\n"
            "}",
            1
        },
        {
            "Limited Domain Keywords As Parameters",
            "func Main(world: Int, zone: Int, participant: Int) -> Void {\n"
            "    Log(world + zone + participant);\n"
            "}",
            1
        },
        {
            "Reserved Domain Keyword As Local Variable Is Rejected",
            "func Main() -> Void {\n"
            "    let object = 1;\n"
            "}",
            0
        },
        {
            "Reserved Domain Keyword As Parameter Is Rejected",
            "func Main(subject: Int) -> Void {\n"
            "    Log(subject);\n"
            "}",
            0
        },
        {
            "Relation Declaration",
            "relation TrustedLink for source: Player, target: Player {\n"
            "    object slot snapshot: PlayerView\n"
            "    tobject slot packet: LinkDto\n"
            "    bind snapshot from source\n"
            "    bind packet from target\n"
            "    shared trust: Int = 100\n"
            "    func Refresh() -> Void {\n"
            "        Log(trust);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Effect Declaration",
            "effect Poisoned for bearer: Player {\n"
            "    object slot view: PlayerView\n"
            "    tobject slot packet: StatusDto\n"
            "    bind view from bearer\n"
            "    bind packet from bearer\n"
            "    shared stacks: Int = 1\n"
            "    func Tick() -> Void {\n"
            "        Log(stacks);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Zone Declaration",
            "zone DungeonZone {\n"
            "    subject slot player: Player\n"
            "    subject slot enemy: Player\n"
            "    object slot playerView: PlayerView = ToObject(PlayerView, player)\n"
            "    tobject slot playerDto: PlayerDto = ToTObject(PlayerDto, player)\n"
            "    relation slot trust: TrustedLink\n"
            "    effect slot poison: Poisoned\n"
            "    authority player requires Commandable, Damageable\n"
            "    state poisoned: effect poison on player\n"
            "    state allied: relation trust between player, enemy\n"
            "    apply poison to player by player\n"
            "    apply poisoned by player\n"
            "    link trust between player, enemy by player\n"
            "    link allied by player\n"
            "    detach poison from enemy by player\n"
            "    detach poisoned by player\n"
            "    unlink trust between player, enemy by player\n"
            "    unlink allied by player\n"
            "    bind playerView from player by player\n"
            "    bind playerDto from player by player\n"
            "    maintain poison on player by player\n"
            "    maintain trust between player, enemy by player\n"
            "    maintain poisoned by player\n"
            "    maintain allied by player\n"
            "    shared level: Int = 3\n"
            "    func Update() -> Void {\n"
            "        Log(level);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Zone Bind Declaration",
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind playerView from player\n"
            "    bind snapshot from player by player\n"
            "}\n",
            4
        },
        {
            "Zone Bind Group Declaration",
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    bind [playerView, snapshot] from player\n"
            "}\n",
            4
        },
        {
            "Zone Refresh Publish Group Declaration",
            "subject Player { let hp: Int; let name: String; }\n"
            "object PlayerView { hp: Int; }\n"
            "object PlayerCard { hp: Int; }\n"
            "tobject PlayerDto { hp: Int; name: String; }\n"
            "tobject PlayerPacket { hp: Int; name: String; }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    object slot playerView: PlayerView\n"
            "    object slot playerCard: PlayerCard\n"
            "    tobject slot snapshot: PlayerDto\n"
            "    tobject slot packet: PlayerPacket\n"
            "    refresh [playerView, playerCard] from player by player\n"
            "    publish [snapshot, packet] from player by player\n"
            "}\n",
            6
        },
        {
            "Zone Effect Pool Declaration",
            "subject Player { let hp: Int; }\n"
            "effect DamageEffect for bearer: Player { }\n"
            "zone BattleZone {\n"
            "    subject slot player: Player\n"
            "    effect pool damage: DamageEffect capacity 8\n"
            "}\n",
            3
        },
        {
            "Zone Declaration With Vessel Slot",
            "vessel HabitatState {\n"
            "    current: Int;\n"
            "}\n"
            "zone MeadowZone {\n"
            "    vessel slot habitat: HabitatState = HabitatState(3)\n"
            "}\n",
            2
        },
        {
            "Subject Declaration",
            "subject Counter {\n"
            "    let count: Int;\n"
            "    func Increment() -> Void {\n"
            "        count = count + 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Subject Declaration Alias",
            "subject Counter {\n"
            "    let count: Int;\n"
            "    func Increment() -> Void {\n"
            "        count = count + 1;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Break Continue",
            "func Looping() -> Void {\n"
            "    while true {\n"
            "        if false { break; }\n"
            "        continue;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Enum Declaration",
            "enum Color { Red, Green, Blue }\n"
            "func Main() -> Void {\n"
            "    let c: Color = Red;\n"
            "    Log(1);\n"
            "}",
            1
        },
        {
            "Event Lambda Subscription",
            "event OnHit(damage: Int);\n"
            "func Main() -> Void {\n"
            "    OnHit += (d: Int) => { Log(d); };\n"
            "    OnHit(77);\n"
            "}",
            1
        },
        {
            "Exported Function Declaration",
            "export func Add(a: Int, b: Int) -> Int {\n"
            "    return a + b;\n"
            "}",
            1
        },
        {
            "Namespace Export Declaration",
            "namespace Math {\n"
            "    export func Add(a: Int, b: Int) -> Int {\n"
            "        return a + b;\n"
            "    }\n"
            "}",
            1
        },
        {
            "Unsafe Block",
            "func Main() -> Void {\n"
            "    unsafe {\n"
            "        Log(1);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Defer Statement",
            "func Main() -> Void {\n"
            "    defer {\n"
            "        Log(1);\n"
            "    };\n"
            "}",
            1
        },
        {
            "Bind Statement",
            "func Main() -> Void {\n"
            "    bind team.fighter = Warrior;\n"
            "}",
            1
        },
        {
            "Context Identifier Allowed",
            "struct StrategyContext {\n"
            "    let threat: Int;\n"
            "}\n"
            "func ReadThreat(context: StrategyContext) -> Int {\n"
            "    return context.threat;\n"
            "}\n"
            "func Main() -> Void {\n"
            "    let context = StrategyContext(7);\n"
            "    Log(ReadThreat(context));\n"
            "}",
            1
        },
        {
            "Function Type Parameter Syntax",
            "struct StrategyContext {\n"
            "    let morale: Int;\n"
            "}\n"
            "func Apply(base: Int, ctx: StrategyContext, policy: func(Int, StrategyContext) -> Int) -> Int {\n"
            "    return policy(base, ctx);\n"
            "}\n",
            1
        },
        {
            "Else If Chain",
            "func Main() -> Void {\n"
            "    if true {\n"
            "        Log(1);\n"
            "    } else if false {\n"
            "        Log(2);\n"
            "    } else {\n"
            "        Log(3);\n"
            "    }\n"
            "}",
            1
        },
        {
            "Type Alias Declaration",
            "type UserId = Int;",
            1
        },
        {
            "Reserved declaration name after subject is rejected",
            "subject class {\n"
            "    let hp: Int;\n"
            "}\n",
            0
        },
        {
            "Reserved declaration name after zone is rejected",
            "zone effect {\n"
            "}\n",
            0
        },
        {
            "Multiline multiline-string literal",
            "func Main() -> Void {\n"
            "    Log(\"\"\"\n"
            "line1\n"
            "line2\n"
            "\"\"\");\n"
            "}",
            1
        },
        {
            "Multiline literal should not interpolate ${...}",
            "func Main() -> Void {\n"
            "    Log(\"\"\"\n"
            "${not-a-template}\n"
            "\"\"\");\n"
            "}",
            1
        }
    };
