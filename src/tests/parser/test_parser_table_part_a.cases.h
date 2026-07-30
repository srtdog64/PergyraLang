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
            "subject Guard {}\n"
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
            "subject Guard {}\n"
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
