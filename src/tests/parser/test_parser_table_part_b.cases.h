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
